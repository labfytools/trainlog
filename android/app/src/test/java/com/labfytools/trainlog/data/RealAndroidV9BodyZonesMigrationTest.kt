package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import java.nio.file.Files
import java.nio.file.StandardCopyOption

/**
 * Optional real-data migration harness.
 *
 * CONTRACT: the source path comes only from TRAINLOG_ANDROID_V9_FIXTURE and is
 * never opened by the production helper. Two test-owned copies are made first;
 * the fixture and the user's installed database cannot be mutated or deleted.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class RealAndroidV9BodyZonesMigrationTest {
    private lateinit var context: Context
    private val databaseNames = mutableListOf<String>()

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
    }

    @After
    fun tearDown() {
        databaseNames.forEach(context::deleteDatabase)
    }

    @Test
    fun realVersionNineCopyMigratesWithoutChangingExistingTables() {
        val source = File(System.getenv("TRAINLOG_ANDROID_V9_FIXTURE") ?: "")
        assumeTrue("TRAINLOG_ANDROID_V9_FIXTURE is not available", source.isFile)

        val beforeName = "body-zones-real-v9-before.db"
        val migratedName = "body-zones-real-v9-migrated.db"
        databaseNames += listOf(beforeName, migratedName)
        val beforePath = context.getDatabasePath(beforeName)
        val migratedPath = context.getDatabasePath(migratedName)
        beforePath.parentFile?.mkdirs()
        Files.copy(source.toPath(), beforePath.toPath(), StandardCopyOption.REPLACE_EXISTING)
        Files.copy(source.toPath(), migratedPath.toPath(), StandardCopyOption.REPLACE_EXISTING)

        SQLiteDatabase.openDatabase(
            beforePath.path, null, SQLiteDatabase.OPEN_READONLY,
        ).use { before ->
            assertEquals(9, scalarInt(before, "PRAGMA user_version;"))
            assertEquals("ok", scalarString(before, "PRAGMA integrity_check;"))
            before.rawQuery("PRAGMA foreign_key_check;", null).use {
                assertFalse(it.moveToFirst())
            }
        }

        val repository = TrainlogRepository(context, migratedName)
        assertEquals(23, repository.listExercises().size)
        repository.close()

        SQLiteDatabase.openDatabase(
            migratedPath.path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { migrated ->
            assertEquals(12, scalarInt(migrated, "PRAGMA user_version;"))
            assertEquals("ok", scalarString(migrated, "PRAGMA integrity_check;"))
            migrated.rawQuery("PRAGMA foreign_key_check;", null).use {
                assertFalse(it.moveToFirst())
            }
            migrated.execSQL("ATTACH DATABASE ? AS before_v9;", arrayOf(beforePath.path))
            val historicalTables = listOf(
                "active_session_draft", "body_observations",
                "catalog_exercise_equipment", "continuous_activity",
                "draft_continuous_activity", "draft_max_results",
                "draft_performed_sets", "equipment",
                "equipment_aliases", "exercise_equipment", "exercises",
                "max_results", "performed_sets", "sessions",
            )
            historicalTables.forEach { table ->
                /* Table names are a closed test constant; values remain bound
                 * and the EXCEPT comparison covers every column and row. */
                assertEquals(0, scalarInt(migrated,
                    "SELECT COUNT(*) FROM (SELECT * FROM main.$table " +
                        "EXCEPT SELECT * FROM before_v9.$table);"))
                assertEquals(0, scalarInt(migrated,
                    "SELECT COUNT(*) FROM (SELECT * FROM before_v9.$table " +
                        "EXCEPT SELECT * FROM main.$table);"))
            }
            for ((table, columns) in listOf(
                "session_exercises" to "id,session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id",
                "draft_session_exercises" to "id,draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id",
            )) {
                assertEquals(0, scalarInt(migrated,
                    "SELECT COUNT(*) FROM (SELECT $columns FROM main.$table EXCEPT SELECT $columns FROM before_v9.$table);"))
                assertEquals(0, scalarInt(migrated,
                    "SELECT COUNT(*) FROM (SELECT $columns FROM before_v9.$table EXCEPT SELECT $columns FROM main.$table);"))
                assertEquals(0, scalarInt(migrated,
                    "SELECT COUNT(*) FROM main.$table WHERE load_mode<>'none' OR rest_seconds<>0 OR target_sets IS NOT NULL OR target_reps IS NOT NULL OR target_duration_seconds IS NOT NULL OR target_weight_kg IS NOT NULL;"))
            }
            assertEquals(32, scalarInt(migrated,
                "SELECT COUNT(*) FROM exercise_body_zones;"))
            assertEquals(20, scalarInt(migrated,
                "SELECT COUNT(DISTINCT exercise_row_id) FROM exercise_body_zones;"))
            assertEquals(23, scalarInt(migrated,
                "SELECT COUNT(*) FROM exercise_body_zone_sync;"))
            assertEquals(3, scalarInt(migrated,
                "SELECT COUNT(*) FROM exercises e WHERE NOT EXISTS(" +
                    "SELECT 1 FROM exercise_body_zones z WHERE z.exercise_row_id=e.id);"))
            migrated.execSQL("DETACH DATABASE before_v9;")
        }
    }

    private fun scalarInt(database: SQLiteDatabase, sql: String): Int =
        database.rawQuery(sql, null).use { cursor ->
            assertTrue(cursor.moveToFirst())
            cursor.getInt(0)
        }

    private fun scalarString(database: SQLiteDatabase, sql: String): String =
        database.rawQuery(sql, null).use { cursor ->
            assertTrue(cursor.moveToFirst())
            cursor.getString(0)
        }
}
