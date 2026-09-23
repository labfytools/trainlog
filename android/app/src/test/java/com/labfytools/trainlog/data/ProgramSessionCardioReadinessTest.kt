package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.HeartRateContextKind
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import java.nio.file.Files
import java.time.OffsetDateTime
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class ProgramSessionCardioReadinessTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun programSessionPreservesTimelineAndHeartRateOwnershipThroughDesktopReplay() {
        val androidName = "program-cardio-readiness-${UUID.randomUUID()}.db"
        val android = TrainlogRepository(context, androidName)
        val root = Files.createTempDirectory("trainlog-program-cardio-readiness-").toFile()
        val repositoryRoot =
            generateSequence(java.io.File(checkNotNull(System.getProperty("user.dir")))) {
                it.parentFile
            }.first { java.io.File(it, "tools/sync_generation_exchange.py").isFile }
        val desktopDb = java.io.File(root, "desktop.sqlite")
        val fixture = java.io.File(repositoryRoot, "build/tui/sync-generation-fixture")
        val generationTool =
            java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath

        fun run(vararg command: String): String {
            val process =
                ProcessBuilder(*command)
                    .directory(repositoryRoot)
                    .redirectErrorStream(true)
                    .start()
            val output = process.inputStream.bufferedReader().readText()
            assertEquals(
                "command failed: ${command.joinToString(" ")}\n$output",
                0,
                process.waitFor(),
            )
            return output
        }

        fun measurement(bpm: Int, rr: Int) =
            ParsedHeartRateMeasurement(
                bpm = bpm,
                sensorContactDetected = true,
                energyExpended = null,
                rrIntervals1024 = listOf(rr),
            )

        try {
            val programId = "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
            val programSessionId = "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"
            val programExerciseId = "ex_dddddddd-dddd-4ddd-8ddd-dddddddddddd"
            val catalog =
                JSONObject()
                    .put("format", "trainlog-pc-catalog")
                    .put("version", 1)
                    .put(
                        "exercises",
                        JSONArray().put(
                            JSONObject()
                                .put("exercise_id", programExerciseId)
                                .put("name", "Program exercise fixture")
                                .put("recording_mode", "sets")
                                .put("tracking_mode", "reps")
                                .put("data_fields", 0),
                        ),
                    )
                    .toString()
            assertTrue(android.applyPcCatalogJson(catalog) is PcCatalogImportResult.Applied)
            val program =
                JSONObject()
                    .put("program_id", programId)
                    .put("revision_id", "pgr_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")
                    .put("title", "Tomorrow readiness")
                    .put("note", JSONObject.NULL)
                    .put("state", "active")
                    .put("start_date", "2026-09-24")
                    .put("end_date", JSONObject.NULL)
                    .put("created_at", "2026-09-23T16:00:00Z")
                    .put("updated_at", "2026-09-23T16:00:00Z")
                    .put("source_format", "trainlog-program")
                    .put("source_version", 1)
                    .put("source_payload_sha256", "a".repeat(64))
                    .put(
                        "sessions",
                        JSONArray().put(
                            JSONObject()
                                .put("program_session_id", programSessionId)
                                .put("title", "Session A")
                                .put("session_type", "training")
                                .put("planned_for", "2026-09-24")
                                .put("note", JSONObject.NULL)
                                .put(
                                    "occurrences",
                                    JSONArray().put(
                                        JSONObject()
                                            .put(
                                                "entry_id",
                                                "pge_cccccccc-cccc-4ccc-8ccc-cccccccccccc",
                                            )
                                            .put("exercise_id", programExerciseId)
                                            .put("equipment_id", JSONObject.NULL)
                                            .put("load_mode", "external")
                                            .put("rest_seconds", 90)
                                            .put("target_sets", 1)
                                            .put("target_reps", 8)
                                            .put("target_duration_seconds", JSONObject.NULL)
                                            .put("target_weight_kg", 40.0)
                                            .put("notes", JSONObject.NULL),
                                    ),
                                ),
                        ),
                    )
            val programs =
                JSONObject()
                    .put("format", "trainlog-programs")
                    .put("version", 1)
                    .put("generated_at", "2026-09-23T16:01:00Z")
                    .put("programs", JSONArray().put(program))
                    .put("deletions", JSONArray())
                    .toString()
            assertEquals(
                ProgramsImportResult.Applied(1, 0, 0),
                android.applyProgramsV1Json(programs),
            )
            val exerciseB =
                (android.createExercise(
                    NewExerciseProfile(
                        "Readiness exercise B",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise

            assertEquals(
                StartProgramSessionResult.Started,
                android.startSyncedProgramSession(programId, programSessionId),
            )
            val opened = android.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            val sessionId = checkNotNull(opened.draft.sessionId)
            val sessionStartedAt = checkNotNull(opened.draft.startedAt)
            assertEquals(programId, opened.draft.sourceProgramId)
            assertEquals(programSessionId, opened.draft.sourceProgramSessionId)
            val exerciseA = opened.draft.exercises.single()
            val exerciseBEntry =
                SessionExerciseDraft(
                    exercise = exerciseB,
                    sets = listOf(SessionSetDraft(reps = 10, weightKg = 25.0)),
                )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                android.saveActiveSessionDraft(
                    opened.draft.copy(
                        exercises =
                            listOf(
                                exerciseA.copy(
                                    sets = listOf(SessionSetDraft(reps = 8, weightKg = 40.0)),
                                ),
                                exerciseBEntry,
                            ),
                    ),
                ),
            )
            val edited = android.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(sessionId, edited.draft.sessionId)
            assertEquals(sessionStartedAt, edited.draft.startedAt)
            assertEquals(programId, edited.draft.sourceProgramId)
            assertEquals(programSessionId, edited.draft.sourceProgramSessionId)

            val preExerciseAt = OffsetDateTime.now().toString()
            assertTrue(
                android.recordLiveHeartRateForActiveSession(
                    "Synthetic CYCPLUS",
                    preExerciseAt,
                    measurement(91, 675),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val capture = checkNotNull(android.activeHeartRateCapture())
            assertEquals(HeartRateContextKind.SESSION, capture.contextKind)
            assertEquals(sessionId, capture.contextId)
            assertEquals(sessionStartedAt, capture.startedAt)

            val exerciseAStartedAt = OffsetDateTime.now().toString()
            assertTrue(
                android.startActiveSessionExercise(exerciseA.entryId, exerciseAStartedAt) is
                    TrainlogRepository.SessionExerciseTimingResult.Started,
            )
            val exerciseASampleAt = OffsetDateTime.now().toString()
            assertTrue(
                android.recordLiveHeartRateForActiveSession(
                    "Synthetic CYCPLUS",
                    exerciseASampleAt,
                    measurement(112, 548),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val exerciseAEndedAt = OffsetDateTime.now().toString()
            assertTrue(
                android.finishActiveSessionExercise(exerciseA.entryId, exerciseAEndedAt) is
                    TrainlogRepository.SessionExerciseTimingResult.Finished,
            )

            val betweenExercisesAt = OffsetDateTime.now().toString()
            assertTrue(
                android.recordLiveHeartRateForActiveSession(
                    "Synthetic CYCPLUS",
                    betweenExercisesAt,
                    measurement(98, 620),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val exerciseBStartedAt = OffsetDateTime.now().toString()
            assertTrue(
                android.startActiveSessionExercise(
                    exerciseBEntry.entryId,
                    exerciseBStartedAt,
                ) is TrainlogRepository.SessionExerciseTimingResult.Started,
            )
            val exerciseBSampleAt = OffsetDateTime.now().toString()
            assertTrue(
                android.recordLiveHeartRateForActiveSession(
                    "Synthetic CYCPLUS",
                    exerciseBSampleAt,
                    measurement(119, 525),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val activeFinalize = android.finalizeActiveSessionDraft()
            assertTrue(activeFinalize is FinalizeActiveDraftResult.Invalid)
            assertTrue(
                (activeFinalize as FinalizeActiveDraftResult.Invalid)
                    .message
                    .startsWith("ACTIVE_EXERCISE:"),
            )
            val exerciseBEndedAt = OffsetDateTime.now().toString()
            assertTrue(
                android.finishActiveSessionExercise(
                    exerciseBEntry.entryId,
                    exerciseBEndedAt,
                ) is TrainlogRepository.SessionExerciseTimingResult.Finished,
            )
            val finalized = android.finalizeActiveSessionDraft()
            assertTrue(finalized is FinalizeActiveDraftResult.Saved)
            assertEquals(sessionId, (finalized as FinalizeActiveDraftResult.Saved).sessionId)
            assertNull(android.activeHeartRateCapture())

            val timeline = JSONObject(android.buildSessionTimelineV1Json())
            val timelineSession = timeline.getJSONArray("sessions").getJSONObject(0)
            val sessionEndedAt = timelineSession.getString("ended_at")
            assertEquals(sessionId, timelineSession.getString("session_id"))
            assertEquals(sessionStartedAt, timelineSession.getString("started_at"))
            assertTrue(OffsetDateTime.parse(sessionEndedAt) > OffsetDateTime.parse(sessionStartedAt))
            val timelineExercises = timelineSession.getJSONArray("exercises")
            assertEquals(2, timelineExercises.length())
            assertEquals(exerciseA.entryId, timelineExercises.getJSONObject(0).getString("entry_id"))
            assertEquals(exerciseAStartedAt, timelineExercises.getJSONObject(0).getString("started_at"))
            assertEquals(exerciseAEndedAt, timelineExercises.getJSONObject(0).getString("ended_at"))
            assertEquals(
                exerciseBEntry.entryId,
                timelineExercises.getJSONObject(1).getString("entry_id"),
            )
            assertEquals(exerciseBStartedAt, timelineExercises.getJSONObject(1).getString("started_at"))
            assertEquals(exerciseBEndedAt, timelineExercises.getJSONObject(1).getString("ended_at"))

            val heart = JSONObject(android.buildHeartRateV1Json())
            val heartCapture = heart.getJSONArray("captures").getJSONObject(0)
            assertEquals(capture.captureId, heartCapture.getString("capture_id"))
            assertEquals("session", heartCapture.getString("context_kind"))
            assertEquals(sessionId, heartCapture.getString("context_id"))
            assertEquals(sessionStartedAt, heartCapture.getString("started_at"))
            assertEquals(sessionEndedAt, heartCapture.getString("ended_at"))
            val samples = heartCapture.getJSONArray("samples")
            assertEquals(4, samples.length())
            val expectedOwnership =
                listOf(null, exerciseA.entryId, null, exerciseBEntry.entryId)
            val expectedTimes =
                listOf(preExerciseAt, exerciseASampleAt, betweenExercisesAt, exerciseBSampleAt)
            val expectedRr = listOf(675, 548, 620, 525)
            for (index in 0 until samples.length()) {
                val sample = samples.getJSONObject(index)
                assertEquals(index.toLong(), sample.getLong("sequence"))
                assertEquals(expectedTimes[index], sample.getString("observed_at"))
                if (expectedOwnership[index] == null) {
                    assertTrue(sample.isNull("exercise_entry_id"))
                } else {
                    assertEquals(expectedOwnership[index], sample.getString("exercise_entry_id"))
                }
                assertEquals(expectedRr[index], sample.getJSONArray("rr_intervals_1024").getInt(0))
            }

            SQLiteDatabase.openDatabase(
                context.getDatabasePath(androidName).absolutePath,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { db ->
                db.rawQuery(
                    "SELECT source_program_id,source_program_session_id FROM sessions " +
                        "WHERE session_id=?",
                    arrayOf(sessionId),
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(programId, cursor.getString(0))
                    assertEquals(programSessionId, cursor.getString(1))
                }
            }

            run(fixture.absolutePath, desktopDb.absolutePath, "program-create")
            val desktopPeer =
                Regex("PEER_ID=(peer_[^\\s]+)")
                    .find(
                        run(
                            "python3",
                            generationTool,
                            "peer-id",
                            "--database",
                            desktopDb.absolutePath,
                        ),
                    )!!
                    .groupValues[1]
            val service = SyncGenerationService(android)
            val captured = service.capture(root, desktopPeer)
            val published = service.publish(captured, java.io.File(root, "objects"))
            val publishedTimeline =
                JSONObject(java.io.File(published, "session-timeline-v1.json").readText())
            val publishedHeart =
                JSONObject(java.io.File(published, "heart-rate-v1.json").readText())
            assertEquals(sessionId, publishedTimeline.getJSONArray("sessions").getJSONObject(0).getString("session_id"))
            assertEquals(capture.captureId, publishedHeart.getJSONArray("captures").getJSONObject(0).getString("capture_id"))

            val firstAck = java.io.File(root, "desktop-ack-first.json")
            run(
                "python3",
                generationTool,
                "consume-desktop",
                published.absolutePath,
                "--database",
                desktopDb.absolutePath,
                "--ack-output",
                firstAck.absolutePath,
            )
            val countsBeforeReplay =
                run(
                    "sqlite3",
                    desktopDb.absolutePath,
                    "SELECT (SELECT count(*) FROM session_exercise_timeline WHERE session_id='$sessionId') || '|' || " +
                        "(SELECT count(*) FROM heart_rate_samples WHERE capture_id='${capture.captureId}') || '|' || " +
                        "(SELECT count(*) FROM heart_rate_rr_intervals WHERE capture_id='${capture.captureId}');",
                ).trim()
            assertEquals("2|4|4", countsBeforeReplay)

            val replayAck = java.io.File(root, "desktop-ack-replay.json")
            run(
                "python3",
                generationTool,
                "consume-desktop",
                published.absolutePath,
                "--database",
                desktopDb.absolutePath,
                "--ack-output",
                replayAck.absolutePath,
            )
            assertEquals(
                countsBeforeReplay,
                run(
                    "sqlite3",
                    desktopDb.absolutePath,
                    "SELECT (SELECT count(*) FROM session_exercise_timeline WHERE session_id='$sessionId') || '|' || " +
                        "(SELECT count(*) FROM heart_rate_samples WHERE capture_id='${capture.captureId}') || '|' || " +
                        "(SELECT count(*) FROM heart_rate_rr_intervals WHERE capture_id='${capture.captureId}');",
                ).trim(),
            )
            assertEquals(
                "acknowledged",
                service.acceptAcknowledgement(firstAck.readBytes()),
            )
            assertEquals(
                0,
                JSONObject(android.buildSessionTimelineV1Json()).getJSONArray("sessions").length(),
            )
            assertEquals(
                0,
                JSONObject(android.buildHeartRateV1Json()).getJSONArray("captures").length(),
            )

            val desktopFacts =
                JSONObject(
                    run(
                        "sqlite3",
                        desktopDb.absolutePath,
                        "SELECT json_object(" +
                            "'program_id',x.program_id,'program_session_id',x.program_session_id," +
                            "'started_at',s.started_at,'ended_at',s.ended_at) FROM sessions s " +
                            "JOIN program_session_executions x ON x.session_id=s.session_id " +
                            "WHERE s.session_id='$sessionId' AND x.state='completed';",
                    ).trim(),
                )
            assertEquals(programId, desktopFacts.getString("program_id"))
            assertEquals(programSessionId, desktopFacts.getString("program_session_id"))
            assertEquals(sessionStartedAt, desktopFacts.getString("started_at"))
            assertEquals(sessionEndedAt, desktopFacts.getString("ended_at"))
            assertEquals(
                listOf(
                    "$exerciseAStartedAt|$exerciseAEndedAt|${exerciseA.entryId}",
                    "$exerciseBStartedAt|$exerciseBEndedAt|${exerciseBEntry.entryId}",
                ),
                run(
                    "sqlite3",
                    "-separator",
                    "|",
                    desktopDb.absolutePath,
                    "SELECT started_at,ended_at,entry_id FROM session_exercise_timeline " +
                        "WHERE session_id='$sessionId' ORDER BY started_at,entry_id;",
                ).lineSequence().filter { it.isNotBlank() }.toList(),
            )
            assertEquals(
                "session|$sessionId|$sessionStartedAt|$sessionEndedAt",
                run(
                    "sqlite3",
                    "-separator",
                    "|",
                    desktopDb.absolutePath,
                    "SELECT context_kind,context_id,started_at,ended_at " +
                        "FROM heart_rate_captures WHERE capture_id='${capture.captureId}';",
                ).trim(),
            )
            assertEquals(
                listOf(
                    "0|$preExerciseAt|",
                    "1|$exerciseASampleAt|${exerciseA.entryId}",
                    "2|$betweenExercisesAt|",
                    "3|$exerciseBSampleAt|${exerciseBEntry.entryId}",
                ),
                run(
                    "sqlite3",
                    "-separator",
                    "|",
                    desktopDb.absolutePath,
                    "SELECT sequence,observed_at,COALESCE(exercise_entry_id,'') " +
                        "FROM heart_rate_samples " +
                        "WHERE capture_id='${capture.captureId}' ORDER BY sequence;",
                ).lineSequence().filter { it.isNotBlank() }.toList(),
            )
            assertEquals(
                expectedRr,
                run(
                    "sqlite3",
                    desktopDb.absolutePath,
                    "SELECT value_1024 FROM heart_rate_rr_intervals " +
                        "WHERE capture_id='${capture.captureId}' ORDER BY sample_sequence,rr_index;",
                ).lineSequence().filter { it.isNotBlank() }.map { it.toInt() }.toList(),
            )
        } finally {
            android.close()
            context.deleteDatabase(androidName)
            root.deleteRecursively()
        }
    }
}
