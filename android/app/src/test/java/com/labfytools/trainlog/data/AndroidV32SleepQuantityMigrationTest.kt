package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.SleepMedication
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV32SleepQuantityMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v31IntakesHydrateToOneAndQuickPublicationStartsEmpty() {
        val name = "sleep-quantity-v31-${UUID.randomUUID()}.db"
        val initial = TrainlogRepository(context, name)
        try {
            val saved = initial.saveSleepMedication(
                SleepMedication(
                    "", "", "2026-09-23T20:00:00+02:00", "2026-09-23T20:00:00+02:00",
                    "Migration medication", 75.0, "mg", "", "", true,
                ),
            ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            initial.quickSleepMedication(saved.entryId, "2026-09-23T21:00:00+02:00")
        } finally {
            initial.close()
        }
        val path = context.getDatabasePath(name).path
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("PRAGMA foreign_keys=OFF")
            db.execSQL("DROP TABLE sleep_diary_quick_publication_state")
            db.execSQL("ALTER TABLE sleep_medication_intakes RENAME TO sleep_medication_intakes_v32")
            db.execSQL(
                """CREATE TABLE sleep_medication_intakes(
                    revision_id TEXT NOT NULL, intake_id TEXT NOT NULL,
                    medication_id TEXT NOT NULL, medication_name TEXT NOT NULL,
                    taken_at TEXT NOT NULL, dose_value REAL, dose_unit TEXT,
                    note TEXT, created_at TEXT NOT NULL,
                    PRIMARY KEY(revision_id,intake_id)
                )""".trimIndent(),
            )
            db.execSQL(
                "INSERT INTO sleep_medication_intakes SELECT revision_id,intake_id,medication_id," +
                    "medication_name,taken_at,dose_value,dose_unit,note,created_at " +
                    "FROM sleep_medication_intakes_v32",
            )
            db.execSQL("DROP TABLE sleep_medication_intakes_v32")
            db.execSQL("PRAGMA user_version=31")
        }

        val migrated = TrainlogRepository(context, name)
        migrated.listSleepDiary()
        migrated.close()
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            assertEquals(32, scalar(db, "PRAGMA user_version"))
            assertEquals(
                1,
                scalar(
                    db,
                    "SELECT COUNT(*) FROM pragma_table_info('sleep_medication_intakes') " +
                        "WHERE name='quantity' AND \"notnull\"=1 AND dflt_value='1'",
                ),
            )
            assertEquals(1, scalar(db, "SELECT quantity FROM sleep_medication_intakes"))
            assertEquals(
                1,
                scalar(
                    db,
                    "SELECT COUNT(*) FROM sqlite_master WHERE type='table' " +
                        "AND name='sleep_diary_quick_publication_state'",
                ),
            )
            assertEquals("ok", text(db, "PRAGMA integrity_check"))
        }
        context.deleteDatabase(name)
    }

    private fun scalar(db: SQLiteDatabase, sql: String): Int =
        db.rawQuery(sql, null).use { cursor ->
            assertTrue(cursor.moveToFirst())
            cursor.getInt(0)
        }

    private fun text(db: SQLiteDatabase, sql: String): String =
        db.rawQuery(sql, null).use { cursor ->
            assertTrue(cursor.moveToFirst())
            cursor.getString(0)
        }
}
