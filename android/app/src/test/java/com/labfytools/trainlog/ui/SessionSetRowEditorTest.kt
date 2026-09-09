package com.labfytools.trainlog.ui

import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
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
