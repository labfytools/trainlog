package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable

@Composable
fun HomeScreen(
    onSession: () -> Unit,
    onExercise: () -> Unit,
    onBody: () -> Unit,
    onHistory: () -> Unit,
) {
    TrainlogScreen(
        subtitle = "A C C U E I L"
    ) {
        TrainlogFrame(
            title = "ENREGISTREMENT"
        ) {
            TrainlogAction(
                label =
                    "Enregistrer une séance",
                description =
                    "Saisir un entraînement et ses exercices.",
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
