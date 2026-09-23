package com.labfytools.trainlog.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.R
import com.labfytools.trainlog.data.CardioGuidanceLiveState
import com.labfytools.trainlog.data.CardioTargetResolver
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.CardioGuidanceInstruction
import com.labfytools.trainlog.model.CardioGuidedPhase
import com.labfytools.trainlog.model.CardioPhaseExitCondition
import com.labfytools.trainlog.model.CardioPhaseKind
import com.labfytools.trainlog.model.CardioTargetDefinition
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import java.util.UUID

@Composable
internal fun CardioGuidancePanel(
    repository: TrainlogRepository,
    entryId: String,
) {
    val strings = localizedContext()
    val colors = LocalTrainlogColors.current
    var revision by remember(entryId) { mutableStateOf(0) }
    val active =
        remember(entryId, revision) {
            repository.activeCardioGuidancePhase()?.takeIf { it.entryId == entryId }
        }
    val calibration =
        remember(revision) {
            repository.listCompletedCardioCalibrations(1).firstOrNull()
        }
    var live by remember { mutableStateOf(CardioGuidanceLiveState.current()) }

    DisposableEffect(entryId) {
        val subscription = CardioGuidanceLiveState.subscribe { live = it }
        onDispose { subscription.close() }
    }

    TrainlogFrame(
        title = strings.getString(R.string.cardio_guidance_title),
        active = active != null,
    ) {
        if (active != null) {
            val phase = active.phase
            val current = live.takeIf { it.phaseId == phase.phaseId }
            val instruction = current?.instruction ?: active.currentInstruction
            val bpm = current?.bpm

            TrainlogInfo(
                strings.getString(
                    R.string.cardio_guidance_phase_value,
                    phaseKindLabel(strings, phase.kind),
                ),
                colors.muted,
            )
            phase.target?.let { target ->
                TrainlogInfo(
                    strings.getString(
                        R.string.cardio_guidance_target_value,
                        target.minimumBpm,
                        target.maximumBpm,
                    ),
                    colors.accent,
                )
                if (target.calibrationId != null) {
                    TrainlogInfo(
                        strings.getString(
                            R.string.cardio_guidance_calibration_value,
                            target.calibrationObservedPeakBpm ?: 0,
                            target.minimumPercent ?: 0,
                            target.maximumPercent ?: 0,
                        ),
                        colors.muted,
                    )
                }
            }
            TrainlogInfo(
                guidanceInstructionLabel(strings, instruction),
                when (instruction) {
                    CardioGuidanceInstruction.MAINTAIN -> colors.success
                    CardioGuidanceInstruction.ACCELERATE -> colors.accent
                    CardioGuidanceInstruction.SLOW_DOWN,
                    CardioGuidanceInstruction.SUSPENDED -> colors.warning
                },
            )
            TrainlogInfo(
                if (bpm == null) {
                    strings.getString(R.string.cardio_guidance_bpm_missing)
                } else {
                    strings.getString(R.string.cardio_guidance_bpm_value, bpm)
                },
                if (bpm == null) colors.warning else colors.text,
            )
            TrainlogButton(
                label = strings.getString(R.string.cardio_guidance_finish_phase),
                onClick = {
                    if (
                        repository.finishCardioGuidancePhase(phase.phaseId) is
                            TrainlogRepository.CardioGuidanceMutationResult.Applied
                    ) {
                        CardioGuidanceLiveState.clear()
                        revision++
                    }
                },
                modifier = Modifier.fillMaxWidth(),
                style = TrainlogButtonStyle.SECONDARY,
            )
            return@TrainlogFrame
        }

        var kind by remember(entryId) { mutableStateOf(CardioPhaseKind.WARMUP) }
        var targetMode by
            remember(entryId, calibration?.calibrationId) {
                mutableStateOf(if (calibration == null) "bpm" else "calibration")
            }
        var minimumBpm by remember(entryId) { mutableStateOf("") }
        var maximumBpm by remember(entryId) { mutableStateOf("") }
        var minimumPercent by remember(entryId) { mutableStateOf("") }
        var maximumPercent by remember(entryId) { mutableStateOf("") }
        var exitMode by remember(entryId) { mutableStateOf("duration") }
        var durationSeconds by remember(entryId) { mutableStateOf("") }
        var recoveryBpm by remember(entryId) { mutableStateOf("") }
        var message by remember(entryId) { mutableStateOf<String?>(null) }

        TrainlogChoiceChips(
            choices =
                listOf(
                    "warmup" to strings.getString(R.string.cardio_phase_warmup),
                    "work" to strings.getString(R.string.cardio_phase_work),
                    "recovery" to strings.getString(R.string.cardio_phase_recovery),
                    "cooldown" to strings.getString(R.string.cardio_phase_cooldown),
                ),
            selectedId = kind.wireValue,
            onSelected = { selected ->
                kind = CardioPhaseKind.entries.first { it.wireValue == selected }
            },
        )

        TrainlogChoiceChips(
            choices =
                buildList {
                    add("bpm" to strings.getString(R.string.cardio_target_absolute))
                    if (calibration?.observedPeakBpm != null) {
                        add(
                            "calibration" to
                                strings.getString(R.string.cardio_target_calibration),
                        )
                    }
                },
            selectedId = targetMode,
            onSelected = { targetMode = it },
        )

        if (targetMode == "calibration") {
            calibration?.observedPeakBpm?.let { peak ->
                TrainlogInfo(
                    strings.getString(R.string.cardio_guidance_latest_calibration, peak),
                    colors.muted,
                )
            }
            NumberPair(
                leftLabel = strings.getString(R.string.cardio_target_min_percent),
                left = minimumPercent,
                onLeft = { minimumPercent = digits(it, 3) },
                rightLabel = strings.getString(R.string.cardio_target_max_percent),
                right = maximumPercent,
                onRight = { maximumPercent = digits(it, 3) },
                suffix = "%",
            )
        } else {
            NumberPair(
                leftLabel = strings.getString(R.string.cardio_target_min_bpm),
                left = minimumBpm,
                onLeft = { minimumBpm = digits(it, 5) },
                rightLabel = strings.getString(R.string.cardio_target_max_bpm),
                right = maximumBpm,
                onRight = { maximumBpm = digits(it, 5) },
                suffix = "BPM",
            )
        }

        TrainlogChoiceChips(
            choices =
                listOf(
                    "duration" to strings.getString(R.string.cardio_exit_duration),
                    "target" to strings.getString(R.string.cardio_exit_target),
                    "recover" to strings.getString(R.string.cardio_exit_recover),
                    "bounded" to strings.getString(R.string.cardio_exit_bounded),
                ),
            selectedId = exitMode,
            onSelected = { exitMode = it },
        )

        if (exitMode == "duration" || exitMode == "bounded") {
            TrainlogInputField(
                label = strings.getString(R.string.cardio_exit_seconds),
                value = durationSeconds,
                onValueChange = { durationSeconds = digits(it, 5) },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                suffix = "s",
                compact = true,
            )
        }
        if (exitMode == "recover" || exitMode == "bounded") {
            TrainlogInputField(
                label = strings.getString(R.string.cardio_exit_recovery_bpm),
                value = recoveryBpm,
                onValueChange = { recoveryBpm = digits(it, 5) },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                suffix = "BPM",
                compact = true,
            )
        }

        val target =
            if (targetMode == "calibration") {
                val peak = calibration?.observedPeakBpm
                val min = minimumPercent.toIntOrNull()
                val max = maximumPercent.toIntOrNull()
                if (calibration == null || peak == null || min == null || max == null) {
                    null
                } else {
                    CardioTargetResolver.resolve(
                        CardioTargetDefinition.CalibrationPercent(
                            calibration.calibrationId,
                            peak,
                            min,
                            max,
                        ),
                    )
                }
            } else {
                val min = minimumBpm.toIntOrNull()
                val max = maximumBpm.toIntOrNull()
                if (min == null || max == null) null
                else CardioTargetResolver.resolve(
                    CardioTargetDefinition.AbsoluteBpm(min, max),
                )
            }

        target?.let {
            TrainlogInfo(
                strings.getString(
                    R.string.cardio_guidance_resolved_target,
                    it.minimumBpm,
                    it.maximumBpm,
                ),
                colors.accent,
            )
        }

        val exit =
            when (exitMode) {
                "duration" ->
                    durationSeconds.toIntOrNull()
                        ?.takeIf { it in 1..86400 }
                        ?.let(CardioPhaseExitCondition::FixedDuration)
                "target" -> CardioPhaseExitCondition.EnterTarget
                "recover" ->
                    recoveryBpm.toIntOrNull()
                        ?.takeIf { it in 1..65535 }
                        ?.let(CardioPhaseExitCondition::RecoverBelow)
                "bounded" -> {
                    val seconds = durationSeconds.toIntOrNull()
                    val bpm = recoveryBpm.toIntOrNull()
                    if (seconds != null && bpm != null &&
                        seconds in 1..86400 && bpm in 1..65535
                    ) {
                        CardioPhaseExitCondition.DurationOrRecoverBelow(seconds, bpm)
                    } else null
                }
                else -> null
            }

        message?.let { TrainlogInfo(it, colors.warning) }

        TrainlogButton(
            label = strings.getString(R.string.cardio_guidance_start_phase),
            onClick = {
                val actualTarget = target
                val actualExit = exit
                if (actualTarget == null || actualExit == null) {
                    message =
                        strings.getString(R.string.cardio_guidance_invalid_configuration)
                    return@TrainlogButton
                }
                val result =
                    repository.startCardioGuidancePhase(
                        entryId,
                        CardioGuidedPhase(
                            phaseId = "cgp_" + UUID.randomUUID(),
                            kind = kind,
                            target = actualTarget,
                            exitCondition = actualExit,
                        ),
                    )
                if (result is TrainlogRepository.CardioGuidanceMutationResult.Applied) {
                    message = null
                    revision++
                } else {
                    message = strings.getString(R.string.cardio_guidance_start_failed)
                }
            },
            modifier = Modifier.fillMaxWidth(),
            enabled = target != null && exit != null,
            style = TrainlogButtonStyle.SUCCESS,
        )
    }
}

