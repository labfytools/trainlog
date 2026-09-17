package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SessionPreparationExchangeTest {
    private val context = ApplicationProvider.getApplicationContext<Context>()

    private fun artifact(repository: TrainlogRepository): String {
        val exercise = repository.listExercises().first { it.recordingMode.wireValue == "sets" }
        val occurrence =
            JSONObject()
                .put("entry_id", "spe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")
                .put("position", 0)
                .put("exercise_id", exercise.exerciseId)
                .put("equipment_id", JSONObject.NULL)
                .put("recording_mode", exercise.recordingMode.wireValue)
                .put("tracking_mode", exercise.trackingMode.wireValue)
                .put("data_fields", exercise.dataFields)
                .put("load_mode", "none")
                .put("rest_seconds", 60)
                .put("target_sets", 2)
                .put("target_reps", if (exercise.trackingMode.wireValue == "reps") 8 else JSONObject.NULL)
                .put(
                    "target_duration_seconds",
                    if (exercise.trackingMode.wireValue == "duration") 45 else JSONObject.NULL,
                )
                .put("target_weight_kg", JSONObject.NULL)
                .put("notes", JSONObject.NULL)
        val delivery =
            JSONObject()
                .put("delivery_id", "spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")
                .put("preparation_id", "sp_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb")
                .put("revision_id", "spr_cccccccc-cccc-4ccc-8ccc-cccccccccccc")
                .put("execution_session_id", "se_dddddddd-dddd-4ddd-8ddd-dddddddddddd")
                .put("state", "pending")
                .put("title", "Préparation contrôlée")
                .put("session_type", "training")
                .put("planned_for", "2026-09-20")
                .put("notes", "Sans faits réalisés")
                .put("source_proposal_id", JSONObject.NULL)
                .put("source_payload_sha256", JSONObject.NULL)
                .put("occurrences", JSONArray().put(occurrence))
        return JSONObject()
            .put("format", "trainlog-session-preparations")
            .put("version", 1)
            .put("generated_at", "2026-09-17T20:00:00Z")
            .put("deliveries", JSONArray().put(delivery))
            .toString()
    }

    @Test
    fun deliveryReplaysStartsExplicitlyAndNeverOverwritesActiveDraft() {
        val name = "prepared-${System.nanoTime()}.db"
        var repository = TrainlogRepository(context, name)
        try {
            assertTrue(repository.createExercise(NewExerciseProfile(
                name = "Test preparation exercise",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.REPS,
                dataFields = 0,
            )) is CreateExerciseResult.Created)
            val payload = artifact(repository)
            assertEquals(AiSessionDraftImportResult.Applied(1, 0), repository.applySessionPreparationsJson(payload))
            assertEquals(AiSessionDraftImportResult.Applied(0, 1), repository.applySessionPreparationsJson(payload))
            assertEquals(1, repository.listPreparedSessions().size)
            assertEquals(ActiveDraftMutationResult.Saved, repository.startActiveSessionDraft())
            assertEquals(
                StartAiSessionDraftResult.ExistingActiveDraft,
                repository.startPreparedSession("spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
            )
            repository.close()
            context.deleteDatabase(name)
            repository = TrainlogRepository(context, name)
            assertTrue(repository.createExercise(NewExerciseProfile(
                name = "Test preparation exercise",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.REPS,
                dataFields = 0,
            )) is CreateExerciseResult.Created)
            val reloadedPayload = artifact(repository)
            assertEquals(AiSessionDraftImportResult.Applied(1, 0), repository.applySessionPreparationsJson(reloadedPayload))
            assertEquals(
                StartAiSessionDraftResult.Started,
                repository.startPreparedSession("spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
            )
            val active = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals("se_dddddddd-dddd-4ddd-8ddd-dddddddddddd", active.draft.sessionId)
            assertTrue(active.draft.exercises.all { it.sets.isEmpty() })
            assertEquals(
                StartAiSessionDraftResult.ExistingActiveDraft,
                repository.startPreparedSession("spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
