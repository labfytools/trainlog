package com.labfytools.trainlog.data

/** STATS_V1 is a read-only projection: none of these values are persisted as facts. */
enum class StatisticsPeriod(val days: Long?, val label: String) {
    DAYS_7(7, "7 j"), DAYS_30(30, "30 j"), DAYS_90(90, "90 j"), YEAR(365, "1 an"), ALL(null, "Tout"),
}

data class StatisticsPoint(val timestamp: String, val value: Double, val label: String = "")
data class StatisticsSeries(val id: String, val label: String, val unit: String, val points: List<StatisticsPoint>)
data class StatisticsFrequency(val week: String, val all: Int, val maxima: Int)
data class StatisticsPerformanceWeek(
    val week: String,
    val workingImprovements: Int,
    val maxImprovements: Int,
)
data class StatisticsSummary(
    val sessions: Int,
    val performedSets: Int,
    val distinctExercises: Int,
    val explicitMaxima: Int,
)
data class StatisticsOverview(
    val period: StatisticsPeriod,
    /** True when malformed legacy rows were conservatively omitted. */
    val hasInvalidData: Boolean,
    val summary: StatisticsSummary,
    /** Weekly event counts for the landing graph; never summed kilograms. */
    val performanceEvents: List<StatisticsPerformanceWeek>,
    /** Comparable, context-owned values retained for exercise drill-downs. */
    val performance: List<StatisticsSeries>,
    val body: List<StatisticsSeries>,
    val frequency: List<StatisticsFrequency>,
    val sessionsLast7Days: Int,
    val sessionsLast30Days: Int,
)