@Composable
private fun NumberPair(
    leftLabel: String,
    left: String,
    onLeft: (String) -> Unit,
    rightLabel: String,
    right: String,
    onRight: (String) -> Unit,
    suffix: String,
) {
    Row(
        Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        TrainlogInputField(
            label = leftLabel,
            value = left,
            onValueChange = onLeft,
            modifier = Modifier.weight(1f),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            suffix = suffix,
            compact = true,
        )
        TrainlogInputField(
            label = rightLabel,
            value = right,
            onValueChange = onRight,
            modifier = Modifier.weight(1f),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            suffix = suffix,
            compact = true,
        )
    }
}

private fun phaseKindLabel(
    strings: android.content.Context,
    kind: CardioPhaseKind,
): String =
    strings.getString(
        when (kind) {
            CardioPhaseKind.WARMUP -> R.string.cardio_phase_warmup
            CardioPhaseKind.WORK -> R.string.cardio_phase_work
            CardioPhaseKind.RECOVERY -> R.string.cardio_phase_recovery
            CardioPhaseKind.COOLDOWN -> R.string.cardio_phase_cooldown
        },
    )

private fun guidanceInstructionLabel(
    strings: android.content.Context,
    instruction: CardioGuidanceInstruction,
): String =
    strings.getString(
        when (instruction) {
            CardioGuidanceInstruction.ACCELERATE -> R.string.cardio_guidance_accelerate
            CardioGuidanceInstruction.MAINTAIN -> R.string.cardio_guidance_maintain
            CardioGuidanceInstruction.SLOW_DOWN -> R.string.cardio_guidance_slow_down
            CardioGuidanceInstruction.SUSPENDED -> R.string.cardio_guidance_suspended
        },
    )

private fun digits(value: String, maximum: Int): String =
    value.filter(Char::isDigit).take(maximum)
