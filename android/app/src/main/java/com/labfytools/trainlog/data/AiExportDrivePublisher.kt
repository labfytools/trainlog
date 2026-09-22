package com.labfytools.trainlog.data

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import java.io.File
import java.io.FileOutputStream
import java.security.MessageDigest
import java.util.concurrent.TimeUnit
import org.json.JSONObject

internal const val AI_EXPORT_FILENAME = "trainlog_ai_export_v1.json"

internal sealed interface AiExportDriveResult {
    data object NotConfigured : AiExportDriveResult
    data object Missing : AiExportDriveResult
    data object Unchanged : AiExportDriveResult
    data class Published(val sha256: String) : AiExportDriveResult
    data class Error(val message: String) : AiExportDriveResult
}

internal class AiExportDriveSettings(private val context: Context) {
    private val preferences =
        context.applicationContext.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)

    val treeUri: Uri?
        get() = preferences.getString(TREE, null)?.let(Uri::parse)

    val connected: Boolean
        get() = treeUri != null

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
        preferences.edit()
            .remove(TREE)
            .remove(LAST_SHA256)
            .remove(LAST_SIZE)
            .remove(LAST_MODIFIED)
            .apply()
        WorkManager.getInstance(context).cancelUniqueWork(PERIODIC_WORK)
        WorkManager.getInstance(context).cancelUniqueWork(IMMEDIATE_WORK)
    }

    fun alreadyPublished(source: File, sha256: String): Boolean =
        preferences.getString(LAST_SHA256, null) == sha256 &&
            preferences.getLong(LAST_SIZE, -1L) == source.length() &&
            preferences.getLong(LAST_MODIFIED, -1L) == source.lastModified()

    fun needsPublication(source: File): Boolean =
        connected &&
            source.isFile &&
            (
                preferences.getString(LAST_SHA256, null) == null ||
                    preferences.getLong(LAST_SIZE, -1L) != source.length() ||
                    preferences.getLong(LAST_MODIFIED, -1L) != source.lastModified()
            )

    fun recordPublished(source: File, sha256: String) {
        preferences.edit()
            .putString(LAST_SHA256, sha256)
            .putLong(LAST_SIZE, source.length())
            .putLong(LAST_MODIFIED, source.lastModified())
            .apply()
    }

    companion object {
        private const val PREFERENCES = "trainlog-ai-export-drive"
        private const val TREE = "tree_uri"
        private const val LAST_SHA256 = "last_sha256"
        private const val LAST_SIZE = "last_size"
        private const val LAST_MODIFIED = "last_modified"
        private const val PERIODIC_WORK = "trainlog-ai-export-drive-periodic-v1"
        private const val IMMEDIATE_WORK = "trainlog-ai-export-drive-now-v1"

        fun sourceFile(): File = File(canonicalExchangeDirectory(), AI_EXPORT_FILENAME)

        fun schedule(context: Context) {
            val constraints =
                Constraints.Builder()
                    .setRequiredNetworkType(NetworkType.CONNECTED)
                    .build()
            val periodic =
                PeriodicWorkRequestBuilder<AiExportDriveWorker>(15, TimeUnit.MINUTES)
                    .setConstraints(constraints)
                    .build()
            WorkManager.getInstance(context).enqueueUniquePeriodicWork(
                PERIODIC_WORK,
                ExistingPeriodicWorkPolicy.UPDATE,
                periodic,
            )
            enqueueImmediate(context)
        }

        fun enqueueImmediate(context: Context) {
            val settings = AiExportDriveSettings(context)
            val source = sourceFile()
            if (!settings.needsPublication(source)) return
            val request =
                OneTimeWorkRequestBuilder<AiExportDriveWorker>()
                    .setConstraints(
                        Constraints.Builder()
                            .setRequiredNetworkType(NetworkType.CONNECTED)
                            .build()
                    )
                    .build()
            WorkManager.getInstance(context).enqueueUniqueWork(
                IMMEDIATE_WORK,
                ExistingWorkPolicy.KEEP,
                request,
            )
        }
    }
}

internal class AiExportDrivePublisher(private val context: Context) {
    private val settings = AiExportDriveSettings(context)

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                digest.update(buffer, 0, count)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    private fun validateSource(source: File) {
        require(source.isFile && !java.nio.file.Files.isSymbolicLink(source.toPath())) {
            "Export IA local indisponible."
        }
        require(source.length() in 1..64L * 1024L * 1024L) {
            "Export IA local invalide."
        }
        val root =
            source.inputStream().bufferedReader(Charsets.UTF_8).use { reader ->
                JSONObject(reader.readText())
            }
        require(
            root.optString("format") == "TRAINLOG_AI_EXPORT" &&
                root.optInt("version", -1) == 1
        ) { "Format d’export IA invalide." }
    }

    fun publishPending(): AiExportDriveResult {
        val uri = settings.treeUri ?: return AiExportDriveResult.NotConfigured
        val source = AiExportDriveSettings.sourceFile()
        if (!source.isFile) return AiExportDriveResult.Missing
        return try {
            validateSource(source)
            val sourceSha = sha256(source)
            if (settings.alreadyPublished(source, sourceSha)) {
                return AiExportDriveResult.Unchanged
            }
            val root =
                DocumentFile.fromTreeUri(context, uri)
                    ?: return AiExportDriveResult.Error("Dossier Drive IA indisponible.")
            require(root.isDirectory) { "Dossier Drive IA invalide." }
            val destination =
                root.findFile(AI_EXPORT_FILENAME)
                    ?: root.createFile("application/json", AI_EXPORT_FILENAME)
                    ?: throw IllegalStateException("Impossible de créer l’export IA sur Drive.")
            require(destination.isFile) { "Destination Drive IA invalide." }

            context.contentResolver.openOutputStream(destination.uri, "wt")?.use { output ->
                source.inputStream().use { input -> input.copyTo(output, 64 * 1024) }
                output.flush()
            } ?: throw IllegalStateException("Impossible d’écrire l’export IA sur Drive.")

            val verification =
                File.createTempFile("trainlog-ai-drive-verify-", ".json", context.cacheDir)
            try {
                context.contentResolver.openInputStream(destination.uri)?.use { input ->
                    FileOutputStream(verification).use { output ->
                        input.copyTo(output, 64 * 1024)
                        output.fd.sync()
                    }
                } ?: throw IllegalStateException("Impossible de relire l’export IA Drive.")
                require(
                    verification.length() == source.length() &&
                        sha256(verification) == sourceSha
                ) { "Vérification de l’export IA Drive échouée." }
            } finally {
                verification.delete()
            }
            settings.recordPublished(source, sourceSha)
            AiExportDriveResult.Published(sourceSha)
        } catch (error: Exception) {
            AiExportDriveResult.Error(error.message ?: error.javaClass.simpleName)
        }
    }
}

class AiExportDriveWorker(
    context: Context,
    parameters: WorkerParameters,
) : CoroutineWorker(context, parameters) {
    override suspend fun doWork(): Result =
        when (AiExportDrivePublisher(applicationContext).publishPending()) {
            AiExportDriveResult.NotConfigured,
            AiExportDriveResult.Missing,
            AiExportDriveResult.Unchanged,
            is AiExportDriveResult.Published -> Result.success()
            is AiExportDriveResult.Error -> Result.retry()
        }
}
