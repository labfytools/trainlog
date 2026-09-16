/*
 * Android CompletedSessionCorrectionScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.input.KeyboardType
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
import com.labfytools.trainlog.R

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
    val strings = localizedContext()
    val original = remember(sessionId) { repository.getSessionDetail(sessionId) }
    var drafts by remember(sessionId) {
        mutableStateOf(original?.exercises?.map(SessionExerciseDetail::correctionDraft).orEmpty())
    }
    var pendingSetRemoval by remember { mutableStateOf<Pair<Int, Int>?>(null) }
    var pendingOccurrenceRemoval by remember { mutableStateOf<Int?>(null) }
    var message by remember { mutableStateOf<String?>(null) }
    val decimalText = remember(sessionId) { mutableStateMapOf<String, String>() }

    TrainlogScreen(strings.getString(R.string.route_session_correction), scrollKey = "session-correction:$sessionId") {
        if (original == null) {
            TrainlogInfo(strings.getString(R.string.session_not_found), colors.error)
            return@TrainlogScreen
        }
        TrainlogInfo(strings.getString(R.string.correction_profile_note), colors.muted)
        drafts.forEachIndexed { occurrenceIndex, draft ->
            /* INVARIANT: Compose editor/focus state follows the stable entry,
             * never a position that can be occupied by another occurrence. */
            key(draft.entryId) {
            var reorderVisualOffset by remember(draft.entryId) { mutableStateOf(0f) }
            TrainlogFrame(
                "${occurrenceIndex + 1}. ${draft.exercise.name}",
                modifier = Modifier.graphicsLayer { translationY = reorderVisualOffset },
                titleTrailing = {
                    ExerciseReorderHandle(
                        entryId = draft.entryId,
                        index = occurrenceIndex,
                        itemCount = drafts.size,
                        onMove = { from, to ->
                            /* CONTRACT: completed history is untouched here;
                             * Save owns the sole transactional mutation. */
                            drafts = reorderExerciseOccurrences(drafts, from, to)
                        },
                        onCommit = {},
                        onVisualOffset = { reorderVisualOffset = it },
                    )
                },
            ) {
                draft.plan?.let { plan ->
                    TrainlogInputField(strings.getString(R.string.field_target_sets), plan.sets.toString(), { raw ->
                        raw.toIntOrNull()?.let { value -> replaceDraft(drafts, occurrenceIndex, draft.copy(plan = plan.copy(sets = value))) { drafts = it } }
                    })
                    TrainlogInputField(strings.getString(if (draft.exercise.trackingMode == TrackingMode.REPS) R.string.field_target_reps else R.string.field_target_duration),
                        (plan.reps ?: plan.durationSeconds ?: 0).toString(), { raw -> raw.toIntOrNull()?.let { value ->
                            replaceDraft(drafts, occurrenceIndex, draft.copy(plan = if (draft.exercise.trackingMode == TrackingMode.REPS) plan.copy(reps = value, durationSeconds = null) else plan.copy(reps = null, durationSeconds = value))) { drafts = it }
                        } })
                    TrainlogInputField(strings.getString(R.string.field_rest_seconds), plan.restSeconds.toString(), { raw -> raw.toIntOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(plan = plan.copy(restSeconds = value))) { drafts = it }
                    } })
                    if (plan.weightKg != null) CompletedDecimalField(
                        "${draft.entryId}:target-weight", strings.getString(R.string.field_target_weight),
                        plan.weightKg, decimalText, allowZero = false,
                    ) { value -> replaceDraft(drafts, occurrenceIndex, draft.copy(plan = plan.copy(weightKg = value))) { drafts = it } }
                }
                if (draft.maxWeightKg != null) {
                    CompletedDecimalField(
                        "${draft.entryId}:max", strings.getString(R.string.field_explicit_max),
                        draft.maxWeightKg, decimalText, allowZero = false,
                    ) { value -> replaceDraft(drafts, occurrenceIndex, draft.copy(maxWeightKg = value)) { drafts = it } }
                } else if (draft.exercise.recordingMode == RecordingMode.CONTINUOUS) {
                    TrainlogInputField(strings.getString(R.string.field_continuous_duration), draft.continuousDurationSeconds.toString(), { raw -> raw.toIntOrNull()?.let { value ->
                        replaceDraft(drafts, occurrenceIndex, draft.copy(continuousDurationSeconds = value)) { drafts = it }
                    } })
                    draft.speedKmh?.let { speed -> CompletedDecimalField(
                        "${draft.entryId}:speed", strings.getString(R.string.field_speed), speed,
                        decimalText, allowZero = false,
                    ) { value -> replaceDraft(drafts, occurrenceIndex, draft.copy(speedKmh = value)) { drafts = it } } }
                    draft.distanceKm?.let { distance -> CompletedDecimalField(
                        "${draft.entryId}:distance", strings.getString(R.string.field_distance), distance,
                        decimalText, allowZero = false,
                    ) { value -> replaceDraft(drafts, occurrenceIndex, draft.copy(distanceKm = value)) { drafts = it } } }
                } else {
                    draft.sets.forEachIndexed { setIndex, set ->
                        TrainlogInfo(strings.getString(R.string.set_number, setIndex + 1), colors.accent)
                        TrainlogInputField(strings.getString(if (draft.exercise.trackingMode == TrackingMode.REPS) R.string.field_reps else R.string.field_duration_seconds),
                            (if (draft.exercise.trackingMode == TrackingMode.REPS) set.reps else set.durationSeconds).toString(), { raw -> raw.toIntOrNull()?.let { value ->
                                val changed = if (draft.exercise.trackingMode == TrackingMode.REPS) set.copy(reps = value) else set.copy(durationSeconds = value)
                                replaceSet(drafts, occurrenceIndex, setIndex, changed) { drafts = it }
                            } })
                        if (set.weightKg != null) CompletedDecimalField(
                            "${draft.entryId}:set-weight:$setIndex", strings.getString(R.string.field_weight),
                            set.weightKg, decimalText, allowZero = true,
                        ) { value -> replaceSet(drafts, occurrenceIndex, setIndex, set.copy(weightKg = value)) { drafts = it } }
                        TrainlogDeleteButton(strings.getString(R.string.a11y_delete_set),
                            { pendingSetRemoval = occurrenceIndex to setIndex },
                            Modifier.testTag("remove-completed-set-${draft.entryId}-$setIndex"))
                    }
                    TrainlogAction(strings.getString(R.string.add_set), strings.getString(R.string.add_set_description), {
                        replaceDraft(drafts, occurrenceIndex, draft.copy(sets = draft.sets + SessionSetDraft())) { drafts = it }
                    }, accent = colors.success)
                }
                TrainlogDeleteButton(strings.getString(R.string.delete_occurrence),
                    { pendingOccurrenceRemoval = occurrenceIndex },
                    Modifier.testTag("remove-completed-occurrence-${draft.entryId}"))
            }
            }
        }
        message?.let { TrainlogInfo(it, colors.error) }
        TrainlogPrimaryAction(strings.getString(R.string.save), strings.getString(R.string.correction_save_description), {
            if (decimalText.any { (field, raw) ->
                    val value = parseFiniteDecimal(raw)
                    value == null || if (field.contains(":set-weight:")) value < 0.0 else value <= 0.0
                }) {
                message = strings.getString(R.string.invalid_decimal_value)
                return@TrainlogPrimaryAction
            }
            when (val result = repository.correctCompletedSession(sessionId, SessionDraft(drafts, original.summary.sessionType))) {
                CorrectCompletedSessionResult.Saved -> onFinished(true)
                is CorrectCompletedSessionResult.Invalid -> message = localizedRepositoryMessage(strings, result.message)
                is CorrectCompletedSessionResult.DatabaseError -> message = localizedRepositoryMessage(strings, result.message)
            }
        })
        TrainlogAction(strings.getString(R.string.dialog_cancel), strings.getString(R.string.cancel_no_database_change), { onFinished(false) }, accent = colors.muted)
    }
    pendingSetRemoval?.let { (occurrence, set) ->
        DestructiveConfirmationDialog(strings.getString(R.string.delete_set_question), strings.getString(R.string.delete_set_detail), strings.getString(R.string.delete), { pendingSetRemoval = null },
            deleteContentDescription = localizedContext().getString(com.labfytools.trainlog.R.string.a11y_delete_set)) {
            val draft = drafts[occurrence]
            replaceDraft(drafts, occurrence, draft.copy(sets = draft.sets.filterIndexed { index, _ -> index != set })) { drafts = it }
            pendingSetRemoval = null
        }
    }
    pendingOccurrenceRemoval?.let { occurrence ->
        val draft = drafts[occurrence]
        DestructiveConfirmationDialog(strings.getString(R.string.delete_occurrence_question),
            strings.getString(R.string.occurrence_delete_impact,
                strings.resources.getQuantityString(R.plurals.performed_set_count, draft.sets.size, draft.sets.size),
                if (draft.continuousDurationSeconds > 0) strings.getString(R.string.continuous_activity_suffix) else "",
                if (draft.maxWeightKg != null) strings.getString(R.string.max_suffix) else ""),
            strings.getString(R.string.delete), { pendingOccurrenceRemoval = null },
            deleteContentDescription = strings.getString(R.string.a11y_delete_occurrence)) {
            drafts = drafts.filterIndexed { index, _ -> index != occurrence }
            pendingOccurrenceRemoval = null
        }
    }
}

/**
 * WHY: a controlled numeric model cannot represent intermediate input such as
 * `0,` without destroying what the user is typing.
 * CONTRACT: visible text remains raw and accepts both decimal separators;
 * finite/sign validation is repeated by Save before repository mutation.
 */
@Composable
private fun CompletedDecimalField(
    fieldKey: String,
    label: String,
    value: Double,
    rawValues: MutableMap<String, String>,
    allowZero: Boolean,
    onParsed: (Double) -> Unit,
) {
    val raw = rawValues[fieldKey] ?: presentationNumber(value, 3)
    TrainlogInputField(
        label = label,
        value = raw,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
        onValueChange = { updated ->
            if (decimalEditorTextValid(updated)) {
                rawValues[fieldKey] = updated
                parseFiniteDecimal(updated)?.takeIf { if (allowZero) it >= 0.0 else it > 0.0 }
                    ?.let(onParsed)
            }
        },
    )
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
