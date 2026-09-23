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
class AndroidV30CardioCalibrationMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v29AddsEmptyCalibrationDomainAndReservedExercise() {
        val name = "calibration-v29-" + UUID.randomUUID() + ".db"
        var repository = TrainlogRepository(context, name)
        try {
            repository.listExercises()
            repository.close()
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READWRITE,
            ).use { db ->
                db.execSQL("DROP TABLE cardio_calibration_generation")
                db.execSQL("DROP TABLE cardio_calibration_recovery")
                db.execSQL("DROP TABLE cardio_calibrations")
                db.version = 29
            }

            repository = TrainlogRepository(context, name)
            assertTrue(repository.listExercises().none { it.name == "Calibration cardio" })
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { db ->
                assertEquals(30, db.version)
                db.rawQuery(
                    "SELECT COUNT(*) FROM exercises WHERE " +
                        "exercise_id='ex_ca1b4a7e-1c2d-4f00-8a11-000000000001' " +
                        "AND name='Calibration cardio'",
                    null,
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(1, cursor.getInt(0))
                }
                db.rawQuery(
                    "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN(" +
                        "'cardio_calibrations','cardio_calibration_recovery'," +
                        "'cardio_calibration_generation')",
                    null,
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(3, cursor.getInt(0))
                }
            }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
