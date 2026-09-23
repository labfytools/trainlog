/*
 * Android SessionScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

/* TRAINLOG_ANDROID_MAX_TEST_SESSION_V1 */

/* TRAINLOG_ANDROID_SESSION_REMOVE */

/* TRAINLOG_VARIABLE_SET_REPS_V1 */

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.data.FinalizeActiveDraftResult
import com.labfytools.trainlog.data.ManualPercentMaxResult
import com.labfytools.trainlog.data.SaveFeedbackResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.TrainlogRepository.SessionExerciseTimingResult
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.SessionLoadMode
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography
import java.text.Normalizer
import java.util.Locale
import com.labfytools.trainlog.R

@Composable
fun SessionScreen(
    repository: TrainlogRepository,
    catalogRevision: Int,
    onBack: () -> Unit,
    onCreateExercise: () -> Unit,
    onCreateEquipment: (editingEntryId: String?) -> Unit,
    onSessionSaved: () -> Unit,
    restoreEditingEntryId: String? = null,
    onEditingContextRestored: () -> Unit = {},
) {
    val colors =
        LocalTrainlogColors.current
    val strings = localizedContext()

    val exercises =
        remember(
            catalogRevision
        ) {
            repository.listExercises()
        }

    val initialLoad =
        remember(catalogRevision) {
            repository.loadActiveSessionDraft()
        }

    var activeDraft by
        remember(catalogRevision) {
            val loadedDraft = (initialLoad as? ActiveDraftLoadResult.Loaded)?.draft
            val restoredIndex = restoreEditingEntryId?.let { entryId ->
                loadedDraft?.exercises?.indexOfFirst { it.entryId == entryId }
                    ?.takeIf { it >= 0 }
            }
            mutableStateOf(
                if (restoredIndex == null) loadedDraft else loadedDraft?.let { draft ->
                    draft.copy(
                        form = draft.form.copy(
                            editingExerciseIndex = restoredIndex,
                            editingEntryId = restoreEditingEntryId,
                        ),
                    )
                }
            )
        }

    LaunchedEffect(restoreEditingEntryId) {
        if (restoreEditingEntryId != null) onEditingContextRestored()
    }

    var message by
        remember(catalogRevision) {
            mutableStateOf<
                String?
            >(
                when (initialLoad) {
                    is ActiveDraftLoadResult.Error ->
                        localizedRepositoryMessage(strings, initialLoad.message)

                    ActiveDraftLoadResult.None ->
                        strings.getString(R.string.current_session_none)

                    is ActiveDraftLoadResult.Loaded ->
                        initialLoad.warning?.let { localizedRepositoryMessage(strings, it) }
                }
            )
        }

    var confirmingDiscard by
        remember {
            mutableStateOf(false)
        }

    var feedbackEntryId by remember { mutableStateOf<String?>(null) }
    var editingFeedbackId by remember { mutableStateOf<String?>(null) }
    var pendingRemoval by remember { mutableStateOf<Pair<Int, SessionExerciseDraft>?>(null) }
    var pendingDestructiveEdit by remember { mutableStateOf<Pair<Int, SessionExerciseDraft>?>(null) }
    var feedbackRevision by remember { mutableStateOf(0) }
    var reorderedDuringGesture by remember { mutableStateOf(false) }
    var timelineRevision by remember { mutableStateOf(0) }
    var pendingExerciseSwitch by remember { mutableStateOf<Pair<String, String>?>(null) }
    var pendingFinalizeActiveEntryId by remember { mutableStateOf<String?>(null) }

    val persistDraft:
        (ActiveSessionDraft, String?) -> Unit =
        { updated, successMessage ->
            when (
                val result =
                    repository
                        .saveActiveSessionDraft(
                            updated
                        )
            ) {
                ActiveDraftMutationResult.Saved -> {
                    activeDraft = updated
                    message = successMessage
                }

                is ActiveDraftMutationResult.Error -> {
                    message =
                        strings.getString(R.string.draft_unsaved, localizedRepositoryMessage(strings, result.message))
                }
            }
        }

    TrainlogScreen(
        subtitle = strings.getString(R.string.route_session_editor)
    ) {
        if (activeDraft == null) {
            TrainlogFrame(
                title = strings.getString(R.string.error_title)
            ) {
                TrainlogInfo(
                    text = message.orEmpty(),
                    color = colors.error,
                )
            }
            return@TrainlogScreen
        }

        val currentDraft = activeDraft!!
        val exerciseTimings =
            remember(timelineRevision, currentDraft.exercises.map { it.entryId }) {
                repository.listActiveSessionExerciseTimings()
            }
        val timingByEntry = exerciseTimings.associateBy { it.entryId }
        val activeTiming = exerciseTimings.firstOrNull { it.active }

        val lastWriteFailed =
            message?.startsWith(
                strings.getString(R.string.draft_unsaved_title)
            ) == true || message?.startsWith(
                strings.getString(R.string.exercise_reorder_failed, "")
            ) == true
        TrainlogInfo(
            text =
                if (lastWriteFailed) {
                    strings.getString(R.string.last_change_unsaved)
                } else {
                    strings.getString(R.string.session_saved_locally)
                },
            color =
                if (lastWriteFailed) {
                    colors.error
                } else {
                    colors.muted
                },
        )

        TrainlogFrame(
            title = strings.getString(R.string.session_type)
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                val training = currentDraft.sessionType == SessionType.TRAINING
                TrainlogButton(
                    label = strings.getString(R.string.training),
                    modifier = Modifier.weight(1f),
                    style = if (training) TrainlogButtonStyle.SUCCESS else TrainlogButtonStyle.SECONDARY,
                    maxLines = 2,
                    onClick = {
                    persistDraft(
                        currentDraft.copy(sessionType = SessionType.TRAINING),
                        null,
                    )
                    },
                )
                val maxTest = currentDraft.sessionType == SessionType.MAX_TEST
                TrainlogButton(
                    label = strings.getString(R.string.session_max_test),
                    modifier = Modifier.weight(1f),
                    style = if (maxTest) TrainlogButtonStyle.SUCCESS else TrainlogButtonStyle.SECONDARY,
                    maxLines = 2,
                    onClick = {
                    persistDraft(
                        currentDraft.copy(sessionType = SessionType.MAX_TEST),
                        null,
                    )
                    },
                )
                val cardio = currentDraft.sessionType == SessionType.CARDIO
                TrainlogButton(
                    label = strings.getString(R.string.session_cardio),
                    modifier = Modifier.weight(1f),
                    style = if (cardio) TrainlogButtonStyle.SUCCESS else TrainlogButtonStyle.SECONDARY,
                    maxLines = 2,
                    onClick = {
                    persistDraft(
                        currentDraft.copy(sessionType = SessionType.CARDIO),
                        null,
                    )
                    },
                )
            }
        }

        ExercisePicker(
            exercises = exercises,
            selectedExercise = currentDraft.form.selectedExercise,
            onExerciseSelected = { exercise ->
                /* CONTRACT: only a tapped catalogue result changes the durable
                 * form identity. The transient query is never an exercise. */
                persistDraft(
                    currentDraft.copy(
                        form = currentDraft.form.copy(selectedExercise = exercise),
                    ),
                    null,
                )
            },
        )

        TrainlogFrame(
            title = strings.getString(R.string.route_session_editor)
        ) {
            TrainlogInfo(
                text =
                    strings.getString(
                        R.string.session_type_value,
                        when (currentDraft.sessionType) {
                            SessionType.TRAINING -> strings.getString(R.string.session_training)
                            SessionType.MAX_TEST -> strings.getString(R.string.session_max_test)
                            SessionType.CARDIO -> strings.getString(R.string.session_cardio)
                        },
                    ),
                color =
                    when (currentDraft.sessionType) {
                        SessionType.MAX_TEST -> colors.warning
                        SessionType.CARDIO -> colors.success
                        SessionType.TRAINING -> colors.accent
                    },
            )

            if (
                currentDraft.exercises.isEmpty()
            ) {
                TrainlogInfo(
                    strings.getString(R.string.exercise_none_added)
                )
            } else {
                currentDraft.exercises
                    .forEachIndexed {
                            index,
                            draft ->

                        key(draft.entryId) {
                        var reorderVisualOffset by remember(draft.entryId) { mutableStateOf(0f) }
                        Column(Modifier.graphicsLayer { translationY = reorderVisualOffset }) {

                        Row(
                            Modifier.fillMaxWidth().heightIn(min = 48.dp),
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            BasicText(
                                text = "${index + 1}. ${draftSummary(draft)}",
                                modifier = Modifier.weight(1f).padding(vertical = 12.dp),
                                style = TrainlogTypography.normal.copy(color = colors.text),
                            )
                            ExerciseReorderHandle(
                                entryId = draft.entryId,
                                index = index,
                                itemCount = currentDraft.exercises.size,
                                onMove = { from, to ->
                                    activeDraft = reorderActiveSessionDraft(currentDraft, from, to)
                                    reorderedDuringGesture = true
                                },
                                onCommit = {
                                    if (reorderedDuringGesture) {
                                        val reordered = activeDraft
                                        if (reordered != null) when (val result = repository.saveActiveSessionDraft(reordered)) {
                                            ActiveDraftMutationResult.Saved -> {
                                                message = strings.getString(R.string.exercise_order_saved)
                                            }
                                            is ActiveDraftMutationResult.Error -> {
                                                activeDraft = when (val durable = repository.loadActiveSessionDraft()) {
                                                    is ActiveDraftLoadResult.Loaded -> durable.draft
                                                    else -> currentDraft
                                                }
                                                message = strings.getString(
                                                    R.string.exercise_reorder_failed,
                                                    localizedRepositoryMessage(strings, result.message),
                                                )
                                            }
                                        }
                                        reorderedDuringGesture = false
                                    }
                                },
                                onVisualOffset = { reorderVisualOffset = it },
                            )
                        }
                        draft.plan?.let { plan ->
                            TrainlogInfo(
                                strings.getString(R.string.plan_summary, plan.sets,
                                    (plan.reps ?: plan.durationSeconds).toString(), plan.restSeconds,
                                    plan.weightKg?.let { strings.getString(R.string.target_load_inline, it) }
                                        ?: strings.getString(R.string.no_numeric_load_proposed)),
                                color = colors.muted,
                            )
                            if (draft.sets.isEmpty()) TrainlogInfo(
                                strings.getString(R.string.performed_sets_none),
                                color = colors.warning,
                            )
                        }

                        val timing = timingByEntry[draft.entryId]
                        when {
                            timing == null -> {
                                TrainlogButton(
                                    label = strings.getString(R.string.exercise_timeline_start),
                                    onClick = {
                                        when (
                                            val result =
                                                repository.startActiveSessionExercise(draft.entryId)
                                        ) {
                                            is SessionExerciseTimingResult.Started -> {
                                                timelineRevision++
                                                message =
                                                    strings.getString(
                                                        R.string.exercise_timeline_started,
                                                        draft.exercise.name,
                                                    )
                                            }
                                            is SessionExerciseTimingResult.ActiveConflict -> {
                                                pendingExerciseSwitch =
                                                    result.activeEntryId to draft.entryId
                                            }
                                            is SessionExerciseTimingResult.Invalid ->
                                                message = result.message
                                            is SessionExerciseTimingResult.DatabaseError ->
                                                message = result.message
                                            is SessionExerciseTimingResult.Finished -> Unit
                                        }
                                    },
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .testTag("start-exercise-${draft.entryId}"),
                                    style = TrainlogButtonStyle.SUCCESS,
                                )
                            }
                            timing.active -> {
                                TrainlogButton(
                                    label = strings.getString(R.string.exercise_timeline_finish),
                                    onClick = {
                                        when (
                                            val result =
                                                repository.finishActiveSessionExercise(draft.entryId)
                                        ) {
                                            is SessionExerciseTimingResult.Finished -> {
                                                timelineRevision++
                                                message =
                                                    strings.getString(
                                                        R.string.exercise_timeline_finished,
                                                        draft.exercise.name,
                                                    )
                                            }
                                            is SessionExerciseTimingResult.Invalid ->
                                                message = result.message
                                            is SessionExerciseTimingResult.DatabaseError ->
                                                message = result.message
                                            is SessionExerciseTimingResult.Started,
                                            is SessionExerciseTimingResult.ActiveConflict -> Unit
                                        }
                                    },
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .testTag("finish-exercise-${draft.entryId}"),
                                    containerColor = colors.warning,
                                    style = TrainlogButtonStyle.SECONDARY,
                                )
                            }
                            else -> {
                                TrainlogButton(
                                    label = strings.getString(R.string.exercise_timeline_completed),
                                    onClick = {},
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .testTag("completed-exercise-${draft.entryId}"),
                                    enabled = false,
                                    style = TrainlogButtonStyle.SECONDARY,
                                )
                            }
                        }

                        if (
                            currentDraft.sessionType == SessionType.CARDIO &&
                            timing?.active == true
                        ) {
                            CardioGuidancePanel(
                                repository = repository,
                                entryId = draft.entryId,
                            )
                        }

                        TrainlogAction(
                            label = strings.getString(R.string.modify_name, draft.exercise.name),
                            description = strings.getString(R.string.edit_exercise_description),
                            accent = colors.accent,
                            modifier = Modifier.testTag("edit-draft-exercise-${draft.entryId}"),
                            onClick = {
                                persistDraft(
                                    currentDraft.copy(
                                        form = formForExistingExercise(draft, index),
                                    ),
                                    strings.getString(R.string.editor_opened, draft.exercise.name),
                                )
                            },
                        )
                        if (currentDraft.form.editingEntryId == draft.entryId) {
                            TrainlogInfo(strings.getString(R.string.editing_scroll_hint, draft.exercise.name), color = colors.accent)
                        }

                        val draftFeedback = remember(feedbackRevision, draft.entryId) {
                            repository.listDraftExerciseFeedback(draft.entryId)
                        }
                        draftFeedback.forEach { feedback ->
                            TrainlogInfo(
                                text = strings.getString(R.string.feedback_value, feedback.rawText,
                                    if (feedback.modified) strings.getString(R.string.modified_suffix) else ""),
                                color = colors.muted,
                            )
                            TrainlogAction(strings.getString(R.string.modify), strings.getString(R.string.feedback_edit_description),
                                onClick = { editingFeedbackId = feedback.feedbackId }, accent = colors.accent)
                            if (editingFeedbackId == feedback.feedbackId) FeedbackEditor(
                                initialText = feedback.rawText, editing = true,
                                onSave = { text -> when (val result = repository.reviseDraftExerciseFeedback(feedback.feedbackId, text)) {
                                    is SaveFeedbackResult.Saved -> { feedbackRevision++; editingFeedbackId = null; null }
                                    is SaveFeedbackResult.Invalid -> localizedRepositoryMessage(strings, result.message)
                                    is SaveFeedbackResult.DatabaseError -> localizedRepositoryMessage(strings, result.message)
                                } }, onCancel = { editingFeedbackId = null })
                        }
                        TrainlogAction(
                            label = if (draftFeedback.isEmpty()) {
                                strings.getString(R.string.feedback_voice)
                            } else {
                                strings.getString(R.string.feedback_add_count, draftFeedback.size)
                            },
                            description = strings.getString(R.string.feedback_now_description),
                            accent = colors.success,
                            modifier = Modifier.testTag("draft-feedback-${draft.entryId}"),
                            onClick = { feedbackEntryId = draft.entryId },
                        )
                        if (feedbackEntryId == draft.entryId) {
                            FeedbackEditor(
                                onSave = { text ->
                                    when (val result = repository.saveDraftExerciseFeedback(draft.entryId, text)) {
                                        is SaveFeedbackResult.Saved -> {
                                            feedbackRevision += 1
                                            feedbackEntryId = null
                                            message = strings.getString(R.string.feedback_saved_current)
                                            null
                                        }
                                        is SaveFeedbackResult.Invalid -> localizedRepositoryMessage(strings, result.message)
                                        is SaveFeedbackResult.DatabaseError -> localizedRepositoryMessage(strings, result.message)
                                    }
                                },
                                onCancel = { feedbackEntryId = null },
                            )
                        }

                        if (currentDraft.sourceSessionId == null) {
                            TrainlogDeleteButton(
                                contentDescription = localizedContext().getString(com.labfytools.trainlog.R.string.a11y_delete_exercise),
                                modifier = Modifier.testTag("delete-draft-exercise-${draft.entryId}"),
                                onClick = {
                                    pendingRemoval = index to draft
                                },
                            )
                        }
                        }
                        }
                    }
            }
        }

        val editingExercise =
            currentDraft.form.selectedExercise

        if (editingExercise != null) {
            SessionExerciseForm(
                key =
                    "${editingExercise.exerciseId}:${currentDraft.sessionType.wireValue}:" +
                        currentDraft.form.editingEntryId.orEmpty(),
                repository = repository,
                exercise =
                    editingExercise,
                sessionType = currentDraft.sessionType,
                initialForm =
                    currentDraft.form,
                initialOccurrence = currentDraft.form.editingExerciseIndex?.let {
                    currentDraft.exercises.getOrNull(it)
                },
                onCreateEquipment = {
                    onCreateEquipment(currentDraft.form.editingEntryId)
                },
                onFormChanged = {
                    form ->
                        persistDraft(
                            currentDraft.copy(
                                form = form.copy(
                                    editingExerciseIndex = currentDraft.form.editingExerciseIndex,
                                    editingEntryId = currentDraft.form.editingEntryId,
                                )
                            ),
                            null,
                        )
                },
                onCancel = {
                    persistDraft(
                        currentDraft.copy(
                            form =
                                SessionDraftForm()
                        ),
                        null,
                    )
                },
                onAdd = {
                    draft ->
                        val editIndex = currentDraft.form.editingExerciseIndex
                        val previous = editIndex?.let(currentDraft.exercises::getOrNull)
                        if (editIndex != null && previous != null &&
                            (previous.exercise.recordingMode != draft.exercise.recordingMode ||
                                previous.exercise.trackingMode != draft.exercise.trackingMode) &&
                            (previous.sets.isNotEmpty() || previous.continuousDurationSeconds > 0 ||
                                previous.maxWeightKg != null || previous.plan != null)) {
                            pendingDestructiveEdit = editIndex to draft
                        } else persistDraft(
                            currentDraft.copy(
                                exercises = editIndex?.let { replacingIndex ->
                                    currentDraft.exercises.mapIndexed { index, existing ->
                                        /* INVARIANT: normal performed-value edits
                                         * preserve generator planning metadata. */
                                        if (index == replacingIndex) draft else existing
                                    }
                                } ?: (currentDraft.exercises + draft),
                                form =
                                    SessionDraftForm(),
                            ),
                            if (editIndex == null) strings.getString(R.string.exercise_added) else strings.getString(R.string.exercise_modified),
                        )
                },
            )
        }

        TrainlogFrame(
            title = strings.getString(R.string.actions),
            active =
                currentDraft.exercises.isNotEmpty(),
        ) {
            Row(Modifier.fillMaxWidth().height(IntrinsicSize.Min).testTag("active-session-actions-row"),
                horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TrainlogButton(strings.getString(R.string.create_new_exercise), onCreateExercise,
                    Modifier.weight(1f).fillMaxHeight().testTag("active-session-create-exercise"),
                    containerColor = colors.surfaceAlt, maxLines = 2)
                TrainlogButton(strings.getString(R.string.session_save), onClick = {
                    val activeEntryId = repository.activeSessionTimelineContext()?.activeEntryId
                    if (activeEntryId != null) {
                        pendingFinalizeActiveEntryId = activeEntryId
                    } else {
                        when (
                            val result =
                                repository.finalizeActiveSessionDraft()
                        ) {
                            is FinalizeActiveDraftResult.Saved -> {
                                onSessionSaved()
                                onBack()
                            }

                            is FinalizeActiveDraftResult.Invalid -> {
                                message = localizedRepositoryMessage(strings, result.message)
                            }

                            is FinalizeActiveDraftResult.DatabaseError -> {
                                message =
                                    strings.getString(
                                        R.string.finalize_failed,
                                        localizedRepositoryMessage(strings, result.message),
                                    )
                            }
                        }
                    }
                }, modifier = Modifier.weight(1f).fillMaxHeight().testTag("active-session-save"),
                    containerColor = colors.success, maxLines = 2)
            }
            TrainlogInfo(strings.getString(R.string.session_exercise_total,
                strings.resources.getQuantityString(R.plurals.exercise_count, currentDraft.exercises.size, currentDraft.exercises.size)), colors.muted)

            TrainlogDeleteButton(
                contentDescription = localizedContext().getString(com.labfytools.trainlog.R.string.a11y_delete_current_session),
                modifier = Modifier.testTag("delete-active-draft"),
                onClick = {
                    confirmingDiscard = true
                },
            )

            if (confirmingDiscard) {
                DestructiveConfirmationDialog(
                    title = strings.getString(R.string.discard_session_question),
                    detail = strings.getString(R.string.discard_session_detail,
                        strings.resources.getQuantityString(R.plurals.exercise_count, currentDraft.exercises.size, currentDraft.exercises.size)),
                    confirmLabel = strings.getString(R.string.discard),
                    onCancel = { confirmingDiscard = false },
                    deleteContentDescription = localizedContext().getString(com.labfytools.trainlog.R.string.a11y_delete_current_session),
                    onConfirm = {
                        when (
                            val result =
                                repository
                                    .discardActiveSessionDraft()
                        ) {
                            ActiveDraftMutationResult.Saved -> {
                                confirmingDiscard = false
                                onBack()
                            }

                            is ActiveDraftMutationResult.Error -> {
                                message = localizedRepositoryMessage(strings, result.message)
                            }
                        }
                    },
                )
            }

            if (
                message != null
            ) {
                TrainlogInfo(
                    text =
                        message.orEmpty(),
                    color =
                        if (
                            message ==
                            strings.getString(R.string.exercise_added) ||
                            message ==
                            strings.getString(R.string.exercise_removed) ||
                            message == strings.getString(R.string.exercise_order_saved) ||
                            currentDraft.exercises.any { exercise ->
                                message ==
                                    strings.getString(
                                        R.string.exercise_timeline_started,
                                        exercise.exercise.name,
                                    ) ||
                                    message ==
                                    strings.getString(
                                        R.string.exercise_timeline_finished,
                                        exercise.exercise.name,
                                    )
                            }
                        ) {
                            colors.success
                        } else {
                            colors.error
                        }
                )
            }
        }

        pendingRemoval?.let { (index, draft) ->
            val feedbackCount = repository.listDraftExerciseFeedback(draft.entryId).size
            val facts = buildList {
                if (draft.sets.isNotEmpty()) add(strings.resources.getQuantityString(R.plurals.performed_set_count, draft.sets.size, draft.sets.size))
                if (draft.continuousDurationSeconds > 0) add(strings.getString(R.string.continuous_fact))
                if (draft.maxWeightKg != null) add(strings.getString(R.string.max_fact))
                if (feedbackCount > 0) add(strings.resources.getQuantityString(R.plurals.feedback_count, feedbackCount, feedbackCount))
            }
            DestructiveConfirmationDialog(
                title = strings.getString(R.string.remove_exercise_question, draft.exercise.name),
                detail = if (facts.isEmpty()) strings.getString(R.string.remove_occurrence_detail) else
                    strings.getString(R.string.delete_facts_detail, facts.joinToString("\n- ")),
                confirmLabel = strings.getString(R.string.delete),
                onCancel = { pendingRemoval = null },
                deleteContentDescription = localizedContext().getString(com.labfytools.trainlog.R.string.a11y_delete_exercise),
                onConfirm = {
                    persistDraft(currentDraft.copy(exercises = currentDraft.exercises.filterIndexed { i, _ -> i != index }),
                        strings.getString(R.string.exercise_removed))
                    pendingRemoval = null
                },
            )
        }
        pendingDestructiveEdit?.let { (index, replacement) ->
            val previous=currentDraft.exercises[index]
            val losses=buildList {
                if(previous.sets.isNotEmpty())add(strings.resources.getQuantityString(R.plurals.performed_set_count, previous.sets.size, previous.sets.size))
                if(previous.continuousDurationSeconds>0)add(strings.getString(R.string.continuous_fact))
                if(previous.maxWeightKg!=null)add(strings.getString(R.string.max_fact))
                if(previous.plan!=null)add(strings.getString(R.string.plan_rest_fact))
            }
            DestructiveConfirmationDialog(
                title=strings.getString(R.string.change_profile_question, previous.exercise.name),
                detail=strings.getString(R.string.incompatible_change_detail, losses.joinToString("\n- ")),
                confirmLabel=strings.getString(R.string.replace),
                onCancel={pendingDestructiveEdit=null},
                onConfirm={
                    persistDraft(currentDraft.copy(exercises=currentDraft.exercises.mapIndexed{i,item->if(i==index)replacement else item},form=SessionDraftForm()),strings.getString(R.string.exercise_modified))
                    pendingDestructiveEdit=null
                },
            )
        }

        pendingExerciseSwitch?.let { (activeEntryId, targetEntryId) ->
            val activeName =
                currentDraft.exercises.firstOrNull { it.entryId == activeEntryId }
                    ?.exercise?.name
                    ?: strings.getString(R.string.exercise_title)
            val targetName =
                currentDraft.exercises.firstOrNull { it.entryId == targetEntryId }
                    ?.exercise?.name
                    ?: strings.getString(R.string.exercise_title)
            AlertDialog(
                onDismissRequest = { pendingExerciseSwitch = null },
                title = { Text(strings.getString(R.string.exercise_timeline_switch_title)) },
                text = {
                    Text(
                        strings.getString(
                            R.string.exercise_timeline_switch_detail,
                            activeName,
                            targetName,
                        ),
                    )
                },
                confirmButton = {
                    TextButton(
                        onClick = {
                            when (
                                val finished =
                                    repository.finishActiveSessionExercise(activeEntryId)
                            ) {
                                is SessionExerciseTimingResult.Finished -> {
                                    when (
                                        val started =
                                            repository.startActiveSessionExercise(targetEntryId)
                                    ) {
                                        is SessionExerciseTimingResult.Started -> {
                                            timelineRevision++
                                            message =
                                                strings.getString(
                                                    R.string.exercise_timeline_started,
                                                    targetName,
                                                )
                                        }
                                        is SessionExerciseTimingResult.Invalid ->
                                            message = started.message
                                        is SessionExerciseTimingResult.DatabaseError ->
                                            message = started.message
                                        is SessionExerciseTimingResult.ActiveConflict,
                                        is SessionExerciseTimingResult.Finished -> Unit
                                    }
                                }
                                is SessionExerciseTimingResult.Invalid ->
                                    message = finished.message
                                is SessionExerciseTimingResult.DatabaseError ->
                                    message = finished.message
                                is SessionExerciseTimingResult.Started,
                                is SessionExerciseTimingResult.ActiveConflict -> Unit
                            }
                            pendingExerciseSwitch = null
                        },
                    ) {
                        Text(strings.getString(R.string.exercise_timeline_switch_confirm))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { pendingExerciseSwitch = null }) {
                        Text(strings.getString(R.string.dialog_cancel))
                    }
                },
            )
        }

        pendingFinalizeActiveEntryId?.let { activeEntryId ->
            val activeName =
                currentDraft.exercises.firstOrNull { it.entryId == activeEntryId }
                    ?.exercise?.name
                    ?: strings.getString(R.string.exercise_title)
            AlertDialog(
                onDismissRequest = { pendingFinalizeActiveEntryId = null },
                title = { Text(strings.getString(R.string.exercise_timeline_finalize_title)) },
                text = {
                    Text(
                        strings.getString(
                            R.string.exercise_timeline_finalize_detail,
                            activeName,
                        ),
                    )
                },
                confirmButton = {
                    TextButton(
                        onClick = {
                            when (
                                val finished =
                                    repository.finishActiveSessionExercise(activeEntryId)
                            ) {
                                is SessionExerciseTimingResult.Finished -> {
                                    timelineRevision++
                                    when (val result = repository.finalizeActiveSessionDraft()) {
                                        is FinalizeActiveDraftResult.Saved -> {
                                            pendingFinalizeActiveEntryId = null
                                            onSessionSaved()
                                            onBack()
                                        }
                                        is FinalizeActiveDraftResult.Invalid ->
                                            message = localizedRepositoryMessage(strings, result.message)
                                        is FinalizeActiveDraftResult.DatabaseError ->
                                            message =
                                                strings.getString(
                                                    R.string.finalize_failed,
                                                    localizedRepositoryMessage(strings, result.message),
                                                )
                                    }
                                }
                                is SessionExerciseTimingResult.Invalid ->
                                    message = finished.message
                                is SessionExerciseTimingResult.DatabaseError ->
                                    message = finished.message
                                is SessionExerciseTimingResult.Started,
                                is SessionExerciseTimingResult.ActiveConflict -> Unit
                            }
                            pendingFinalizeActiveEntryId = null
                        },
                    ) {
                        Text(strings.getString(R.string.exercise_timeline_finalize_confirm))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { pendingFinalizeActiveEntryId = null }) {
                        Text(strings.getString(R.string.dialog_cancel))
                    }
                },
            )
        }
    }
}

