package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import java.io.File
import java.nio.file.Files
import java.time.Duration
import java.util.UUID
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SyncConversationOwnershipTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    private fun request(root: File, runId: String, androidPeer: String, desktopPeer: String) {
        File(root, "request-v1.json").writeText(
            JSONObject()
                .put("format", "trainlog-sync-generation-request")
                .put("version", 1)
                .put("run_id", runId)
                .put("android_peer_id", androidPeer)
                .put("desktop_peer_id", desktopPeer)
                .toString()
        )
    }

    private fun archiveAcknowledgements(
        root: File,
        runId: String,
        androidPeer: String,
        desktopPeer: String,
    ) {
        File(root, "desktop-archive-acknowledgements-v1.json").writeText(
            JSONObject()
                .put("format", "trainlog-sync-archive-acknowledgements")
                .put("version", 1)
                .put("run_id", runId)
                .put("android_peer_id", androidPeer)
                .put("desktop_peer_id", desktopPeer)
                .put("acknowledgements", JSONArray())
                .toString()
        )
    }

    @Test
    fun explicitClickYieldsOldBackgroundAndPublishesFreshTriggerForNewRun() {
        val databaseName = "generation-ownership-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-ownership-").toFile()
        val repository = TrainlogRepository(context, databaseName)
        val arbiter = SyncConversationArbiter()
        val oldObserved = CountDownLatch(1)
        val capturedRuns = mutableListOf<String>()
        val desktopPeer = "peer_${UUID.randomUUID()}"
        val oldRun = "sy_${UUID.randomUUID()}"
        val newRun = "sy_${UUID.randomUUID()}"
        val service = SyncGenerationService(repository)
        val background =
            SyncGenerationCoordinator(
                repository,
                arbiter = arbiter,
                trace = SyncGenerationTrace { runId, phase, _, _, _ ->
                    if (phase == "generation_request_observed" && runId == oldRun) oldObserved.countDown()
                },
            )
        val foreground =
            SyncGenerationCoordinator(
                repository,
                arbiter = arbiter,
                trace = SyncGenerationTrace { runId, phase, _, _, _ ->
                    if (phase == "generation_captured") synchronized(capturedRuns) {
                        capturedRuns += checkNotNull(runId)
                    }
                },
            )
        val executor = java.util.concurrent.Executors.newFixedThreadPool(2)
        try {
            request(root, oldRun, service.peerId(), desktopPeer)
            File(root, "trainlog-sync-request-v1.json").writeText(
                """{"format":"trainlog-sync-request","version":1,"request_id":"sr_old"}"""
            )
            val old = executor.submit<ForegroundGenerationResult> {
                background.run(root, Duration.ofSeconds(3))
            }
            assertTrue(oldObserved.await(2, TimeUnit.SECONDS))

            val claim = arbiter.announceExplicit()
            val snapshot =
                (DirectExchangePublisher { directExchangeDirectoryForPath(root) }.snapshot()
                    as DirectExchangeSnapshotResult.Ready).snapshot
            val outbox = SyncRequestOutbox { directExchangeDirectoryForPath(root) }
            var freshRequestId = ""
            val explicit = executor.submit<ForegroundGenerationResult> {
                foreground.run(
                    root,
                    Duration.ofMillis(700),
                    afterPeerPublication = {
                        freshRequestId =
                            (outbox.requestSync(snapshot) as SyncRequestResult.Requested).requestId
                        request(root, newRun, service.peerId(), desktopPeer)
                        archiveAcknowledgements(root, newRun, service.peerId(), desktopPeer)
                    },
                    intent = SyncConversationIntent.NEW_EXPLICIT,
                    explicitClaim = claim,
                )
            }

            assertTrue(old.get(2, TimeUnit.SECONDS) is ForegroundGenerationResult.Superseded)
            assertTrue(explicit.get(3, TimeUnit.SECONDS) is ForegroundGenerationResult.Failed)
            assertNotEquals("sr_old", freshRequestId)
            assertEquals(
                freshRequestId,
                JSONObject(File(root, "trainlog-sync-request-v1.json").readText())
                    .getString("request_id"),
            )
            assertEquals(listOf(newRun), capturedRuns)
            repository.inSyncGenerationTransaction { database ->
                database.rawQuery("SELECT run_id FROM sync_generations", null).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(newRun, cursor.getString(0))
                    assertEquals(false, cursor.moveToNext())
                }
            }
        } finally {
            executor.shutdownNow()
            repository.close()
            context.deleteDatabase(databaseName)
            root.deleteRecursively()
        }
    }

    @Test
    fun terminalBackgroundRunStaysSuppressedUntilNewRequestOrRemoteEvidence() {
        val databaseName = "generation-suppression-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-suppression-").toFile()
        val repository = TrainlogRepository(context, databaseName)
        try {
            val service = SyncGenerationService(repository)
            val coordinator = SyncGenerationCoordinator(repository, arbiter = SyncConversationArbiter())
            val desktopPeer = "peer_${UUID.randomUUID()}"
            val runId = "sy_${UUID.randomUUID()}"
            request(root, runId, service.peerId(), desktopPeer)
            service.capture(root, desktopPeer, runId)
            val terminal = BackgroundTerminalRun(runId, coordinator.remoteEvidence(root, runId))

            repeat(5) {
                assertEquals(
                    BackgroundRequestActionability.RESUMABLE_BUT_STALE,
                    coordinator.classifyBackgroundRequest(root, terminal),
                )
            }

            File(root, "desktop-consumption-ack-v1.json").writeText(
                JSONObject()
                    .put("format", "trainlog-sync-ack")
                    .put("version", 1)
                    .put("run_id", runId)
                    .put("proof", "new")
                    .toString()
            )
            assertEquals(
                BackgroundRequestActionability.NEW_REMOTE_EVIDENCE,
                coordinator.classifyBackgroundRequest(root, terminal),
            )

            request(root, "sy_${UUID.randomUUID()}", service.peerId(), desktopPeer)
            assertEquals(
                BackgroundRequestActionability.NEW_REQUEST,
                coordinator.classifyBackgroundRequest(root, terminal),
            )
        } finally {
            repository.close()
            context.deleteDatabase(databaseName)
            root.deleteRecursively()
        }
    }

    @Test
    fun cancelledExplicitConversationHandsSameGenerationToBackgroundWithoutDuplication() {
        val databaseName = "generation-handoff-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-handoff-").toFile()
        val repository = TrainlogRepository(context, databaseName)
        val arbiter = SyncConversationArbiter()
        val published = CountDownLatch(1)
        val result = AtomicReference<ForegroundGenerationResult>()
        val desktopPeer = "peer_${UUID.randomUUID()}"
        val oldRunId = "sy_${UUID.randomUUID()}"
        val runId = "sy_${UUID.randomUUID()}"
        try {
            val service = SyncGenerationService(repository)
            request(root, oldRunId, service.peerId(), desktopPeer)
            val coordinator =
                SyncGenerationCoordinator(
                    repository,
                    arbiter = arbiter,
                    trace = SyncGenerationTrace { _, phase, _, _, _ ->
                        if (phase == "generation_reference_published") published.countDown()
                    },
                )
            val claim = arbiter.announceExplicit()
            val thread = Thread {
                result.set(
                    coordinator.run(
                        root,
                        Duration.ofSeconds(5),
                        afterPeerPublication = {
                            request(root, runId, service.peerId(), desktopPeer)
                            archiveAcknowledgements(root, runId, service.peerId(), desktopPeer)
                        },
                        intent = SyncConversationIntent.NEW_EXPLICIT,
                        explicitClaim = claim,
                    )
                )
            }
            thread.start()
            assertTrue(published.await(3, TimeUnit.SECONDS))
            thread.interrupt()
            thread.join(3_000)
            assertTrue(result.get() is ForegroundGenerationResult.Cancelled)
            assertEquals(
                BackgroundRequestActionability.ACTIVE_HANDOFF,
                coordinator.classifyBackgroundRequest(root, null),
            )

            val resumed = coordinator.run(root, Duration.ofMillis(250))
            assertTrue(resumed is ForegroundGenerationResult.Failed)
            repository.inSyncGenerationTransaction { database ->
                database.rawQuery("SELECT COUNT(*) FROM sync_generations WHERE run_id=?", arrayOf(runId))
                    .use { cursor ->
                        assertTrue(cursor.moveToFirst())
                        assertEquals(1, cursor.getInt(0))
                    }
            }
        } finally {
            repository.close()
            context.deleteDatabase(databaseName)
            root.deleteRecursively()
        }
    }
}
