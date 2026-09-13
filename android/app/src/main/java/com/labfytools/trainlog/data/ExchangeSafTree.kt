package com.labfytools.trainlog.data

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import android.util.Log
import java.io.OutputStream

internal const val SYNC_PREFERENCES_NAME = "trainlog-sync"
internal const val SYNC_TREE_URI_KEY = "trainlog_tree_uri"
internal const val EXCHANGE_MEDIASTORE_LOG_TAG = "TrainlogMediaStore"

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

/* WHY: document providers may throw the same message from different SAF
 * operations. CONTRACT: traces contain artifact identity, operation phase,
 * URI and exception metadata only; user JSON is never logged. */
internal fun logExchangeMediaStore(displayName: String, phase: String, detail: String? = null) {
    val suffix = detail?.let { " $it" } ?: ""
    Log.d(EXCHANGE_MEDIASTORE_LOG_TAG, "${exchangeArtifactIdentity(displayName)} $phase$suffix")
}

internal fun logExchangeMediaStoreError(displayName: String, phase: String, error: Throwable) {
    Log.e(
        EXCHANGE_MEDIASTORE_LOG_TAG,
        "${exchangeArtifactIdentity(displayName)} error phase=$phase " +
            "exception=${error.javaClass.name}:${error.message}",
    )
}

internal data class ExchangeSafDocument(
    val uri: Uri,
    val displayName: String,
)

internal interface ExchangeSafDirectory {
    fun listDirectChildren(): List<ExchangeSafDocument>
    fun createJson(displayName: String): ExchangeSafDocument?
    fun openForRewrite(document: ExchangeSafDocument): OutputStream?
}

private class DocumentFileExchangeSafDirectory(
    private val context: Context,
    private val directory: DocumentFile,
) : ExchangeSafDirectory {
    override fun listDirectChildren(): List<ExchangeSafDocument> =
        directory.listFiles().mapNotNull { child ->
            child.name?.let { ExchangeSafDocument(child.uri, it) }
        }

    override fun createJson(displayName: String): ExchangeSafDocument? =
        directory.createFile("application/json", displayName)?.let { created ->
            val actualName = created.name
            require(actualName == displayName) {
                "Le fournisseur SAF a créé $actualName au lieu de $displayName."
            }
            ExchangeSafDocument(created.uri, displayName)
        }

    override fun openForRewrite(document: ExchangeSafDocument): OutputStream? =
        context.contentResolver.openOutputStream(document.uri, "wt")
}

internal fun persistedExchangeSafDirectory(context: Context): ExchangeSafDirectory? {
    val appContext = context.applicationContext
    val treeUri = appContext.getSharedPreferences(SYNC_PREFERENCES_NAME, Context.MODE_PRIVATE)
        .getString(SYNC_TREE_URI_KEY, null)?.let(Uri::parse) ?: return null
    val directory = DocumentFile.fromTreeUri(appContext, treeUri) ?: return null
    if (!directory.isDirectory || !directory.canWrite()) return null
    return DocumentFileExchangeSafDirectory(appContext, directory)
}

internal class ExchangeSafPublisher(
    private val directoryProvider: () -> ExchangeSafDirectory?,
) {
    fun snapshot(): ExchangeSafSnapshotResult {
        val directory = directoryProvider()
            ?: return ExchangeSafSnapshotResult.Error(
                "Dossier Trainlog non autorisé ou inaccessible.",
            )
        val started = android.os.SystemClock.elapsedRealtime()
        return try {
            logExchangeMediaStore("SAF_TREE", "snapshot.begin", "thread=${Thread.currentThread().name}")
            val children = directory.listDirectChildren()
            val byName = children.groupByTo(linkedMapOf()) { it.displayName }
            val elapsed = android.os.SystemClock.elapsedRealtime() - started
            logExchangeMediaStore(
                "SAF_TREE",
                "snapshot.success",
                "children=${children.size} duration_ms=$elapsed thread=${Thread.currentThread().name}",
            )
            ExchangeSafSnapshotResult.Ready(ExchangeSafSnapshot(directory, byName))
        } catch (error: Exception) {
            logExchangeMediaStoreError("SAF_TREE", "snapshot", error)
            ExchangeSafSnapshotResult.Error(error.message ?: "Lecture du dossier SAF impossible.")
        }
    }
}

internal sealed interface ExchangeSafSnapshotResult {
    data class Ready(val snapshot: ExchangeSafSnapshot) : ExchangeSafSnapshotResult
    data class Error(val message: String) : ExchangeSafSnapshotResult
}

internal class ExchangeSafSnapshot(
    private val directory: ExchangeSafDirectory,
    private val byName: MutableMap<String, MutableList<ExchangeSafDocument>>,
) {
    fun writeJson(displayName: String, json: String): String? {
        var phase = "saf.resolve"
        return try {
            logExchangeMediaStore(displayName, "saf.resolve.begin")
            val exact = byName[displayName].orEmpty()
            require(exact.size <= 1) {
                "Plusieurs documents SAF canoniques exacts pour $displayName (${exact.size})."
            }
            val existing = exact.singleOrNull()
            val document = if (existing != null) {
                logExchangeMediaStore(displayName, "saf.resolve.existing", "uri=${existing.uri}")
                existing
            } else {
                phase = "saf.create"
                logExchangeMediaStore(displayName, "saf.resolve.absent")
                logExchangeMediaStore(displayName, "saf.create.begin", "mime=application/json")
                directory.createJson(displayName)?.also { created ->
                    /* INVARIANT: creation mutates this transaction-local index
                     * so later artifacts never rescan the live directory. */
                    byName.getOrPut(displayName) { mutableListOf() }.add(created)
                    logExchangeMediaStore(displayName, "saf.create.success", "uri=${created.uri}")
                } ?: return "Création SAF de $displayName impossible."
            }
            phase = "saf.openOutputStream"
            logExchangeMediaStore(
                displayName,
                "saf.write.begin",
                "uri=${document.uri} mode=wt thread=${Thread.currentThread().name}",
            )
            val stream = directory.openForRewrite(document)
                ?: return "Écriture SAF de $displayName impossible."
            phase = "saf.stream.write"
            stream.use {
                it.write(json.toByteArray(Charsets.UTF_8))
                it.flush()
            }
            logExchangeMediaStore(displayName, "saf.write.success", "uri=${document.uri}")
            null
        } catch (error: Exception) {
            logExchangeMediaStoreError(displayName, phase, error)
            error.message ?: "Publication SAF de $displayName impossible."
        }
    }
}