@Composable
private fun CatalogChoice(
    exercise: ExerciseProfile,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val colors =
        LocalTrainlogColors.current

    val profile =
        exerciseProfileLabel(
            exercise
        )

    Row(
        modifier =
            Modifier
                .then(modifier)
                .fillMaxWidth()
                .padding(vertical = 2.dp)
                .heightIn(min = 48.dp)
                .background(
                    if (selected) {
                        colors.surfaceAlt
                    } else {
                        colors.surface
                    }
                )
                .clickable(
                    onClick = onClick,
                )
                .padding(
                    horizontal = 10.dp,
                    vertical = 9.dp,
                )
    ) {
        BasicText(
            text =
                (
                    if (selected) {
                        "▌ "
                    } else {
                        "  "
                    }
                ) +
                    exercise.name +
                    "  ·  " +
                    profile,
            style =
                TrainlogTypography.normal.copy(
                    color =
                        if (selected) {
                            colors.warning
                        } else {
                            colors.text
                        },
                    fontWeight =
                        if (selected) {
                            FontWeight.Bold
                        } else {
                            FontWeight.Normal
                        },
                ),
        )
    }
}

/**
 * WHY: the session picker keeps query state outside [SessionDraftForm] so
 * typing and cancelling are harmless to the selected exercise and raw values.
 * CONTRACT: matching is presentation-only prefix matching; it never changes
 * stored names or treats query text as a catalogue identity.
 * INVARIANT: [exercises] is the complete, stable repository catalogue, so a
 * movement already used in this session remains selectable for another entry.
 */
