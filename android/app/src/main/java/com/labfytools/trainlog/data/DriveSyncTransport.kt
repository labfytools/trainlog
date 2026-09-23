package com.labfytools.trainlog.data

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import java.io.File
import java.io.FileOutputStream
import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.security.MessageDigest
import java.time.Duration
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.TimeUnit
import org.json.JSONObject

internal class DriveSyncSettings(private val context: Context) {
    private val preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)

    val treeUri: Uri? get() = preferences.getString(TREE, null)?.let(Uri::parse)
    val connected: Boolean get() = treeUri != null

    fun connect(uri: Uri) {
        context.contentResolver.takePersistableUriPermission(
            uri,
            Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
        )
        preferences.edit().putString(TREE, uri.toString()).apply()
        schedule(context)
    }

    fun disconnect() {
        treeUri?.let { uri ->
            runCatching {
                context.contentResolver.releasePersistableUriPermission(
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                )
            }
        }
        preferences.edit().remove(TREE).apply()
        WorkManager.getInstance(context).cancelUniqueWork(WORK)
    }

    companion object {
        private const val PREFERENCES = "trainlog-drive-sync"
        private const val TREE = "private_tree_uri"
        private const val WORK = "trainlog-drive-sync-v1"

        fun schedule(context: Context) {
            val request = PeriodicWorkRequestBuilder<DriveSyncWorker>(15, TimeUnit.MINUTES)
                .setConstraints(Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build())
                .build()
            WorkManager.getInstance(context).enqueueUniquePeriodicWork(
                WORK,
                ExistingPeriodicWorkPolicy.UPDATE,
                request,
            )
            WorkManager.getInstance(context).enqueue(OneTimeWorkRequestBuilder<DriveSyncWorker>().build())
        }
    }
}

/** Byte-only adapter over one user-selected private Drive folder. */
internal class DriveDocumentTransport(private val context: Context, treeUri: Uri) {
    private val root = DocumentFile.fromTreeUri(context, treeUri)
        ?: throw IllegalStateException("Drive folder is unavailable")
    private val resolver = context.contentResolver
    private val directories = mutableMapOf("" to root)

    private fun segments(relative: String): List<String> {
        val parts = relative.split('/')
        require(parts.size in 1..4 && parts.all { it.isNotEmpty() && it != "." && it != ".." && '\\' !in it })
        return parts
    }

    private fun find(relative: String): DocumentFile? {
        val parts = segments(relative)
        var parent = root
        var prefix = ""
        parts.forEachIndexed { index, name ->
            if (index == parts.lastIndex) return parent.findFile(name)
            prefix = if (prefix.isEmpty()) name else "$prefix/$name"
            parent =
                directories[prefix]
                    ?: parent.findFile(name)?.also {
                        if (it.isDirectory) directories[prefix] = it
                    }
                    ?: return null
        }
        return null
    }

