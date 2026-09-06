package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import com.labfytools.trainlog.data.TrainlogRepository
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

    TrainlogScreen(
        subtitle = "H I S T O R I Q U E"
    ) {
        TrainlogAction(
            label = "< Retour",
            description =
                "Revenir à l'accueil.",
            onClick = onBack,
            accent = colors.muted,
        )

        TrainlogFrame(
            title = "SEANCES",
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
                            "${session.exerciseCount} exercice(s)",
                        onClick = {
                            onOpenSession(
                                session.sessionId
                            )
                        },
                    )
                }
            }
        }
    }
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