@Composable
private fun ExercisePicker(
    exercises: List<ExerciseProfile>,
    selectedExercise: ExerciseProfile?,
    onExerciseSelected: (ExerciseProfile) -> Unit,
) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    var searchOpen by remember(selectedExercise?.exerciseId) {
        mutableStateOf(selectedExercise == null)
    }
    var query by remember(selectedExercise?.exerciseId) { mutableStateOf("") }
    val results = remember(exercises, query) { exercisePrefixMatches(exercises, query) }

    TrainlogFrame(title = strings.getString(R.string.exercise_title), active = exercises.isNotEmpty()) {
        if (exercises.isEmpty()) {
            TrainlogInfo(strings.getString(R.string.exercise_none))
            return@TrainlogFrame
        }

        TrainlogAction(
            label = selectedExercise?.name ?: strings.getString(R.string.choose_exercise),
            description = strings.getString(R.string.exercise_picker_description),
            accent = if (selectedExercise == null) colors.muted else colors.success,
            modifier = Modifier.testTag("exercise-picker-open"),
            onClick = { searchOpen = true },
        )

        TrainlogInputField(
            label = strings.getString(R.string.search_exercise),
            value = query,
            onValueChange = {
                query = it
                searchOpen = true
            },
            testTag = "exercise-search-input",
        )

        if (!searchOpen) {
            return@TrainlogFrame
        }

        if (results.isEmpty()) {
            TrainlogInfo(strings.getString(R.string.exercise_none_found_short), color = colors.muted)
        } else {
            /* The viewport stays keyboard-friendly while LazyColumn preserves
             * access to every prefix result instead of truncating it. */
            LazyColumn(
                modifier = Modifier.heightIn(max = 280.dp).testTag("exercise-search-results"),
            ) {
                items(results, key = { it.exerciseId }) { exercise ->
                    CatalogChoice(
                        exercise = exercise,
                        selected = selectedExercise?.exerciseId == exercise.exerciseId,
                        modifier = Modifier.testTag("exercise-search-result-${exercise.exerciseId}"),
                        onClick = {
                            onExerciseSelected(exercise)
                            query = ""
                            searchOpen = false
                        },
                    )
                }
            }
        }

        if (selectedExercise != null) {
            TrainlogAction(
                label = strings.getString(R.string.cancel_search),
                description = strings.getString(R.string.keep_named_values, selectedExercise.name),
                accent = colors.muted,
                onClick = {
                    query = ""
                    searchOpen = false
                },
            )
        }
    }
}

