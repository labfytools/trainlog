/*
 * Android SessionGeneratorScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.AcceptGeneratedSessionResult
import com.labfytools.trainlog.data.GenerationWarningLevel
import com.labfytools.trainlog.data.GeneratorLoadChoice
import com.labfytools.trainlog.data.SessionGenerationPreview
import com.labfytools.trainlog.data.SessionGenerationRequest
import com.labfytools.trainlog.data.SessionGenerationResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import java.time.OffsetDateTime
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import com.labfytools.trainlog.R

internal object SessionGeneratorPreviewController {
    fun remove(preview: SessionGenerationPreview, index: Int): SessionGenerationPreview {
        val removed = preview.exercises.getOrNull(index) ?: return preview
        return preview.copy(
            exercises = preview.exercises.filterIndexed { at, _ -> at != index },
            estimatedDurationSeconds = preview.estimatedDurationSeconds - removed.estimatedSeconds,
        )
    }

    fun move(preview: SessionGenerationPreview, index: Int, offset: Int): SessionGenerationPreview {
        val destination = index + offset
        if (index !in preview.exercises.indices || destination !in preview.exercises.indices) return preview
        val changed = preview.exercises.toMutableList()
        val value = changed.removeAt(index)
        changed.add(destination, value)
        return preview.copy(exercises = changed)
    }
}

internal object SessionGeneratorFormController {
    val goals = listOf(
        "general", "strength", "hypertrophy", "endurance",
    )
    fun duration(text: String, allowed: IntRange): Int? = text.toIntOrNull()?.takeIf { it in allowed }

    fun manualWeight(text: String): Result<Double?> {
        val normalized = text.trim().replace(',', '.')
        if (normalized.isEmpty()) return Result.success(null)
        val value = normalized.toDoubleOrNull()
        return if (value != null && value.isFinite() && value > 0.0) Result.success(value)
        else Result.failure(IllegalArgumentException("generator_weight_invalid"))
    }
}

/**
 * INVARIANT: a generated proposal is transient application state. Keeping this
 * holder above the route preserves edits across drawer navigation without ever
 * representing the proposal as the repository-owned active draft.
 */
class SessionGeneratorUiState {
    val zoneId = mutableStateOf("full_body")
    val goalId = mutableStateOf("general")
    val durationText = mutableStateOf("30")
    val preview = mutableStateOf<SessionGenerationPreview?>(null)
    val message = mutableStateOf<String?>(null)
    val warningAcknowledged = mutableStateOf(false)
    val editingIndex = mutableStateOf<Int?>(null)
    val setsText = mutableStateOf("")
    val repsText = mutableStateOf("")
    val restText = mutableStateOf("")
    val loadText = mutableStateOf("")
    val loadChoice = mutableStateOf(GeneratorLoadChoice.AUTOMATIC)
    val maxPercentText = mutableStateOf("70")
    val busy = mutableStateOf(false)
    var requestIdentity: Long = 0

    val hasUnacceptedWork: Boolean
        get() = preview.value != null || zoneId.value != "full_body" ||
            goalId.value != "general" || durationText.value != "30"

    fun abandon() {
        zoneId.value = "full_body"
        goalId.value = "general"
        durationText.value = "30"
        preview.value = null
        message.value = null
        warningAcknowledged.value = false
        editingIndex.value = null
        setsText.value = ""
        repsText.value = ""
        restText.value = ""
        loadText.value = ""
        loadChoice.value = GeneratorLoadChoice.AUTOMATIC
        maxPercentText.value = "70"
    }
}

