package com.labfytools.trainlog.model

data class BodyObservationDraft(
    val bodyWeightKg: Double? = null,
    val neckCm: Double? = null,
    val shouldersCm: Double? = null,
    val chestCm: Double? = null,
    val waistCm: Double? = null,
    val hipsCm: Double? = null,
    val leftArmCm: Double? = null,
    val rightArmCm: Double? = null,
    val leftForearmCm: Double? = null,
    val rightForearmCm: Double? = null,
    val leftThighCm: Double? = null,
    val rightThighCm: Double? = null,
    val leftCalfCm: Double? = null,
    val rightCalfCm: Double? = null,
) {
    fun hasAnyMetric(): Boolean =
        listOf(
            bodyWeightKg,
            neckCm,
            shouldersCm,
            chestCm,
            waistCm,
            hipsCm,
            leftArmCm,
            rightArmCm,
            leftForearmCm,
            rightForearmCm,
            leftThighCm,
            rightThighCm,
            leftCalfCm,
            rightCalfCm,
        ).any {
            it != null
        }

    fun allValuesPositive(): Boolean =
        listOf(
            bodyWeightKg,
            neckCm,
            shouldersCm,
            chestCm,
            waistCm,
            hipsCm,
            leftArmCm,
            rightArmCm,
            leftForearmCm,
            rightForearmCm,
            leftThighCm,
            rightThighCm,
            leftCalfCm,
            rightCalfCm,
        ).filterNotNull()
            .all {
                it > 0.0
            }
}

data class BodyObservationSummary(
    val observationId: String,
    val observedAt: String,
    val bodyWeightKg: Double?,
    val metricCount: Int,
)
