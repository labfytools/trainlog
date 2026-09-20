package com.labfytools.trainlog.data

import java.nio.file.Files
import org.json.JSONObject
import org.junit.Assert.assertNotEquals
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
}
