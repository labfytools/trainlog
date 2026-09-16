package com.labfytools.trainlog.ui

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CreateEquipmentResult
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.FinalizeActiveDraftResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import java.util.UUID
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class ActiveSessionReorderTest {
    private lateinit var context: Context
    private lateinit var repository: TrainlogRepository
    private lateinit var databaseName: String

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "reorder-${UUID.randomUUID()}.db"
        repository = TrainlogRepository(context, databaseName)
    }

    @After fun tearDown() {
        repository.close()
        context.deleteDatabase(databaseName)
    }

    @Test fun `reorder is identity preserving durable atomic and finalizes in new order`() {
        val exerciseA = (repository.createExercise(NewExerciseProfile(
            "Reorder A", RecordingMode.SETS, TrackingMode.REPS, 0,
        )) as CreateExerciseResult.Created).exercise
        val exerciseB = (repository.createExercise(NewExerciseProfile(
            "Reorder B", RecordingMode.SETS, TrackingMode.REPS, 0,
        )) as CreateExerciseResult.Created).exercise
        val equipment = (repository.createCustomEquipment("Reorder machine") as
            CreateEquipmentResult.Created).equipment
        val first = SessionExerciseDraft(
            entryId = "sxe_00000000-0000-4000-8000-0000000000a1",
            exercise = exerciseA,
            equipmentId = equipment.equipmentId,
            plan = SessionExercisePlan(sets = 2, reps = 8, weightKg = 42.5,
                loadMode = com.labfytools.trainlog.model.SessionLoadMode.EXTERNAL, restSeconds = 90),
            sets = listOf(SessionSetDraft(reps = 7, weightKg = 41.0)),
        )
        val duplicate = first.copy(
            entryId = "sxe_00000000-0000-4000-8000-0000000000a2",
            sets = listOf(SessionSetDraft(reps = 6, weightKg = 39.0)),
        )
        val different = SessionExerciseDraft(
            entryId = "sxe_00000000-0000-4000-8000-0000000000b1",
            exercise = exerciseB,
            sets = listOf(SessionSetDraft(reps = 12, weightKg = 12.0)),
        )
        val form = SessionDraftForm(
            selectedExercise = exerciseA,
            editingExerciseIndex = 0,
            editingEntryId = first.entryId,
            selectedEquipmentId = equipment.equipmentId,
            repsText = "7, 6",
            weightText = "41",
        )
        val initial = ActiveSessionDraft(exercises = listOf(first, duplicate, different), form = form)
        assertEquals(ActiveDraftMutationResult.Saved, repository.saveActiveSessionDraft(initial))

        val reorderedDraft = reorderActiveSessionDraft(initial, 2, 0)
        val reorderedItems = reorderedDraft.exercises
        assertEquals(form.copy(editingExerciseIndex = 1), reorderedDraft.form)
        assertEquals(listOf(different.entryId, first.entryId, duplicate.entryId), reorderedItems.map { it.entryId })
        assertEquals(initial.exercises.toSet(), reorderedItems.toSet())
        assertEquals(ActiveDraftMutationResult.Saved,
            repository.saveActiveSessionDraft(reorderedDraft))

        repository.close()
        repository = TrainlogRepository(context, databaseName)
        val restored = (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
        assertEquals(reorderedItems, restored.exercises)
        assertEquals(form.repsText, restored.form.repsText)
        assertEquals(form.weightText, restored.form.weightText)
        assertEquals(form.selectedEquipmentId, restored.form.selectedEquipmentId)

        repository.close()
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null,
            SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("CREATE TRIGGER fail_reorder BEFORE INSERT ON draft_session_exercises " +
                "BEGIN SELECT RAISE(ABORT,'synthetic reorder failure'); END;")
        }
        repository = TrainlogRepository(context, databaseName)
        val failedOrder = restored.copy(exercises = reorderExerciseOccurrences(restored.exercises, 0, 2))
        assertTrue(repository.saveActiveSessionDraft(failedOrder) is ActiveDraftMutationResult.Error)
        assertEquals(restored, (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft)

        repository.close()
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null,
            SQLiteDatabase.OPEN_READWRITE).use { it.execSQL("DROP TRIGGER fail_reorder") }
        repository = TrainlogRepository(context, databaseName)
        val saved = repository.finalizeActiveSessionDraft() as FinalizeActiveDraftResult.Saved
        val completed = repository.getSessionDetail(saved.sessionId)!!
        assertEquals(reorderedItems.map { it.entryId }, completed.exercises.map { it.entryId })
        assertEquals(reorderedItems.map { it.exercise.exerciseId }, completed.exercises.map { it.exerciseId })
        assertEquals(reorderedItems.map { it.equipmentId }, completed.exercises.map { it.equipmentId })
        assertEquals(reorderedItems.map { it.plan }, completed.exercises.map { it.plan })
        assertEquals(reorderedItems.map { it.sets }, completed.exercises.map { it.sets })
    }
}
