package com.labfytools.trainlog.data

enum class GeneratorLoadChoice { AUTOMATIC, PERCENT_MAX, NONE }

/** Pure user-directed arithmetic; this is not a recommendation engine. */
object PercentMaxCalculator {
    fun calculate(
        maximum: ExplicitMaxContext,
        equipmentId: String,
        percent: Int,
    ): Double? {
        /* WHY: display names and similar machine families cannot establish a
         * transferable MAX. CONTRACT: exact equipment identity plus external
         * resistance is required; assistance is deliberately unavailable. */
        if (percent !in 1..100 || maximum.equipmentId != equipmentId ||
            maximum.loadSemantics != EquipmentLoadSemantics.EXTERNAL ||
            !maximum.maxWeightKg.isFinite() || maximum.maxWeightKg <= 0.0
        ) return null
        val target = maximum.maxWeightKg * percent.toDouble() / 100.0
        /* INVARIANT: only the returned kg value may enter a SessionExercisePlan;
         * percentage and MAX provenance remain transient preview state. */
        return target.takeIf { it.isFinite() && it > 0.0 }
    }
}