    private fun ensureParent(parts: List<String>): DocumentFile {
        var parent = root
        var prefix = ""
        for (name in parts) {
            prefix = if (prefix.isEmpty()) name else "$prefix/$name"
            parent =
                directories[prefix]
                    ?: parent.findFile(name)
                    ?: parent.createDirectory(name)
                    ?: throw IllegalStateException("Cannot create Drive folder")
            require(parent.isDirectory)
            directories[prefix] = parent
        }
        return parent
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { stream ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val count = stream.read(buffer)
                if (count < 0) break
                digest.update(buffer, 0, count)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    private fun downloadDocument(source: DocumentFile, destination: File): Boolean {
        if (!source.isFile || source.length() > 64L * 1024L * 1024L) return false
        destination.parentFile?.mkdirs()
        val temporary = File(destination.parentFile, ".${destination.name}.drive.tmp-${UUID.randomUUID()}")
        resolver.openInputStream(source.uri)?.use { input ->
            FileOutputStream(temporary).use { output -> input.copyTo(output); output.fd.sync() }
        } ?: return false
        /* WHY: coordination files are deliberately replaced across runs, and
         * File.renameTo() does not reliably replace an existing destination.
         * CONTRACT: consumers observe the old complete file or the newly
         * downloaded complete file, never a partial provider stream.
         * INVARIANT: the temporary is unique and on the destination filesystem. */
        try {
            Files.move(
                temporary.toPath(),
                destination.toPath(),
                StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING,
            )
        } catch (_: AtomicMoveNotSupportedException) {
            Files.move(
                temporary.toPath(),
                destination.toPath(),
                StandardCopyOption.REPLACE_EXISTING,
            )
        } catch (error: Exception) {
            temporary.delete()
            throw error
        }
        return true
    }

    private fun download(relative: String, destination: File): Boolean =
        find(relative)?.let { downloadDocument(it, destination) } ?: false

    private fun upload(relative: String, source: File) {
        require(source.isFile && !java.nio.file.Files.isSymbolicLink(source.toPath()))
        val parts = segments(relative)
        val parent = ensureParent(parts.dropLast(1))
        val destination = parent.findFile(parts.last())
            ?: parent.createFile("application/json", parts.last())
            ?: throw IllegalStateException("Cannot create Drive object")
        resolver.openOutputStream(destination.uri, "wt")?.use { output ->
            source.inputStream().use { it.copyTo(output) }
            output.flush()
        } ?: throw IllegalStateException("Cannot write Drive object")
        val verification = File.createTempFile("trainlog-drive-verify-", ".json", context.cacheDir)
        try {
            /* WHY: some SAF providers do not make a newly created child
             * discoverable by name immediately. CONTRACT: verification reads
             * the exact object returned by createFile/findFile above.
             * INVARIANT: size and SHA-256 are checked before publication can
             * advance to a manifest or coordination reference. */
            require(downloadDocument(destination, verification))
            require(verification.length() == source.length() && sha256(verification) == sha256(source))
        } finally {
            verification.delete()
        }
    }

    fun pull(localRoot: File) {
        localRoot.mkdirs()
        val coordination = listOf(
            "android-peer-v1.json", "android-generation-v1.json", "android-consumption-ack-v1.json",
            "android-archive-acknowledgements-v1.json", "android-generation-error-v1.json",
            "request-v1.json",
            "desktop-archive-acknowledgements-v1.json", "desktop-consumption-ack-v1.json",
            "desktop-generation-v1.json",
        )
        coordination.forEach { download(it, File(localRoot, it)) }
        listOf("android-generation-v1.json", "desktop-generation-v1.json").forEach { name ->
            val reference = File(localRoot, name)
            if (!reference.isFile || reference.length() > 64 * 1024) return@forEach
            val value = JSONObject(reference.readText())
            val relative = value.getString("relative_path")
            val generationParts = relative.split('/')
            require(
                generationParts.size == 3 &&
                    generationParts[0] in setOf("android-objects", "desktop-objects") &&
                    generationParts[1] == "generations" &&
                    generationParts[2] == value.getString("generation_id")
            )
            val manifestPath = "$relative/manifest.json"
            val manifest = File(localRoot, manifestPath)
            if (!download(manifestPath, manifest)) return@forEach
            val artifacts = JSONObject(manifest.readText()).getJSONArray("artifacts")
            require(artifacts.length() <= 32)
            val generationRelative = "${generationParts[1]}/${generationParts[2]}"
            for (index in 0 until artifacts.length()) {
                val filename = artifacts.getJSONObject(index).getString("filename")
                require(filename.substringBeforeLast('/', "") == generationRelative)
                val artifactPath = "${generationParts[0]}/$filename"
                require(download(artifactPath, File(localRoot, artifactPath)))
            }
        }
    }

    fun pushPeer(localRoot: File) {
        upload("android-peer-v1.json", File(localRoot, "android-peer-v1.json"))
    }

    fun pushGeneration(localRoot: File) {
        val reference = File(localRoot, "android-generation-v1.json")
        require(reference.isFile && reference.length() <= 64 * 1024)
        val value = JSONObject(reference.readText())
        val relative = value.getString("relative_path")
        val parts = relative.split('/')
        require(
            parts.size == 3 &&
                parts[0] == "android-objects" &&
                parts[1] == "generations" &&
                parts[2] == value.getString("generation_id")
        )
        val generation = File(localRoot, relative)
        require(generation.isDirectory && !java.nio.file.Files.isSymbolicLink(generation.toPath()))
        val files =
            generation.listFiles()?.filter { it.isFile }?.sortedWith(
                compareBy<File> { it.name == "manifest.json" }
                    .thenBy { it.name },
            ) ?: throw IllegalStateException("Drive generation is unavailable")
        require(files.lastOrNull()?.name == "manifest.json")
        /* WHY: a Drive tree may contain downloaded desktop objects, stale
         * generations, and local staging. Uploading that cache made a single
         * phase exceed WorkManager's execution window and crossed transport
         * ownership boundaries. CONTRACT: Android publishes only the current
         * immutable Android generation. INVARIANT: artifacts precede the
         * manifest commit marker, and the correlated reference is last. */
        files.forEach { upload("$relative/${it.name}", it) }
        upload("android-generation-v1.json", reference)
    }

    fun pushRecoveryAcknowledgements(localRoot: File) {
        upload(
            "android-archive-acknowledgements-v1.json",
            File(localRoot, "android-archive-acknowledgements-v1.json"),
        )
    }

    fun pushAcknowledgement(localRoot: File) {
        upload(
            "android-consumption-ack-v1.json",
            File(localRoot, "android-consumption-ack-v1.json"),
        )
    }
}

class DriveSyncWorker(context: Context, parameters: WorkerParameters) :
    CoroutineWorker(context, parameters) {
    companion object {
        /* WHY: reconnecting Drive updates periodic work and also enqueues an
         * immediate pass; Android may start both before either reaches the
         * Core conversation lock. CONTRACT: only one Drive byte conversation
         * runs in this process. INVARIANT: a duplicate trigger invents no
         * generation and the active worker remains the durable authority. */
        private val running = AtomicBoolean(false)
    }

    override suspend fun doWork(): Result {
        val uri = DriveSyncSettings(applicationContext).treeUri ?: return Result.success()
        if (!running.compareAndSet(false, true)) return Result.success()
        val local = File(applicationContext.filesDir, "sync-drive-v1")
        val transport = DriveDocumentTransport(applicationContext, uri)
        val repository = TrainlogRepository(applicationContext)
        var stage = "pull"
        return try {
            transport.pull(local)
            stage = "coordinate"
            val coordinator = SyncGenerationCoordinator(repository)
            val terminalLedger = BackgroundSyncRunLedger(applicationContext)
            coordinator.publishPeer(local)
            stage = "peer-publication"
            transport.pushPeer(local)
            val actionability = coordinator.classifyBackgroundRequest(local, terminalLedger.terminal())
            if (
                actionability == BackgroundRequestActionability.NOT_ACTIONABLE ||
                    actionability == BackgroundRequestActionability.RESUMABLE_BUT_STALE
            ) {
                if (coordinator.hasReplayableAcknowledgement(local)) {
                    stage = "acknowledgement-replay"
                    transport.pushAcknowledgement(local)
                }
                return Result.success()
            }
            var lastPull = System.nanoTime()
            val boundedPull = {
                val now = System.nanoTime()
                if (now - lastPull >= Duration.ofSeconds(5).toNanos()) {
                    transport.pull(local)
                    lastPull = System.nanoTime()
                }
            }
            val result = coordinator.run(
                local,
                Duration.ofMinutes(10),
                afterPeerPublication = { transport.pushPeer(local) },
                afterRecoveryAcknowledgements = {
                    stage = "recovery-acknowledgement"
                    transport.pushRecoveryAcknowledgements(local)
                },
                afterPublication = {
                    stage = "generation-publication"
                    transport.pushGeneration(local)
                },
                pollTransport = boundedPull,
                afterAcknowledgement = {
                    stage = "acknowledgement"
                    transport.pushAcknowledgement(local)
                },
                intent = SyncConversationIntent.RESUME_BACKGROUND,
            )
            when (result) {
                is ForegroundGenerationResult.Completed -> {
                    terminalLedger.recordTerminal(
                        result.runId,
                        coordinator.remoteEvidence(local, result.runId),
                    )
                    Result.success()
                }
                ForegroundGenerationResult.Busy,
                is ForegroundGenerationResult.Cancelled -> Result.success()
                is ForegroundGenerationResult.Superseded -> {
                    result.runId?.let {
                        terminalLedger.recordTerminal(it, coordinator.remoteEvidence(local, it))
                    }
                    Result.success()
                }
                is ForegroundGenerationResult.Failed -> {
                    result.runId?.let {
                        terminalLedger.recordTerminal(it, coordinator.remoteEvidence(local, it))
                    }
                    Result.retry()
                }
            }
        } catch (error: Exception) {
            /* SECURITY: stage and exception class are fixed application
             * metadata.  Provider messages may contain private URIs and are
             * deliberately never written to logs. */
            Log.w("TrainlogDriveSync", "Drive sync failed at $stage (${error.javaClass.simpleName})")
            Result.retry()
        } finally {
            repository.close()
            running.set(false)
        }
    }
}
