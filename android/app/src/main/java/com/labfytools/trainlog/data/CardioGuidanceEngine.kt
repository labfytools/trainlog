package com.labfytools.trainlog.data

import com.labfytools.trainlog.model.CardioGuidanceInstruction
import com.labfytools.trainlog.model.CardioGuidanceOutput
import com.labfytools.trainlog.model.CardioTargetDefinition
import com.labfytools.trainlog.model.CardioTargetSnapshot
import kotlin.math.roundToInt

internal object CardioTargetResolver {
    fun resolve(definition: CardioTargetDefinition): CardioTargetSnapshot? =
        when (definition) {
            is CardioTargetDefinition.AbsoluteBpm -> {
                if (!validBounds(definition.minimumBpm, definition.maximumBpm)) return null
                CardioTargetSnapshot(
                    minimumBpm = definition.minimumBpm,
                    maximumBpm = definition.maximumBpm,
                )
            }

            is CardioTargetDefinition.CalibrationPercent -> {
                if (
                    definition.calibrationId.isBlank() ||
                    definition.observedPeakBpm !in 1..65535 ||
                    definition.minimumPercent !in 1..100 ||
                    definition.maximumPercent !in 1..100 ||
                    definition.minimumPercent > definition.maximumPercent
                ) return null
                val minimum =
                    (definition.observedPeakBpm * definition.minimumPercent / 100.0)
                        .roundToInt()
                val maximum =
                    (definition.observedPeakBpm * definition.maximumPercent / 100.0)
                        .roundToInt()
                if (!validBounds(minimum, maximum)) return null
                CardioTargetSnapshot(
                    minimumBpm = minimum,
                    maximumBpm = maximum,
                    calibrationId = definition.calibrationId,
                    calibrationObservedPeakBpm = definition.observedPeakBpm,
                    minimumPercent = definition.minimumPercent,
                    maximumPercent = definition.maximumPercent,
                )
            }
        }

    private fun validBounds(minimum: Int, maximum: Int): Boolean =
        minimum in 1..65535 && maximum in 1..65535 && minimum <= maximum
}

/**
 * Pure state machine for BPM guidance.
 *
 * Hysteresis and confirmation time are UI/control stability parameters, not
 * physiological zones. The engine never extrapolates a stale measurement.
 */
internal object CardioPhaseCompletion {
    fun isComplete(
        phase: com.labfytools.trainlog.model.CardioGuidedPhase,
        elapsedSeconds: Int,
        bpm: Int?,
        fresh: Boolean,
    ): Boolean {
        if (elapsedSeconds < 0) return false
        return when (val exit = phase.exitCondition) {
            is com.labfytools.trainlog.model.CardioPhaseExitCondition.FixedDuration ->
                elapsedSeconds >= exit.seconds

            com.labfytools.trainlog.model.CardioPhaseExitCondition.EnterTarget -> {
                val target = phase.target ?: return false
                fresh && bpm != null && bpm in target.minimumBpm..target.maximumBpm
            }

            is com.labfytools.trainlog.model.CardioPhaseExitCondition.RecoverBelow ->
                fresh && bpm != null && bpm <= exit.bpm

            is com.labfytools.trainlog.model.CardioPhaseExitCondition.DurationOrRecoverBelow ->
                elapsedSeconds >= exit.maximumSeconds ||
                    (fresh && bpm != null && bpm <= exit.bpm)
        }
    }
}

internal class CardioGuidanceEngine(
    private val hysteresisBpm: Int = DEFAULT_HYSTERESIS_BPM,
    private val confirmationMs: Long = DEFAULT_CONFIRMATION_MS,
    initialInstruction: CardioGuidanceInstruction = CardioGuidanceInstruction.MAINTAIN,
) {
    private var current =
        initialInstruction.takeUnless { it == CardioGuidanceInstruction.SUSPENDED }
            ?: CardioGuidanceInstruction.MAINTAIN
    private var candidate: CardioGuidanceInstruction? = null
    private var candidateSinceMs: Long? = null

    init {
        require(hysteresisBpm in 0..20)
        require(confirmationMs in 0L..30_000L)
    }

    fun reset() {
        current = CardioGuidanceInstruction.MAINTAIN
        candidate = null
        candidateSinceMs = null
    }

    fun evaluate(
        target: CardioTargetSnapshot?,
        bpm: Int?,
        fresh: Boolean,
        nowMs: Long,
    ): CardioGuidanceOutput {
        require(nowMs >= 0L)
        if (!fresh || bpm == null || bpm !in 1..65535 || target == null) {
            candidate = null
            candidateSinceMs = null
            return CardioGuidanceOutput(
                instruction = CardioGuidanceInstruction.SUSPENDED,
                bpm = null,
                target = target,
            )
        }
        require(target.minimumBpm in 1..65535)
        require(target.maximumBpm in target.minimumBpm..65535)

        val desired = desiredInstruction(target, bpm)
        if (desired == current) {
            candidate = null
            candidateSinceMs = null
            return CardioGuidanceOutput(current, bpm, target)
        }

        if (candidate != desired) {
            candidate = desired
            candidateSinceMs = nowMs
        }
        val since = checkNotNull(candidateSinceMs)
        if (nowMs - since >= confirmationMs) {
            current = desired
            candidate = null
            candidateSinceMs = null
        }

        return CardioGuidanceOutput(
            instruction = current,
            bpm = bpm,
            target = target,
            pendingInstruction = candidate,
        )
    }

    private fun desiredInstruction(
        target: CardioTargetSnapshot,
        bpm: Int,
    ): CardioGuidanceInstruction {
        // A target entered from either side immediately trends toward MAINTAIN.
        if (bpm in target.minimumBpm..target.maximumBpm) {
            return CardioGuidanceInstruction.MAINTAIN
        }

        return when (current) {
            CardioGuidanceInstruction.ACCELERATE ->
                if (bpm >= target.minimumBpm + hysteresisBpm) {
                    CardioGuidanceInstruction.MAINTAIN
                } else {
                    CardioGuidanceInstruction.ACCELERATE
                }

            CardioGuidanceInstruction.SLOW_DOWN ->
                if (bpm <= target.maximumBpm - hysteresisBpm) {
                    CardioGuidanceInstruction.MAINTAIN
                } else {
                    CardioGuidanceInstruction.SLOW_DOWN
                }

            CardioGuidanceInstruction.MAINTAIN,
            CardioGuidanceInstruction.SUSPENDED ->
                when {
                    bpm < target.minimumBpm - hysteresisBpm ->
                        CardioGuidanceInstruction.ACCELERATE
                    bpm > target.maximumBpm + hysteresisBpm ->
                        CardioGuidanceInstruction.SLOW_DOWN
                    else -> CardioGuidanceInstruction.MAINTAIN
                }
        }
    }

    companion object {
        const val DEFAULT_HYSTERESIS_BPM = 2
        const val DEFAULT_CONFIRMATION_MS = 3_000L
    }
}
