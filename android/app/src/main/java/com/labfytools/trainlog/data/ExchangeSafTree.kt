package com.labfytools.trainlog.data

import android.content.Context
import android.os.Build
import android.os.Environment
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream
import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.util.UUID

internal const val SYNC_PREFERENCES_NAME = "trainlog-sync"
internal const val SYNC_TREE_URI_KEY = "trainlog_tree_uri"
internal const val DIRECT_EXCHANGE_LOG_TAG = "TrainlogDirectStorage"
internal const val EXCHANGE_FOLDER_DISPLAY_PATH = "Documents/Trainlog"

/* CONTRACT: direct exchange storage has one fixed public path. The legacy SAF
 * preference is not authority and is removed without touching external files. */
internal fun clearLegacyExchangeTreePreference(context: Context) {
    context.applicationContext.getSharedPreferences(SYNC_PREFERENCES_NAME, Context.MODE_PRIVATE)
        .edit().remove(SYNC_TREE_URI_KEY).apply()
}

internal fun hasDirectExchangePermission(): Boolean =
    Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
        runCatching { Environment.isExternalStorageManager() }.getOrDefault(false)

internal fun canonicalExchangeDirectory(): File =
    File(Environment.getExternalStorageDirectory(), "Documents/Trainlog")

internal fun exchangeArtifactIdentity(displayName: String): String = when (displayName) {
    "trainlog-exercise-body-zones-v1.json" -> "BODY_ZONES"
    "trainlog-sync-request-v1.json" -> "SYNC_REQUEST"
    "trainlog-mobile-export-v3.json" -> "MOBILE_EXPORT"
    "trainlog-mobile-equipment-definitions-v1.json" -> "EQUIPMENT_DEFINITIONS"
    "trainlog-equipment-associations-v2.json" -> "EQUIPMENT_ASSOCIATIONS"
    "trainlog-exercise-aliases-v1.json" -> "EXERCISE_ALIASES"
    "trainlog-training-feedback-v2.json" -> "TRAINING_FEEDBACK"
    "trainlog-exercise-profile-state-v1.json" -> "EXERCISE_PROFILE_STATE"
    else -> displayName
}

internal fun logDirectExchange(displayName: String, phase: String, detail: String? = null) {
    val suffix = detail?.let { " $it" } ?: ""
    Log.d(DIRECT_EXCHANGE_LOG_TAG, "${exchangeArtifactIdentity(displayName)} $phase$suffix")
}

internal fun logDirectExchangeError(displayName: String, phase: String, error: Throwable) {
    Log.e(DIRECT_EXCHANGE_LOG_TAG, "${exchangeArtifactIdentity(displayName)} error phase=$phase " +
        "exception=${error.javaClass.name}:${error.message}")
}

internal data class DirectExchangeDocument(val file: File, val displayName: String)

internal interface DirectExchangeDirectoryAccess {
    fun listDirectChildren(): List<DirectExchangeDocument>
    fun findExact(displayName: String): List<DirectExchangeDocument>
    fun createJson(displayName: String): DirectExchangeDocument?
    fun openForRewrite(document: DirectExchangeDocument): OutputStream?
}

/* WHY: provider writes allowed Samsung to manufacture numbered conflict
 * copies. Direct files make the exact canonical name authoritative.
 * INVARIANT: bytes are fully written and fsync'd in this directory before a
 * same-directory move replaces the canonical path. */
private class DirectExchangeDirectory(private val directory: File) : DirectExchangeDirectoryAccess {
    override fun listDirectChildren(): List<DirectExchangeDocument> =
        directory.listFiles().orEmpty().filter(File::isFile).map { DirectExchangeDocument(it, it.name) }

    override fun findExact(displayName: String): List<DirectExchangeDocument> {
        requireCanonicalLeaf(displayName)
        val candidate = File(directory, displayName)
        return if (candidate.isFile) listOf(DirectExchangeDocument(candidate, displayName)) else emptyList()
    }

    override fun createJson(displayName: String): DirectExchangeDocument {
        requireCanonicalLeaf(displayName)
        return DirectExchangeDocument(File(directory, displayName), displayName)
    }

