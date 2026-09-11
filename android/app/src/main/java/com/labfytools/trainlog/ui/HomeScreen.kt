package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.LatestExerciseMax
import com.labfytools.trainlog.model.SessionSummary
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun HomeScreen(
    activeDraft: ActiveSessionDraft?,
    draftError: String?,
    latestSession: SessionSummary?,
    latestMaximum: LatestExerciseMax?,
    onSession: () -> Unit,
    onBody: () -> Unit,
    onOpenLatestSession: (String) -> Unit,
    onSync: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    TrainlogScreen("Accueil", scrollKey = "home") {
        draftError?.let { TrainlogInfo(it, colors.error) }
        TrainlogFrame("Séance") {
            if (activeDraft != null) {
                val kind = if (activeDraft.sessionType == SessionType.MAX_TEST) "Test max" else "Entraînement"
                TrainlogPrimaryAction("Reprendre la séance en cours", "$kind · ${activeDraft.exercises.size} exercice(s)", onSession)
            } else {
                TrainlogPrimaryAction("Nouvelle séance manuelle", "Créer explicitement un brouillon durable.", onSession)
            }
        }
        latestSession?.let { session ->
            TrainlogFrame("Dernière séance") {
                TrainlogAction(formatStartedAt(session.startedAt), "${sessionTypeLabel(session.sessionType)} · ${session.exerciseCount} exercice(s)", { onOpenLatestSession(session.sessionId) })
            }
        }
        latestMaximum?.let { maximum ->
            val weight = "%.2f".format(java.util.Locale.FRANCE, maximum.maxWeightKg).trimEnd('0').trimEnd(',')
            TrainlogInfo("Dernier MAX · ${maximum.exerciseName} · $weight kg · ${maximum.startedAt.take(10)}", colors.warning)
        }
        TrainlogFrame("Accès rapides", active = false) {
            TrainlogAction("Mensurations", "Ajouter ou consulter les relevés locaux.", onBody)
            TrainlogAction("Synchronisation", "Consulter l'état connu et lancer une action explicite.", onSync)
        }
    }
}
