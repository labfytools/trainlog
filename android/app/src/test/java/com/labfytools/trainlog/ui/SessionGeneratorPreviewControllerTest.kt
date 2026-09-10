package com.labfytools.trainlog.ui

import com.labfytools.trainlog.data.BodyZoneRecentExposure
import com.labfytools.trainlog.data.ExposureWindowSummary
import com.labfytools.trainlog.data.GenerationWarningLevel
import com.labfytools.trainlog.data.SessionGenerationPreview
import com.labfytools.trainlog.data.SessionGenerationPreviewExercise
import com.labfytools.trainlog.data.SessionGenerationRequest
import com.labfytools.trainlog.data.TrainingRecencyWarning
import com.labfytools.trainlog.model.SessionExercisePlan
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class SessionGeneratorPreviewControllerTest {
    @Test
    fun formExposesTheCompleteFrenchChoiceSetAndValidatesCustomDuration() {
        assertEquals(
            listOf("Général", "Force", "Hypertrophie", "Endurance locale"),
            SessionGeneratorFormController.goals.map { it.second },
        )
        val allowed = 10..120
        assertEquals(10, SessionGeneratorFormController.duration("10", allowed))
        assertEquals(120, SessionGeneratorFormController.duration("120", allowed))
        assertNull(SessionGeneratorFormController.duration("9", allowed))
        assertNull(SessionGeneratorFormController.duration("121", allowed))
    }

    @Test
    fun manualLoadAcceptsFrenchDecimalAndEmptyMeansAutomaticRequalification() {
        assertEquals(42.5, SessionGeneratorFormController.manualWeight("42,5").getOrThrow()!!, 0.0)
        assertNull(SessionGeneratorFormController.manualWeight("  ").getOrThrow())
        assertTrue(SessionGeneratorFormController.manualWeight("abc").isFailure)
        assertTrue(SessionGeneratorFormController.manualWeight("0").isFailure)
        assertTrue(SessionGeneratorFormController.manualWeight("Infinity").isFailure)
    }

    @Test
    fun removeRetainsTheSelectedOrderAndUpdatesOnlyTransientDuration() {
        val original = preview()

        val changed = SessionGeneratorPreviewController.remove(original, 1)

        assertEquals(listOf("ex_a", "ex_c"), changed.exercises.map { it.exerciseId })
        assertEquals(650, changed.estimatedDurationSeconds)
        assertEquals(listOf("ex_a", "ex_b", "ex_c"), original.exercises.map { it.exerciseId })
        assertEquals(900, original.estimatedDurationSeconds)
    }

    @Test
    fun moveChangesOnlyTheRequestedPositionAndInvalidMovesAreNoOps() {
        val original = preview()
        val moved = SessionGeneratorPreviewController.move(original, 2, -1)

        assertEquals(listOf("ex_a", "ex_c", "ex_b"), moved.exercises.map { it.exerciseId })
        assertEquals(original.estimatedDurationSeconds, moved.estimatedDurationSeconds)
        assertSame(original, SessionGeneratorPreviewController.move(original, 0, -1))
        assertSame(original, SessionGeneratorPreviewController.remove(original, 9))
    }

    private fun preview(): SessionGenerationPreview {
        val request = SessionGenerationRequest("full_body", "general", 30, "2026-09-09T12:00:00Z")
        val items = listOf(
            exercise("ex_a", 100), exercise("ex_b", 250), exercise("ex_c", 250),
        )
        val emptyWindow = ExposureWindowSummary(0, 0, 0, emptyList())
        return SessionGenerationPreview(
            request = request,
            exercises = items,
            estimatedDurationSeconds = 900,
            insufficientResolvedCandidates = false,
            exposure = BodyZoneRecentExposure(
                emptyWindow, emptyWindow, false, false, GenerationWarningLevel.NONE,
                null, null, null, emptyList(), 0,
            ),
        )
    }

    private fun exercise(id: String, seconds: Int) = SessionGenerationPreviewExercise(
        exerciseId = id,
        exerciseName = id,
        equipmentId = "eq_$id",
        equipmentName = "Équipement $id",
        primaryZoneId = "thighs",
        primaryZoneName = "Cuisses",
        patternIds = listOf("knee_dominant"),
        patternNames = listOf("Dominante genou"),
        plan = SessionExercisePlan(2, reps = 10, restSeconds = 90),
        estimatedSeconds = seconds,
        recency = TrainingRecencyWarning(false, false),
        rationaleCodes = listOf("numeric_load_absent"),
        loadSourceSessionId = null,
        loadSourceOccurrenceId = null,
        loadSourceStartedAt = null,
    )
}
