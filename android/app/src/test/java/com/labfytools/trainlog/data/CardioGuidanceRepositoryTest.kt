package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.CardioGuidanceInstruction
import com.labfytools.trainlog.model.CardioGuidanceOutput
import com.labfytools.trainlog.model.CardioGuidedPhase
import com.labfytools.trainlog.model.CardioPhaseExitCondition
import com.labfytools.trainlog.model.CardioPhaseKind
import com.labfytools.trainlog.model.CardioTargetSnapshot
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
