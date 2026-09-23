package com.labfytools.trainlog.data

import com.labfytools.trainlog.model.CardioGuidedPhase
import com.labfytools.trainlog.model.CardioPhaseExitCondition
import com.labfytools.trainlog.model.CardioPhaseKind
import com.labfytools.trainlog.model.CardioTargetSnapshot
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CardioPhaseCompletionTest {
    private val target = CardioTargetSnapshot(120, 140)

    private fun phase(
        exit: CardioPhaseExitCondition,
        targetOverride: CardioTargetSnapshot? = target,
    ) = CardioGuidedPhase(
        phaseId = "phase_test",
        kind = CardioPhaseKind.WORK,
        target = targetOverride,
        exitCondition = exit,
    )

    @Test
    fun fixedDurationUsesElapsedTimeOnly() {
        val p = phase(CardioPhaseExitCondition.FixedDuration(180))
        assertFalse(CardioPhaseCompletion.isComplete(p, 179, null, false))
        assertTrue(CardioPhaseCompletion.isComplete(p, 180, null, false))
    }

    @Test
    fun enterTargetRequiresFreshMeasurementInsideSnapshot() {
        val p = phase(CardioPhaseExitCondition.EnterTarget)
        assertFalse(CardioPhaseCompletion.isComplete(p, 10, 130, false))
        assertFalse(CardioPhaseCompletion.isComplete(p, 10, 119, true))
        assertTrue(CardioPhaseCompletion.isComplete(p, 10, 120, true))
        assertTrue(CardioPhaseCompletion.isComplete(p, 10, 140, true))
    }

    @Test
    fun enterTargetWithoutTargetNeverCompletes() {
        val p = phase(CardioPhaseExitCondition.EnterTarget, null)
        assertFalse(CardioPhaseCompletion.isComplete(p, 100, 130, true))
    }

    @Test
    fun recoveryThresholdRequiresFreshMeasurement() {
        val p = phase(CardioPhaseExitCondition.RecoverBelow(110), null)
        assertFalse(CardioPhaseCompletion.isComplete(p, 30, 105, false))
        assertFalse(CardioPhaseCompletion.isComplete(p, 30, 111, true))
        assertTrue(CardioPhaseCompletion.isComplete(p, 30, 110, true))
    }

    @Test
    fun durationOrRecoveryCanFinishFromEitherRealCondition() {
        val p = phase(CardioPhaseExitCondition.DurationOrRecoverBelow(300, 110), null)
        assertFalse(CardioPhaseCompletion.isComplete(p, 299, 120, true))
        assertTrue(CardioPhaseCompletion.isComplete(p, 100, 108, true))
        assertTrue(CardioPhaseCompletion.isComplete(p, 300, null, false))
    }

    @Test
    fun negativeElapsedTimeCannotCompleteAnything() {
        val p = phase(CardioPhaseExitCondition.FixedDuration(0))
        assertFalse(CardioPhaseCompletion.isComplete(p, -1, 130, true))
    }
}
