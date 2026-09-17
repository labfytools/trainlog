package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import java.io.File
import java.io.FileInputStream
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class BridgeBackupMigrationTest {
    @Test
    fun verifiedSchemaSeventeenBridgeBackupMigratesThroughCurrentOwner() {
        val source = System.getenv("TRAINLOG_BRIDGE_BACKUP_INPUT")?.let(::File)
        assumeTrue(source?.isFile == true)
        val context: Context = ApplicationProvider.getApplicationContext()
        val name = "bridge-migrate-${UUID.randomUUID()}.db"
        val service = AndroidBackupService(context)
        var repository = TrainlogRepository(context, name)
        try {
            val verified = FileInputStream(source!!).use { service.verify(it).getOrThrow() }
            assertTrue(service.restore(repository, verified, name) is AndroidBackupResult.Success)
            repository = TrainlogRepository(context, name)
            assertEquals(
                "21,",
                (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded)
                    .draft
                    .form
                    .weightText,
            )
            android.database.sqlite.SQLiteDatabase.openDatabase(
                    context.getDatabasePath(name).path,
                    null,
                    android.database.sqlite.SQLiteDatabase.OPEN_READONLY,
                )
                .use { assertEquals(20, it.version) }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