internal fun exercisePrefixMatches(
    exercises: List<ExerciseProfile>,
    query: String,
): List<ExerciseProfile> {
    val normalizedQuery = normalizeExerciseSearchText(query)
    return exercises.filter { exercise ->
        normalizedQuery.isEmpty() ||
            normalizeExerciseSearchText(exercise.name).startsWith(normalizedQuery)
    }
}

private fun normalizeExerciseSearchText(value: String): String =
    Normalizer.normalize(value, Normalizer.Form.NFD)
        .replace("\\p{M}+".toRegex(), "")
        .lowercase(Locale.ROOT)
        .trim()
        .replace("\\s+".toRegex(), " ")

internal enum class ManualTargetChoice { KG, PERCENT_MAX, NONE }

/**
 * Build occurrence planning metadata separately from performed rows.
 * CONTRACT: an existing generated/manual dose keeps its sets/reps/duration/rest;
 * only the user-selected load target changes. A new manual occurrence derives
 * its initial dose shape from the confirmed performed-row form without copying
 * target weight into any performed set.
 */
internal fun buildManualTargetPlan(
    draft: SessionExerciseDraft,
    existing: SessionExercisePlan?,
    choice: ManualTargetChoice,
    directKgText: String,
    percentResult: ManualPercentMaxResult?,
    equipmentSemantics: EquipmentLoadSemantics?,
): Result<SessionExercisePlan?> {
    if (choice == ManualTargetChoice.NONE) return Result.success(null)
    val weight = when (choice) {
        ManualTargetChoice.KG -> directKgText.trim().replace(',', '.').toDoubleOrNull()
            ?.takeIf { it.isFinite() && it > 0.0 }
            ?: return Result.failure(IllegalArgumentException(
                "target_positive"))
        ManualTargetChoice.PERCENT_MAX ->
            (percentResult as? ManualPercentMaxResult.Available)?.targetWeightKg
                ?: return Result.failure(IllegalArgumentException(
                    (percentResult as? ManualPercentMaxResult.Unavailable)?.message
                        ?: "max_unavailable"))
        ManualTargetChoice.NONE -> error("handled above")
    }
    val mode = when (equipmentSemantics) {
        EquipmentLoadSemantics.EXTERNAL -> SessionLoadMode.EXTERNAL
        EquipmentLoadSemantics.ASSISTANCE -> if (choice == ManualTargetChoice.PERCENT_MAX)
            return Result.failure(IllegalArgumentException(
                "max_assistance_unavailable"))
        else SessionLoadMode.ASSISTANCE
        else -> return Result.failure(IllegalArgumentException(
            "compatible_load_equipment_required"))
    }
    val dose = existing ?: when (draft.exercise.trackingMode) {
        TrackingMode.REPS -> SessionExercisePlan(
            sets = draft.sets.size,
            reps = draft.sets.firstOrNull()?.reps,
        )
        TrackingMode.DURATION -> SessionExercisePlan(
            sets = draft.sets.size,
            durationSeconds = draft.sets.firstOrNull()?.durationSeconds,
        )
    }
    if (dose.sets !in 1..MAX_SESSION_SETS ||
        (draft.exercise.trackingMode == TrackingMode.REPS &&
            (dose.reps == null || dose.reps !in 1..MAX_REPS_PER_SET)) ||
        (draft.exercise.trackingMode == TrackingMode.DURATION &&
            (dose.durationSeconds == null || dose.durationSeconds <= 0)))
        return Result.failure(IllegalArgumentException(
            "positive_dose_required"))
    return Result.success(dose.copy(weightKg = weight, loadMode = mode))
}

