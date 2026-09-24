package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV31CardioGuidanceMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v30AddsEmptyGuidanceDomainAndReopens() {
        val name = "guidance-v30-" + UUID.randomUUID() + ".db"
        var repository = TrainlogRepository(context, name)
        try {
            repository.listSessions()
            repository.close()
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READWRITE,
            ).use { db ->
                db.execSQL("DROP TABLE cardio_guidance_generation")
                db.execSQL("DROP TABLE cardio_guidance_events")
                db.execSQL("DROP TABLE cardio_guidance_phases")
                db.execSQL("DROP TABLE cardio_guidance_runs")
                db.version = 30
            }

            repository = TrainlogRepository(context, name)
            assertTrue(repository.activeCardioGuidancePhase() == null)
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { db ->
                assertEquals(33, db.version)
                db.rawQuery(
                    "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN(" +
                        "'cardio_guidance_runs','cardio_guidance_phases'," +
                        "'cardio_guidance_events','cardio_guidance_generation')",
                    null,
                ).use { c ->
                    assertTrue(c.moveToFirst())
                    assertEquals(4, c.getInt(0))
                }
                db.rawQuery("SELECT COUNT(*) FROM cardio_guidance_runs", null).use { c ->
                    assertTrue(c.moveToFirst())
                    assertEquals(0, c.getInt(0))
                }
            }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
