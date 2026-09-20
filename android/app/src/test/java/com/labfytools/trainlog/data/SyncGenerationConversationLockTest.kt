package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import java.nio.file.Files
import java.time.Duration
import java.util.UUID
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SyncGenerationConversationLockTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun foregroundConversationCannotBeStolenByBackgroundCoordinator() {
        val databaseName = "generation-lock-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-lock-").toFile()
        val repository = TrainlogRepository(context, databaseName)
        val entered = CountDownLatch(1)
        val executor = java.util.concurrent.Executors.newSingleThreadExecutor()
        try {
            val foreground = SyncGenerationCoordinator(repository)
            val background = SyncGenerationCoordinator(repository)
            val active = executor.submit<ForegroundGenerationResult> {
                foreground.run(
                    root,
                    Duration.ofMillis(300),
                    afterPeerPublication = { entered.countDown() },
                )
            }
            assertTrue(entered.await(2, TimeUnit.SECONDS))
            val duplicate = background.run(root, Duration.ofMillis(10))
            assertTrue(duplicate is ForegroundGenerationResult.Error)
            assertEquals(
                "Synchronization already in progress.",
                (duplicate as ForegroundGenerationResult.Error).message,
            )
            assertTrue(active.get(2, TimeUnit.SECONDS) is ForegroundGenerationResult.Error)
            repository.inSyncGenerationTransaction { database ->
                database.rawQuery("SELECT COUNT(*) FROM sync_generations", null).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(0, cursor.getInt(0))
                }
            }
        } finally {
            executor.shutdownNow()
            repository.close()
            context.deleteDatabase(databaseName)
            root.deleteRecursively()
        }
    }
}
