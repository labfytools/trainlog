package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDetail
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun SessionDetailScreen(
    repository: TrainlogRepository,
    sessionId: String?,
    onBack: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    val detail =
        remember(sessionId) {
            sessionId?.let {
                repository.getSessionDetail(
                    it
                )
            }
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
                "${detail.summary.exerciseCount} exercice(s)"
            )
        }

        detail.exercises
            .forEachIndexed {
                    index,
                    exercise ->

                TrainlogFrame(
                    title =
                        "${index + 1}. ${exercise.exerciseName}"
                ) {
                    if (
                        exercise.recordingMode ==
                        RecordingMode.CONTINUOUS
                    ) {
                        ContinuousDetail(
                            exercise
                        )
                    } else {
                        SetsDetail(
                            exercise
                        )
                    }
                }
            }
    }
}

@Composable
private fun SetsDetail(
    exercise: SessionExerciseDetail,
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

    exercise.sets
        .forEachIndexed {
                index,
                set ->

            TrainlogInfo(
                if (
                    exercise.trackingMode ==
                    TrackingMode.REPS
                ) {
                    "Série ${index + 1} : ${set.reps} reps"
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
