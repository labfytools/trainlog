package com.labfytools.trainlog.data

import android.content.Context
import java.io.File
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

    fun hasFolderAccess(): Boolean =
        directExchangeDirectory() != null

    fun importPcCatalog():
        CatalogInboxResult {
        val directory = directExchangeDirectory()
            ?: return CatalogInboxResult.FolderNotAuthorized

        return importPcCatalogFromDirectory(directory)
    }

    /** Test seam for the production companion ordering on a fresh database. */
    internal fun importPcCatalogFromDirectoryForTest(directory: File): CatalogInboxResult =
        importPcCatalogFromDirectory(directory)

    private fun importPcCatalogFromDirectory(directory: File): CatalogInboxResult {

        val file =
            directory.findDirectFile("trainlog-pc-catalog-v1.json")
                ?: return CatalogInboxResult.FileNotFound

        return try {
            val json = file.readText(Charsets.UTF_8)

            val definitionsError = importPcEquipmentDefinitions(directory)
            if (definitionsError != null) return CatalogInboxResult.Error(definitionsError)
            val profileFile = directory.findDirectFile("trainlog-exercise-profile-state-v1.json")
            val profileJson = profileFile?.readText(Charsets.UTF_8)
            if (profileJson != null) when(val profile=repository.applyExerciseProfileStateJson(profileJson,allowPending=true)) {
                is ExerciseProfileStateImportResult.Applied -> Unit
                is ExerciseProfileStateImportResult.Invalid -> return CatalogInboxResult.Error(profile.message)
                is ExerciseProfileStateImportResult.Conflict -> return CatalogInboxResult.Error("Conflit de profil : ${profile.exerciseId}")
                ExerciseProfileStateImportResult.DatabaseError -> return CatalogInboxResult.Error("Erreur base locale profils.")
            }

            when (
                val result =
                    repository
                        .applyPcCatalogJson(
                            json
                        )
            ) {
                is PcCatalogImportResult.Applied -> {
                    /* WHY: a fresh Android database cannot validate an alias
                     * target until the catalog has created that canonical
                     * exercise. CONTRACT: aliases still precede session import
                     * so retired creator IDs resolve during the same sync. */
                    val aliasesError = importExerciseAliases(directory)
                    if (aliasesError != null) return CatalogInboxResult.Error(aliasesError)
                    if (profileJson != null) when(val profile=repository.applyExerciseProfileStateJson(profileJson)) {
                        is ExerciseProfileStateImportResult.Applied -> Unit
                        is ExerciseProfileStateImportResult.Invalid -> return CatalogInboxResult.Error(profile.message)
                        is ExerciseProfileStateImportResult.Conflict -> return CatalogInboxResult.Error("Conflit de profil : ${profile.exerciseId}")
                        ExerciseProfileStateImportResult.DatabaseError -> return CatalogInboxResult.Error("Erreur base locale profils.")
                    }
                    /* Catalogue identities now exist and the strict second pass
                     * has either installed or verified their causal state. */
                    importPcSessions(directory)?.let { return CatalogInboxResult.Error(it) }
                    importPcEquipmentAssociations(directory)?.let { return CatalogInboxResult.Error(it) }
                    importAiSessionDrafts(directory)?.let { return CatalogInboxResult.Error(it) }
                    importPcBodyZones(directory)?.let { return CatalogInboxResult.Error(it) }
                    importTrainingFeedback(directory)?.let { return CatalogInboxResult.Error(it) }
                    CatalogInboxResult.Imported(
                        imported = result.imported,
                        reconciled = result.reconciled,
                        skipped = result.skipped,
                    )
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

    private fun importPcSessions(directory: File): String? {
        /* CONTRACT: a present V3 artifact is authoritative. Invalid V3 must
         * surface its error and never fall back to a stale V2 snapshot. */
        val v3 = directory.findDirectFile("trainlog-pc-mobile-export-v3.json")
        val file = v3 ?: directory.findDirectFile("trainlog-pc-mobile-export-v2.json") ?: return null
        return try {
            val json = file.readText(Charsets.UTF_8)
            val result = if (v3 != null) repository.applyPcMobileExportV3Json(json)
                else repository.applyPcMobileExportV2Json(json)
            when (result) {
                is MobileSessionImportResult.Applied -> null
                is MobileSessionImportResult.Invalid -> result.message
                MobileSessionImportResult.DatabaseError -> "Erreur base locale séances."
            }
        } catch (error: Exception) { error.message ?: "Import séances impossible." }
    }

    private fun importTrainingFeedback(directory: File): String? {
        /* V2 is authoritative for revision history. V1 remains a fallback for
         * an older peer and can only add deterministic initial revisions. */
        val file = directory.findDirectFile("trainlog-training-feedback-v2.json")
            ?: directory.findDirectFile("trainlog-training-feedback-v1.json") ?: return null
        return try {
            val json = file.readText(Charsets.UTF_8)
            when (val result = repository.applyTrainingFeedbackJson(json)) {
                is TrainingFeedbackImportResult.Applied -> null
                is TrainingFeedbackImportResult.Invalid -> result.message
                is TrainingFeedbackImportResult.Conflict -> "Conflit de ressenti : ${result.stableId}"
                TrainingFeedbackImportResult.DatabaseError -> "Erreur base locale ressentis."
            }
        } catch (error: Exception) { error.message ?: "Import des ressentis impossible." }
    }

    /** Test seam for the real filename-priority boundary; production uses the same method. */
    internal fun importPcSessionsFromDirectoryForTest(directory: File): String? =
        importPcSessions(directory)

    private fun importAiSessionDrafts(directory: File): String? {
        val file = directory.findDirectFile("trainlog-ai-session-drafts-v1.json") ?: return null
        return try {
            val json = file.readText(Charsets.UTF_8)
            when (val result = repository.applyAiSessionDraftsJson(json)) {
                is AiSessionDraftImportResult.Applied -> null
                is AiSessionDraftImportResult.Invalid -> result.message
                AiSessionDraftImportResult.DatabaseError -> "Erreur base locale brouillons IA."
            }
        } catch (error: Exception) {
            error.message ?: "Import des brouillons IA impossible."
        }
    }

    /** Test seam for the production one-sync prerequisite ordering. */
    internal fun importAiSessionDraftsFromDirectoryForTest(directory: File): String? =
        importAiSessionDrafts(directory)

    private fun importPcBodyZones(directory: File): String? {
        val file = directory.findDirectFile("trainlog-exercise-body-zones-v1.json") ?: return null
        return try {
            val json = file.readText(Charsets.UTF_8)
            when (val result = repository.applyExerciseBodyZonesJson(json)) {
                is ExerciseBodyZoneImportResult.Applied -> null
                is ExerciseBodyZoneImportResult.Invalid -> result.message
                is ExerciseBodyZoneImportResult.Conflict ->
                    "Conflit de zones corporelles : ${result.exerciseId}"
                ExerciseBodyZoneImportResult.DatabaseError ->
                    "Erreur base locale zones corporelles."
            }
        } catch (error: Exception) {
            error.message ?: "Import zones corporelles impossible."
        }
    }

    private fun importExerciseAliases(directory: File): String? {
        val file = directory.findDirectFile("trainlog-exercise-aliases-v1.json") ?: return null
        return try {
            val json = file.readText(Charsets.UTF_8)
            when (val result = repository.applyExerciseAliasesJson(json)) {
                is ExerciseAliasImportResult.Applied -> null
                is ExerciseAliasImportResult.Invalid -> result.message
                is ExerciseAliasImportResult.Conflict ->
                    "Conflit d'alias exercice : ${result.sourceExerciseId}"
                ExerciseAliasImportResult.DatabaseError ->
                    "Erreur base locale alias exercice."
            }
        } catch (error: Exception) {
            error.message ?: "Import des alias exercice impossible."
        }
    }

    private fun importPcEquipmentDefinitions(directory: File): String? {
        val file = directory.findDirectFile("trainlog-pc-equipment-definitions-v1.json") ?: return null
        return try {
            val json = file.readText(Charsets.UTF_8)
            when (val result = repository.applyPcEquipmentDefinitionsJson(json)) {
                is EquipmentDefinitionImportResult.Applied -> null
                is EquipmentDefinitionImportResult.Invalid -> result.message
                EquipmentDefinitionImportResult.DatabaseError -> "Erreur base locale définitions équipement."
            }
        } catch (error: Exception) { error.message ?: "Import définitions équipement impossible." }
    }

    private fun importPcEquipmentAssociations(directory: File): String? {
        val file = directory.findDirectFile("trainlog-equipment-associations-v2.json")
            ?: directory.findDirectFile("trainlog-equipment-associations-v1.json")
            ?: return null /* Historic PC sync: absence means no information. */
        return try {
            val json = file.readText(Charsets.UTF_8)
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
        val directory = directExchangeDirectory()
            ?: return SyncReceiptResult.FolderNotAuthorized

        val file =
            directory.findDirectFile("trainlog-sync-receipt-v1.json")
                ?: return SyncReceiptResult.Pending

        return try {
            val json = file.readText(Charsets.UTF_8)

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

    private fun directExchangeDirectory(): File? {
        clearLegacyExchangeTreePreference(appContext)
        if (!hasDirectExchangePermission()) return null
        val directory = canonicalExchangeDirectory()
        if (!directory.exists() && !directory.mkdirs()) return null
        return directory.takeIf { it.isDirectory && it.canRead() && it.canWrite() }
    }

    private fun File.findDirectFile(name: String): File? {
        require('/' !in name && '\\' !in name)
        return File(this, name).takeIf(File::isFile)
    }

}
