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
import java.nio.file.Files
import java.time.OffsetDateTime
import java.util.UUID
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CardioGuidanceGenerationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    private fun measurement(bpm: Int) =
        ParsedHeartRateMeasurement(
            bpm = bpm,
            sensorContactDetected = null,
            energyExpended = null,
            rrIntervals1024 = listOf(1024),
        )

    @Test
    fun completedGuidanceSynchronizesOnceAndAckStopsRepublication() {
        val name = "guidance-generation-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        val root = Files.createTempDirectory("trainlog-guidance-generation-").toFile()
        try {
            val exercise =
                (repository.createExercise(
                    NewExerciseProfile(
                        "Vélo guidé sync",
                        RecordingMode.CONTINUOUS,
                        TrackingMode.DURATION,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise

            val entry =
                SessionExerciseDraft(
                    exercise = exercise,
                    plan = SessionExercisePlan(sets = 0, durationSeconds = 30),
                )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    ActiveSessionDraft(
                        startedAt = OffsetDateTime.now().minusMinutes(1).toString(),
                        sessionType = SessionType.CARDIO,
                        exercises = listOf(entry),
                    ),
                ),
            )
            val active =
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            val base = OffsetDateTime.parse(checkNotNull(active.startedAt))
            val exerciseStart = base.plusSeconds(1).toString()
            assertTrue(
                repository.startActiveSessionExercise(entry.entryId, exerciseStart) is
                    TrainlogRepository.SessionExerciseTimingResult.Started,
            )

            val target = CardioTargetSnapshot(120, 140)
            val phase =
                CardioGuidedPhase(
                    phaseId = "cgp_" + UUID.randomUUID(),
                    kind = CardioPhaseKind.WORK,
                    target = target,
                    exitCondition = CardioPhaseExitCondition.FixedDuration(30),
                )
            assertTrue(
                repository.startCardioGuidancePhase(
                    entry.entryId,
                    phase,
                    base.plusSeconds(2).toString(),
                ) is TrainlogRepository.CardioGuidanceMutationResult.Applied,
            )

            val lowAt = base.plusSeconds(3).toString()
            repository.recordLiveHeartRateForActiveSession(
                "Synthetic HR",
                lowAt,
                measurement(110),
            )
            repository.recordCardioGuidanceOutput(
                phase.phaseId,
                lowAt,
                CardioGuidanceOutput(
                    CardioGuidanceInstruction.ACCELERATE,
                    110,
                    target,
                ),
            )
            val targetAt = base.plusSeconds(4).toString()
            repository.recordLiveHeartRateForActiveSession(
                "Synthetic HR",
                targetAt,
                measurement(130),
            )
            repository.recordCardioGuidanceOutput(
                phase.phaseId,
                targetAt,
                CardioGuidanceOutput(
                    CardioGuidanceInstruction.MAINTAIN,
                    130,
                    target,
                ),
            )

            repository.finishCardioGuidancePhase(
                phase.phaseId,
                base.plusSeconds(5).toString(),
            )
            repository.finishActiveSessionExercise(
                entry.entryId,
                base.plusSeconds(6).toString(),
            )
            val completedDraft =
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    completedDraft.copy(
                        exercises =
                            completedDraft.exercises.map {
                                if (it.entryId == entry.entryId) {
                                    it.copy(continuousDurationSeconds = 5)
                                } else {
                                    it
                                }
                            },
                    ),
                ),
            )
            assertTrue(repository.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved)
            assertEquals(
                1,
                JSONObject(repository.buildCardioGuidanceV1Json())
                    .getJSONArray("runs")
                    .length(),
            )

            val repositoryRoot =
                generateSequence(java.io.File(checkNotNull(System.getProperty("user.dir")))) {
                        it.parentFile
                    }
                    .first { java.io.File(it, "tools/sync_generation_exchange.py").isFile }
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

            val desktopDb = java.io.File(root, "desktop.sqlite")
            run(
                java.io.File(repositoryRoot, "build/tui/sync-generation-fixture").absolutePath,
                desktopDb.absolutePath,
            )
            val desktopPeer =
                Regex("PEER_ID=(peer_[^\\s]+)")
                    .find(
                        run(
                            "python3",
                            java.io.File(
                                repositoryRoot,
                                "tools/sync_generation_exchange.py",
                            ).absolutePath,
                            "peer-id",
                            "--database",
                            desktopDb.absolutePath,
                        ),
                    )!!
                    .groupValues[1]

            val service = SyncGenerationService(repository)
            val captured = service.capture(root, desktopPeer)
            assertEquals(
                1,
                JSONObject(
                        java.io.File(
                            captured.stagingDirectory,
                            "cardio-guidance-v1.json",
                        ).readText(),
                    )
                    .getJSONArray("runs")
                    .length(),
            )
            val published =
                service.publish(captured, java.io.File(root, "objects"))
            val ackFile = java.io.File(root, "ack.json")
            run(
                "python3",
                java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath,
                "consume-desktop",
                published.absolutePath,
                "--database",
                desktopDb.absolutePath,
                "--ack-output",
                ackFile.absolutePath,
            )
            assertEquals(
                "consumed",
                JSONObject(ackFile.readText()).getString("result"),
            )
            assertEquals(
                "acknowledged",
                service.acceptAcknowledgement(ackFile.readBytes()),
            )
            assertEquals(
                0,
                JSONObject(repository.buildCardioGuidanceV1Json())
                    .getJSONArray("runs")
                    .length(),
            )
            assertEquals(
                "1|1|2",
                run(
                        "sqlite3",
                        desktopDb.absolutePath,
                        "SELECT (SELECT count(*) FROM cardio_guidance_runs) || '|' || " +
                            "(SELECT count(*) FROM cardio_guidance_phases) || '|' || " +
                            "(SELECT count(*) FROM cardio_guidance_events);",
                    )
                    .trim(),
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
            root.deleteRecursively()
        }
    }
}
