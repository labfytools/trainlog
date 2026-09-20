package com.labfytools.trainlog.data

import java.nio.file.Files
import org.json.JSONObject
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SyncRequestOutboxTest {
    @Test
    fun distinctExplicitRequestsAlwaysPublishFreshIdentities() {
        val directory = Files.createTempDirectory("trainlog-request-outbox-").toFile()
        try {
            val outbox = SyncRequestOutbox { directExchangeDirectoryForPath(directory) }
            val snapshot =
                (DirectExchangePublisher { directExchangeDirectoryForPath(directory) }.snapshot()
                    as DirectExchangeSnapshotResult.Ready).snapshot
            val first = outbox.requestSync(snapshot) as SyncRequestResult.Requested
            val firstBytes = java.io.File(directory, "trainlog-sync-request-v1.json").readText()
            val second = outbox.requestSync(snapshot) as SyncRequestResult.Requested
            val secondBytes = java.io.File(directory, "trainlog-sync-request-v1.json").readText()

            assertNotEquals(first.requestId, second.requestId)
            assertNotEquals(firstBytes, secondBytes)
            assertTrue(JSONObject(secondBytes).getString("request_id") == second.requestId)
        } finally {
            directory.deleteRecursively()
        }
    }

    @Test
    fun oneIntentUsesSameIdentityAcrossFullGenerationAndLegacyChannels() {
        val directory = Files.createTempDirectory("trainlog-dual-request-").toFile()
        try {
            val outbox = SyncRequestOutbox { directExchangeDirectoryForPath(directory) }
            val snapshot =
                (DirectExchangePublisher { directExchangeDirectoryForPath(directory) }.snapshot()
                    as DirectExchangeSnapshotResult.Ready).snapshot
            val intent = outbox.newIntent()

            assertTrue(outbox.publishFullGeneration(snapshot, intent) is SyncRequestResult.Requested)
            assertTrue(outbox.publishLegacy(snapshot, intent) is SyncRequestResult.Requested)

            val full = JSONObject(java.io.File(directory, FULL_GENERATION_REQUEST_NAME).readText())
            val legacy = JSONObject(java.io.File(directory, LEGACY_SYNC_REQUEST_NAME).readText())
            assertEquals(intent.requestId, full.getString("request_id"))
            assertEquals(intent.requestId, legacy.getString("request_id"))
            assertEquals(full.toString(), legacy.toString())
        } finally {
            directory.deleteRecursively()
        }
    }
}
