package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDetail
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun SessionDetailScreen(
    repository: TrainlogRepository,
    sessionId: String?,
    onBack: () -> Unit,
    onResumeMaxTest: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    var revision by remember(sessionId) { mutableStateOf(0) }
    var editingEntryId by remember(sessionId) { mutableStateOf<String?>(null) }
    var equipmentQuery by remember(sessionId) { mutableStateOf("") }
    var resumeMessage by remember(sessionId) { mutableStateOf<String?>(null) }

    val detail =
        remember(sessionId, revision) {
            sessionId?.let {
                repository.getSessionDetail(
                    it
                )
            }
        }
    val equipmentEntries =
        remember(sessionId, revision) {
            repository.listEquipment()
        }

    TrainlogScreen(
        subtitle = "D E T A I L   S E A N C E"
    ) {
        TrainlogAction(
            label =
                "< Retour à l'historique",
            description =
                "Revenir à la liste des séances.",
            onClick = onBack,
            accent = colors.muted,
        )

        if (detail == null) {
            TrainlogFrame(
                title = "ERREUR"
            ) {
                TrainlogInfo(
                    text =
                        "Séance introuvable.",
                    color =
                        colors.error,
                )
            }

            return@TrainlogScreen
        }

        TrainlogFrame(
            title = "SEANCE"
        ) {
            TrainlogInfo(
                formatStartedAt(
                    detail.summary.startedAt
                )
            )

            TrainlogInfo(
                text =
                    "Type : " +
                        sessionTypeLabel(
                            detail.summary
                                .sessionType
                        ),
                color =
                    if (
                        detail.summary
                            .sessionType ==
                        SessionType.MAX_TEST
                    ) {
                        colors.warning
                    } else {
                        colors.accent
                    },
            )

            TrainlogInfo(
                "${detail.summary.exerciseCount} exercice(s)"
            )

            if (detail.summary.sessionType == SessionType.MAX_TEST) {
                TrainlogAction(
                    label = "Reprendre ce Test max",
                    description = "Continuer la même séance en conservant son identifiant et sa date.",
                    accent = colors.success,
                    onClick = {
                        when (val result = repository.resumeMaxTestSession(detail.summary.sessionId)) {
                            ActiveDraftMutationResult.Saved -> onResumeMaxTest()
                            is ActiveDraftMutationResult.Error -> resumeMessage = result.message
                        }
                    },
                )
                resumeMessage?.let { TrainlogInfo(it, color = colors.error) }
            }
        }

        detail.exercises
            .forEachIndexed {
                    index,
                    exercise ->

                TrainlogFrame(
                    title =
                        "${index + 1}. ${exercise.exerciseName}"
                ) {
                    exercise.equipmentDisplayName?.let { equipment ->
                        TrainlogInfo("Équipement : $equipment", color = colors.muted)
                    }
                    if (editingEntryId == exercise.entryId) {
                        TrainlogInputField(
                            label = "Rechercher une machine",
                            value = equipmentQuery,
                            onValueChange = { equipmentQuery = it },
                        )
                        TrainlogAction(
                            label = "Retirer l'équipement",
                            description = "Conserver l'exercice sans machine associée.",
                            accent = colors.warning,
                            onClick = {
                                if (repository.setCompletedSessionEquipment(detail.summary.sessionId, exercise.entryId, null)) {
                                    revision++; editingEntryId = null; equipmentQuery = ""
                                }
                            },
                        )
                        repository.searchEquipment(equipmentQuery).take(8).forEach { equipment ->
                            TrainlogAction(
                                label = equipment.displayName,
                                description = equipment.labelName.ifBlank { equipment.type },
                                accent = colors.success,
                                onClick = {
                                    if (repository.setCompletedSessionEquipment(detail.summary.sessionId, exercise.entryId, equipment.equipmentId)) {
                                        revision++; editingEntryId = null; equipmentQuery = ""
                                    }
                                },
                            )
                        }
                    } else {
                        TrainlogAction(
                            label = "Modifier l'équipement",
                            description = "Choisir, remplacer ou retirer la machine de cette entrée.",
                            accent = colors.muted,
                            onClick = { editingEntryId = exercise.entryId },
                        )
                    }
                    if (exercise.maxWeightKg != null) {
                        val rendered = "%.2f".format(java.util.Locale.FRANCE, exercise.maxWeightKg)
                            .trimEnd('0').trimEnd(',')
                        TrainlogInfo(
                            text = "Max : $rendered kg",
                            color = colors.warning,
                        )
                    } else if (
                        exercise.recordingMode ==
                        RecordingMode.CONTINUOUS
                    ) {
                        ContinuousDetail(
                            exercise
                        )
                    } else {
                        SetsDetail(
                            exercise = exercise,
                            loadSemantics = equipmentEntries
                                .firstOrNull { it.equipmentId == exercise.equipmentId }
                                ?.loadSemantics,
                        )
                    }
                }
            }
    }
}

@Composable
private fun SetsDetail(
    exercise: SessionExerciseDetail,
    loadSemantics: EquipmentLoadSemantics?,
) {
    val colors =
        LocalTrainlogColors.current

    TrainlogInfo(
        text =
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                "Mode : séries · répétitions"
            } else {
                "Mode : séries · durée"
            },
        color = colors.accent,
    )

    if (exercise.trackingMode == TrackingMode.REPS) {
        val loadHeading =
            if (loadSemantics == EquipmentLoadSemantics.ASSISTANCE) {
                "Assistance (kg)"
            } else {
                "Charge (kg)"
            }
        /* Readable row table: history must expose every persisted value and
         * distinguish an absent load from an explicit zero. */
        TrainlogInfo("Série | Répétitions | $loadHeading", color = colors.muted)
    }

    exercise.sets
        .forEachIndexed {
                index,
                set ->

            TrainlogInfo(
                if (
                    exercise.trackingMode ==
                    TrackingMode.REPS
                ) {
                    val renderedWeight = set.weightKg?.let {
                        "%.2f".format(java.util.Locale.FRANCE, it).trimEnd('0').trimEnd(',')
                    } ?: "—"
                    "${index + 1} | ${set.reps} | $renderedWeight"
                } else {
                    "Série ${index + 1} : ${formatDuration(set.durationSeconds)}"
                }
            )
        }
}

@Composable
private fun ContinuousDetail(
    exercise: SessionExerciseDetail,
) {
    val colors =
        LocalTrainlogColors.current

    TrainlogInfo(
        text =
            "Mode : continu",
        color = colors.accent,
    )

    TrainlogInfo(
        "Durée : ${formatDuration(exercise.continuousDurationSeconds)}"
    )

    if (
        exercise.dataFields and
            ExerciseDataFields.SPEED_KMH != 0
    ) {
        TrainlogInfo(
            "Vitesse : %.1f km/h".format(
                exercise.speedKmh ?: 0.0
            )
        )
    }

    if (
        exercise.dataFields and
            ExerciseDataFields.DISTANCE_KM != 0
    ) {
        TrainlogInfo(
            "Distance : %.2f km".format(
                exercise.distanceKm ?: 0.0
            )
        )
    }
}

private fun formatDuration(
    seconds: Int,
): String {
    if (
        seconds > 0 &&
        seconds % 60 == 0
    ) {
        return "${seconds / 60} min"
    }

    return "$seconds s"
}
