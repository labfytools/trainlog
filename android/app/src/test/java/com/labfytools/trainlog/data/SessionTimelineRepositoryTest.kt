package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
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
class SessionTimelineRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun startedOccurrenceCannotBeRemovedOrSilentlyClosedBySessionFinalization() {
        val name = "timeline-guard-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            val exercise =
                (repository.createExercise(
                    NewExerciseProfile(
                        "Timeline guarded exercise",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.startActiveSessionDraft(),
            )
            val opened =
                repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            val occurrence =
                SessionExerciseDraft(
                    exercise = exercise,
                    sets = listOf(SessionSetDraft(reps = 5)),
                )
            val activeDraft = opened.draft.copy(exercises = listOf(occurrence))
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(activeDraft),
            )
            assertTrue(
                repository.startActiveSessionExercise(occurrence.entryId) is
                    TrainlogRepository.SessionExerciseTimingResult.Started,
            )

            val removed = repository.saveActiveSessionDraft(activeDraft.copy(exercises = emptyList()))
            assertTrue(removed is ActiveDraftMutationResult.Error)
            val finalize = repository.finalizeActiveSessionDraft()
            assertTrue(finalize is FinalizeActiveDraftResult.Invalid)
            assertTrue(
                (finalize as FinalizeActiveDraftResult.Invalid)
                    .message
                    .startsWith("ACTIVE_EXERCISE:"),
            )

            assertTrue(
                repository.finishActiveSessionExercise(occurrence.entryId) is
                    TrainlogRepository.SessionExerciseTimingResult.Finished,
            )
            assertTrue(
                repository.finalizeActiveSessionDraft() is
                    FinalizeActiveDraftResult.Saved,
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun trainingTimelineCorrelatesMeasuredSamplesWithoutInventingRestOwner() {
        val name = "timeline-${UUID.randomUUID()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            fun exercise(label: String) =
                (repository.createExercise(
                    NewExerciseProfile(label, RecordingMode.SETS, TrackingMode.REPS, 0),
                ) as CreateExerciseResult.Created).exercise

            val first = exercise("Timeline A")
            val second = exercise("Timeline B")
            assertEquals(ActiveDraftMutationResult.Saved, repository.startActiveSessionDraft())
            val opened = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            val firstEntry = SessionExerciseDraft(
                exercise = first,
                sets = listOf(SessionSetDraft(reps = 10)),
            )
            val secondEntry = SessionExerciseDraft(
                exercise = second,
                sets = listOf(SessionSetDraft(reps = 10)),
            )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    opened.draft.copy(exercises = listOf(firstEntry, secondEntry)),
                ),
            )

            val startA = OffsetDateTime.now().toString()
            val startedA = repository.startActiveSessionExercise(firstEntry.entryId, startA)
            assertTrue(startedA is TrainlogRepository.SessionExerciseTimingResult.Started)
            val conflict = repository.startActiveSessionExercise(secondEntry.entryId)
            assertEquals(
                firstEntry.entryId,
                (conflict as TrainlogRepository.SessionExerciseTimingResult.ActiveConflict).activeEntryId,
            )

            val measurement = ParsedHeartRateMeasurement(
                bpm = 101,
                sensorContactDetected = null,
                energyExpended = null,
                rrIntervals1024 = listOf(620),
            )
            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR", OffsetDateTime.now().toString(), measurement,
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                repository.finishActiveSessionExercise(firstEntry.entryId)
                    is TrainlogRepository.SessionExerciseTimingResult.Finished,
            )

            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR", OffsetDateTime.now().toString(), measurement.copy(bpm = 88),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                repository.startActiveSessionExercise(secondEntry.entryId)
                    is TrainlogRepository.SessionExerciseTimingResult.Started,
            )
            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR", OffsetDateTime.now().toString(), measurement.copy(bpm = 109),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                repository.finishActiveSessionExercise(secondEntry.entryId)
                    is TrainlogRepository.SessionExerciseTimingResult.Finished,
            )
            val finalized = repository.finalizeActiveSessionDraft()
            assertTrue(finalized is FinalizeActiveDraftResult.Saved)

            val timeline = JSONObject(repository.buildSessionTimelineV1Json())
            val session = timeline.getJSONArray("sessions").getJSONObject(0)
            assertEquals(2, session.getJSONArray("exercises").length())
            assertEquals(firstEntry.entryId, session.getJSONArray("exercises").getJSONObject(0).getString("entry_id"))
            assertEquals(secondEntry.entryId, session.getJSONArray("exercises").getJSONObject(1).getString("entry_id"))

            val heart = JSONObject(repository.buildHeartRateV1Json())
            val samples = heart.getJSONArray("captures").getJSONObject(0).getJSONArray("samples")
            assertEquals(3, samples.length())
            assertEquals(firstEntry.entryId, samples.getJSONObject(0).getString("exercise_entry_id"))
            assertTrue(samples.getJSONObject(1).isNull("exercise_entry_id"))
            assertEquals(secondEntry.entryId, samples.getJSONObject(2).getString("exercise_entry_id"))
            assertEquals(101, samples.getJSONObject(0).getInt("bpm"))
            assertEquals(88, samples.getJSONObject(1).getInt("bpm"))
            assertEquals(109, samples.getJSONObject(2).getInt("bpm"))
            assertNull(repository.activeHeartRateCapture())
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun programSessionFinalizesMeasuredContinuousTargetAndExtraPerformedSet() {
        val name = "program-session-finish-${UUID.randomUUID()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            val programId = "pg_11111111-1111-4111-8111-111111111111"
            val programSessionId = "pgs_22222222-2222-4222-8222-222222222222"
            val warmupId = "ex_33333333-3333-4333-8333-333333333333"
            val rowId = "ex_44444444-4444-4444-8444-444444444444"
            val pressId = "ex_55555555-5555-4555-8555-555555555555"
            val curlId = "ex_66666666-6666-4666-8666-666666666666"

            fun catalogExercise(
                exerciseId: String,
                exerciseName: String,
                recordingMode: String,
                trackingMode: String,
            ) =
                JSONObject()
                    .put("exercise_id", exerciseId)
                    .put("name", exerciseName)
                    .put("recording_mode", recordingMode)
                    .put("tracking_mode", trackingMode)
                    .put("data_fields", 0)

            val catalog =
                JSONObject()
                    .put("format", "trainlog-pc-catalog")
                    .put("version", 1)
                    .put(
                        "exercises",
                        JSONArray()
                            .put(catalogExercise(warmupId, "Program warmup", "continuous", "duration"))
                            .put(catalogExercise(rowId, "Program row", "sets", "reps"))
                            .put(catalogExercise(pressId, "Program press", "sets", "reps"))
                            .put(catalogExercise(curlId, "Program arm curl", "sets", "reps")),
                    )
                    .toString()
            assertTrue(repository.applyPcCatalogJson(catalog) is PcCatalogImportResult.Applied)

            fun occurrence(
                entryId: String,
                exerciseId: String,
                targetSets: Int?,
                targetReps: Int?,
                targetDuration: Int?,
                targetWeight: Double?,
                restSeconds: Int,
            ) =
                JSONObject()
                    .put("entry_id", entryId)
                    .put("exercise_id", exerciseId)
                    .put("equipment_id", JSONObject.NULL)
                    .put("load_mode", if (targetWeight == null) "none" else "external")
                    .put("rest_seconds", restSeconds)
                    .put("target_sets", targetSets ?: JSONObject.NULL)
                    .put("target_reps", targetReps ?: JSONObject.NULL)
                    .put("target_duration_seconds", targetDuration ?: JSONObject.NULL)
                    .put("target_weight_kg", targetWeight ?: JSONObject.NULL)
                    .put("notes", JSONObject.NULL)

            val program =
                JSONObject()
                    .put("program_id", programId)
                    .put("revision_id", "pgr_77777777-7777-4777-8777-777777777777")
                    .put("title", "Session finalization regression")
                    .put("note", JSONObject.NULL)
                    .put("state", "active")
                    .put("start_date", "2026-09-24")
                    .put("end_date", JSONObject.NULL)
                    .put("created_at", "2026-09-24T08:00:00Z")
                    .put("updated_at", "2026-09-24T08:00:00Z")
                    .put("source_format", "trainlog-program")
                    .put("source_version", 1)
                    .put("source_payload_sha256", "a".repeat(64))
                    .put(
                        "sessions",
                        JSONArray().put(
                            JSONObject()
                                .put("program_session_id", programSessionId)
                                .put("title", "Workout")
                                .put("session_type", "training")
                                .put("planned_for", "2026-09-24")
                                .put("note", JSONObject.NULL)
                                .put(
                                    "occurrences",
                                    JSONArray()
                                        .put(
                                            occurrence(
                                                "pge_88888888-8888-4888-8888-888888888888",
                                                warmupId,
                                                null,
                                                null,
                                                480,
                                                null,
                                                0,
                                            ),
                                        )
                                        .put(
                                            occurrence(
                                                "pge_99999999-9999-4999-8999-999999999999",
                                                rowId,
                                                3,
                                                10,
                                                null,
                                                39.0,
                                                90,
                                            ),
                                        )
                                        .put(
                                            occurrence(
                                                "pge_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                                pressId,
                                                3,
                                                10,
                                                null,
                                                32.0,
                                                90,
                                            ),
                                        )
                                        .put(
                                            occurrence(
                                                "pge_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                                                curlId,
                                                2,
                                                10,
                                                null,
                                                23.0,
                                                60,
                                            ),
                                        ),
                                ),
                        ),
                    )
            val programs =
                JSONObject()
                    .put("format", "trainlog-programs")
                    .put("version", 1)
                    .put("generated_at", "2026-09-24T08:01:00Z")
                    .put("programs", JSONArray().put(program))
                    .put("deletions", JSONArray())
                    .toString()
            assertEquals(
                ProgramsImportResult.Applied(1, 0, 0),
                repository.applyProgramsV1Json(programs),
            )
            assertEquals(
                StartProgramSessionResult.Started,
                repository.startSyncedProgramSession(programId, programSessionId),
            )

            val opened = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            val sessionId = checkNotNull(opened.draft.sessionId)
            val sessionStart = OffsetDateTime.parse(checkNotNull(opened.draft.startedAt))
            val warmup = opened.draft.exercises[0]
            val row = opened.draft.exercises[1]
            val press = opened.draft.exercises[2]
            val curl = opened.draft.exercises[3]
            val performed =
                opened.draft.copy(
                    exercises =
                        listOf(
                            warmup,
                            row.copy(
                                sets = List(3) { SessionSetDraft(reps = 10, weightKg = 39.0) },
                            ),
                            press.copy(
                                sets = List(3) { SessionSetDraft(reps = 10, weightKg = 32.0) },
                            ),
                            curl.copy(
                                sets = List(3) { SessionSetDraft(reps = 10, weightKg = 23.0) },
                            ),
                        ),
                )
            assertEquals(ActiveDraftMutationResult.Saved, repository.saveActiveSessionDraft(performed))

            val warmupStart = sessionStart.plusSeconds(10)
            val warmupEnd = warmupStart.plusSeconds(433)
            assertTrue(
                repository.startActiveSessionExercise(warmup.entryId, warmupStart.toString()) is
                    TrainlogRepository.SessionExerciseTimingResult.Started,
            )
            assertTrue(
                repository.finishActiveSessionExercise(warmup.entryId, warmupEnd.toString()) is
                    TrainlogRepository.SessionExerciseTimingResult.Finished,
            )
            val materialized = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(433, materialized.draft.exercises[0].continuousDurationSeconds)
            /* Recreate the exact v49 persisted state: its timeline was closed,
             * but the target-only continuous occurrence had no performed row. */
            context.openOrCreateDatabase(name, Context.MODE_PRIVATE, null).use { db ->
                db.execSQL(
                    "DELETE FROM draft_continuous_activity WHERE draft_exercise_row_id=(" +
                        "SELECT id FROM draft_session_exercises WHERE draft_id=1 AND entry_id=?)",
                    arrayOf(warmup.entryId),
                )
            }
            val legacy = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(0, legacy.draft.exercises[0].continuousDurationSeconds)

            listOf(row, press, curl).forEachIndexed { index, exercise ->
                val startedAt = warmupEnd.plusSeconds(30L + index * 90L)
                val endedAt = startedAt.plusSeconds(60)
                assertTrue(
                    repository.startActiveSessionExercise(exercise.entryId, startedAt.toString()) is
                        TrainlogRepository.SessionExerciseTimingResult.Started,
                )
                assertTrue(
                    repository.finishActiveSessionExercise(exercise.entryId, endedAt.toString()) is
                        TrainlogRepository.SessionExerciseTimingResult.Finished,
                )
            }
            assertTrue(
                repository.saveDraftExerciseFeedback(
                    curl.entryId,
                    "Valid effort feedback",
                    warmupEnd.plusSeconds(400).toString(),
                ) is SaveFeedbackResult.Saved,
            )

            val measurement =
                ParsedHeartRateMeasurement(
                    bpm = 107,
                    sensorContactDetected = true,
                    energyExpended = null,
                    rrIntervals1024 = listOf(620),
                )
            val finalizationAt = warmupEnd.plusSeconds(500)
            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    finalizationAt.minusSeconds(1).toString(),
                    measurement,
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val lateSampleAt = finalizationAt.plusSeconds(10)
            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    lateSampleAt.toString(),
                    measurement.copy(bpm = 99),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )

            val finalized = repository.finalizeActiveSessionDraft(finalizationAt.toString())
            assertEquals(
                sessionId,
                (finalized as FinalizeActiveDraftResult.Saved).sessionId,
            )
            val detail = checkNotNull(repository.getSessionDetail(sessionId))
            assertEquals(finalizationAt.toString(), detail.summary.endedAt)
            assertEquals(4, detail.exercises.size)
            assertEquals(433, detail.exercises[0].continuousDurationSeconds)
            assertEquals(2, detail.exercises[3].plan?.sets)
            assertEquals(3, detail.exercises[3].sets.size)
            assertTrue(detail.exercises[3].sets.all { it.reps == 10 && it.weightKg == 23.0 })
            assertEquals(1, detail.exercises[3].feedback.size)

            val timeline = JSONObject(repository.buildSessionTimelineV1Json())
            val timelineSession = timeline.getJSONArray("sessions").getJSONObject(0)
            assertEquals(finalizationAt.toString(), timelineSession.getString("ended_at"))
            assertEquals(4, timelineSession.getJSONArray("exercises").length())

            val heart = JSONObject(repository.buildHeartRateV1Json())
            val capture = heart.getJSONArray("captures").getJSONObject(0)
            assertEquals(finalizationAt.toString(), capture.getString("ended_at"))
            assertEquals(1, capture.getJSONArray("samples").length())
            assertEquals(1, capture.getJSONArray("samples").getJSONObject(0)
                .getJSONArray("rr_intervals_1024").length())
            assertNull(repository.activeHeartRateCapture())
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