@Composable
private fun SessionExerciseForm(
    key: String,
    repository: TrainlogRepository,
    exercise: ExerciseProfile,
    sessionType: SessionType,
    initialForm: SessionDraftForm,
    initialOccurrence: SessionExerciseDraft?,
    onCreateEquipment: () -> Unit,
    onFormChanged: (SessionDraftForm) -> Unit,
    onCancel: () -> Unit,
    onAdd:
        (SessionExerciseDraft) ->
            Unit,
) {
    val colors =
        LocalTrainlogColors.current
    val strings = localizedContext()
    val initialPlan = initialOccurrence?.plan

    var setCountText by
        remember(key) {
            mutableStateOf(
                initialForm.setCountText
            )
        }

    val initialSetRows =
        remember(key) {
            rawSetRowsFromForm(initialForm)
        }

    var repsText by
        remember(key) {
            mutableStateOf(
                encodeRawReps(initialSetRows)
            )
        }

    var weightText by
        remember(key) {
            mutableStateOf(
                encodeRawWeights(initialSetRows)
            )
        }

    /* CONTRACT: SETS + REPS is edited as occurrence-owned rows. The raw
     * strings (including blanks and invalid fragments) are mirrored into the
     * durable form after every mutation, while the saved occurrence is only
     * replaced when the user confirms the add-to-session action. */
    var setRows by
        remember(key) {
            mutableStateOf(
                initialSetRows
            )
        }
    var pendingSetRemoval by remember(key) { mutableStateOf<Int?>(null) }

    var maxWeightText by
        remember(key) {
            mutableStateOf(initialForm.maxWeightText)
        }

    var durationText by
        remember(key) {
            mutableStateOf(
                initialForm.durationText
            )
        }

    var speedText by
        remember(key) {
            mutableStateOf(
                initialForm.speedText
            )
        }

    var distanceText by
        remember(key) {
            mutableStateOf(
                initialForm.distanceText
            )
        }

    val equipmentEntries = remember(key) { repository.listEquipment() }
    val fixedEquipmentId = remember(exercise.exerciseId) {
        repository.machineExerciseLegacyEquipmentId(exercise.exerciseId)
    }
    var equipmentSearch by remember(key) { mutableStateOf("") }
    var selectedEquipmentId by remember(key) {
        mutableStateOf(initialForm.selectedEquipmentId ?: fixedEquipmentId)
    }
    var targetChoice by remember(key) {
        mutableStateOf(if (initialPlan?.weightKg != null) ManualTargetChoice.KG
            else ManualTargetChoice.NONE)
    }
    var targetKgText by remember(key) {
        mutableStateOf(initialPlan?.weightKg?.let(::formatMaxWeight).orEmpty())
    }
    var targetPercentText by remember(key) { mutableStateOf("70") }

    var error by
        remember(key) {
            mutableStateOf<
                String?
            >(null)
        }

    TrainlogFrame(
        title =
            strings.getString(R.string.entry_title, exercise.name)
    ) {
        TrainlogInfo(
            text = if (sessionType == SessionType.MAX_TEST) strings.getString(R.string.session_max_test) else exerciseProfileLabel(exercise),
            color = colors.accent,
        )

        if (fixedEquipmentId == null) {
            /* Legacy/custom/unresolved rows retain the compatibility selector;
             * resolved fixed machines never require a second user choice. */
            TrainlogInputField(
                label = strings.getString(R.string.historical_context_optional),
                value = equipmentSearch,
                onValueChange = { equipmentSearch = it },
            )
            TrainlogAction(
                label = strings.getString(R.string.create_equipment),
                description = strings.getString(R.string.create_equipment_from_session),
                accent = colors.success,
                modifier = Modifier.testTag("session-create-equipment"),
                onClick = onCreateEquipment,
            )
        }
        val selectedEquipment = equipmentEntries.firstOrNull { it.equipmentId == selectedEquipmentId }
        val percentResult = remember(
            exercise.exerciseId, selectedEquipmentId, targetPercentText,
            targetChoice,
        ) {
            if (targetChoice == ManualTargetChoice.PERCENT_MAX)
                repository.calculateManualPercentMaxTarget(
                    exercise.exerciseId, selectedEquipmentId,
                    targetPercentText.toIntOrNull() ?: 0,
                )
            else null
        }
        if (selectedEquipment != null && fixedEquipmentId == null) {
            TrainlogAction(
                label = "✓ ${selectedEquipment.displayName}",
                description = selectedEquipment.labelName.ifBlank { strings.getString(R.string.equipment_selected) },
                accent = colors.success,
                onClick = {
                    selectedEquipmentId = null
                    onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, null, weightText, maxWeightText))
                },
            )
        }
        if (fixedEquipmentId == null) {
            /* WHY: alphabetical truncation made valid equipment beyond the
             * eighth row impossible to select. CONTRACT: the bounded nested
             * viewport exposes every search result while the surrounding
             * exercise editor remains independently scrollable. */
            Column(
                Modifier
                    .heightIn(max = 320.dp)
                    .verticalScroll(rememberScrollState())
                    .testTag("session-equipment-results"),
            ) {
                repository.searchEquipment(equipmentSearch).forEach { equipment ->
                    TrainlogAction(
                        label = if (equipment.equipmentId == selectedEquipmentId) "✓ ${equipment.displayName}" else equipment.displayName,
                        description = equipment.labelName.ifBlank { equipment.type },
                        accent = if (equipment.equipmentId == selectedEquipmentId) colors.success else colors.muted,
                        onClick = {
                            selectedEquipmentId = equipment.equipmentId
                            onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, equipment.equipmentId, weightText, maxWeightText))
                        },
                    )
                }
            }
        }

        if (sessionType == SessionType.MAX_TEST) {
            SessionNumberField(
                label = strings.getString(R.string.max_weight),
                value = maxWeightText,
                decimal = true,
                onValueChange = {
                    maxWeightText = it
                    error = null
                    onFormChanged(
                        currentForm(
                            exercise, setCountText, repsText, durationText,
                            speedText, distanceText, selectedEquipmentId,
                            weightText, it,
                        )
                    )
                },
            )
            TrainlogInfo(
                text = strings.getString(R.string.positive_decimal_note),
                color = colors.muted,
            )
        } else if (
            exercise.recordingMode ==
            RecordingMode.SETS
        ) {
            TrainlogInfo(strings.getString(R.string.target_load_note), colors.muted)
            TrainlogChoiceChips(
                listOf(
                    ManualTargetChoice.KG.name to strings.getString(R.string.load_kg_value),
                    ManualTargetChoice.PERCENT_MAX.name to strings.getString(R.string.percent_my_max),
                    ManualTargetChoice.NONE.name to strings.getString(R.string.value_none_feminine),
                ),
                targetChoice.name,
            ) { selected -> targetChoice = ManualTargetChoice.valueOf(selected) }
            when (targetChoice) {
                ManualTargetChoice.KG -> SessionNumberField(
                    label = strings.getString(R.string.field_target_weight), value = targetKgText,
                    decimal = true,
                    onValueChange = { targetKgText = it; error = null },
                )
                ManualTargetChoice.PERCENT_MAX -> {
                    SessionNumberField(
                        label = strings.getString(R.string.percent_my_max_field),
                        value = targetPercentText,
                        onValueChange = { targetPercentText = it; error = null },
                    )
                    when (val result = percentResult) {
                        is ManualPercentMaxResult.Available -> {
                            TrainlogInfo(
                                strings.getString(R.string.compatible_max, presentationNumber(result.maxWeightKg, 3), formatDate(result.maxStartedAt)),
                                colors.muted,
                            )
                            TrainlogInfo(
                                strings.getString(R.string.calculated_target, presentationNumber(result.targetWeightKg, 3)),
                                colors.success,
                            )
                        }
                        is ManualPercentMaxResult.Unavailable ->
                            TrainlogInfo(localizedTargetError(result.message, strings), colors.error)
                        null -> Unit
                    }
                }
                ManualTargetChoice.NONE ->
                    TrainlogInfo(strings.getString(R.string.target_none), colors.muted)
            }
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                TrainlogInfo(
                    text = strings.getString(R.string.set_independent_values),
                    color = colors.muted,
                )
                val loadLabel =
                    if (selectedEquipment?.loadSemantics == EquipmentLoadSemantics.ASSISTANCE) {
                        strings.getString(R.string.assistance_kg)
                    } else {
                        strings.getString(R.string.field_weight)
                    }
                setRows.forEachIndexed { index, row ->
                    Column(
                        Modifier
                            .fillMaxWidth()
                            .padding(bottom = 8.dp)
                            .background(colors.surface, MaterialTheme.shapes.medium)
                            .padding(10.dp),
                    ) {
                        Row(
                            Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                        ) {
                            BasicText(
                                text = strings.getString(R.string.set_number, index + 1),
                                modifier = Modifier.padding(vertical = 12.dp),
                                style = TrainlogTypography.small.copy(
                                    color = colors.accent,
                                    fontWeight = FontWeight.Bold,
                                ),
                            )
                            TrainlogIconAction(
                                icon = TrainlogIcons.DeleteOutline,
                                contentDescription = localizedContext().getString(R.string.a11y_delete_set),
                                modifier = Modifier.testTag("delete-set-${index + 1}"),
                                accent = colors.error,
                                onClick = { pendingSetRemoval = index },
                            )
                        }
                        Row(
                            Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            SessionNumberField(
                                label = strings.getString(R.string.field_reps),
                                value = row.repsText,
                                modifier = Modifier.weight(1f),
                                testTag = "session-set-$index-reps",
                                onValueChange = { value ->
                                    val updated = setRows.replaceAt(index, row.copy(repsText = value))
                                    setRows = updated
                                    repsText = encodeRawReps(updated)
                                    weightText = encodeRawWeights(updated)
                                    error = null
                                    onFormChanged(
                                        currentForm(
                                            exercise, setCountText, repsText, durationText,
                                            speedText, distanceText, selectedEquipmentId, weightText,
                                        )
                                    )
                                },
                            )
                            SessionNumberField(
                                label = loadLabel,
                                value = row.weightText,
                                modifier = Modifier.weight(1f),
                                decimal = true,
                                testTag = "session-set-$index-weight",
                                onValueChange = { value ->
                                    val updated = setRows.replaceAt(index, row.copy(weightText = value))
                                    setRows = updated
                                    repsText = encodeRawReps(updated)
                                    weightText = encodeRawWeights(updated)
                                    error = null
                                    onFormChanged(
                                        currentForm(
                                            exercise, setCountText, repsText, durationText,
                                            speedText, distanceText, selectedEquipmentId, weightText,
                                        )
                                    )
                                },
                            )
                        }
                    }
                }
                TrainlogAction(
                    label = strings.getString(R.string.add_set_plain),
                    description = strings.getString(R.string.add_set_row_description),
                    accent = colors.success,
                    onClick = {
                        if (setRows.size < MAX_SESSION_SETS) {
                            val updated = setRows + RawSetRow()
                            setRows = updated
                            repsText = encodeRawReps(updated)
                            weightText = encodeRawWeights(updated)
                            error = null
                            onFormChanged(
                                currentForm(
                                    exercise, setCountText, repsText, durationText,
                                    speedText, distanceText, selectedEquipmentId, weightText,
                                )
                            )
                        }
                    },
                )
                TrainlogInfo(
                    text = strings.getString(R.string.optional_load_note),
                    color = colors.muted,
                )
            } else {
                SessionNumberField(
                    label =
                        strings.getString(R.string.sets_count),
                    value =
                        setCountText,
                    onValueChange = {
                        setCountText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                it,
                                repsText,
                                durationText,
                                speedText,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )

                SessionNumberField(
                    label =
                        strings.getString(R.string.set_duration_seconds),
                    value =
                        durationText,
                    onValueChange = {
                        durationText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                setCountText,
                                repsText,
                                it,
                                speedText,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )
            }
        } else {
            SessionNumberField(
                label =
                    strings.getString(R.string.duration_minutes),
                testTag = "session-continuous-duration",
                value =
                    durationText,
                onValueChange = {
                    durationText = it
                    error = null
                    onFormChanged(
                        currentForm(
                            exercise,
                            setCountText,
                            repsText,
                            it,
                                speedText,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                        )
                    )
                },
            )

            if (
                exercise.dataFields and
                    ExerciseDataFields
                        .SPEED_KMH != 0
            ) {
                SessionNumberField(
                    label =
                        strings.getString(R.string.speed_kmh),
                    value =
                        speedText,
                    decimal = true,
                    onValueChange = {
                        speedText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                setCountText,
                                repsText,
                                durationText,
                                it,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )
            }

            if (
                exercise.dataFields and
                    ExerciseDataFields
                        .DISTANCE_KM != 0
            ) {
                SessionNumberField(
                    label =
                        strings.getString(R.string.distance_km),
                    value =
                        distanceText,
                    decimal = true,
                    onValueChange = {
                        distanceText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                setCountText,
                                repsText,
                                durationText,
                                speedText,
                                it,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )
            }
        }

        TrainlogButton(
            label = strings.getString(R.string.add_to_session),
            modifier = Modifier.fillMaxWidth().testTag("add-to-session"),
            containerColor = colors.success,
            onClick = {
                val capturedDraft =
                    buildSessionExerciseDraft(
                        exercise =
                            exercise,
                        sessionType = sessionType,
                        setCountText =
                            setCountText,
                        repsText =
                            repsText,
                        durationText =
                            durationText,
                        speedText =
                            speedText,
                        distanceText =
                            distanceText,
                        equipmentId = selectedEquipmentId,
                        weightText = weightText,
                        maxWeightText = maxWeightText,
                        entryId = initialForm.editingEntryId,
                        rawSetRows = setRows,
                    )

                /* WHY: a prepared occurrence may legitimately have a target
                 * but no performed sets yet. CONTRACT: changing its equipment
                 * edits planning metadata in place without fabricating actual
                 * work. INVARIANT: any nonblank partial performed row still
                 * follows normal validation and can never be discarded here. */
                val targetOnlyDraft = initialOccurrence?.takeIf {
                    sessionType == SessionType.TRAINING &&
                        it.plan != null &&
                        it.sets.isEmpty() &&
                        it.maxWeightKg == null &&
                        it.continuousDurationSeconds == 0 &&
                        setRows.all { row ->
                            row.repsText.isBlank() && row.weightText.isBlank()
                        }
                }?.copy(
                    exercise = exercise,
                    equipmentId = selectedEquipmentId,
                )
                val draft = capturedDraft ?: targetOnlyDraft

                if (draft == null) {
                    error =
                        if (sessionType == SessionType.MAX_TEST) {
                            strings.getString(R.string.max_weight_invalid)
                        } else if (
                            exercise.recordingMode == RecordingMode.SETS &&
                            exercise.trackingMode == TrackingMode.REPS
                        ) {
                            localizedSetRowValidationError(setRowValidationError(setRows), strings)
                        } else {
                            strings.getString(R.string.invalid_values)
                        }
                } else if (sessionType == SessionType.TRAINING &&
                    exercise.recordingMode == RecordingMode.SETS) {
                    val plan = buildManualTargetPlan(
                        draft, initialPlan, targetChoice, targetKgText,
                        percentResult, selectedEquipment?.loadSemantics,
                    )
                    plan.fold(
                        onSuccess = { onAdd(draft.copy(plan = it)) },
                        onFailure = { error = localizedTargetError(it.message, strings) },
                    )
                } else {
                    /* CONTRACT: editing performed continuous facts must not
                     * erase the independent target copied from preparation. */
                    onAdd(draft.copy(plan = initialPlan))
                }
            },
        )

        TrainlogAction(
            label =
                strings.getString(R.string.cancel_entry),
            description =
                strings.getString(R.string.return_catalog),
            accent =
                colors.muted,
            modifier = Modifier.testTag("cancel-session-exercise-editor"),
            onClick =
                onCancel,
        )

        if (
            error != null
        ) {
            TrainlogInfo(
                text =
                    error.orEmpty(),
                color = colors.error,
            )
        }
        pendingSetRemoval?.let { index ->
            DestructiveConfirmationDialog(
                title = strings.getString(R.string.delete_numbered_set_question, index + 1),
                detail = strings.getString(R.string.delete_set_entered_detail),
                confirmLabel = strings.getString(R.string.delete),
                onCancel = { pendingSetRemoval = null },
                deleteContentDescription = localizedContext().getString(com.labfytools.trainlog.R.string.a11y_delete_set),
                onConfirm = {
                    val updated=setRows.filterIndexed { rowIndex, _ -> rowIndex != index }
                    setRows=updated;repsText=encodeRawReps(updated);weightText=encodeRawWeights(updated);error=null
                    onFormChanged(currentForm(exercise,setCountText,repsText,durationText,speedText,distanceText,selectedEquipmentId,weightText,maxWeightText))
                    pendingSetRemoval=null
                },
            )
        }
    }
}

/**
 * WHY: gym execution order changes frequently, while occurrence identity and
 * its captured facts must not. The dedicated handle avoids accidental row
 * drags while the user opens an editor.
 * CONTRACT: movement is a pure list permutation and persistence is requested
 * only after the gesture completes. Accessibility actions use the same path.
 * INVARIANT: the exact [SessionExerciseDraft] objects are moved; none of their
 * entry, exercise, equipment, target, actual, MAX, or feedback keys is rebuilt.
 */
@Composable
internal fun ExerciseReorderHandle(
    entryId: String,
    index: Int,
    itemCount: Int,
    onMove: (from: Int, to: Int) -> Unit,
    onCommit: () -> Unit,
    onVisualOffset: (Float) -> Unit = {},
) {
    val strings = localizedContext()
    val currentIndex by rememberUpdatedState(index)
    val currentItemCount by rememberUpdatedState(itemCount)
    val currentMove by rememberUpdatedState(onMove)
    val currentCommit by rememberUpdatedState(onCommit)
    val currentVisualOffset by rememberUpdatedState(onVisualOffset)
    var dragOrigin by remember(entryId) { mutableStateOf<Int?>(null) }
    var totalDragDistance by remember(entryId) { mutableStateOf(0f) }
    var residualDragDistance by remember(entryId) { mutableStateOf(0f) }
    BasicText(
        text = "≡",
        modifier = Modifier
            .heightIn(min = 48.dp)
            .padding(horizontal = 14.dp, vertical = 10.dp)
            .testTag("reorder-exercise-$entryId")
            .semantics {
                contentDescription = strings.getString(R.string.a11y_reorder_exercise)
                customActions = buildList {
                    if (index > 0) add(CustomAccessibilityAction(
                        strings.getString(R.string.a11y_move_exercise_up),
                    ) { currentMove(currentIndex, currentIndex - 1); currentCommit(); true })
                    if (index + 1 < itemCount) add(CustomAccessibilityAction(
                        strings.getString(R.string.a11y_move_exercise_down),
                    ) { currentMove(currentIndex, currentIndex + 1); currentCommit(); true })
                }
            }
            /* CONTRACT: entry_id, unlike position, remains stable while rows
             * move. Keeping this pointer coroutine alive is what permits one
             * gesture to cross any number of positions. */
            .pointerInput(entryId) {
                val stepPx = 48.dp.toPx()
                detectDragGestures(
                    onDragStart = {
                        dragOrigin = currentIndex
                        totalDragDistance = 0f
                        residualDragDistance = 0f
                        currentVisualOffset(0f)
                    },
                    onDragEnd = {
                        dragOrigin = null
                        totalDragDistance = 0f
                        residualDragDistance = 0f
                        currentVisualOffset(0f)
                        currentCommit()
                    },
                    onDragCancel = {
                        dragOrigin?.let { origin ->
                            if (currentIndex != origin) currentMove(currentIndex, origin)
                        }
                        dragOrigin = null
                        totalDragDistance = 0f
                        residualDragDistance = 0f
                        currentVisualOffset(0f)
                    },
                ) { change, amount ->
                    change.consume()
                    val origin = dragOrigin ?: currentIndex.also { dragOrigin = it }
                    totalDragDistance += amount.y
                    val target = reorderTargetIndex(
                        origin,
                        totalDragDistance,
                        stepPx,
                        currentItemCount,
                    )
                    if (target != currentIndex) {
                        currentMove(currentIndex, target)
                    }
                    residualDragDistance = totalDragDistance - (target - origin) * stepPx
                    currentVisualOffset(residualDragDistance)
                }
            },
        style = TrainlogTypography.title.copy(color = LocalTrainlogColors.current.muted),
    )
}

/** Deterministic direct destination for one continuous pointer gesture. */
internal fun reorderTargetIndex(
    origin: Int,
    totalDragDistancePx: Float,
    rowStepPx: Float,
    itemCount: Int,
): Int {
    if (itemCount <= 0 || rowStepPx <= 0f || !totalDragDistancePx.isFinite()) return origin
    val crossedRows = kotlin.math.round(totalDragDistancePx / rowStepPx).toInt()
    return (origin + crossedRows).coerceIn(0, itemCount - 1)
}

/** Pure stable permutation used by gesture UI and identity regression tests. */
internal fun reorderExerciseOccurrences(
    items: List<SessionExerciseDraft>,
    from: Int,
    to: Int,
): List<SessionExerciseDraft> {
    if (from !in items.indices || to !in items.indices || from == to) return items
    return items.toMutableList().apply { add(to, removeAt(from)) }
}

/** Keep the durable editor anchored to its stable entry after a permutation. */
internal fun reorderActiveSessionDraft(
    draft: ActiveSessionDraft,
    from: Int,
    to: Int,
): ActiveSessionDraft {
    val reordered = reorderExerciseOccurrences(draft.exercises, from, to)
    val editingIndex = draft.form.editingEntryId?.let { entryId ->
        reordered.indexOfFirst { it.entryId == entryId }.takeIf { it >= 0 }
    }
    return draft.copy(
        exercises = reordered,
        form = draft.form.copy(editingExerciseIndex = editingIndex),
    )
}

private fun currentForm(
    exercise: ExerciseProfile,
    setCountText: String,
    repsText: String,
    durationText: String,
    speedText: String,
    distanceText: String,
    equipmentId: String? = null,
    weightText: String = "",
    maxWeightText: String = "",
): SessionDraftForm =
    SessionDraftForm(
        selectedExercise = exercise,
        selectedEquipmentId = equipmentId,
        setCountText = setCountText,
        repsText = repsText,
        weightText = weightText,
        maxWeightText = maxWeightText,
        durationText = durationText,
        speedText = speedText,
        distanceText = distanceText,
    )

@Composable
private fun SessionNumberField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    testTag: String? = null,
    decimal: Boolean = false,
) {
    TrainlogInputField(
        label = label,
        value = value,
        onValueChange = { raw ->
            if (!decimal || decimalEditorTextValid(raw)) onValueChange(raw)
        },
        testTag = testTag,
        keyboardOptions = if (decimal) {
            KeyboardOptions(keyboardType = KeyboardType.Decimal)
        } else {
            KeyboardOptions(keyboardType = KeyboardType.Number)
        },
        modifier = modifier,
    )
}

