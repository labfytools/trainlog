package com.labfytools.trainlog.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import com.labfytools.trainlog.R
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.CardioCalibrationPhase
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun CardioCalibrationScreen(
    repository: TrainlogRepository,
    onChanged: () -> Unit,
) {
    val strings = localizedContext()
    val colors = LocalTrainlogColors.current
    var revision by remember { mutableIntStateOf(0) }
    var message by remember { mutableStateOf<String?>(null) }
    val active = remember(revision) { repository.activeCardioCalibration() }
    val latest = remember(revision) {
        repository.listCompletedCardioCalibrations(1).firstOrNull()
    }

    fun apply(result: TrainlogRepository.CardioCalibrationResult) {
        message =
            when (result) {
                is TrainlogRepository.CardioCalibrationResult.Applied -> {
                    revision++
                    onChanged()
                    null
                }
                is TrainlogRepository.CardioCalibrationResult.Invalid -> result.message
                TrainlogRepository.CardioCalibrationResult.Conflict ->
                    strings.getString(R.string.calibration_conflict)
                is TrainlogRepository.CardioCalibrationResult.DatabaseError ->
                    result.message
            }
    }

    fun phaseLabel(phase: CardioCalibrationPhase): String =
        strings.getString(
            when (phase) {
                CardioCalibrationPhase.WARMUP -> R.string.calibration_phase_warmup
                CardioCalibrationPhase.PROGRESSIVE -> R.string.calibration_phase_progressive
                CardioCalibrationPhase.HIGH_EFFORT -> R.string.calibration_phase_high_effort
                CardioCalibrationPhase.RECOVERY -> R.string.calibration_phase_recovery
                CardioCalibrationPhase.COMPLETED -> R.string.calibration_phase_completed
                CardioCalibrationPhase.ABORTED -> R.string.calibration_phase_aborted
            },
        )

    TrainlogScreen(
        strings.getString(R.string.route_cardio_calibration),
        scrollKey = "cardio-calibration",
    ) {
        TrainlogInfo(strings.getString(R.string.calibration_explanation), colors.muted)

        if (active == null) {
            TrainlogPrimaryAction(
                strings.getString(R.string.calibration_start),
                strings.getString(R.string.calibration_start_description),
            ) {
                apply(repository.startCardioCalibration())
            }
            latest?.let { profile ->
                TrainlogFrame(strings.getString(R.string.calibration_latest)) {
                    TrainlogInfo(
                        strings.getString(
                            R.string.calibration_peak_value,
                            profile.observedPeakBpm ?: 0,
                        ),
                        colors.accent,
                    )
                    profile.recovery.forEach { point ->
                        TrainlogInfo(
                            strings.getString(
                                R.string.calibration_recovery_value,
                                point.targetOffsetSeconds / 60,
                                point.bpm,
                            ),
                            colors.muted,
                        )
                    }
                }
            }
        } else {
            TrainlogFrame(strings.getString(R.string.calibration_current)) {
                TrainlogInfo(
                    strings.getString(
                        R.string.calibration_phase_value,
                        phaseLabel(active.phase),
                    ),
                    colors.accent,
                )
                active.observedPeakBpm?.let { bpm ->
                    TrainlogInfo(
                        strings.getString(R.string.calibration_peak_value, bpm),
                        colors.warning,
                    )
                }
                active.recovery.forEach { point ->
                    TrainlogInfo(
                        strings.getString(
                            R.string.calibration_recovery_value,
                            point.targetOffsetSeconds / 60,
                            point.bpm,
                        ),
                        colors.muted,
                    )
                }
            }

            when (active.phase) {
                CardioCalibrationPhase.WARMUP -> {
                    TrainlogPrimaryAction(
                        strings.getString(R.string.calibration_to_progressive),
                        strings.getString(R.string.calibration_progressive_description),
                    ) {
                        apply(repository.advanceCardioCalibrationPhase())
                    }
                }
                CardioCalibrationPhase.PROGRESSIVE -> {
                    TrainlogPrimaryAction(
                        strings.getString(R.string.calibration_to_high_effort),
                        strings.getString(R.string.calibration_high_effort_description),
                    ) {
                        apply(repository.advanceCardioCalibrationPhase())
                    }
                }
                CardioCalibrationPhase.HIGH_EFFORT -> {
                    TrainlogPrimaryAction(
                        strings.getString(R.string.calibration_end_effort),
                        strings.getString(R.string.calibration_end_effort_description),
                    ) {
                        apply(repository.markCardioCalibrationEffortEnd())
                    }
                }
                CardioCalibrationPhase.RECOVERY -> {
                    TrainlogPrimaryAction(
                        strings.getString(R.string.calibration_finish),
                        strings.getString(R.string.calibration_finish_description),
                    ) {
                        apply(repository.finishCardioCalibration())
                    }
                }
                CardioCalibrationPhase.COMPLETED,
                CardioCalibrationPhase.ABORTED -> Unit
            }

            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.End,
            ) {
                TrainlogButton(
                    strings.getString(R.string.calibration_abort),
                    { apply(repository.abortCardioCalibration()) },
                    style = TrainlogButtonStyle.DESTRUCTIVE,
                )
            }
        }

        message?.let { TrainlogInfo(it, colors.error) }
    }
}
