package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
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
            output.write(value.toByteArray())
            output.flush()
            output.fd.sync()
        }
        check(temporary.renameTo(file)) { "cannot publish ${file.name}" }
    }

    @Test
    fun foregroundAndroidServiceCompletesCorrelatedDesktopConversation() {
        val suppliedTransport = System.getenv("TRAINLOG_SYNC_TRANSPORT_ROOT")
        val transport =
            suppliedTransport?.let(::File)
                ?: kotlin.io.path.createTempDirectory("deployment-conversation-").toFile()
        val repositoryRoot =
            generateSequence(File(checkNotNull(System.getProperty("user.dir")))) { it.parentFile }
                .first { File(it, "tools/sync_peer_worker.py").isFile }
        val databaseName = "deployment-peer-${UUID.randomUUID()}.db"
        var repository = TrainlogRepository(context, databaseName)
        var localWorker: Process? = null
        var lostDesktopAckGeneration: String? = null
        try {
            transport.mkdirs()
            // A previous interrupted exchange leaves durable coordination
            // objects behind. The next run must wait for correlated replacements.
            File(transport, "desktop-consumption-ack-v1.json").writeText(
                """{"run_id":"sy_11111111-1111-4111-8111-111111111111","generation_id":"gen_11111111-1111-4111-8111-111111111111"}"""
            )
            File(transport, "desktop-generation-v1.json").writeText(
                """{"run_id":"sy_11111111-1111-4111-8111-111111111111","generation_id":"gen_22222222-2222-4222-8222-222222222222"}"""
            )
            val exercise =
                (repository.createExercise(
                        NewExerciseProfile(
                            "Deployment conversation",
                            RecordingMode.SETS,
                            TrackingMode.REPS,
                            0,
                        )
                    ) as CreateExerciseResult.Created)
                    .exercise
            val occurrence =
                SessionExerciseDraft(
                    exercise = exercise,
                    sets = listOf(SessionSetDraft(reps = 7, weightKg = 22.5)),
                )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    ActiveSessionDraft(exercises = listOf(occurrence))
                ),
            )
            val release = System.getenv("TRAINLOG_SYNC_TEST_RELEASE_FILE")
            val desktop = File(transport, "desktop.sqlite")
            if (suppliedTransport == null) {
                val fixture =
                    ProcessBuilder(
                            File(repositoryRoot, "build/tui/sync-generation-fixture").absolutePath,
                            desktop.absolutePath,
                        )
                        .directory(repositoryRoot)
                        .start()
                assertEquals(0, fixture.waitFor())
            }
            val conversations = if (suppliedTransport == null) 24 else 1
            repeat(conversations) {
                val visibilityEvents = mutableListOf<List<String>>()
                val tracePhases = mutableListOf<String>()
                val coordinator = SyncGenerationForegroundCoordinator(
                    repository,
                    MtpPublicationVisibility { files ->
                        visibilityEvents += files.map(File::getName)
                    },
                    SyncGenerationTrace { _, phase, _, _, _ -> tracePhases += phase },
                )
                val executor = java.util.concurrent.Executors.newSingleThreadExecutor()
                var requestSignals = 0
                var refreshedAfterDesktopReference = false
                val result =
                    executor.submit<ForegroundGenerationResult> {
                        coordinator.run(
                            transport,
                            Duration.ofSeconds(30),
                            afterPeerPublication = { requestSignals += 1 },
                            afterPublication = { release?.let { awaitFile(File(it)) } },
                            pollTransport = {
                                refreshedAfterDesktopReference =
                                    runCatching {
                                        JSONObject(File(transport, "desktop-generation-v1.json").readText())
                                            .optString("format") ==
                                            "trainlog-sync-generation-reference"
                                    }.getOrDefault(false)
                            },
                        )
                    }
                val peer = JSONObject(awaitFile(File(transport, "android-peer-v1.json")).readText())
                if (suppliedTransport == null) {
                    localWorker =
                    ProcessBuilder(
                            "python3",
                            File(repositoryRoot, "tools/sync_peer_worker.py").absolutePath,
                            "--database",
                            desktop.absolutePath,
                            "--transport-root",
                            transport.absolutePath,
                            "--owned-root",
                            File(transport, "owned").absolutePath,
                            "--run-id",
                            "sy_${UUID.randomUUID()}",
                            "--expected-peer",
                            peer.getString("peer_id"),
                            "--timeout",
                            "30",
                        )
                        .directory(repositoryRoot)
                        .redirectErrorStream(true)
                        .start()
                }
                val completed = result.get(35, java.util.concurrent.TimeUnit.SECONDS)
                executor.shutdownNow()
                assertTrue(
                    "conversation ${it + 1}: $completed",
                    completed is ForegroundGenerationResult.Completed,
                )
                assertEquals("one fresh request signal per conversation", 1, requestSignals)
                assertTrue(
                    "transport is refreshed after the correlated desktop reference",
                    refreshedAfterDesktopReference,
                )
                val referenceVisibility = visibilityEvents.indexOfFirst {
                    it == listOf("android-generation-v1.json")
                }
                val objectVisibility = visibilityEvents.indexOfFirst { "manifest.json" in it }
                assertTrue("generation objects must become visible", objectVisibility >= 0)
                assertTrue(
                    "reference must follow complete object visibility",
                    referenceVisibility > objectVisibility,
                )
                assertTrue(
                    tracePhases.containsAll(
                        listOf(
                            "peer_published",
                            "full_generation_trigger_published",
                            "generation_request_observed",
                            "archive_ack_observed",
                            "android_archive_ack_published",
                            "generation_capture_started",
                            "generation_captured",
                            "generation_objects_published",
                            "generation_reference_published",
                            "desktop_ack_observed",
                            "desktop_generation_observed",
                            "desktop_generation_consumed",
                            "android_ack_published",
                            "completed",
                        )
                    )
                )
                var workerOutput = ""
                localWorker?.let { worker ->
                    workerOutput = worker.inputStream.bufferedReader().readText()
                    assertEquals("worker failed:\n$workerOutput", 0, worker.waitFor())
                    assertTrue(workerOutput.contains("\"result\":\"completed\""))
                }
                if (suppliedTransport == null && it == 7) {
                    // Reproduce the deployed pre-archive state: those builds
                    // advanced producer status but did not retain received
                    // ACK rows. The next real worker conversation must return
                    // the desktop consumer's exact evidence and unblock the
                    // full active quota without deleting any generation.
                    repository.inSyncGenerationTransaction { database ->
                        database.delete("sync_acknowledgements", null, null)
                        database.rawQuery(
                            "SELECT COUNT(*) FROM sync_generations WHERE status='acknowledged'",
                            null,
                        ).use { cursor ->
                            assertTrue(cursor.moveToFirst())
                            assertEquals(8, cursor.getInt(0))
                        }
                    }
                }
                if (suppliedTransport == null && it == 11) {
                    /* Reproduce the inverse deployed failure: Android has
                     * durably consumed the newest desktop generation, but its
                     * ACK was lost before the desktop producer committed it.
                     * The next conversation must recover that exact Android
                     * evidence before capturing another desktop generation. */
                    SQLiteDatabase.openDatabase(
                        desktop.absolutePath,
                        null,
                        SQLiteDatabase.OPEN_READWRITE,
                    ).use { database ->
                        val generation =
                            database.rawQuery(
                                "SELECT generation_id FROM sync_generations " +
                                    "WHERE status='acknowledged' ORDER BY generated_at DESC LIMIT 1",
                                null,
                            ).use { cursor ->
                                assertTrue(cursor.moveToFirst())
                                cursor.getString(0)
                            }
                        database.delete(
                            "sync_acknowledgements",
                            "generation_id=?",
                            arrayOf(generation),
                        )
                        database.execSQL(
                            "UPDATE sync_generations SET status='waiting_acknowledgement'," +
                                "acknowledged_at=NULL WHERE generation_id=?",
                            arrayOf(generation),
                        )
                        database.rawQuery(
                            "SELECT status FROM sync_generations WHERE generation_id=?",
                            arrayOf(generation),
                        ).use { cursor ->
                            assertTrue(cursor.moveToFirst())
                            assertEquals("waiting_acknowledgement", cursor.getString(0))
                        }
                        lostDesktopAckGeneration = generation
                    }
                }
                if (suppliedTransport == null && it == 12) {
                    val recovered = checkNotNull(lostDesktopAckGeneration)
                    SQLiteDatabase.openDatabase(
                        desktop.absolutePath,
                        null,
                        SQLiteDatabase.OPEN_READONLY,
                    ).use { database ->
                        database.rawQuery(
                            "SELECT status FROM sync_generations WHERE generation_id=?",
                            arrayOf(recovered),
                        ).use { cursor ->
                            assertTrue(cursor.moveToFirst())
                            assertEquals("acknowledged", cursor.getString(0))
                        }
                        database.rawQuery(
                            "SELECT parent_generation_id,status FROM sync_generations " +
                                "ORDER BY generated_at DESC LIMIT 1",
                            null,
                        ).use { cursor ->
                            assertTrue(cursor.moveToFirst())
                            assertEquals(recovered, cursor.getString(0))
                            assertEquals("acknowledged", cursor.getString(1))
                        }
                    }
                }
                localWorker = null
            }
            repository.close()
            repository = TrainlogRepository(context, databaseName)
            assertTrue(repository.loadActiveSessionDraft() is ActiveDraftLoadResult.Loaded)
            if (suppliedTransport == null) {
                SQLiteDatabase.openDatabase(
                    context.getDatabasePath(databaseName).absolutePath,
                    null,
                    SQLiteDatabase.OPEN_READONLY,
                ).use { database ->
                    database.rawQuery("SELECT COUNT(*) FROM sync_generation_archives", null).use {
                        assertTrue(it.moveToFirst())
                        assertTrue(it.getInt(0) >= 16)
                    }
                    database.rawQuery("SELECT COUNT(*) FROM sync_generations", null).use {
                        assertTrue(it.moveToFirst())
                        assertEquals(24, it.getInt(0))
                    }
                }
            }
        } finally {
            localWorker?.destroyForcibly()
            repository.close()
            context.deleteDatabase(databaseName)
            if (suppliedTransport == null) transport.deleteRecursively()
        }
    }
}
