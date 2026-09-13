package com.labfytools.trainlog.data

import android.content.Context
import android.os.Build
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

sealed interface SyncExportResult {
    data class Exported(
        val displayPath: String,
        val bytes: Int,
    ) : SyncExportResult

    data object Unsupported :
        SyncExportResult

    data class Error(
        val message: String,
    ) : SyncExportResult
}

class SyncExporter private constructor(
    private val repository:
        TrainlogRepository,
    private val safPublisher: ExchangeSafPublisher,
) {
    constructor(context: Context, repository: TrainlogRepository) : this(
        repository,
        ExchangeSafPublisher { persistedExchangeSafDirectory(context) },
    )

    internal constructor(
        context: Context,
        repository: TrainlogRepository,
        directoryProvider: () -> ExchangeSafDirectory?,
    ) : this(repository, ExchangeSafPublisher(directoryProvider))

    /* CONTRACT: repository snapshot construction and every SAF operation are
     * blocking I/O work and must never execute on the Compose/main thread. */
    suspend fun exportMobileBundle(): SyncExportResult = withContext(Dispatchers.IO) {
        when (val opened = openSafSnapshot()) {
            is ExchangeSafSnapshotResult.Ready -> exportMobileBundle(opened.snapshot)
            is ExchangeSafSnapshotResult.Error -> SyncExportResult.Error(opened.message)
        }
    }

    internal fun openSafSnapshot(): ExchangeSafSnapshotResult = safPublisher.snapshot()

    internal fun exportMobileBundle(snapshot: ExchangeSafSnapshot): SyncExportResult {
        if (
            Build.VERSION.SDK_INT <
            Build.VERSION_CODES.Q
        ) {
            return SyncExportResult.Unsupported
        }

        /* CONTRACT: capture every artifact before publishing any of them.
         * The files are separate for compatibility, but a user edit
         * must not make a newly-written V2 reference a definition assembled
         * from a different logical export state. */
        val definitionsJson: String
        val mobileJson: String
        val associationsJson: String
        val bodyZonesJson: String
        val aliasesJson: String
        val feedbackJson: String
        val profileStateJson: String
        try {
            definitionsJson = repository.buildEquipmentDefinitionsJson()
            /* V3 is the authoritative mobile session exchange. V1/V2 remain
             * readable by desktop for historic devices but is not published. */
            mobileJson = repository.buildMobileExportV3Json()
            associationsJson = repository.buildEquipmentAssociationsJson()
            bodyZonesJson = repository.buildExerciseBodyZonesJson()
            aliasesJson = repository.buildExerciseAliasesJson()
            feedbackJson = repository.buildTrainingFeedbackJson()
            profileStateJson = repository.buildExerciseProfileStateJson()
        } catch (error: Exception) {
            return SyncExportResult.Error(
                error.message ?: "Préparation de l'export impossible.",
            )
        }

        val artifacts = listOf(
            "trainlog-mobile-equipment-definitions-v1.json" to definitionsJson,
            "trainlog-mobile-export-v3.json" to mobileJson,
            "trainlog-exercise-body-zones-v1.json" to bodyZonesJson,
            "trainlog-exercise-aliases-v1.json" to aliasesJson,
            "trainlog-training-feedback-v2.json" to feedbackJson,
            "trainlog-exercise-profile-state-v1.json" to profileStateJson,
        )
        /* CONTRACT: the persisted user-authorized SAF tree is the sole
         * outbound directory authority. MediaStore ownership must neither hide
         * a canonical document nor cause creation of a numbered conflict. */
        artifacts.forEach { (displayName, json) ->
            snapshot.writeJson(displayName, json)?.let { error ->
                return SyncExportResult.Error(error)
            }
        }

        /* CONTRACT: publication, not JSON construction, establishes the
         * common sync ancestor. Applying the exact local snapshot can only
         * record equal baselines; the strict reconciler never unions zones. */
        when (val acknowledgement = repository.applyExerciseBodyZonesJson(bodyZonesJson)) {
            is ExerciseBodyZoneImportResult.Applied -> Unit
            is ExerciseBodyZoneImportResult.Conflict ->
                return SyncExportResult.Error(
                    "Conflit pendant l'enregistrement de la baseline zones : " +
                        acknowledgement.exerciseId,
                )
            is ExerciseBodyZoneImportResult.Invalid ->
                return SyncExportResult.Error(acknowledgement.message)
            ExerciseBodyZoneImportResult.DatabaseError ->
                return SyncExportResult.Error(
                    "Enregistrement de la baseline zones impossible.",
                )
        }
        snapshot.writeJson(
            "trainlog-equipment-associations-v2.json",
            associationsJson,
        )?.let { return SyncExportResult.Error(it) }
        return SyncExportResult.Exported(
            displayPath = "Download/Trainlog/trainlog-mobile-export-v3.json",
            bytes = mobileJson.toByteArray(Charsets.UTF_8).size,
        )
    }
}
