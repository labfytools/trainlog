package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import java.nio.file.Files
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
class CardioSessionSyncGenerationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun dedicatedCardioCompanionImportsWithoutWideningFrozenHistory() {
        val sourceName = "cardio-sync-" + UUID.randomUUID() + ".db"
        val root = Files.createTempDirectory("trainlog-cardio-sync-").toFile()
        val source = TrainlogRepository(context, sourceName)
        try {
            val exercise =
                (source.createExercise(
                    NewExerciseProfile(
                        "Cardio sync exercise",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise
            val entry =
                SessionExerciseDraft(
                    exercise = exercise,
                    sets = listOf(SessionSetDraft(reps = 15)),
                )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                source.saveActiveSessionDraft(
                    ActiveSessionDraft(
                        sessionType = SessionType.CARDIO,
                        exercises = listOf(entry),
                    ),
                ),
            )
            assertTrue(
                source.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved,
            )
            val sessionId = source.listSessions().single().sessionId

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
                java.io.File(
                    repositoryRoot,
                    "build/tui/sync-generation-fixture",
                ).absolutePath,
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
            val history =
                JSONObject(
                    java.io.File(
                        captured.stagingDirectory,
                        "history-v4.json",
                    ).readText(),
                )
            assertEquals(0, history.getJSONArray("sessions").length())

            val cardio =
                JSONObject(
                    java.io.File(
                        captured.stagingDirectory,
                        "cardio-sessions-v1.json",
                    ).readText(),
                )
            assertEquals("trainlog-cardio-sessions", cardio.getString("format"))
            assertEquals(1, cardio.getJSONArray("sessions").length())
            assertEquals(
                "cardio",
                cardio
                    .getJSONArray("sessions")
                    .getJSONObject(0)
                    .getString("session_type"),
            )

            val published = service.publish(captured, java.io.File(root, "objects"))
            val ackFile = java.io.File(root, "ack.json")
            run(
                "python3",
                java.io.File(
                    repositoryRoot,
                    "tools/sync_generation_exchange.py",
                ).absolutePath,
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
            assertEquals(
                "training|cardio|15",
                run(
                    "sqlite3",
                    desktopDb.absolutePath,
                    "SELECT s.session_type || '|' || s.session_kind || '|' || ps.reps " +
                        "FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id " +
                        "JOIN performed_sets ps ON ps.session_exercise_row_id=se.id " +
                        "WHERE s.session_id='$sessionId';",
                ).trim(),
            )

            assertEquals(
                "acknowledged",
                service.acceptAcknowledgement(ackFile.readBytes()),
            )
            assertEquals(
                ack.toString(),
                JSONObject(
                    run(
                        "python3",
                        java.io.File(
                            repositoryRoot,
                            "tools/sync_generation_exchange.py",
                        ).absolutePath,
                        "consume-desktop",
                        published.absolutePath,
                        "--database",
                        desktopDb.absolutePath,
                        "--ack-output",
                        java.io.File(root, "replay-ack.json").absolutePath,
                    ).let { java.io.File(root, "replay-ack.json").readText() },
                ).toString(),
            )
        } finally {
            source.close()
            context.deleteDatabase(sourceName)
            root.deleteRecursively()
        }
    }
}
