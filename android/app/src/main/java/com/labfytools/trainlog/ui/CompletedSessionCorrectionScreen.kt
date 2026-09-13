package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import com.labfytools.trainlog.data.CorrectCompletedSessionResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExerciseDetail
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

/**
 * WHY: a completed-session correction must be reviewed as one bounded form,
 * not persisted field-by-field while the user is still editing.
 * CONTRACT: initial values are historical occurrence snapshots; Save performs
 * the sole repository mutation and Cancel performs none. Destructive removal
 * always passes through an explicit, non-dismissible confirmation.
 */
@Composable
fun CompletedSessionCorrectionScreen(
    repository: TrainlogRepository,
    sessionId: String,
    onFinished: (saved: Boolean) -> Unit,
) {
    val colors = LocalTrainlogColors.current
    val original = remember(sessionId) { repository.getSessionDetail(sessionId) }
    var drafts by remember(sessionId) {
        mutableStateOf(original?.exercises?.map(SessionExerciseDetail::correctionDraft).orEmpty())
    }
    var pendingSetRemoval by remember { mutableStateOf<Pair<Int, Int>?>(null) }
    var pendingOccurrenceRemoval by remember { mutableStateOf<Int?>(null) }
    var message by remember { mutableStateOf<String?>(null) }

    TrainlogScreen("Modifier la séance", scrollKey = "session-correction:$sessionId") {
        if (original == null) {
            TrainlogInfo("Séance introuvable.", colors.error)
            return@TrainlogScreen
        }
        TrainlogInfo("Les anciennes interprétations de profil sont conservées. Les identifiants de la séance et des occurrences retenues ne changent pas.", colors.muted)
        drafts.forEachIndexed { occurrenceIndex, draft ->
            TrainlogFrame("${occurrenceIndex + 1}. ${draft.exercise.name}") {
                draft.plan?.let { plan ->
                    TrainlogInputField("Nombre de séries cible", plan.sets.toString(), { raw ->
                        raw.toIntOrNull()?.let { value -> replaceDraft(drafts, occurrenceIndex, draft.copy(plan = plan.copy(sets = value))) { drafts = it } }
                    })
                    TrainlogInputField(if (draft.exercise.trackingMode == TrackingMode.REPS) "Répétitions cibles" else "Durée cible par série (s)",
                        (plan.reps ?: plan.durationSeconds ?: 0).toString(), { raw -> raw.toIntOrNull()?.let { value ->
                            replaceDraft(drafts, occurrenceIndex, draft.copy(plan = if (draft.exercise.trackingMode == TrackingMode.REPS) plan.copy(reps = value, durationSeconds = null) else plan.copy(reps = null, durationSeconds = value))) { drafts = it }
                        } })
                    TrainlogInputField("Repos (s)", plan.restSeconds.toString(), { raw -> raw.toIntOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(plan = plan.copy(restSeconds = value))) { drafts = it }
                    } })
                    if (plan.weightKg != null) TrainlogInputField("Charge cible (kg)", plan.weightKg.toString(), { raw -> raw.toDoubleOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(plan = plan.copy(weightKg = value))) { drafts = it }
                    } })
                }
                if (draft.maxWeightKg != null) {
                    TrainlogInputField("MAX explicite (kg)", draft.maxWeightKg.toString(), { raw -> raw.toDoubleOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(maxWeightKg = value)) { drafts = it }
                    } })
                } else if (draft.exercise.recordingMode == RecordingMode.CONTINUOUS) {
                    TrainlogInputField("Durée continue (s)", draft.continuousDurationSeconds.toString(), { raw -> raw.toIntOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(continuousDurationSeconds = value)) { drafts = it }
                    } })
                    draft.speedKmh?.let { speed -> TrainlogInputField("Vitesse (km/h)", speed.toString(), { raw -> raw.toDoubleOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(speedKmh = value)) { drafts = it }
                    } }) }
                    draft.distanceKm?.let { distance -> TrainlogInputField("Distance (km)", distance.toString(), { raw -> raw.toDoubleOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(distanceKm = value)) { drafts = it }
                    } }) }
                } else {
                    draft.sets.forEachIndexed { setIndex, set ->
                        TrainlogInfo("Série ${setIndex + 1}", colors.accent)
                        TrainlogInputField(if (draft.exercise.trackingMode == TrackingMode.REPS) "Répétitions" else "Durée (s)",
                            (if (draft.exercise.trackingMode == TrackingMode.REPS) set.reps else set.durationSeconds).toString(), { raw -> raw.toIntOrNull()?.let { value ->
                                val changed = if (draft.exercise.trackingMode == TrackingMode.REPS) set.copy(reps = value) else set.copy(durationSeconds = value)
                                replaceSet(drafts, occurrenceIndex, setIndex, changed) { drafts = it }
                            } })
                        if (set.weightKg != null) TrainlogInputField("Charge (kg)", set.weightKg.toString(), { raw -> raw.toDoubleOrNull()?.let { value ->
                            replaceSet(drafts, occurrenceIndex, setIndex, set.copy(weightKg = value)) { drafts = it }
                        } })
                        TrainlogIconAction(TrainlogIcons.DeleteOutline, "Supprimer cette série",
                            { pendingSetRemoval = occurrenceIndex to setIndex },
                            Modifier.testTag("remove-completed-set-${draft.entryId}-$setIndex"), colors.error)
                    }
                    TrainlogAction("+ Ajouter une série", "Ajouter explicitement un fait réalisé.", {
                        replaceDraft(drafts, occurrenceIndex, draft.copy(sets = draft.sets + SessionSetDraft())) { drafts = it }
                    }, accent = colors.success)
                }
                TrainlogIconAction(TrainlogIcons.DeleteOutline, "Supprimer cette occurrence",
                    { pendingOccurrenceRemoval = occurrenceIndex },
                    Modifier.testTag("remove-completed-occurrence-${draft.entryId}"), colors.error)
            }
        }
        message?.let { TrainlogInfo(it, colors.error) }
        TrainlogPrimaryAction("Enregistrer", "Appliquer toute la correction dans une transaction.", {
            when (val result = repository.correctCompletedSession(sessionId, SessionDraft(drafts, original.summary.sessionType))) {
                CorrectCompletedSessionResult.Saved -> onFinished(true)
                is CorrectCompletedSessionResult.Invalid -> message = result.message
                is CorrectCompletedSessionResult.DatabaseError -> message = result.message
            }
        })
        TrainlogAction("Annuler", "Quitter sans modifier la base.", { onFinished(false) }, accent = colors.muted)
    }
    pendingSetRemoval?.let { (occurrence, set) ->
        DestructiveConfirmationDialog("Supprimer cette série ?", "Ce fait réalisé disparaîtra de la séance corrigée.", "Supprimer", { pendingSetRemoval = null }) {
            val draft = drafts[occurrence]
            replaceDraft(drafts, occurrence, draft.copy(sets = draft.sets.filterIndexed { index, _ -> index != set })) { drafts = it }
            pendingSetRemoval = null
        }
    }
    pendingOccurrenceRemoval?.let { occurrence ->
        val draft = drafts[occurrence]
        DestructiveConfirmationDialog("Supprimer cette occurrence ?",
            "Impact : ${draft.sets.size} série(s) réalisée(s)${if (draft.continuousDurationSeconds > 0) ", activité continue" else ""}${if (draft.maxWeightKg != null) ", MAX" else ""}. Ses ressentis et révisions seront supprimés avec leur parent.",
            "Supprimer", { pendingOccurrenceRemoval = null }) {
            drafts = drafts.filterIndexed { index, _ -> index != occurrence }
            pendingOccurrenceRemoval = null
        }
    }
}

private fun SessionExerciseDetail.correctionDraft() = SessionExerciseDraft(
    entryId = entryId,
    exercise = ExerciseProfile(exerciseId, exerciseName, exerciseName, recordingMode, trackingMode, dataFields),
    equipmentId = equipmentId,
    maxWeightKg = maxWeightKg,
    plan = plan,
    sets = sets,
    continuousDurationSeconds = continuousDurationSeconds,
    speedKmh = speedKmh,
    distanceKm = distanceKm,
)

private inline fun replaceDraft(items: List<SessionExerciseDraft>, index: Int, value: SessionExerciseDraft, update: (List<SessionExerciseDraft>) -> Unit) =
    update(items.toMutableList().apply { this[index] = value })

private inline fun replaceSet(items: List<SessionExerciseDraft>, occurrence: Int, set: Int, value: SessionSetDraft, update: (List<SessionExerciseDraft>) -> Unit) {
    val draft = items[occurrence]
    replaceDraft(items, occurrence, draft.copy(sets = draft.sets.toMutableList().apply { this[set] = value }), update)
}
