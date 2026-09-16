/*
 * Android SessionDetailScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.foundation.layout.fillMaxWidth
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.SaveFeedbackResult
import com.labfytools.trainlog.data.exerciseFeedbackElapsedLabel
import com.labfytools.trainlog.data.sessionFollowUpElapsedLabel
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDetail
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.R

@Composable
fun SessionDetailScreen(
    repository: TrainlogRepository,
    sessionId: String?,
    onBack: () -> Unit,
    onCorrect: () -> Unit = {},
    onResumeMaxTest: () -> Unit = {},
) {
    val colors =
        LocalTrainlogColors.current
    val locale = presentationLocale()
    val strings = localizedContext()

    var revision by remember(sessionId) { mutableStateOf(0) }
    var editingEntryId by remember(sessionId) { mutableStateOf<String?>(null) }
    var pendingEquipmentChange by remember(sessionId) { mutableStateOf<Pair<String, String?>?>(null) }
    var equipmentQuery by remember(sessionId) { mutableStateOf("") }
    var resumeMessage by remember(sessionId) { mutableStateOf<String?>(null) }
    var feedbackEntryId by remember(sessionId) { mutableStateOf<String?>(null) }
    var editingFeedbackId by remember(sessionId) { mutableStateOf<String?>(null) }
    var editingFollowUpId by remember(sessionId) { mutableStateOf<String?>(null) }
    var addingFollowUp by remember(sessionId) { mutableStateOf(false) }

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
        subtitle = strings.getString(R.string.route_session_detail)
    ) {
        if (detail == null) {
            TrainlogFrame(
                title = strings.getString(R.string.error_title)
            ) {
                TrainlogInfo(
                    text =
                        strings.getString(R.string.session_not_found),
                    color =
                        colors.error,
                )
            }

            return@TrainlogScreen
        }

        TrainlogFrame(
            title = strings.getString(R.string.home_session)
        ) {
            TrainlogInfo(
                formatStartedAt(
                    detail.summary.startedAt
                )
            )

            TrainlogInfo(
                text =
                    strings.getString(R.string.session_type_value,
                        sessionTypeLabel(detail.summary.sessionType)),
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
                strings.resources.getQuantityString(R.plurals.exercise_count,
                    detail.summary.exerciseCount, detail.summary.exerciseCount)
            )

            TrainlogButton(strings.getString(R.string.route_session_correction), onCorrect,
                Modifier.fillMaxWidth().testTag("edit-completed-session"))

            if (detail.summary.sessionType == SessionType.MAX_TEST) {
                TrainlogAction(
                    label = strings.getString(R.string.resume_max_test),
                    description = strings.getString(R.string.resume_max_description),
                    accent = colors.success,
                    onClick = {
                        when (val result = repository.resumeMaxTestSession(detail.summary.sessionId)) {
                            ActiveDraftMutationResult.Saved -> onResumeMaxTest()
                            is ActiveDraftMutationResult.Error -> resumeMessage = localizedRepositoryMessage(strings, result.message)
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
                        TrainlogInfo(strings.getString(R.string.equipment_inline, equipment), color = colors.muted)
                    }
                    exercise.plan?.let { plan ->
                        TrainlogInfo(
                            strings.getString(R.string.plan_summary, plan.sets,
                                (plan.reps ?: plan.durationSeconds).toString(), plan.restSeconds,
                                plan.weightKg?.let { strings.getString(R.string.target_load_inline, it) }
                                    ?: strings.getString(R.string.no_numeric_load)),
                            color = colors.muted,
                        )
                    }
                    if (editingEntryId == exercise.entryId) {
                        TrainlogInputField(
                            label = strings.getString(R.string.search_machine),
                            value = equipmentQuery,
                            onValueChange = { equipmentQuery = it },
                        )
                        TrainlogDeleteButton(
                            contentDescription = localizedContext().getString(com.labfytools.trainlog.R.string.a11y_remove_machine),
                            modifier = Modifier.testTag("delete-completed-equipment-${exercise.entryId}"),
                            onClick = {
                                pendingEquipmentChange = exercise.entryId to null
                            },
                        )
                        repository.searchEquipment(equipmentQuery).take(8).forEach { equipment ->
                            TrainlogAction(
                                label = equipment.displayName,
                                description = equipment.labelName.ifBlank { equipment.type },
                                accent = colors.success,
                                onClick = {
                                    if (exercise.equipmentId == null) {
                                        if (repository.setCompletedSessionEquipment(detail.summary.sessionId, exercise.entryId, equipment.equipmentId)) {
                                            revision++; editingEntryId = null; equipmentQuery = ""
                                        }
                                    } else pendingEquipmentChange = exercise.entryId to equipment.equipmentId
                                },
                            )
                        }
                    } else {
                        TrainlogAction(
                            label = strings.getString(R.string.modify_equipment),
                            description = strings.getString(R.string.modify_equipment_description),
                            accent = colors.muted,
                            onClick = { editingEntryId = exercise.entryId },
                        )
                    }
                    if (exercise.maxWeightKg != null) {
                        val rendered = "%.2f".format(locale, exercise.maxWeightKg)
                            .trimEnd('0').trimEnd(',')
                        TrainlogInfo(
                            text = strings.getString(R.string.max_value, rendered),
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
                    exercise.feedback.forEach { item ->
                        val elapsed = exerciseFeedbackElapsedLabel(detail.summary.endedAt, detail.summary.startedAt, item.observedAt)
                        // CONTRACT: repository diagnostics remain stable data; only this visible sentinel is localized.
                        val localizedElapsed = if (elapsed == "Pendant la séance") strings.getString(R.string.during_session) else elapsed
                        TrainlogInfo("$localizedElapsed · ${item.rawText}${if (item.modified) strings.getString(R.string.modified_suffix) else ""}")
                        TrainlogAction(strings.getString(R.string.modify), strings.getString(R.string.feedback_move_description),
                            onClick = { editingFeedbackId = item.feedbackId }, accent = colors.muted)
                        if (editingFeedbackId == item.feedbackId) FeedbackEditor(
                            initialText = item.rawText, editing = true,
                            onSave = { text -> when (val result = repository.reviseExerciseFeedback(item.feedbackId, text)) {
                                is SaveFeedbackResult.Saved -> { editingFeedbackId = null; revision++; null }
                                is SaveFeedbackResult.Invalid -> localizedRepositoryMessage(strings, result.message)
                                is SaveFeedbackResult.DatabaseError -> localizedRepositoryMessage(strings, result.message)
                            } }, onCancel = { editingFeedbackId = null })
                    }
                    if (feedbackEntryId == exercise.entryId) {
                        FeedbackEditor(onSave = { text ->
                            when (val result = repository.saveExerciseFeedback(detail.summary.sessionId, exercise.entryId, text)) {
                                is SaveFeedbackResult.Saved -> { feedbackEntryId = null; revision++; null }
                                is SaveFeedbackResult.Invalid -> localizedRepositoryMessage(strings, result.message)
                                is SaveFeedbackResult.DatabaseError -> localizedRepositoryMessage(strings, result.message)
                            }
                        }, onCancel = { feedbackEntryId = null })
                    } else {
                        TrainlogButton(strings.getString(R.string.add_feedback), { feedbackEntryId = exercise.entryId },
                            Modifier.fillMaxWidth().testTag("add-exercise-feedback-${exercise.entryId}"),
                            icon = TrainlogIcons.Mic)
                    }
                }
            }

        TrainlogFrame(title = strings.getString(R.string.session_follow_up)) {
            detail.followUps.forEach { item ->
                TrainlogInfo(sessionFollowUpElapsedLabel(detail.summary.endedAt, detail.summary.startedAt, item.observedAt), color = colors.accent)
                TrainlogInfo(item.rawText + if (item.modified) strings.getString(R.string.modified_suffix) else "")
                TrainlogAction(strings.getString(R.string.modify), strings.getString(R.string.feedback_marker_description),
                    onClick = { editingFollowUpId = item.followupId }, accent = colors.muted)
                if (editingFollowUpId == item.followupId) FeedbackEditor(
                    initialText = item.rawText, editing = true,
                    onSave = { text -> when (val result = repository.reviseSessionFollowUp(item.followupId, text)) {
                        is SaveFeedbackResult.Saved -> { editingFollowUpId = null; revision++; null }
                        is SaveFeedbackResult.Invalid -> localizedRepositoryMessage(strings, result.message)
                        is SaveFeedbackResult.DatabaseError -> localizedRepositoryMessage(strings, result.message)
                    } }, onCancel = { editingFollowUpId = null })
            }
            if (addingFollowUp) {
                FeedbackEditor(onSave = { text ->
                    when (val result = repository.saveSessionFollowUp(detail.summary.sessionId, text)) {
                        is SaveFeedbackResult.Saved -> { addingFollowUp = false; revision++; null }
                        is SaveFeedbackResult.Invalid -> localizedRepositoryMessage(strings, result.message)
                        is SaveFeedbackResult.DatabaseError -> localizedRepositoryMessage(strings, result.message)
                    }
                }, onCancel = { addingFollowUp = false })
            } else TrainlogButton(strings.getString(R.string.add_record), { addingFollowUp = true },
                Modifier.fillMaxWidth().testTag("add-session-follow-up"), icon = TrainlogIcons.Mic,
                containerColor = colors.success)
        }
        pendingEquipmentChange?.let { (entryId, equipmentId) ->
            DestructiveConfirmationDialog(
                title = strings.getString(if (equipmentId == null) R.string.remove_machine_question else R.string.replace_machine_question),
                detail = strings.getString(R.string.machine_change_detail),
                confirmLabel = strings.getString(if (equipmentId == null) R.string.remove else R.string.replace),
                onCancel = { pendingEquipmentChange = null },
                deleteContentDescription = if (equipmentId == null) {
                    strings.getString(R.string.a11y_remove_machine)
                } else null,
                onConfirm = {
                    if (repository.setCompletedSessionEquipment(detail.summary.sessionId,entryId,equipmentId)) {
                        revision++;editingEntryId=null;equipmentQuery="";pendingEquipmentChange=null
                    }
                },
            )
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
    val locale = presentationLocale()
    val strings = localizedContext()

    TrainlogInfo(
        text =
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                strings.getString(R.string.mode_sets_reps)
            } else {
                strings.getString(R.string.mode_sets_duration)
            },
        color = colors.accent,
    )

    if (exercise.trackingMode == TrackingMode.REPS) {
        val loadHeading =
            if (loadSemantics == EquipmentLoadSemantics.ASSISTANCE) {
                strings.getString(R.string.assistance_kg)
            } else {
                strings.getString(R.string.field_weight)
            }
        /* Readable row table: history must expose every persisted value and
         * distinguish an absent load from an explicit zero. */
        TrainlogInfo(strings.getString(R.string.sets_table, loadHeading), color = colors.muted)
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
                        "%.2f".format(locale, it).trimEnd('0').trimEnd(',', '.')
                    } ?: "—"
                    "${index + 1} | ${set.reps} | $renderedWeight"
                } else {
                    strings.getString(R.string.set_duration, index + 1, formatDuration(set.durationSeconds))
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
    val strings = localizedContext()

    TrainlogInfo(
        text =
            strings.getString(R.string.mode_continuous),
        color = colors.accent,
    )

    TrainlogInfo(
        strings.getString(R.string.duration_value, formatDuration(exercise.continuousDurationSeconds))
    )

    if (
        exercise.dataFields and
            ExerciseDataFields.SPEED_KMH != 0
    ) {
        TrainlogInfo(
            strings.getString(R.string.speed_value, exercise.speedKmh ?: 0.0)
        )
    }

    if (
        exercise.dataFields and
            ExerciseDataFields.DISTANCE_KM != 0
    ) {
        TrainlogInfo(
            strings.getString(R.string.distance_value, exercise.distanceKm ?: 0.0)
        )
    }
}

@Composable
private fun formatDuration(
    seconds: Int,
): String {
    if (
        seconds > 0 &&
        seconds % 60 == 0
    ) {
        return uiQuantity(R.plurals.duration_minutes_value, seconds / 60, seconds / 60)
    }

    return uiQuantity(R.plurals.duration_seconds_value, seconds, seconds)
}
