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
}
