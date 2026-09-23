package com.labfytools.trainlog.data

import com.labfytools.trainlog.model.CardioGuidanceInstruction
import com.labfytools.trainlog.model.CardioTargetDefinition
import com.labfytools.trainlog.model.CardioTargetSnapshot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CardioGuidanceEngineTest {
    private val target = CardioTargetSnapshot(120, 140)

    @Test
    fun absoluteAndCalibrationTargetsResolveToImmutableBpmSnapshot() {
        assertEquals(
            CardioTargetSnapshot(120, 140),
            CardioTargetResolver.resolve(CardioTargetDefinition.AbsoluteBpm(120, 140)),
        )
        assertEquals(
            CardioTargetSnapshot(
                minimumBpm = 100,
                maximumBpm = 125,
                calibrationId = "cal_11111111-1111-4111-8111-111111111111",
                calibrationObservedPeakBpm = 167,
                minimumPercent = 60,
                maximumPercent = 75,
            ),
            CardioTargetResolver.resolve(
                CardioTargetDefinition.CalibrationPercent(
                    calibrationId = "cal_11111111-1111-4111-8111-111111111111",
                    observedPeakBpm = 167,
                    minimumPercent = 60,
                    maximumPercent = 75,
                ),
            ),
        )
    }

    @Test
    fun invalidTargetDefinitionsAreRejectedInsteadOfClamped() {
        assertNull(CardioTargetResolver.resolve(CardioTargetDefinition.AbsoluteBpm(140, 120)))
        assertNull(
            CardioTargetResolver.resolve(
                CardioTargetDefinition.CalibrationPercent("cal_x", 0, 60, 70),
            ),
        )
        assertNull(
            CardioTargetResolver.resolve(
                CardioTargetDefinition.CalibrationPercent("cal_x", 160, 80, 60),
            ),
        )
    }

    @Test
    fun lowBpmMustPersistBeforeAccelerateInstructionChanges() {
        val engine = CardioGuidanceEngine(hysteresisBpm = 2, confirmationMs = 3_000)
        assertEquals(
            CardioGuidanceInstruction.MAINTAIN,
            engine.evaluate(target, 110, true, 1_000).instruction,
        )
        assertEquals(
            CardioGuidanceInstruction.MAINTAIN,
            engine.evaluate(target, 111, true, 3_999).instruction,
        )
        val changed = engine.evaluate(target, 111, true, 4_000)
        assertEquals(CardioGuidanceInstruction.ACCELERATE, changed.instruction)
        assertNull(changed.pendingInstruction)
    }

    @Test
    fun highBpmMustPersistBeforeSlowDownInstructionChanges() {
        val engine = CardioGuidanceEngine(hysteresisBpm = 2, confirmationMs = 2_000)
        engine.evaluate(target, 150, true, 0)
        assertEquals(
            CardioGuidanceInstruction.SLOW_DOWN,
            engine.evaluate(target, 150, true, 2_000).instruction,
        )
    }

    @Test
    fun hysteresisPreventsBoundaryYoyo() {
        val engine = CardioGuidanceEngine(hysteresisBpm = 2, confirmationMs = 0)
        assertEquals(
            CardioGuidanceInstruction.ACCELERATE,
            engine.evaluate(target, 117, true, 0).instruction,
        )
        // 119 is still below target, but not enough to leave ACCELERATE.
        assertEquals(
            CardioGuidanceInstruction.ACCELERATE,
            engine.evaluate(target, 119, true, 1).instruction,
        )
        assertEquals(
            CardioGuidanceInstruction.MAINTAIN,
            engine.evaluate(target, 122, true, 2).instruction,
        )
        // 141 is just above the zone and remains MAINTAIN within hysteresis.
        assertEquals(
            CardioGuidanceInstruction.MAINTAIN,
            engine.evaluate(target, 141, true, 3).instruction,
        )
        assertEquals(
            CardioGuidanceInstruction.SLOW_DOWN,
            engine.evaluate(target, 143, true, 4).instruction,
        )
    }

    @Test
    fun isolatedOppositeSampleDoesNotReplaceCurrentInstruction() {
        val engine = CardioGuidanceEngine(hysteresisBpm = 2, confirmationMs = 3_000)
        engine.evaluate(target, 110, true, 0)
        engine.evaluate(target, 110, true, 3_000)
        assertEquals(
            CardioGuidanceInstruction.ACCELERATE,
            engine.evaluate(target, 150, true, 3_100).instruction,
        )
        assertEquals(
            CardioGuidanceInstruction.ACCELERATE,
            engine.evaluate(target, 112, true, 3_200).instruction,
        )
    }

    @Test
    fun staleOrMissingSignalSuspendsImmediatelyAndNeverUsesOldBpm() {
        val engine = CardioGuidanceEngine(hysteresisBpm = 2, confirmationMs = 0)
        assertEquals(
            CardioGuidanceInstruction.ACCELERATE,
            engine.evaluate(target, 110, true, 0).instruction,
        )
        val stale = engine.evaluate(target, 110, false, 1_000)
        assertEquals(CardioGuidanceInstruction.SUSPENDED, stale.instruction)
        assertNull(stale.bpm)

        // A fresh sample must re-establish a candidate from real current data.
        val resumed = engine.evaluate(target, 130, true, 2_000)
        assertTrue(resumed.instruction != CardioGuidanceInstruction.SUSPENDED)
        assertEquals(130, resumed.bpm)
    }

    @Test
    fun noTargetSuspendsGuidance() {
        val engine = CardioGuidanceEngine()
        val output = engine.evaluate(null, 130, true, 0)
        assertEquals(CardioGuidanceInstruction.SUSPENDED, output.instruction)
        assertNull(output.bpm)
    }
}