    override fun openForRewrite(document: DirectExchangeDocument): OutputStream {
        requireCanonicalLeaf(document.displayName)
        require(document.file.parentFile?.canonicalFile == directory.canonicalFile) {
            "Chemin d'artefact hors du dossier Trainlog."
        }
        val temporary = File(directory, ".trainlog-${document.displayName}-${UUID.randomUUID()}.tmp")
        val delegate = FileOutputStream(temporary, false)
        return object : OutputStream() {
            private var closed = false
            private var failed = false
            override fun write(value: Int) = attempt { delegate.write(value) }
            override fun write(bytes: ByteArray, offset: Int, length: Int) =
                attempt { delegate.write(bytes, offset, length) }
            override fun flush() = attempt { delegate.flush() }
            override fun close() {
                if (closed) return
                closed = true
                if (failed) {
                    runCatching { delegate.close() }
                    temporary.delete()
                    return
                }
                try {
                    delegate.flush()
                    delegate.fd.sync()
                    delegate.close()
                    try {
                        Files.move(temporary.toPath(), document.file.toPath(),
                            StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
                    } catch (_: AtomicMoveNotSupportedException) {
                        Files.move(temporary.toPath(), document.file.toPath(),
                            StandardCopyOption.REPLACE_EXISTING)
                    }
                } catch (error: Exception) {
                    runCatching { delegate.close() }
                    temporary.delete()
                    throw error
                }
            }

            private inline fun attempt(operation: () -> Unit) {
                try {
                    operation()
                } catch (error: Exception) {
                    failed = true
                    throw error
                }
            }
        }
    }

    private fun requireCanonicalLeaf(displayName: String) {
        require(displayName.isNotEmpty() && displayName != "." && displayName != "..")
        require('/' !in displayName && '\\' !in displayName) { "Nom d'artefact invalide." }
    }
}

internal fun directExchangeDirectoryForPath(directory: File): DirectExchangeDirectoryAccess? {
    if (!directory.exists() && !directory.mkdirs()) return null
    if (!directory.isDirectory || !directory.canRead() || !directory.canWrite()) return null
    return DirectExchangeDirectory(directory)
}

internal fun directExchangeDirectory(context: Context): DirectExchangeDirectoryAccess? {
    clearLegacyExchangeTreePreference(context)
    if (!hasDirectExchangePermission()) return null
    return directExchangeDirectoryForPath(canonicalExchangeDirectory())
}

internal class DirectExchangePublisher(private val directoryProvider: () -> DirectExchangeDirectoryAccess?) {
    fun snapshot(): DirectExchangeSnapshotResult {
        val directory = directoryProvider()
            ?: return DirectExchangeSnapshotResult.Error("Accès fichiers requis pour Documents/Trainlog.")
        val started = android.os.SystemClock.elapsedRealtime()
        return try {
            logDirectExchange("DIRECT_STORAGE", "snapshot.begin", "thread=${Thread.currentThread().name}")
            val children = directory.listDirectChildren()
            val byName = children.groupByTo(linkedMapOf()) { it.displayName }
            logDirectExchange("DIRECT_STORAGE", "snapshot.success",
                "children=${children.size} duration_ms=${android.os.SystemClock.elapsedRealtime() - started} " +
                    "thread=${Thread.currentThread().name}")
            DirectExchangeSnapshotResult.Ready(DirectExchangeSnapshot(directory, byName))
        } catch (error: Exception) {
            logDirectExchangeError("DIRECT_STORAGE", "snapshot", error)
            DirectExchangeSnapshotResult.Error(error.message ?: "Lecture de Documents/Trainlog impossible.")
        }
    }
}

internal sealed interface DirectExchangeSnapshotResult {
    data class Ready(val snapshot: DirectExchangeSnapshot) : DirectExchangeSnapshotResult
    data class Error(val message: String) : DirectExchangeSnapshotResult
}

internal class DirectExchangeSnapshot(
    private val directory: DirectExchangeDirectoryAccess,
    private val byName: MutableMap<String, MutableList<DirectExchangeDocument>>,
) {
    fun writeJson(displayName: String, json: String): String? {
        var phase = "direct.resolve"
        return try {
            val exact = byName[displayName].orEmpty()
            require(exact.size <= 1) { "DIRECT_STORAGE_DUPLICATE_CANONICAL: nom=$displayName count=${exact.size}" }
            val resolved = exact.singleOrNull() ?: directory.findExact(displayName).also {
                require(it.size <= 1) { "DIRECT_STORAGE_DUPLICATE_CANONICAL: nom=$displayName count=${it.size}" }
            }.singleOrNull()
            val document = resolved ?: directory.createJson(displayName)?.also {
                require(it.displayName == displayName) {
                    "DIRECT_STORAGE_CANONICAL_NAME_CONFLICT: attendu=$displayName obtenu=${it.displayName}"
                }
                byName.getOrPut(displayName) { mutableListOf() }.add(it)
            } ?: return "Création de $displayName impossible."
            phase = "direct.atomicWrite"
            val stream = directory.openForRewrite(document) ?: return "Écriture de $displayName impossible."
            stream.use { it.write(json.toByteArray(Charsets.UTF_8)) }
            logDirectExchange(displayName, "direct.write.success")
            null
        } catch (error: Exception) {
            logDirectExchangeError(displayName, phase, error)
            error.message ?: "Publication de $displayName impossible."
        }
    }
}
