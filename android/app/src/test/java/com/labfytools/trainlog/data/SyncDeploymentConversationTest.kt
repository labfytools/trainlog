package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import java.io.File
import java.time.Duration
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
class SyncDeploymentConversationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    private fun awaitFile(file: File): File {
        val deadline = System.nanoTime() + Duration.ofSeconds(30).toNanos()
        while (!file.isFile) {
            if (System.nanoTime() >= deadline) error("timeout waiting for ${file.name}")
            Thread.sleep(25)
        }
        return file
    }

    private fun publish(file: File, value: String) {
        val temporary = File(file.parentFile, ".${file.name}.tmp-${UUID.randomUUID()}")
        temporary.outputStream().use { output ->
            output.write(value.toByteArray()); output.flush(); output.fd.sync()
        }
        check(temporary.renameTo(file)) { "cannot publish ${file.name}" }
    }

    @Test
    fun foregroundAndroidServiceCompletesCorrelatedDesktopConversation() {
        val suppliedTransport = System.getenv("TRAINLOG_SYNC_TRANSPORT_ROOT")
        val transport = suppliedTransport?.let(::File) ?: kotlin.io.path.createTempDirectory("deployment-conversation-").toFile()
        val repositoryRoot = generateSequence(File(checkNotNull(System.getProperty("user.dir")))) { it.parentFile }
            .first { File(it, "tools/sync_peer_worker.py").isFile }
        val databaseName = "deployment-peer-${UUID.randomUUID()}.db"
        var repository = TrainlogRepository(context, databaseName)
        var localWorker: Process? = null
        try {
            transport.mkdirs()
            val exercise = (repository.createExercise(NewExerciseProfile(
                "Deployment conversation", RecordingMode.SETS, TrackingMode.REPS, 0,
            )) as CreateExerciseResult.Created).exercise
            val occurrence = SessionExerciseDraft(exercise = exercise,
                sets = listOf(SessionSetDraft(reps = 7, weightKg = 22.5)))
            assertEquals(ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(ActiveSessionDraft(exercises = listOf(occurrence))))
            val coordinator = SyncGenerationForegroundCoordinator(repository)
            val release = System.getenv("TRAINLOG_SYNC_TEST_RELEASE_FILE")
            val executor = java.util.concurrent.Executors.newSingleThreadExecutor()
            val result = executor.submit<ForegroundGenerationResult> {
                coordinator.run(transport, Duration.ofSeconds(30)) {
                    release?.let { awaitFile(File(it)) }
                }
            }
            val peer = JSONObject(awaitFile(File(transport, "android-peer-v1.json")).readText())
            if (suppliedTransport == null) {
                val desktop = File(transport, "desktop.sqlite")
                val fixture = ProcessBuilder(File(repositoryRoot, "build/tui/sync-generation-fixture").absolutePath,
                    desktop.absolutePath).directory(repositoryRoot).start()
                assertEquals(0, fixture.waitFor())
                localWorker = ProcessBuilder("python3", File(repositoryRoot, "tools/sync_peer_worker.py").absolutePath,
                    "--database", desktop.absolutePath, "--transport-root", transport.absolutePath,
                    "--owned-root", File(transport, "owned").absolutePath, "--run-id", "sy_${UUID.randomUUID()}",
                    "--expected-peer", peer.getString("peer_id"), "--timeout", "30").directory(repositoryRoot)
                    .redirectErrorStream(true).start()
            }
            assertTrue(result.get(35, java.util.concurrent.TimeUnit.SECONDS) is ForegroundGenerationResult.Completed)
            executor.shutdownNow()
            repository.close()
            repository = TrainlogRepository(context, databaseName)
            assertTrue(repository.loadActiveSessionDraft() is ActiveDraftLoadResult.Loaded)
            localWorker?.let { worker ->
                val output = worker.inputStream.bufferedReader().readText()
                assertEquals("worker failed:\n$output", 0, worker.waitFor())
                assertTrue(output.contains("\"result\":\"completed\""))
            }
        } finally {
            localWorker?.destroyForcibly()
            repository.close()
            context.deleteDatabase(databaseName)
            if (suppliedTransport == null) transport.deleteRecursively()
        }
    }
}