@Composable
fun SessionGeneratorScreen(
    repository: TrainlogRepository,
    state: SessionGeneratorUiState,
    onBack: () -> Unit,
    onAccepted: () -> Unit,
    onExistingDraft: () -> Unit,
) {
    val strings = localizedContext()
    val colors = LocalTrainlogColors.current
    val scope = rememberCoroutineScope()
    val options = remember { repository.sessionGenerationFormOptions() }
    val zones = remember { repository.listBodyZones().filter { it.zoneId in options.zoneIds } }
    var zoneId by state.zoneId
    var goalId by state.goalId
    var durationText by state.durationText
    var preview by state.preview
    var message by state.message
    var warningAcknowledged by state.warningAcknowledged
    var editingIndex by state.editingIndex
    var setsText by state.setsText
    var repsText by state.repsText
    var restText by state.restText
    var loadText by state.loadText
    var loadChoice by state.loadChoice
    var maxPercentText by state.maxPercentText
    var busy by state.busy

    fun generate(request: SessionGenerationRequest) {
        if (busy) return
        val requestIdentity = ++state.requestIdentity
        scope.launch {
            busy = true
            try {
                when (val result = withContext(Dispatchers.IO) { repository.generateSessionPreview(request) }) {
                    is SessionGenerationResult.Generated -> {
                        /* INVARIANT: an obsolete async generation request may
                         * not replace a newer proposal after navigation. */
                        if (requestIdentity != state.requestIdentity) return@launch
                        preview = result.preview
                        warningAcknowledged = result.preview.exposure.warningLevel == GenerationWarningLevel.NONE
                        message = null
                    }
                    is SessionGenerationResult.Invalid -> if (requestIdentity == state.requestIdentity) message = localizedRepositoryMessage(strings, result.message)
                    is SessionGenerationResult.DatabaseError -> if (requestIdentity == state.requestIdentity) message = localizedRepositoryMessage(strings, result.message)
                }
            } finally {
                busy = false
            }
        }
    }

    TrainlogScreen(subtitle = strings.getString(R.string.route_session_generator)) {
        val current = preview
        if (current == null) {
            TrainlogFrame(strings.getString(R.string.generator_zone)) {
                TrainlogChoiceChips(zones.map { it.zoneId to localizedBodyZoneName(strings, it.zoneId, it.displayName) }, zoneId) { zoneId = it }
            }
            TrainlogFrame(strings.getString(R.string.generator_goal)) {
                TrainlogChoiceChips(SessionGeneratorFormController.goals.filter { it in options.goalIds }.map {
                    it to strings.getString(when (it) { "general" -> R.string.goal_general; "strength" -> R.string.goal_strength
                        "hypertrophy" -> R.string.goal_hypertrophy; else -> R.string.goal_endurance })
                }, goalId) { goalId = it }
            }
            TrainlogFrame(strings.getString(R.string.field_duration)) {
                TrainlogChoiceChips(options.durationPresets.map { it.toString() to "$it min" }, durationText) { durationText = it }
                TrainlogInputField(
                    strings.getString(R.string.generator_custom_duration, options.customMinutes.first, options.customMinutes.last),
                    durationText,
                    onValueChange = { durationText = it },
                )
            }
            TrainlogAction(strings.getString(R.string.generate), strings.getString(R.string.generate_description), accent = colors.success, onClick = {
                val minutes = SessionGeneratorFormController.duration(durationText, options.customMinutes)
                if (minutes == null) message = strings.getString(R.string.duration_range_error, options.customMinutes.first, options.customMinutes.last)
                else generate(SessionGenerationRequest(zoneId, goalId, minutes, OffsetDateTime.now().toString()))
            })
        } else {
            TrainlogFrame(strings.getString(R.string.proposal)) {
                TrainlogInfo(strings.getString(R.string.target_duration_estimate, current.request.durationMinutes, current.estimatedDurationSeconds / 60))
                if (current.request.durationMinutes * 60 - current.estimatedDurationSeconds >= 300)
                    TrainlogInfo(strings.getString(R.string.proposal_short), colors.warning)
                TrainlogInfo(strings.getString(R.string.warmup_not_generated), colors.muted)
                if (current.insufficientResolvedCandidates) TrainlogInfo(
                    strings.getString(R.string.coverage_incomplete,
                        current.shortageCodes.takeIf { it.isNotEmpty() }
                            ?.joinToString(prefix = " : ", postfix = ".") { generationShortageLabel(it, strings) }.orEmpty()),
                    colors.warning,
                )
                val exposure = current.exposure
                TrainlogInfo(strings.getString(R.string.exposure_summary, exposure.within24h.primarySetCount, exposure.within24h.secondarySetCount, exposure.within72h.primarySetCount, exposure.within72h.secondarySetCount))
                exposure.latestStartedAt?.let { TrainlogInfo(strings.getString(R.string.latest_exposure, formatStartedAt(it)), colors.muted) }
                if (exposure.warningLevel != GenerationWarningLevel.NONE) {
                    TrainlogInfo(strings.getString(R.string.recent_exposure_warning), colors.warning)
                    if (!warningAcknowledged) TrainlogAction(strings.getString(R.string.continue_warning), strings.getString(R.string.continue_warning_description), accent = colors.warning, onClick = {
                        if (!busy) warningAcknowledged = true
                    })
                }
            }
            current.exercises.forEachIndexed { index, item ->
                TrainlogFrame("${index + 1}. ${item.exerciseName}") {
                    TrainlogInfo(strings.getString(R.string.equipment_generator, item.equipmentName))
                    TrainlogInfo(strings.getString(R.string.target_summary, item.plan.sets, item.plan.reps.toString(), item.plan.restSeconds))
                    TrainlogInfo(if (item.plan.weightKg == null) strings.getString(R.string.target_load_none_proposed)
                        else strings.getString(R.string.target_load_proposed, item.plan.weightKg))
                    TrainlogInfo(strings.getString(R.string.primary_zone_value_short, item.primaryZoneName))
                    TrainlogInfo(strings.getString(R.string.movement_value, item.patternNames.joinToString()))
                    if (item.recency.recentSameExercise || item.recency.recentSamePattern)
                        TrainlogInfo(strings.getString(R.string.recent_similar), colors.warning)
                    item.loadSourceStartedAt?.let {
                        TrainlogInfo(
                            strings.getString(R.string.observed_load_note, formatDate(it)),
                            colors.muted,
                        )
                    }
                    TrainlogInfo(
                        strings.getString(R.string.reasons_value, item.rationaleCodes.joinToString { generationReasonLabel(it, strings) }),
                        colors.muted,
                    )
                    if (editingIndex == index) {
                        TrainlogInputField(strings.getString(R.string.sets_label), setsText, onValueChange = { setsText = it })
                        TrainlogInputField(strings.getString(R.string.field_reps), repsText, onValueChange = { repsText = it })
                        TrainlogInputField(strings.getString(R.string.rest_seconds), restText, onValueChange = { restText = it })
                        TrainlogInfo(strings.getString(R.string.user_load_note), colors.muted)
                        TrainlogChoiceChips(
                            listOf("AUTOMATIC" to strings.getString(R.string.automatic), "PERCENT_MAX" to "% MAX", "NONE" to strings.getString(R.string.none)),
                            loadChoice.name,
                        ) { selected ->
                            loadChoice = GeneratorLoadChoice.valueOf(selected)
                            if (loadChoice != GeneratorLoadChoice.AUTOMATIC) loadText = ""
                        }
                        if (loadChoice == GeneratorLoadChoice.PERCENT_MAX)
                            TrainlogInputField(strings.getString(R.string.max_percentage_field), maxPercentText,
                                onValueChange = { maxPercentText = it })
                        if (loadChoice == GeneratorLoadChoice.AUTOMATIC)
                            TrainlogInputField(
                                strings.getString(R.string.manual_target_load),
                                loadText,
                                onValueChange = { loadText = it },
                            )
                        TrainlogAction(strings.getString(R.string.apply), strings.getString(R.string.reestimate_description), accent = colors.success, onClick = {
                            val parsedWeight = if (loadChoice == GeneratorLoadChoice.AUTOMATIC)
                                SessionGeneratorFormController.manualWeight(loadText)
                            else Result.success(null)
                            if (parsedWeight.isFailure) {
                                message = strings.getString(R.string.generator_weight_invalid)
                            } else {
                                val weight = parsedWeight.getOrNull()
                                if (!busy) scope.launch {
                                    busy = true
                                    try {
                                        when (val result = withContext(Dispatchers.IO) {
                                            repository.editGeneratedDose(current, index,
                                                setsText.toIntOrNull() ?: -1, repsText.toIntOrNull() ?: -1,
                                                restText.toIntOrNull() ?: -1, weight, loadChoice,
                                                maxPercentText.toIntOrNull())
                                        }) {
                                            is SessionGenerationResult.Generated -> { preview = result.preview; editingIndex = null; message = null }
                                            is SessionGenerationResult.Invalid -> message = localizedRepositoryMessage(strings, result.message)
                                            is SessionGenerationResult.DatabaseError -> message = localizedRepositoryMessage(strings, result.message)
                                        }
                                    } finally {
                                        busy = false
                                    }
                                }
                            }
                        })
                    } else TrainlogAction(strings.getString(R.string.modify_target), strings.getString(R.string.modify_target_description), onClick = {
                        if (!busy) {
                            editingIndex = index; setsText = item.plan.sets.toString(); repsText = item.plan.reps.toString()
                            restText = item.plan.restSeconds.toString()
                            // Preserve an explicit user value across later edits. An
                            // automatic observed value stays display-only: empty asks
                            // the engine to qualify it again for the changed dose.
                            loadChoice = when {
                                "user_selected_max_percentage" in item.rationaleCodes ||
                                    "compatible_max_unavailable" in item.rationaleCodes -> GeneratorLoadChoice.PERCENT_MAX
                                "numeric_load_absent" in item.rationaleCodes -> GeneratorLoadChoice.NONE
                                else -> GeneratorLoadChoice.AUTOMATIC
                            }
                            loadText = if ("manual_target_load" in item.rationaleCodes)
                                item.plan.weightKg?.toString().orEmpty() else ""
                        }
                    })
                    TrainlogAction(strings.getString(R.string.move_up), strings.getString(R.string.move_up_description), onClick = {
                        if (!busy) preview = SessionGeneratorPreviewController.move(current, index, -1)
                    }, accent = colors.muted)
                    TrainlogAction(strings.getString(R.string.move_down), strings.getString(R.string.move_down_description), onClick = {
                        if (!busy) preview = SessionGeneratorPreviewController.move(current, index, 1)
                    }, accent = colors.muted)
                    TrainlogDeleteButton(strings.getString(R.string.remove_proposal_exercise), onClick = {
                        if (!busy) preview = SessionGeneratorPreviewController.remove(current, index)
                    })
                }
            }
            TrainlogAction(strings.getString(R.string.regenerate), strings.getString(R.string.regenerate_description), onClick = { generate(current.request) })
            TrainlogAction(strings.getString(R.string.accept_actual_values), strings.getString(R.string.accept_actual_description), accent = colors.success, onClick = {
                if (!warningAcknowledged) message = strings.getString(R.string.acknowledge_exposure_first)
                else if (!busy) scope.launch {
                    busy = true
                    try {
                        when (val result = withContext(Dispatchers.IO) { repository.acceptGeneratedSession(current) }) {
                            AcceptGeneratedSessionResult.Accepted -> onAccepted()
                            AcceptGeneratedSessionResult.ExistingActiveDraft -> onExistingDraft()
                            is AcceptGeneratedSessionResult.Invalid -> message = localizedRepositoryMessage(strings, result.message)
                            is AcceptGeneratedSessionResult.DatabaseError -> message = localizedRepositoryMessage(strings, result.message)
                        }
                    } finally {
                        busy = false
                    }
                }
            })
            TrainlogAction(strings.getString(R.string.cancel_proposal), strings.getString(R.string.cancel_proposal_description), onClick = {
                if (!busy) onBack()
            }, accent = colors.muted)
        }
        if (busy) TrainlogFrame(strings.getString(R.string.processing)) { TrainlogInfo(strings.getString(R.string.analysis_running), colors.muted) }
        message?.let { TrainlogFrame(strings.getString(R.string.message_title)) { TrainlogInfo(it, colors.error) } }
    }
}

