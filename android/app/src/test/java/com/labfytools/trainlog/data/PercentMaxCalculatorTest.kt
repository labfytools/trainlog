package com.labfytools.trainlog.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PercentMaxCalculatorTest {
    private fun maximum(
        equipmentId: String? = "leg_press",
        semantics: EquipmentLoadSemantics? = EquipmentLoadSemantics.EXTERNAL,
    ) = ExplicitMaxContext(
        sessionId = "se_test", entryId = "sxe_test", startedAt = "2026-09-10T08:00:00Z",
        maxWeightKg = 137.5, equipmentId = equipmentId,
        equipmentDisplayName = "Presse", loadSemantics = semantics,
    )

    @Test
    fun exactExternalContextUsesUnroundedFormula() {
        assertEquals(100.375,
            PercentMaxCalculator.calculate(maximum(), "leg_press", 73)!!, 0.0000001)
    }

    @Test
    fun incompatibleMissingAssistanceAndBoundsAreUnavailable() {
        assertNull(PercentMaxCalculator.calculate(maximum(), "other", 73))
        assertNull(PercentMaxCalculator.calculate(
            maximum(semantics = EquipmentLoadSemantics.ASSISTANCE), "leg_press", 73))
        assertNull(PercentMaxCalculator.calculate(maximum(equipmentId = null), "leg_press", 73))
        assertNull(PercentMaxCalculator.calculate(maximum(), "leg_press", 0))
        assertNull(PercentMaxCalculator.calculate(maximum(), "leg_press", 101))
    }
}
