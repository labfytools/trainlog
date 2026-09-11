package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun HistoryScreen(
    repository: TrainlogRepository,
    onBack: () -> Unit,
    onOpenSession: (String) -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    val sessions =
        remember {
            repository.listSessions()
        }
    val latestMaxima =
        remember {
            repository.listLatestExerciseMaxima()
        }

    TrainlogScreen(
        subtitle = "Séances effectuées"
    ) {
        TrainlogFrame(
            title = "Séances",
            active =
                sessions.isNotEmpty(),
        ) {
            if (sessions.isEmpty()) {
                TrainlogInfo(
                    "Aucune séance enregistrée."
                )
            } else {
                sessions.forEach {
                    session ->

                    TrainlogAction(
                        label =
                            formatStartedAt(
                                session.startedAt
                            ),
                        description =
                            sessionTypeLabel(
                                session.sessionType
                            ) +
                                " · " +
                                "${session.exerciseCount} exercice(s)",
                        onClick = {
                            onOpenSession(
                                session.sessionId
                            )
                        },
                        accent =
                            if (
                                session.sessionType ==
                                SessionType.MAX_TEST
                            ) {
                                colors.warning
                            } else {
                                colors.accent
                            },
                    )
                }
            }
        }

        TrainlogFrame(
            title = "Derniers MAX",
            active = latestMaxima.isNotEmpty(),
        ) {
            if (latestMaxima.isEmpty()) {
                TrainlogInfo("Aucun max explicite enregistré.")
            } else {
                latestMaxima.forEach { max ->
                    val weight = "%.2f".format(java.util.Locale.FRANCE, max.maxWeightKg)
                        .trimEnd('0').trimEnd(',')
                    TrainlogInfo(
                        text = "${max.exerciseName} · $weight kg · ${formatStartedAt(max.startedAt).take(10)}",
                        color = colors.warning,
                    )
                    TrainlogInfo(
                        text = "Équipement : ${max.equipmentDisplayName ?: "aucun"}",
                        color = colors.muted,
                    )
                }
            }
        }
    }
}

internal fun sessionTypeLabel(
    value: SessionType,
): String =
    if (
        value ==
        SessionType.MAX_TEST
    ) {
        "TEST MAX"
    } else {
        "ENTRAÎNEMENT"
    }

internal fun formatStartedAt(
    value: String,
): String {
    return value
        .replace(
            'T',
            ' '
        )
        .take(16)
}