private fun generationReasonLabel(code: String, context: android.content.Context): String = context.getString(when (code) {
    "observed_repeated_dose_anchor" -> R.string.reason_observed_dose
    "explicit_max_present_no_numeric_prescription" -> R.string.reason_max_no_prescription
    "user_selected_max_percentage" -> R.string.reason_user_max
    "compatible_max_unavailable" -> R.string.reason_max_unavailable
    "assistance_numeric_load_omitted" -> R.string.reason_assistance_omitted
    "numeric_load_absent" -> R.string.reason_load_absent
    "manual_target_load" -> R.string.reason_manual_load
    "preferred_exercise" -> R.string.reason_preferred
    "requested_primary_zone" -> R.string.reason_primary_zone
    "requested_secondary_zone" -> R.string.reason_secondary_zone
    "new_exact_pattern" -> R.string.reason_distinct_movement
    "recent_same_exercise_penalty" -> R.string.reason_recent_exercise
    "recent_same_pattern_penalty" -> R.string.reason_recent_pattern
    else -> return code.replace('_', ' ')
})

private fun generationShortageLabel(code: String, context: android.content.Context): String = context.getString(when (code) {
    "missing_upper_region" -> R.string.shortage_upper; "missing_lower_region" -> R.string.shortage_lower
    "missing_core_region" -> R.string.shortage_core; "missing_upper_push" -> R.string.shortage_push
    "missing_upper_pull" -> R.string.shortage_pull; "missing_lower_extension" -> R.string.shortage_lower_extension
    "missing_lower_flexion" -> R.string.shortage_lower_flexion; "fewer_than_max_exercises" -> R.string.shortage_exercises
    else -> return code.replace('_', ' ')
})
