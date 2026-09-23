package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
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

    private fun withdrawalArtifact(): String {
        val delivery =
            JSONObject()
                .put("delivery_id", "spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")
                .put("revision_id", "spr_cccccccc-cccc-4ccc-8ccc-cccccccccccc")
                .put("execution_session_id", "se_dddddddd-dddd-4ddd-8ddd-dddddddddddd")
        val withdrawal =
            JSONObject()
                .put("withdrawal_id", "spw_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee")
                .put("preparation_id", "sp_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb")
                .put("revision_id", "spr_cccccccc-cccc-4ccc-8ccc-cccccccccccc")
                .put("requested_at", "2026-09-18T12:00:00Z")
                .put("deliveries", JSONArray().put(delivery))
        return JSONObject()
            .put("format", "trainlog-session-preparations")
            .put("version", 2)
            .put("generated_at", "2026-09-18T12:01:00Z")
            .put("deliveries", JSONArray())
            .put("withdrawals", JSONArray().put(withdrawal))
            .toString()
    }

    private fun scalarText(name: String, sql: String): String {
        val path = context.getDatabasePath(name)
        return SQLiteDatabase.openDatabase(path.absolutePath, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            db.rawQuery(sql, null).use { cursor ->
                assertTrue(cursor.moveToFirst())
                cursor.getString(0)
            }
        }
    }

    private fun scalarInt(name: String, sql: String): Int {
        val path = context.getDatabasePath(name)
        return SQLiteDatabase.openDatabase(path.absolutePath, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            db.rawQuery(sql, null).use { cursor ->
                assertTrue(cursor.moveToFirst())
                cursor.getInt(0)
            }
        }
    }

    @Test
    fun startingMixedPreparationCopiesTargetsWithoutPerformedRows() {
        val name = "prepared-mixed-${System.nanoTime()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            assertTrue(
                repository.createExercise(
                    NewExerciseProfile(
                        name = "Test prepared walk",
                        recordingMode = RecordingMode.CONTINUOUS,
                        trackingMode = TrackingMode.DURATION,
                        dataFields = 0,
                    ),
                ) is CreateExerciseResult.Created,
            )
            assertTrue(
                repository.createExercise(
                    NewExerciseProfile(
                        name = "Test prepared press",
                        recordingMode = RecordingMode.SETS,
                        trackingMode = TrackingMode.REPS,
                        dataFields = 0,
                    ),
                ) is CreateExerciseResult.Created,
            )
            val continuous = repository.listExercises().single { it.name == "Test prepared walk" }
            val sets = repository.listExercises().single { it.name == "Test prepared press" }
            val payload = JSONObject(artifact(repository))
            val delivery = payload.getJSONArray("deliveries").getJSONObject(0)
            delivery.put(
                "occurrences",
                JSONArray()
                    .put(
                        JSONObject()
                            .put("entry_id", "spe_11111111-1111-4111-8111-111111111111")
                            .put("position", 0)
                            .put("exercise_id", continuous.exerciseId)
                            .put("equipment_id", JSONObject.NULL)
                            .put("recording_mode", "continuous")
                            .put("tracking_mode", "duration")
                            .put("data_fields", continuous.dataFields)
                            .put("load_mode", "none")
                            .put("rest_seconds", 0)
                            .put("target_sets", JSONObject.NULL)
                            .put("target_reps", JSONObject.NULL)
                            .put("target_duration_seconds", 600)
                            .put("target_weight_kg", JSONObject.NULL)
                            .put("notes", "Target only"),
                    )
                    .put(
                        JSONObject()
                            .put("entry_id", "spe_22222222-2222-4222-8222-222222222222")
                            .put("position", 1)
                            .put("exercise_id", sets.exerciseId)
                            .put("equipment_id", JSONObject.NULL)
                            .put("recording_mode", "sets")
                            .put("tracking_mode", "reps")
                            .put("data_fields", sets.dataFields)
                            .put("load_mode", "external")
                            .put("rest_seconds", 90)
                            .put("target_sets", 3)
                            .put("target_reps", 10)
                            .put("target_duration_seconds", JSONObject.NULL)
                            .put("target_weight_kg", 52.0)
                            .put("notes", JSONObject.NULL),
                    ),
            )

            assertEquals(
                AiSessionDraftImportResult.Applied(1, 0),
                repository.applySessionPreparationsJson(payload.toString()),
            )
            assertEquals(
                StartAiSessionDraftResult.Started,
                repository.startPreparedSession("spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
            )
            val loadResult = repository.loadActiveSessionDraft()
            assertTrue(loadResult.toString(), loadResult is ActiveDraftLoadResult.Loaded)
            val active = loadResult as ActiveDraftLoadResult.Loaded
            assertEquals(2, active.draft.exercises.size)
            assertEquals(600, active.draft.exercises[0].plan?.durationSeconds)
            assertEquals(0, active.draft.exercises[0].continuousDurationSeconds)
            assertEquals(3, active.draft.exercises[1].plan?.sets)
            assertEquals(10, active.draft.exercises[1].plan?.reps)
            assertEquals(52.0, active.draft.exercises[1].plan?.weightKg)
            assertTrue(active.draft.exercises[1].sets.isEmpty())
            assertEquals(1, scalarInt(name, "SELECT COUNT(*) FROM active_session_draft"))
            assertEquals(0, scalarInt(name, "SELECT COUNT(*) FROM draft_continuous_activity"))
            assertEquals(0, scalarInt(name, "SELECT COUNT(*) FROM draft_performed_sets"))
            assertEquals(0, scalarInt(name, "SELECT COUNT(*) FROM draft_max_results"))
            assertEquals(0, scalarInt(name, "SELECT COUNT(*) FROM draft_exercise_feedback"))
            assertEquals(0, scalarInt(name, "SELECT COUNT(*) FROM sessions"))
            assertEquals("ok", scalarText(name, "PRAGMA integrity_check"))
            assertEquals(0, scalarInt(name, "SELECT COUNT(*) FROM pragma_foreign_key_check"))
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
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

    @Test
    fun withdrawalCancelsOnlyPendingDeliveryAndPreventsOlderReplayResurrection() {
        val name = "prepared-withdraw-pending-${System.nanoTime()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            assertTrue(repository.createExercise(NewExerciseProfile(
                name = "Test pending withdrawal",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.REPS,
                dataFields = 0,
            )) is CreateExerciseResult.Created)
            val delivery = artifact(repository)
            val withdrawal = withdrawalArtifact()
            assertEquals(AiSessionDraftImportResult.Applied(1, 0), repository.applySessionPreparationsJson(delivery))
            val invalidWithdrawal = JSONObject(withdrawal)
            invalidWithdrawal.getJSONArray("withdrawals").getJSONObject(0)
                .put("withdrawal_id", "spw_invalid")
            assertTrue(repository.applySessionPreparationsJson(invalidWithdrawal.toString()) is AiSessionDraftImportResult.Invalid)
            assertEquals(1, repository.listPreparedSessions().size)
            assertEquals(AiSessionDraftImportResult.Applied(0, 0), repository.applySessionPreparationsJson(withdrawal))
            assertTrue(repository.listPreparedSessions().isEmpty())
            assertEquals(
                StartAiSessionDraftResult.NotPending,
                repository.startPreparedSession("spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
            )
            assertEquals(AiSessionDraftImportResult.Applied(0, 0), repository.applySessionPreparationsJson(withdrawal))
            assertEquals(AiSessionDraftImportResult.Applied(0, 1), repository.applySessionPreparationsJson(delivery))
            assertTrue(repository.listPreparedSessions().isEmpty())
            repository.close()
            assertEquals(
                "cancelled",
                scalarText(name, "SELECT state FROM session_preparation_deliveries"),
            )
            assertEquals(
                "pending_cancelled",
                scalarText(name, "SELECT result FROM session_preparation_withdrawals"),
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun withdrawalPreservesStartedDeliveryAndActiveExecutionIdentity() {
        val name = "prepared-withdraw-started-${System.nanoTime()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            assertTrue(repository.createExercise(NewExerciseProfile(
                name = "Test started withdrawal",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.REPS,
                dataFields = 0,
            )) is CreateExerciseResult.Created)
            assertEquals(
                AiSessionDraftImportResult.Applied(1, 0),
                repository.applySessionPreparationsJson(artifact(repository)),
            )
            assertEquals(
                StartAiSessionDraftResult.Started,
                repository.startPreparedSession("spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
            )
            val before = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(AiSessionDraftImportResult.Applied(0, 0), repository.applySessionPreparationsJson(withdrawalArtifact()))
            val after = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(before.draft.sessionId, after.draft.sessionId)
            assertEquals("se_dddddddd-dddd-4ddd-8ddd-dddddddddddd", after.draft.sessionId)
            repository.close()
            assertEquals(
                "started",
                scalarText(name, "SELECT state FROM session_preparation_deliveries"),
            )
            assertEquals(
                "execution_preserved",
                scalarText(name, "SELECT result FROM session_preparation_withdrawals"),
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun version22MigrationAddsWithdrawalLedgerWithoutChangingDeliveries() {
        val name = "prepared-withdraw-migration-${System.nanoTime()}.db"
        var repository = TrainlogRepository(context, name)
        try {
            assertTrue(repository.createExercise(NewExerciseProfile(
                name = "Test withdrawal migration",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.REPS,
                dataFields = 0,
            )) is CreateExerciseResult.Created)
            val delivery = artifact(repository)
            assertEquals(AiSessionDraftImportResult.Applied(1, 0), repository.applySessionPreparationsJson(delivery))
            repository.close()
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).absolutePath,
                null,
                SQLiteDatabase.OPEN_READWRITE,
            ).use { db ->
                db.execSQL("DROP TABLE session_preparation_withdrawals")
                db.execSQL("PRAGMA user_version=22")
            }
            repository = TrainlogRepository(context, name)
            assertEquals(1, repository.listPreparedSessions().size)
            repository.close()
            assertEquals("29", scalarText(name, "PRAGMA user_version"))
            assertEquals(
                "0",
                scalarText(name, "SELECT COUNT(*) FROM session_preparation_withdrawals"),
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun withdrawalCancelsAllPendingTargetDeliveriesButPreservesStartedAndWitnessRows() {
        val name = "prepared-withdraw-multiple-${System.nanoTime()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            assertTrue(repository.createExercise(NewExerciseProfile(
                name = "Test multiple withdrawal",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.REPS,
                dataFields = 0,
            )) is CreateExerciseResult.Created)
            val deliveryRoot = JSONObject(artifact(repository))
            val targetTemplate = deliveryRoot.getJSONArray("deliveries").getJSONObject(0)
            val secondTarget = JSONObject(targetTemplate.toString())
                .put("delivery_id", "spd_11111111-1111-4111-8111-111111111111")
                .put("execution_session_id", "se_11111111-1111-4111-8111-111111111111")
            val witness = JSONObject(targetTemplate.toString())
                .put("delivery_id", "spd_22222222-2222-4222-8222-222222222222")
                .put("preparation_id", "sp_22222222-2222-4222-8222-222222222222")
                .put("revision_id", "spr_22222222-2222-4222-8222-222222222222")
                .put("execution_session_id", "se_22222222-2222-4222-8222-222222222222")
                .put("title", "Préparation témoin")
            deliveryRoot.getJSONArray("deliveries").put(secondTarget).put(witness)
            assertEquals(
                AiSessionDraftImportResult.Applied(3, 0),
                repository.applySessionPreparationsJson(deliveryRoot.toString()),
            )
            assertEquals(
                StartAiSessionDraftResult.Started,
                repository.startPreparedSession("spd_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
            )
            val withdrawalRoot = JSONObject(withdrawalArtifact())
            withdrawalRoot.getJSONArray("withdrawals").getJSONObject(0)
                .getJSONArray("deliveries")
                .put(
                    JSONObject()
                        .put("delivery_id", "spd_11111111-1111-4111-8111-111111111111")
                        .put("revision_id", "spr_cccccccc-cccc-4ccc-8ccc-cccccccccccc")
                        .put("execution_session_id", "se_11111111-1111-4111-8111-111111111111"),
                )
            assertEquals(
                AiSessionDraftImportResult.Applied(0, 0),
                repository.applySessionPreparationsJson(withdrawalRoot.toString()),
            )
            assertEquals(
                listOf("spd_22222222-2222-4222-8222-222222222222"),
                repository.listPreparedSessions().map { it.deliveryId },
            )
            val active = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals("se_dddddddd-dddd-4ddd-8ddd-dddddddddddd", active.draft.sessionId)
            repository.close()
            assertEquals(1, scalarInt(name, "SELECT COUNT(*) FROM session_preparation_deliveries WHERE state='started'"))
            assertEquals(1, scalarInt(name, "SELECT COUNT(*) FROM session_preparation_deliveries WHERE state='cancelled'"))
            assertEquals(1, scalarInt(name, "SELECT COUNT(*) FROM session_preparation_deliveries WHERE state='pending'"))
            assertEquals("execution_preserved", scalarText(name, "SELECT result FROM session_preparation_withdrawals"))
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
