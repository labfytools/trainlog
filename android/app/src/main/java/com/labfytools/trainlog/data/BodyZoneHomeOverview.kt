package com.labfytools.trainlog.data

enum class BodyZoneHomeState(val label: String) {
    PRIORITIZE("À privilégier"),
    LITTLE_RECENT_WORK("Peu travaillé récemment"),
    RECENT_WORK("Travaillé récemment"),
    HIGH_RECENT_EXPOSURE("Forte exposition récente"),
    INSUFFICIENT_DATA("Données insuffisantes"),
    UNSUPPORTED("Aucun exercice résolu disponible"),
}

data class BodyZoneHomeStatus(
    val zoneId: String,
    val displayName: String,
    val lastPrimaryExposure: String?,
    val lastSecondaryExposure: String?,
    val primaryExposureAgeSeconds: Long?,
    val secondaryExposureAgeSeconds: Long?,
    val primaryWork7Days: Int,
    val secondaryWork7Days: Int,
    val primaryWork30Days: Int,
    val secondaryWork30Days: Int,
    /** Distinct completed sessions with qualifying primary or secondary work in 30 days. */
    val recentSessionCount: Int,
    val availableExerciseCount: Int,
    val state: BodyZoneHomeState,
    val reasons: List<String>,
)

data class BodyZoneHomeOverview(
    val zones: List<BodyZoneHomeStatus>,
    val recommendations: List<BodyZoneHomeStatus>,
    val hasTrainingHistory: Boolean,
    val hasInvalidHistoryTimestamp: Boolean,
)
