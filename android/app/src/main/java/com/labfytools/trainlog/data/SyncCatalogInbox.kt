package com.labfytools.trainlog.data

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import org.json.JSONObject

sealed interface CatalogInboxResult {
    data class Imported(
        val imported: Int,
        val reconciled: Int,
        val skipped: Int,
    ) : CatalogInboxResult

    data object FolderNotAuthorized :
        CatalogInboxResult

    data object FileNotFound :
        CatalogInboxResult

    data class Error(
        val message: String,
    ) : CatalogInboxResult
}

sealed interface SyncReceiptResult {
    data object Pending :
        SyncReceiptResult

    data object FolderNotAuthorized :
        SyncReceiptResult

    data class Received(
        val syncId: String,
        val success: Boolean,
        val summary: String,
    ) : SyncReceiptResult

    data class Error(
        val message: String,
    ) : SyncReceiptResult
}

class SyncCatalogInbox(
    context: Context,
    private val repository:
        TrainlogRepository,
) {
    private val appContext =
        context.applicationContext

    private val preferences =
        appContext.getSharedPreferences(
            "trainlog-sync",
            Context.MODE_PRIVATE,
        )

    fun hasFolderAccess(): Boolean =
        savedTreeUri() != null

    fun saveTreeUri(
        uri: Uri,
    ): Boolean {
        return try {
            appContext
                .contentResolver
                .takePersistableUriPermission(
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION or
                        Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                )

            preferences
                .edit()
                .putString(
                    KEY_TREE_URI,
                    uri.toString(),
                )
                .apply()

            true
        } catch (
            error: SecurityException
        ) {
            false
        }
    }

    fun importPcCatalog():
        CatalogInboxResult {
        val treeUri =
            savedTreeUri()
                ?: return CatalogInboxResult
                    .FolderNotAuthorized

        val directory =
            DocumentFile
                .fromTreeUri(
                    appContext,
                    treeUri,
                )
                ?: return CatalogInboxResult.Error(
                    "Dossier Trainlog inaccessible."
                )

        val file =
            directory.findFile(
                "trainlog-pc-catalog-v1.json"
            )
                ?: return CatalogInboxResult.FileNotFound

        return try {
            val stream =
                appContext
                    .contentResolver
                    .openInputStream(
                        file.uri
                    )
                    ?: return CatalogInboxResult.Error(
                        "Lecture du catalogue impossible."
                    )

            val json =
                stream.bufferedReader(
                    Charsets.UTF_8
                )
                    .use {
                        it.readText()
                    }

            val definitionsError = importPcEquipmentDefinitions(directory)
            if (definitionsError != null) return CatalogInboxResult.Error(definitionsError)

            when (
                val result =
                    repository
                        .applyPcCatalogJson(
                            json
                        )
            ) {
                is PcCatalogImportResult.Applied ->
                    when (val sessions = importPcSessions(directory)) {
                        null -> when (val equipment = importPcEquipmentAssociations(directory)) {
                            null -> CatalogInboxResult.Imported(
                            imported = result.imported,
                            reconciled = result.reconciled,
                            skipped = result.skipped,
                        )
                            else -> CatalogInboxResult.Error(equipment)
                        }
                        else -> CatalogInboxResult.Error(sessions)
                    }

                is PcCatalogImportResult.Invalid ->
                    CatalogInboxResult.Error(
                        result.message
                    )

                PcCatalogImportResult.DatabaseError ->
                    CatalogInboxResult.Error(
                        "Erreur base locale."
                    )
            }
        } catch (
            error: Exception
        ) {
            CatalogInboxResult.Error(
                error.message
                    ?: "Import catalogue impossible."
            )
        }
    }

    private fun importPcSessions(directory: DocumentFile): String? {
        val file = directory.findFile("trainlog-pc-mobile-export-v2.json") ?: return null
        return try {
            val json = appContext.contentResolver.openInputStream(file.uri)
                ?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }
                ?: return "Lecture snapshot séances V2 impossible."
            when (val result = repository.applyPcMobileExportV2Json(json)) {
                is MobileSessionImportResult.Applied -> null
                is MobileSessionImportResult.Invalid -> result.message
                MobileSessionImportResult.DatabaseError -> "Erreur base locale séances V2."
            }
        } catch (error: Exception) { error.message ?: "Import séances V2 impossible." }
    }

    private fun importPcEquipmentDefinitions(directory: DocumentFile): String? {
        val file = directory.findFile("trainlog-pc-equipment-definitions-v1.json") ?: return null
        return try {
            val json = appContext.contentResolver.openInputStream(file.uri)
                ?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }
                ?: return "Lecture définitions équipement impossible."
            when (val result = repository.applyPcEquipmentDefinitionsJson(json)) {
                is EquipmentDefinitionImportResult.Applied -> null
                is EquipmentDefinitionImportResult.Invalid -> result.message
                EquipmentDefinitionImportResult.DatabaseError -> "Erreur base locale définitions équipement."
            }
        } catch (error: Exception) { error.message ?: "Import définitions équipement impossible." }
    }

    private fun importPcEquipmentAssociations(directory: DocumentFile): String? {
        val file = directory.findFile("trainlog-equipment-associations-v2.json")
            ?: directory.findFile("trainlog-equipment-associations-v1.json")
            ?: return null /* Historic PC sync: absence means no information. */
        return try {
            val json = appContext.contentResolver.openInputStream(file.uri)
                ?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }
                ?: return "Lecture extension équipement impossible."
            when (val result = repository.applyPcEquipmentAssociationsJson(json)) {
                is EquipmentAssociationImportResult.Applied -> null
                is EquipmentAssociationImportResult.Invalid -> result.message
                EquipmentAssociationImportResult.DatabaseError -> "Erreur base locale équipement."
            }
        } catch (error: Exception) {
            error.message ?: "Import extension équipement impossible."
        }
    }

