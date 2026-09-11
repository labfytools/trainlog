package com.labfytools.trainlog.ui

import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.SessionLoadMode
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.data.ManualPercentMaxResult
import com.labfytools.trainlog.model.TrackingMode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SessionSetRowEditorTest {
    @Test
    fun heterogeneousRowsAcceptFrenchCommaBlankAndExplicitZero() {
        val parsed = parseRawSetRows(
            listOf(
                RawSetRow("12", "30"),
                RawSetRow("8", "32,5"),
                RawSetRow("6", ""),
                RawSetRow("5", "0"),
            )
        )

        assertEquals(
            listOf(
                SessionSetDraft(reps = 12, weightKg = 30.0),
                SessionSetDraft(reps = 8, weightKg = 32.5),
                SessionSetDraft(reps = 6, weightKg = null),
                SessionSetDraft(reps = 5, weightKg = 0.0),
            ),
            parsed,
        )
    }

    @Test
    fun invalidFieldDoesNotDestroyRawRowState() {
        val rows = listOf(
            RawSetRow("10", "20"),
            RawSetRow("8x", "32,"),
            RawSetRow("6", ""),
        )

        assertNull(parseRawSetRows(rows))
        assertEquals(
            "Série 2 : saisissez des répétitions entre 0 et 10000.",
            setRowValidationError(rows),
        )
        assertEquals("10;8x;6", encodeRawReps(rows))
        assertEquals("20;32,;", encodeRawWeights(rows))
        assertEquals(
            rows,
            rawSetRowsFromForm(
                SessionDraftForm(
                    repsText = encodeRawReps(rows),
                    weightText = encodeRawWeights(rows),
                )
            ),
        )
    }

    @Test
    fun legacyPartialCompactListKeepsItsTrailingRawRow() {
        assertEquals(
            listOf(
                RawSetRow("4", ""),
                RawSetRow("5", ""),
                RawSetRow("6", ""),
                RawSetRow("", ""),
            ),
            rawSetRowsFromForm(SessionDraftForm(repsText = "4,5,6,")),
        )
    }

    @Test
    fun editDeleteAndAddAreScopedToTheirRows() {
        var rows = listOf(
            RawSetRow("12", "30"),
            RawSetRow("10", "31"),
            RawSetRow("8", "32"),
        )

        rows = rows.replaceAt(1, rows[1].copy(weightText = "32,5"))
        assertEquals(RawSetRow("12", "30"), rows[0])
        assertEquals(RawSetRow("8", "32"), rows[2])

        rows = rows.filterIndexed { index, _ -> index != 0 }
        rows = rows + RawSetRow("6", "")
        assertEquals(
            listOf(
                RawSetRow("10", "32,5"),
                RawSetRow("8", "32"),
                RawSetRow("6", ""),
            ),
            rows,
        )
    }

    @Test
    fun reopeningOccurrenceKeepsMixedNullWeightsAligned() {
        val form = formForExistingExercise(
            SessionExerciseDraft(
                entryId = "sxe_test",
                exercise = repsExercise,
                sets = listOf(
                    SessionSetDraft(reps = 12, weightKg = 30.0),
                    SessionSetDraft(reps = 10, weightKg = null),
                    SessionSetDraft(reps = 8, weightKg = 32.5),
                ),
            ),
            index = 2,
        )

        assertEquals("12,10,8", form.repsText)
        assertEquals("30;;32,5", form.weightText)
        assertEquals(
            listOf(
                RawSetRow("12", "30"),
                RawSetRow("10", ""),
                RawSetRow("8", "32,5"),
            ),
            rawSetRowsFromForm(form),
        )
        assertEquals(2, form.editingExerciseIndex)
        assertEquals("sxe_test", form.editingEntryId)
    }

    @Test
    fun manualTargetPlanNeverChangesPerformedWeights() {
        val actuals = listOf(
            SessionSetDraft(reps = 10, weightKg = 40.0),
            SessionSetDraft(reps = 8, weightKg = 42.5),
        )
        val draft = SessionExerciseDraft(exercise = repsExercise, equipmentId = "leg_press", sets = actuals)
        val result = buildManualTargetPlan(
            draft, null, ManualTargetChoice.PERCENT_MAX, "",
            ManualPercentMaxResult.Available(100.0, "2026-09-10T08:00:00Z", 73.0),
            EquipmentLoadSemantics.EXTERNAL,
        ).getOrThrow()!!
        assertEquals(SessionExercisePlan(2, reps = 10, weightKg = 73.0,
            loadMode = SessionLoadMode.EXTERNAL), result)
        assertEquals(actuals, draft.sets)
        assertNull(buildManualTargetPlan(draft, result, ManualTargetChoice.NONE,
            "", null, EquipmentLoadSemantics.EXTERNAL).getOrThrow())
    }

    @Test
    fun directKgPreservesExistingDoseAndPercentRejectsAssistance() {
        val draft = SessionExerciseDraft(exercise = repsExercise,
            equipmentId = "leg_press", sets = listOf(SessionSetDraft(reps = 6)))
        val existing = SessionExercisePlan(3, reps = 8, weightKg = 50.0,
            loadMode = SessionLoadMode.EXTERNAL, restSeconds = 120)
        val direct = buildManualTargetPlan(draft, existing, ManualTargetChoice.KG,
            "62,5", null, EquipmentLoadSemantics.EXTERNAL).getOrThrow()!!
        assertEquals(existing.copy(weightKg = 62.5), direct)
        val assistance = buildManualTargetPlan(draft, existing,
            ManualTargetChoice.PERCENT_MAX, "",
            ManualPercentMaxResult.Available(100.0, "2026-09-10T08:00:00Z", 70.0),
            EquipmentLoadSemantics.ASSISTANCE)
        assertEquals(
            "Le %MAX est indisponible pour une assistance ; choisissez une résistance externe.",
            assistance.exceptionOrNull()?.message,
        )
    }

    private val repsExercise =
        ExerciseProfile(
            exerciseId = "ex_00000000-0000-4000-8000-000000000001",
            name = "Développé",
            normalizedName = "développé",
            recordingMode = RecordingMode.SETS,
            trackingMode = TrackingMode.REPS,
            dataFields = 0,
        )
}