/** Permissive typing grammar; numeric/domain validation belongs to commit. */
internal fun decimalEditorTextValid(raw: String): Boolean =
    raw.matches(Regex("^-?(?:[0-9]+(?:[.,][0-9]*)?|[.,][0-9]*)?$"))

/** Locale-independent finite parse shared by capture and correction forms. */
internal fun parseFiniteDecimal(raw: String): Double? =
    raw.trim().replace(',', '.').toDoubleOrNull()?.takeIf(Double::isFinite)

private const val MAX_SESSION_SETS = 64
private const val MAX_REPS_PER_SET = 10000

internal data class RawSetRow(
    val repsText: String = "",
    val weightText: String = "",
)

internal fun List<RawSetRow>.replaceAt(index: Int, value: RawSetRow): List<RawSetRow> =
    mapIndexed { rowIndex, existing -> if (rowIndex == index) value else existing }

internal fun encodeRawReps(rows: List<RawSetRow>): String =
    rows.joinToString(";") { it.repsText }

internal fun encodeRawWeights(rows: List<RawSetRow>): String =
    rows.joinToString(";") { it.weightText }

/**
 * WHY: schema v9 already has durable raw form columns. Parallel token strings
 * preserve row order and interior blanks without inventing a schema migration.
 * Legacy compact rep expressions are expanded once when the row editor opens.
 */
