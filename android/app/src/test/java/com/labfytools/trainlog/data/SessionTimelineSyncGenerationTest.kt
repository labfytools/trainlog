package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
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
class SessionTimelineSyncGenerationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun timelineAndHeartRateArriveTogetherAndStopPublishingAfterAck() {
        val sourceName = "timeline-sync-" + UUID.randomUUID() + ".db"
        val root = Files.createTempDirectory("trainlog-timeline-sync-").toFile()
        val source = TrainlogRepository(context, sourceName)
        try {
            fun exercise(label: String) =
                (source.createExercise(
                    NewExerciseProfile(label, RecordingMode.SETS, TrackingMode.REPS, 0),
                ) as CreateExerciseResult.Created).exercise
            val a = SessionExerciseDraft(
                exercise = exercise("Sync timeline A"),
                sets = listOf(SessionSetDraft(reps = 8)),
            )
            val b = SessionExerciseDraft(
                exercise = exercise("Sync timeline B"),
                sets = listOf(SessionSetDraft(reps = 9)),
            )
            assertEquals(ActiveDraftMutationResult.Saved, source.startActiveSessionDraft())
            val opened = source.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(
                ActiveDraftMutationResult.Saved,
                source.saveActiveSessionDraft(opened.draft.copy(exercises = listOf(a, b))),
            )
            assertTrue(
                source.startActiveSessionExercise(a.entryId) is
                    TrainlogRepository.SessionExerciseTimingResult.Started,
            )
            val base = ParsedHeartRateMeasurement(96, null, null, listOf(640))
            assertTrue(
                source.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    OffsetDateTime.now().toString(),
                    base,
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                source.finishActiveSessionExercise(a.entryId) is
                    TrainlogRepository.SessionExerciseTimingResult.Finished,
            )
            assertTrue(
                source.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    OffsetDateTime.now().toString(),
                    base.copy(bpm = 84),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                source.startActiveSessionExercise(b.entryId) is
                    TrainlogRepository.SessionExerciseTimingResult.Started,
            )
            assertTrue(
                source.recordLiveHeartRateForActiveSession(
                    "Synthetic HR",
                    OffsetDateTime.now().toString(),
                    base.copy(bpm = 105),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                source.finishActiveSessionExercise(b.entryId) is
                    TrainlogRepository.SessionExerciseTimingResult.Finished,
            )
            assertTrue(source.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved)

            val repositoryRoot =
                generateSequence(java.io.File(checkNotNull(System.getProperty("user.dir")))) {
                    it.parentFile
                }.first { java.io.File(it, "tools/sync_generation_exchange.py").isFile }
            fun run(vararg command: String): String {
                val process =
                    ProcessBuilder(*command)
                        .directory(repositoryRoot)
                        .redirectErrorStream(true)
                        .start()
                val output = process.inputStream.bufferedReader().readText()
                assertEquals(
                    "command failed: " + command.joinToString(" ") + "\n" + output,
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

            val service = SyncGenerationService(source)
            val captured = service.capture(root, desktopPeer)
            val timelineJson =
                JSONObject(
                    java.io.File(
                        captured.stagingDirectory,
                        "session-timeline-v1.json",
                    ).readText(),
                )
            assertEquals(1, timelineJson.getJSONArray("sessions").length())
            assertEquals(
                2,
                timelineJson
                    .getJSONArray("sessions")
                    .getJSONObject(0)
                    .getJSONArray("exercises")
                    .length(),
            )
            val heartJson =
                JSONObject(
                    java.io.File(captured.stagingDirectory, "heart-rate-v1.json").readText(),
                )
            assertEquals(
                3,
                heartJson
                    .getJSONArray("captures")
                    .getJSONObject(0)
                    .getJSONArray("samples")
                    .length(),
            )

            val published = service.publish(captured, java.io.File(root, "objects"))
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
            val ack = JSONObject(ackFile.readText())
            assertEquals(
                "desktop ACK diagnostic: " + ack.getString("diagnostic"),
                "consumed",
                ack.getString("result"),
            )
            assertEquals("acknowledged", service.acceptAcknowledgement(ackFile.readBytes()))

            assertEquals(
                "2|3|3",
                run(
                    "sqlite3",
                    desktopDb.absolutePath,
                    "SELECT (SELECT count(*) FROM session_exercise_timeline) || '|' || " +
                        "(SELECT count(*) FROM heart_rate_samples) || '|' || " +
                        "(SELECT count(*) FROM heart_rate_rr_intervals);",
                ).trim(),
            )
            assertEquals(
                0,
                JSONObject(source.buildSessionTimelineV1Json())
                    .getJSONArray("sessions")
                    .length(),
            )
            assertEquals(
                0,
                JSONObject(source.buildHeartRateV1Json())
                    .getJSONArray("captures")
                    .length(),
            )
        } finally {
            source.close()
            context.deleteDatabase(sourceName)
            root.deleteRecursively()
        }
    }
}
