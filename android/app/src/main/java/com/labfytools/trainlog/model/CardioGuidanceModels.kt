package com.labfytools.trainlog.model

enum class CardioPhaseKind(val wireValue: String) {
    WARMUP("warmup"),
    WORK("work"),
    RECOVERY("recovery"),
    COOLDOWN("cooldown"),
}

sealed interface CardioTargetDefinition {
    data class AbsoluteBpm(
        val minimumBpm: Int,
        val maximumBpm: Int,
    ) : CardioTargetDefinition

    data class CalibrationPercent(
        val calibrationId: String,
        val observedPeakBpm: Int,
        val minimumPercent: Int,
        val maximumPercent: Int,
    ) : CardioTargetDefinition
}

/**
 * Immutable target actually used by one phase.
 *
 * The calibration identity and percentages are provenance only. Runtime
 * guidance consumes the resolved BPM bounds so later calibration changes
 * cannot rewrite an already-started session.
 */
data class CardioTargetSnapshot(
    val minimumBpm: Int,
    val maximumBpm: Int,
    val calibrationId: String? = null,
    val calibrationObservedPeakBpm: Int? = null,
    val minimumPercent: Int? = null,
    val maximumPercent: Int? = null,
)

sealed interface CardioPhaseExitCondition {
    data class FixedDuration(val seconds: Int) : CardioPhaseExitCondition
    data object EnterTarget : CardioPhaseExitCondition
    data class RecoverBelow(val bpm: Int) : CardioPhaseExitCondition

    data class DurationOrRecoverBelow(
        val maximumSeconds: Int,
        val bpm: Int,
    ) : CardioPhaseExitCondition
}

data class CardioGuidedPhase(
    val phaseId: String,
    val kind: CardioPhaseKind,
    val target: CardioTargetSnapshot?,
    val exitCondition: CardioPhaseExitCondition,
)

enum class CardioGuidanceInstruction {
    ACCELERATE,
    MAINTAIN,
    SLOW_DOWN,
    SUSPENDED,
}

data class CardioGuidanceOutput(
    val instruction: CardioGuidanceInstruction,
    val bpm: Int?,
    val target: CardioTargetSnapshot?,
    val pendingInstruction: CardioGuidanceInstruction? = null,
)

data class CardioGuidancePhaseRecord(
    val runId: String,
    val sessionId: String,
    val entryId: String,
    val phase: CardioGuidedPhase,
    val position: Int,
    val startedAt: String,
    val endedAt: String?,
    val currentInstruction: CardioGuidanceInstruction,
)

data class CardioGuidanceEvent(
    val sequence: Long,
    val observedAt: String,
    val instruction: CardioGuidanceInstruction,
    val bpm: Int?,
    val targetMinimumBpm: Int?,
    val targetMaximumBpm: Int?,
)

data class CardioGuidanceRun(
    val runId: String,
    val sessionId: String,
    val startedAt: String,
    val endedAt: String?,
    val phases: List<CardioGuidancePhaseRecord>,
)