internal fun rawSetRowsFromForm(form: SessionDraftForm): List<RawSetRow> {
    val repTokens =
        if (';' in form.repsText) {
            form.repsText.split(';')
        } else if (',' in form.repsText) {
            /* Legacy compact lists may end in an unfinished token. Keep that
             * blank row instead of normalizing it away during first reopen. */
            form.repsText.split(',')
        } else {
            val compact = parseRepSequence(form.repsText)
            if (compact != null) {
                compact.map(Int::toString)
            } else {
                listOf(form.repsText)
            }
        }
    val weightTokens =
        if (';' in form.weightText) {
            form.weightText.split(';')
        } else {
            listOf(form.weightText)
        }

    if (repTokens.isEmpty()) return listOf(RawSetRow())
    return repTokens.mapIndexed { index, reps ->
        RawSetRow(
            repsText = reps,
            /* Compatibility only: a legacy single compact load was broadcast
             * by the old editor. New edits always persist one token per row. */
            weightText =
                if (weightTokens.size == 1) weightTokens.single()
                else weightTokens.getOrElse(index) { "" },
        )
    }
}

internal fun parseRawSetRows(rows: List<RawSetRow>): List<SessionSetDraft>? {
    if (rows.isEmpty() || rows.size > MAX_SESSION_SETS) return null
    return rows.map { row ->
        val reps = row.repsText.trim().toIntOrNull()
        if (reps == null || reps !in 0..MAX_REPS_PER_SET) return null

        val rawWeight = row.weightText.trim()
        val weight =
            if (rawWeight.isEmpty()) {
                null
            } else {
                rawWeight.replace(',', '.').toDoubleOrNull()
                    ?.takeIf { it.isFinite() && it >= 0.0 }
                    ?: return null
            }
        SessionSetDraft(reps = reps, weightKg = weight)
    }
}

internal fun setRowValidationError(rows: List<RawSetRow>): String {
    if (rows.isEmpty()) return "set_required"
    rows.forEachIndexed { index, row ->
        val reps = row.repsText.trim().toIntOrNull()
        if (reps == null || reps !in 0..MAX_REPS_PER_SET) {
            return "set_reps:${index + 1}"
        }
        if (row.weightText.isNotBlank()) {
            val weight = row.weightText.trim().replace(',', '.').toDoubleOrNull()
            if (weight == null || !weight.isFinite() || weight < 0.0) {
                return "set_load:${index + 1}"
            }
        }
    }
    return "sets_invalid"
}

private fun localizedSetRowValidationError(code: String, context: android.content.Context): String = when {
    code == "set_required" -> context.getString(R.string.validation_set_required)
    code.startsWith("set_reps:") -> context.getString(R.string.validation_set_reps,
        code.substringAfter(':').toInt(), MAX_REPS_PER_SET)
    code.startsWith("set_load:") -> context.getString(R.string.validation_set_load,
        code.substringAfter(':').toInt())
    else -> context.getString(R.string.validation_sets_invalid)
}

/** CONTRACT: repository diagnostics remain unchanged; this presentation-only
 * classifier maps stable known outcomes without putting locale into storage.
 * TECHNICAL_LITERAL: French strings below are legacy diagnostic classification
 * keys and are never emitted directly to the user. */
private fun localizedTargetError(raw: String?, context: android.content.Context): String = when (raw) {
    "target_positive" -> context.getString(R.string.target_positive)
    "max_unavailable" -> context.getString(R.string.max_unavailable)
    "max_assistance_unavailable", "Le %MAX est indisponible pour une assistance ; choisissez une résistance externe." -> context.getString(R.string.max_assistance_unavailable)
    "compatible_load_equipment_required" -> context.getString(R.string.compatible_load_equipment_required)
    "positive_dose_required" -> context.getString(R.string.positive_dose_required)
    "Le pourcentage doit être compris entre 1 et 100." -> context.getString(R.string.percent_range)
    "MAX compatible indisponible : choisissez un équipement à résistance externe." -> context.getString(R.string.max_choose_external)
    "Exercice introuvable." -> context.getString(R.string.exercise_not_found)
    "Le %MAX est indisponible sans équipement connu à résistance externe." -> context.getString(R.string.max_known_external_required)
    "MAX compatible indisponible pour cet exercice et cet équipement exacts." -> context.getString(R.string.max_exact_unavailable)
    "Lecture du MAX impossible." -> context.getString(R.string.max_read_failed)
    null -> context.getString(R.string.invalid_target_load)
    else -> raw
}