fun readSyncReceipt(
        requestId: String,
    ): SyncReceiptResult {
        val treeUri =
            savedTreeUri()
                ?: return SyncReceiptResult
                    .FolderNotAuthorized

        val directory =
            DocumentFile
                .fromTreeUri(
                    appContext,
                    treeUri,
                )
                ?: return SyncReceiptResult.Error(
                    "Dossier Trainlog inaccessible."
                )

        val file =
            directory.findFile(
                "trainlog-sync-receipt-v1.json"
            )
                ?: return SyncReceiptResult.Pending

        return try {
            val stream =
                appContext
                    .contentResolver
                    .openInputStream(
                        file.uri
                    )
                    ?: return SyncReceiptResult.Error(
                        "Lecture du reçu impossible."
                    )

            val json =
                stream.bufferedReader(
                    Charsets.UTF_8
                )
                    .use {
                        it.readText()
                    }

            val root =
                JSONObject(json)

            if (
                root.optString(
                    "format"
                ) !=
                "trainlog-sync-receipt" ||
                root.optInt(
                    "version",
                    -1
                ) != 1
            ) {
                return SyncReceiptResult.Error(
                    "Reçu de synchronisation invalide."
                )
            }

            if (
                root.optString(
                    "request_id"
                ) != requestId
            ) {
                return SyncReceiptResult.Pending
            }

            val status =
                root.optString(
                    "status"
                )

            if (
                status != "success" &&
                status != "failure"
            ) {
                return SyncReceiptResult.Error(
                    "État de synchronisation invalide."
                )
            }

            SyncReceiptResult.Received(
                syncId =
                    root.optString(
                        "sync_id"
                    ),
                success =
                    status == "success",
                summary =
                    root.optString(
                        "summary",
                        "Synchronisation terminée."
                    ),
            )
        } catch (
            error: Exception
        ) {
            SyncReceiptResult.Error(
                error.message
                    ?: "Lecture du reçu impossible."
            )
        }
    }

    private fun savedTreeUri(): Uri? =
        preferences
            .getString(
                KEY_TREE_URI,
                null,
            )
            ?.let {
                Uri.parse(it)
            }

    private companion object {
        const val KEY_TREE_URI =
            "trainlog_tree_uri"
    }
}
