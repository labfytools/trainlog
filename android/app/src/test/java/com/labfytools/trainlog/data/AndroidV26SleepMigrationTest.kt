package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV26SleepMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v25AddsEmptySleepDomainAndReopens() {
        val name = "sleep-v25-${UUID.randomUUID()}.db"
        TrainlogRepository(context, name).useForTest { assertTrueEmpty(it) }
        SQLiteDatabase.openDatabase(context.getDatabasePath(name).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("DROP TABLE sleep_diary_events")
            db.execSQL("DROP TABLE sleep_diary_revisions")
            db.execSQL("DROP TABLE sleep_diary_entries")
            db.version = 25
        }
        var repository = TrainlogRepository(context, name)
        try {
            assertTrueEmpty(repository)
            repository.close()
            repository = TrainlogRepository(context, name)
            assertTrueEmpty(repository)
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { assertEquals(30, it.version) }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    private fun assertTrueEmpty(repository: TrainlogRepository) = assertEquals(emptyList<Any>(), repository.listSleepDiary())

    private fun TrainlogRepository.useForTest(block: (TrainlogRepository) -> Unit) {
        try { block(this) } finally { close() }
    }
}
