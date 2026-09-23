package com.labfytools.trainlog.model

enum class CardioCalibrationPhase(val wireValue: String) {
    WARMUP("warmup"),
    PROGRESSIVE("progressive"),
    HIGH_EFFORT("high_effort"),
    RECOVERY("recovery"),
    COMPLETED("completed"),
    ABORTED("aborted");

    companion object {
        fun fromWire(value: String): CardioCalibrationPhase =
            entries.firstOrNull { it.wireValue == value }
                ?: error("Unknown cardio calibration phase: $value")
    }
}

data class CardioCalibrationRecoveryPoint(
    val targetOffsetSeconds: Int,
    val observedAt: String,
    val bpm: Int,
)

data class CardioCalibrationProfile(
    val calibrationId: String,
    val protocolVersion: Int,
    val sessionId: String,
    val entryId: String,
    val phase: CardioCalibrationPhase,
    val startedAt: String,
    val phaseStartedAt: String,
    val effortEndAt: String?,
    val endedAt: String?,
    val heartRateCaptureId: String?,
    val observedPeakBpm: Int?,
    val recovery: List<CardioCalibrationRecoveryPoint>,
)
