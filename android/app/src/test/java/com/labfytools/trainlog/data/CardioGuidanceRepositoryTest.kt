package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.CardioGuidanceInstruction
import com.labfytools.trainlog.model.CardioGuidanceOutput
import com.labfytools.trainlog.model.CardioGuidedPhase
import com.labfytools.trainlog.model.CardioPhaseExitCondition
import com.labfytools.trainlog.model.CardioPhaseKind
import com.labfytools.trainlog.model.CardioTargetSnapshot
import com.labfytools.trainlog.model.HeartRateContextKind
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import java.time.OffsetDateTime
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CardioGuidanceRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    private data class ActiveCardio(
        val repository: TrainlogRepository,
        val databaseName: String,
        val entryId: String,
        val exerciseStartedAt: String,
    ) {
        fun at(seconds: Long): String =
            OffsetDateTime.parse(exerciseStartedAt).plusSeconds(seconds).toString()
    }

    private fun activeCardio(): ActiveCardio {
        val name = "guidance-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        val exercise =
            (repository.createExercise(
                NewExerciseProfile(
                    "Tapis guidé",
                    RecordingMode.CONTINUOUS,
                    TrackingMode.DURATION,
                    0,
                ),
            ) as CreateExerciseResult.Created).exercise
        val entry =
            SessionExerciseDraft(
                exercise = exercise,
                plan = SessionExercisePlan(sets = 0, durationSeconds = 600),
            )
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repository.saveActiveSessionDraft(
                ActiveSessionDraft(
                    startedAt = OffsetDateTime.now().minusMinutes(20).toString(),
                    sessionType = SessionType.CARDIO,
                    exercises = listOf(entry),
                ),
            ),
        )
        val loaded = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
        val exerciseStartedAt =
            OffsetDateTime.parse(checkNotNull(loaded.draft.startedAt))
                .plusSeconds(10)
                .toString()
        val started =
            repository.startActiveSessionExercise(
                entry.entryId,
                exerciseStartedAt,
            )
        assertTrue(started is TrainlogRepository.SessionExerciseTimingResult.Started)
        return ActiveCardio(repository, name, entry.entryId, exerciseStartedAt)
    }

    @Test
    fun trainingCaptureTransitionsToCardioWithoutStarvingGuidance() {
        val name = "guidance-transition-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            val exercise =
                (repository.createExercise(
                    NewExerciseProfile(
                        "Vélo guidé après transition",
                        RecordingMode.CONTINUOUS,
                        TrackingMode.DURATION,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise
            assertEquals(ActiveDraftMutationResult.Saved, repository.startActiveSessionDraft())
            val training =
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            val base = OffsetDateTime.parse(checkNotNull(training.startedAt))
            val firstMeasurement =
                ParsedHeartRateMeasurement(
                    bpm = 101,
                    sensorContactDetected = true,
                    energyExpended = 12,
                    rrIntervals1024 = listOf(611),
                )
            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    base.plusSeconds(1).toString(),
                    firstMeasurement,
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val originalCapture = checkNotNull(repository.activeHeartRateCapture())
            assertEquals(HeartRateContextKind.SESSION, originalCapture.contextKind)

            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(training.copy(sessionType = SessionType.CARDIO)),
            )
            val transitionedCapture = checkNotNull(repository.activeHeartRateCapture())
            assertEquals(originalCapture.captureId, transitionedCapture.captureId)
            assertEquals(HeartRateContextKind.CARDIO, transitionedCapture.contextKind)

            val entry =
                SessionExerciseDraft(
                    exercise = exercise,
                    plan = SessionExercisePlan(sets = 0, durationSeconds = 60),
                )
            val cardio =
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(cardio.copy(exercises = listOf(entry))),
            )
            assertTrue(
                repository.startActiveSessionExercise(entry.entryId, base.plusSeconds(2).toString()) is
                    TrainlogRepository.SessionExerciseTimingResult.Started,
            )
            val target = CardioTargetSnapshot(100, 110)
            val phase =
                CardioGuidedPhase(
                    phaseId = "cgp_" + UUID.randomUUID(),
                    kind = CardioPhaseKind.WARMUP,
                    target = target,
                    exitCondition = CardioPhaseExitCondition.FixedDuration(60),
                )
            assertTrue(
                repository.startCardioGuidancePhase(
                    entry.entryId,
                    phase,
                    base.plusSeconds(3).toString(),
                ) is TrainlogRepository.CardioGuidanceMutationResult.Applied,
            )
            val secondAt = base.plusSeconds(4).toString()
            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    secondAt,
                    firstMeasurement.copy(bpm = 99),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                repository.recordCardioGuidanceOutput(
                    phase.phaseId,
                    secondAt,
                    CardioGuidanceOutput(CardioGuidanceInstruction.ACCELERATE, 99, target),
                ) is TrainlogRepository.CardioGuidanceMutationResult.Applied,
            )
            assertEquals(
                listOf(CardioGuidanceInstruction.ACCELERATE),
                repository.listCardioGuidanceEvents(phase.phaseId).map { it.instruction },
            )

            val lateReverse =
                repository.saveActiveSessionDraft(
                    (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded)
                        .draft
                        .copy(sessionType = SessionType.TRAINING),
                )
            assertTrue(lateReverse is ActiveDraftMutationResult.Error)
            assertEquals(
                SessionType.CARDIO,
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded)
                    .draft
                    .sessionType,
            )
            assertEquals(
                HeartRateContextKind.CARDIO,
                checkNotNull(repository.activeHeartRateCapture()).contextKind,
            )

            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { db ->
                db.rawQuery(
                    "SELECT sequence,bpm,exercise_entry_id FROM heart_rate_samples " +
                        "WHERE capture_id=? ORDER BY sequence",
                    arrayOf(originalCapture.captureId),
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(0L, cursor.getLong(0))
                    assertEquals(101, cursor.getInt(1))
                    assertTrue(cursor.isNull(2))
                    assertTrue(cursor.moveToNext())
                    assertEquals(1L, cursor.getLong(0))
                    assertEquals(99, cursor.getInt(1))
                    assertEquals(entry.entryId, cursor.getString(2))
                    assertTrue(!cursor.moveToNext())
                }
            }

            assertEquals(ActiveDraftMutationResult.Saved, repository.discardActiveSessionDraft())
            assertNull(repository.activeHeartRateCapture())
            assertNull(repository.activeCardioGuidancePhase())
            assertTrue(repository.listActiveSessionExerciseTimings().isEmpty())
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun cardioCaptureCanReturnToTrainingBeforeCardioOnlyFactsExist() {
        val name = "guidance-early-reverse-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            assertEquals(ActiveDraftMutationResult.Saved, repository.startActiveSessionDraft())
            val training =
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            val observedAt =
                OffsetDateTime.parse(checkNotNull(training.startedAt)).plusSeconds(1).toString()
            assertTrue(
                repository.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    observedAt,
                    ParsedHeartRateMeasurement(103, null, null, listOf(600)),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val captureId = checkNotNull(repository.activeHeartRateCapture()).captureId
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(training.copy(sessionType = SessionType.CARDIO)),
            )
            val cardio =
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(cardio.copy(sessionType = SessionType.TRAINING)),
            )
            val reversed = checkNotNull(repository.activeHeartRateCapture())
            assertEquals(captureId, reversed.captureId)
            assertEquals(HeartRateContextKind.SESSION, reversed.contextKind)

            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { db ->
                db.rawQuery(
                    "SELECT COUNT(*),MIN(bpm) FROM heart_rate_samples WHERE capture_id=?",
                    arrayOf(captureId),
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(1, cursor.getInt(0))
                    assertEquals(103, cursor.getInt(1))
                }
            }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun phaseRequiresActiveCardioExerciseAndRecordsOnlyInstructionChanges() {
        val active = activeCardio()
        val repository = active.repository
        try {
            val target = CardioTargetSnapshot(120, 140)
            val phase =
                CardioGuidedPhase(
                    phaseId = "cgp_" + UUID.randomUUID(),
                    kind = CardioPhaseKind.WORK,
                    target = target,
                    exitCondition = CardioPhaseExitCondition.FixedDuration(180),
                )
            val started =
                repository.startCardioGuidancePhase(
                    active.entryId,
                    phase,
                    active.at(10),
                ) as TrainlogRepository.CardioGuidanceMutationResult.Applied
            assertEquals(CardioGuidanceInstruction.MAINTAIN, started.phase!!.currentInstruction)

            repository.recordCardioGuidanceOutput(
                phase.phaseId,
                active.at(13),
                CardioGuidanceOutput(
                    CardioGuidanceInstruction.ACCELERATE,
                    110,
                    target,
                ),
            )
            repository.recordCardioGuidanceOutput(
                phase.phaseId,
                active.at(14),
                CardioGuidanceOutput(
                    CardioGuidanceInstruction.ACCELERATE,
                    111,
                    target,
                ),
            )
            repository.recordCardioGuidanceOutput(
                phase.phaseId,
                active.at(30),
                CardioGuidanceOutput(
                    CardioGuidanceInstruction.MAINTAIN,
                    130,
                    target,
                ),
            )
            assertEquals(
                listOf(
                    CardioGuidanceInstruction.ACCELERATE,
                    CardioGuidanceInstruction.MAINTAIN,
                ),
                repository.listCardioGuidanceEvents(phase.phaseId).map { it.instruction },
            )

            repository.finishActiveSessionExercise(
                active.entryId,
                active.at(600),
            )
            assertNull(repository.activeCardioGuidancePhase())
            val completedDraft =
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    completedDraft.copy(
                        exercises =
                            completedDraft.exercises.map {
                                if (it.entryId == active.entryId) {
                                    it.copy(continuousDurationSeconds = 600)
                                } else {
                                    it
                                }
                            },
                    ),
                ),
            )
            val finalized = repository.finalizeActiveSessionDraft()
            assertTrue(finalized is FinalizeActiveDraftResult.Saved)
        } finally {
            repository.close()
            context.deleteDatabase(active.databaseName)
        }
    }

    @Test
    fun guidanceCannotStartBeforeExerciseTimingIsActive() {
        val name = "guidance-inactive-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            val exercise =
                (repository.createExercise(
                    NewExerciseProfile(
                        "Vélo guidé",
                        RecordingMode.CONTINUOUS,
                        TrackingMode.DURATION,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise
            repository.startActiveSessionDraft()
            val loaded = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            val entry = SessionExerciseDraft(exercise = exercise, continuousDurationSeconds = 60)
            repository.saveActiveSessionDraft(
                loaded.draft.copy(sessionType = SessionType.CARDIO, exercises = listOf(entry)),
            )
            val result =
                repository.startCardioGuidancePhase(
                    entry.entryId,
                    CardioGuidedPhase(
                        "cgp_" + UUID.randomUUID(),
                        CardioPhaseKind.WARMUP,
                        CardioTargetSnapshot(100, 115),
                        CardioPhaseExitCondition.FixedDuration(60),
                    ),
                    "2026-09-23T13:00:00+02:00",
                )
            assertTrue(result is TrainlogRepository.CardioGuidanceMutationResult.Invalid)
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun abandonedDraftDeletesOnlyUnfinishedGuidanceRun() {
        val active = activeCardio()
        val repository = active.repository
        try {
            repository.startCardioGuidancePhase(
                active.entryId,
                CardioGuidedPhase(
                    "cgp_" + UUID.randomUUID(),
                    CardioPhaseKind.WORK,
                    CardioTargetSnapshot(120, 140),
                    CardioPhaseExitCondition.FixedDuration(120),
                ),
                active.at(10),
            )
            assertTrue(repository.activeCardioGuidancePhase() != null)
            repository.discardActiveSessionDraft()
            assertNull(repository.activeCardioGuidancePhase())
        } finally {
            repository.close()
            context.deleteDatabase(active.databaseName)
        }
    }
}