private fun parseRepSequence(
    text: String,
): List<Int>? {
    val normalized =
        text.trim()
            .lowercase()
            .replace(
                '×',
                'x'
            )

    if (normalized.isEmpty()) {
        return null
    }

    val repeated =
        Regex(
            """^(\d+)\s*x\s*(\d+)$"""
        ).matchEntire(
            normalized
        )

    if (repeated != null) {
        val count =
            repeated.groupValues[1]
                .toIntOrNull()

        val reps =
            repeated.groupValues[2]
                .toIntOrNull()

        if (
            count == null ||
            reps == null ||
            count !in 1..MAX_SESSION_SETS ||
            reps !in 0..MAX_REPS_PER_SET
        ) {
            return null
        }

        return List(count) {
            reps
        }
    }

    val pyramid =
        Regex(
            """^(\d+)\s*\.\.\s*(\d+)\s*\.\.\s*(\d+)$"""
        ).matchEntire(
            normalized
        )

    if (pyramid != null) {
        val start =
            pyramid.groupValues[1]
                .toIntOrNull()

        val peak =
            pyramid.groupValues[2]
                .toIntOrNull()

        val end =
            pyramid.groupValues[3]
                .toIntOrNull()

        if (
            start == null ||
            peak == null ||
            end == null ||
            start !in 0..MAX_REPS_PER_SET ||
            peak !in 0..MAX_REPS_PER_SET ||
            end !in 0..MAX_REPS_PER_SET ||
            start > peak ||
            end > peak
        ) {
            return null
        }

        val values =
            mutableListOf<Int>()

        for (value in start..peak) {
            values += value

            if (
                values.size >
                MAX_SESSION_SETS
            ) {
                return null
            }
        }

        if (peak > end) {
            for (
                value in
                    (peak - 1) downTo end
            ) {
                values += value

                if (
                    values.size >
                    MAX_SESSION_SETS
                ) {
                    return null
                }
            }
        }

        return values
    }

    val parts =
        normalized
            .split(
                Regex(
                    """[\s,;]+"""
                )
            )
            .filter {
                it.isNotEmpty()
            }

    if (
        parts.isEmpty() ||
        parts.size >
        MAX_SESSION_SETS
    ) {
        return null
    }

    val values =
        mutableListOf<Int>()

    for (part in parts) {
        val reps =
            part.toIntOrNull()
                ?: return null

        if (
            reps !in
            0..MAX_REPS_PER_SET
        ) {
            return null
        }

        values += reps
    }

    return values
}

private fun buildSessionExerciseDraft(
    exercise: ExerciseProfile,
    sessionType: SessionType,
    setCountText: String,
    repsText: String,
    durationText: String,
    speedText: String,
    distanceText: String,
    equipmentId: String? = null,
    weightText: String = "",
    maxWeightText: String = "",
    entryId: String? = null,
    rawSetRows: List<RawSetRow>? = null,
): SessionExerciseDraft? {
    if (sessionType == SessionType.MAX_TEST) {
        val maxWeight = maxWeightText.trim().replace(',', '.').toDoubleOrNull()
        if (maxWeight == null || !maxWeight.isFinite() || maxWeight <= 0.0) {
            return null
        }
        return SessionExerciseDraft(
            entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
            exercise = exercise,
            equipmentId = equipmentId,
            maxWeightKg = maxWeight,
        )
    }

    return if (
        exercise.recordingMode ==
        RecordingMode.CONTINUOUS
    ) {
        val minutes =
            durationText.toIntOrNull()

        if (
            minutes == null ||
            minutes <= 0 ||
            minutes > 1440
        ) {
            null
        } else {
            val wantsSpeed =
                exercise.dataFields and
                    ExerciseDataFields.SPEED_KMH != 0

            val wantsDistance =
                exercise.dataFields and
                    ExerciseDataFields.DISTANCE_KM != 0

            val speed =
                if (wantsSpeed) {
                    speedText
                        .replace(',', '.')
                        .toDoubleOrNull()
                } else {
                    null
                }

            val distance =
                if (wantsDistance) {
                    distanceText
                        .replace(',', '.')
                        .toDoubleOrNull()
                } else {
                    null
                }

            if (
                (
                    wantsSpeed &&
                    (
                        speed == null ||
                        speed <= 0.0
                    )
                ) ||
                (
                    wantsDistance &&
                    (
                        distance == null ||
                        distance <= 0.0
                    )
                )
            ) {
                null
            } else {
                SessionExerciseDraft(
                    entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
                    exercise = exercise,
                    equipmentId = equipmentId,
                    continuousDurationSeconds =
                        minutes * 60,
                    speedKmh = speed,
                    distanceKm = distance,
                )
            }
        }
    } else if (
        exercise.trackingMode ==
        TrackingMode.REPS
    ) {
        val parsedSets =
            parseRawSetRows(
                rawSetRows ?: rawSetRowsFromForm(
                    SessionDraftForm(repsText = repsText, weightText = weightText)
                )
            ) ?: return null

        SessionExerciseDraft(
            entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
            exercise = exercise,
            equipmentId = equipmentId,
            sets = parsedSets,
        )
    } else {
        val count =
            setCountText.toIntOrNull()

        val seconds =
            durationText.toIntOrNull()

        if (
            count == null ||
            count <= 0 ||
            count > MAX_SESSION_SETS ||
            seconds == null ||
            seconds <= 0 ||
            seconds > 86400
        ) {
            null
        } else {
            SessionExerciseDraft(
                entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
                exercise = exercise,
                equipmentId = equipmentId,
                sets =
                    List(count) {
                        SessionSetDraft(
                            durationSeconds =
                                seconds
                        )
                    },
            )
        }
    }
}

/** Reconstruct editable text from the entry itself; editing never mutates a
 * different entry or the global exercise definition. */
internal fun formForExistingExercise(
    draft: SessionExerciseDraft,
    index: Int,
): SessionDraftForm =
    SessionDraftForm(
        selectedExercise = draft.exercise,
        editingExerciseIndex = index,
        editingEntryId = draft.entryId,
        selectedEquipmentId = draft.equipmentId,
        maxWeightText = draft.maxWeightKg?.let(::formatMaxWeight).orEmpty(),
        setCountText = draft.sets.size.toString(),
        repsText = if (draft.exercise.trackingMode == TrackingMode.REPS) {
            draft.sets.joinToString(",") { it.reps.toString() }
        } else {
            "3x10"
        },
        /* INVARIANT: keep one load token per performed-set row. mapNotNull
         * would shift later weights left when an earlier row is blank. */
        weightText = draft.sets.joinToString(";") {
            it.weightKg?.let(::formatMaxWeight).orEmpty()
        },
        durationText = if (draft.exercise.recordingMode == RecordingMode.CONTINUOUS) {
            (draft.continuousDurationSeconds / 60).toString()
        } else {
            draft.sets.firstOrNull()?.durationSeconds?.toString() ?: "30"
        },
        speedText = draft.speedKmh?.toString().orEmpty(),
        distanceText = draft.distanceKm?.toString().orEmpty(),
    )

@Composable
private fun draftSummary(
    draft: SessionExerciseDraft,
): String {
    val strings = localizedContext()
    val locale = presentationLocale()
    return if (draft.maxWeightKg != null) {
        strings.getString(R.string.draft_max_summary, draft.exercise.name, presentationNumber(draft.maxWeightKg, 3))
    } else if (
        draft.exercise.recordingMode ==
        RecordingMode.CONTINUOUS
    ) {
        buildString {
            append(
                draft.exercise.name
            )

            append(
                " · ${draft.continuousDurationSeconds / 60} min"
            )

            draft.speedKmh?.let {
                append(
                    " · %.1f km/h".format(locale, it)
                )
            }

            draft.distanceKm?.let {
                append(
                    " · %.2f km".format(locale, it)
                )
            }
        }
    } else if (
        draft.exercise.trackingMode ==
        TrackingMode.REPS
    ) {
        val reps =
            draft.sets.map {
                it.reps
            }

        if (
            reps.isNotEmpty() &&
            draft.sets.all { it.weightKg == null } &&
            reps.all {
                it == reps.first()
            }
        ) {
            (
                "${draft.exercise.name} · " +
                "${reps.size} × " +
                "${reps.first()} ${strings.getString(R.string.reps_short)}"
            )
        } else {
            (
                "${draft.exercise.name} · " +
                strings.resources.getQuantityString(R.plurals.performed_set_count, reps.size, reps.size) + " · " +
                draft.sets.joinToString(separator = " ; ") { set ->
                    buildString {
                        append("${set.reps} ${strings.getString(R.string.reps_short)}")
                        set.weightKg?.let { append(" @ ${formatPresentationNumber(it, locale)} kg") }
                    }
                }
            )
        }
    } else {
        val first =
            draft.sets.firstOrNull()

        (
            "${draft.exercise.name} · " +
                "${draft.sets.size} × " +
                "${first?.durationSeconds ?: 0} s"
        )
    }
}

private fun formatMaxWeight(value: Double): String =
    java.math.BigDecimal.valueOf(value).stripTrailingZeros().toPlainString().replace('.', ',')

private fun formatPresentationNumber(value: Double, locale: java.util.Locale): String =
    java.text.NumberFormat.getNumberInstance(locale).apply {
        isGroupingUsed = false
        maximumFractionDigits = 3
    }.format(value)

@Composable
private fun exerciseProfileLabel(
    exercise: ExerciseProfile,
): String {
    return buildString {
        append(
            if (
                exercise.recordingMode ==
                RecordingMode.CONTINUOUS
            ) {
                uiString(R.string.profile_continuous)
            } else {
                uiString(R.string.profile_sets)
            }
        )

        append(" · ")

        append(
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                uiString(R.string.profile_reps)
            } else {
                uiString(R.string.profile_duration)
            }
        )

        if (
            exercise.dataFields and
                ExerciseDataFields
                    .SPEED_KMH != 0
        ) {
            append(uiString(R.string.profile_speed_suffix))
        }

        if (
            exercise.dataFields and
                ExerciseDataFields
                    .DISTANCE_KM != 0
        ) {
            append(uiString(R.string.profile_distance_suffix))
        }
    }
}
