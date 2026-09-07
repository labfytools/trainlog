package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun HomeScreen(
    activeDraft: ActiveSessionDraft?,
    draftError: String?,
    onSession: () -> Unit,
    onDiscardDraft: () -> Unit,
    onExercise: () -> Unit,
    onBody: () -> Unit,
    onHistory: () -> Unit,
    onSync: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    var confirmingDiscard by
        remember(activeDraft != null) {
            mutableStateOf(false)
        }

    TrainlogScreen(
        subtitle = "A C C U E I L"
    ) {
        if (activeDraft != null) {
            TrainlogFrame(
                title = "SÉANCE EN COURS"
            ) {
                TrainlogAction(
                    label =
                        "Reprendre la séance en cours",
                    description =
                        (if (
                            activeDraft.sessionType ==
                            SessionType.MAX_TEST
                        ) {
                            "Test max"
                        } else {
                            "Entraînement"
                        }) +
                            " · ${activeDraft.exercises.size} exercice(s)",
                    accent = colors.success,
                    onClick = onSession,
                )

                TrainlogAction(
                    label =
                        "Supprimer la séance en cours",
                    description =
                        "Supprimer le brouillon, sans modifier l'historique.",
                    accent = colors.error,
                    onClick = {
                        confirmingDiscard = true
                    },
                )

                if (confirmingDiscard) {
                    TrainlogAction(
                        label =
                            "Confirmer la suppression",
                        description =
                            "Abandonner définitivement cette séance en cours.",
                        accent = colors.error,
                        onClick = {
                            confirmingDiscard = false
                            onDiscardDraft()
                        },
                    )
                    TrainlogAction(
                        label = "Annuler",
                        description =
                            "Conserver la séance en cours.",
                        accent = colors.muted,
                        onClick = {
                            confirmingDiscard = false
                        },
                    )
                }
            }
        }

        if (draftError != null) {
            TrainlogFrame(
                title = "BROUILLON"
            ) {
                TrainlogInfo(
                    text = draftError,
                    color = colors.error,
                )
            }
        }

        TrainlogFrame(
            title = "ENREGISTREMENT"
        ) {
            TrainlogAction(
                label =
                    "Enregistrer une séance",
                description =
                    if (activeDraft == null) {
                        "Saisir un entraînement et ses exercices."
                    } else {
                        "Ouvrir la séance en cours sans l'écraser."
                    },
                onClick = onSession,
            )

            TrainlogAction(
                label =
                    "Enregistrer un exercice",
                description =
                    "Créer une entrée dans le catalogue Trainlog.",
                onClick = onExercise,
            )

            TrainlogAction(
                label =
                    "Enregistrer des mensurations",
                description =
                    "Ajouter un relevé corporel.",
                onClick = onBody,
            )
        }

        TrainlogFrame(
            title = "CONSULTATION"
        ) {
            TrainlogAction(
                label =
                    "Historique des séances",
                description =
                    "Consulter les séances enregistrées et leur détail.",
                onClick = onHistory,
            )
        }

        TrainlogFrame(
            title = "SYNCHRONISATION"
        ) {
            TrainlogAction(
                label = "Synchroniser avec le PC",
                description = "Préparer les données pour le transport MTP.",
                onClick = onSync,
            )
        }

        TrainlogFrame(
            title = "STATUT",
            active = false,
        ) {
            TrainlogInfo(
                "Stockage Android local actif."
            )

            TrainlogInfo(
                "Synchronisation MTP : après les workflows locaux."
            )
        }
    }
}
