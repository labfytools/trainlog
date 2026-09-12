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
 * Optional real-copy v12 -> v13 production migration harness.
 *
 * WHY: synthetic fixtures cannot demonstrate that the production SQLiteOpenHelper
 * accepts the user's complete database graph and preserves its durable draft.
 * CONTRACT: the immutable source is copied before TrainlogRepository sees it;
 * only the test-owned database is opened read/write, and the migrated artifact is
 * exported to the explicit TRAINLOG_ANDROID_V13_OUTPUT path for host-side audit.
 * INVARIANT: a second production open is a logical no-op at schema v13.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class RealAndroidV12MachineExerciseMigrationTest {
    private lateinit var context: Context
    private val databaseName = "machine-exercise-real-v12.db"

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
    }

    @After
    fun tearDown() {
        context.deleteDatabase(databaseName)
    }

    @Test
    fun realVersionThirteenCopyUsesProductionMigrationPreservesFactsAndIsIdempotent() {
        val source = File(System.getenv("TRAINLOG_ANDROID_V13_FIXTURE") ?: "")
        val output = File(System.getenv("TRAINLOG_ANDROID_V14_OUTPUT") ?: "")
        assumeTrue("TRAINLOG_ANDROID_V13_FIXTURE is not available", source.isFile)
        assumeTrue("TRAINLOG_ANDROID_V14_OUTPUT is not available", output.parentFile?.isDirectory == true)
        val databasePath = context.getDatabasePath(databaseName)
        databasePath.parentFile?.mkdirs()
        Files.copy(source.toPath(), databasePath.toPath(), StandardCopyOption.REPLACE_EXISTING)
        val before = historicalDump(databasePath)
        val beforeDraftCounts = SQLiteDatabase.openDatabase(databasePath.path, null, SQLiteDatabase.OPEN_READONLY).use { database ->
            scalarInt(database, "SELECT COUNT(*) FROM active_session_draft;") to
                scalarInt(database, "SELECT COUNT(*) FROM draft_session_exercises;")
        }
        SQLiteDatabase.openDatabase(databasePath.path, null, SQLiteDatabase.OPEN_READONLY).use { database ->
            assertEquals(13, scalarInt(database, "PRAGMA user_version;"))
            assertEquals("ok", scalarString(database, "PRAGMA integrity_check;"))
        }
        TrainlogRepository(context, databaseName).let { repository ->
            try { assertTrue(repository.listExercises().isNotEmpty()) } finally { repository.close() }
        }
        assertValidVersionThirteen(databasePath)
        assertEquals(before, historicalDump(databasePath, before.keys))
        SQLiteDatabase.openDatabase(databasePath.path, null, SQLiteDatabase.OPEN_READONLY).use { database ->
            assertEquals(0, scalarInt(database, "SELECT COUNT(*) FROM exercise_feedback;"))
            assertEquals(0, scalarInt(database, "SELECT COUNT(*) FROM draft_exercise_feedback;"))
            assertEquals(0, scalarInt(database, "SELECT COUNT(*) FROM session_followups;"))
            assertEquals(beforeDraftCounts.first, scalarInt(database, "SELECT COUNT(*) FROM active_session_draft;"))
            assertEquals(beforeDraftCounts.second, scalarInt(database, "SELECT COUNT(*) FROM draft_session_exercises;"))
            /* No historical value is synthesized by the nullable new column. */
            assertEquals(0, scalarInt(database, "SELECT COUNT(*) FROM sessions WHERE ended_at IS NOT NULL;"))
        }
        val first = logicalDump(databasePath)
        TrainlogRepository(context, databaseName).let { repository ->
            try { assertTrue(repository.listExercises().isNotEmpty()) } finally { repository.close() }
        }
        assertEquals(first, logicalDump(databasePath))
        Files.copy(databasePath.toPath(), output.toPath(), StandardCopyOption.REPLACE_EXISTING)
    }

    @Test
    fun realVersionTwelveCopyUsesProductionMigrationAndIsIdempotent() {
        val source = File(System.getenv("TRAINLOG_ANDROID_V12_FIXTURE") ?: "")
        val output = File(System.getenv("TRAINLOG_ANDROID_V13_OUTPUT") ?: "")
        assumeTrue("TRAINLOG_ANDROID_V12_FIXTURE is not available", source.isFile)
        assumeTrue("TRAINLOG_ANDROID_V13_OUTPUT is not available", output.parentFile?.isDirectory == true)

        val databasePath = context.getDatabasePath(databaseName)
        databasePath.parentFile?.mkdirs()
        Files.copy(source.toPath(), databasePath.toPath(), StandardCopyOption.REPLACE_EXISTING)

        SQLiteDatabase.openDatabase(databasePath.path, null, SQLiteDatabase.OPEN_READONLY).use { before ->
            assertEquals(12, scalarInt(before, "PRAGMA user_version;"))
            assertEquals("ok", scalarString(before, "PRAGMA integrity_check;"))
            before.rawQuery("PRAGMA foreign_key_check;", null).use { assertFalse(it.moveToFirst()) }
        }

        TrainlogRepository(context, databaseName).let { repository ->
            try {
            assertTrue(repository.listExercises().isNotEmpty())
            } finally {
                repository.close()
            }
        }
        assertValidVersionThirteen(databasePath)
        val firstLogicalState = logicalDump(databasePath)

        TrainlogRepository(context, databaseName).let { repository ->
            try {
            assertTrue(repository.listExercises().isNotEmpty())
            } finally {
                repository.close()
            }
        }
        assertValidVersionThirteen(databasePath)
        assertEquals(firstLogicalState, logicalDump(databasePath))

        Files.copy(databasePath.toPath(), output.toPath(), StandardCopyOption.REPLACE_EXISTING)
    }

    private fun assertValidVersionThirteen(databasePath: File) {
        SQLiteDatabase.openDatabase(databasePath.path, null, SQLiteDatabase.OPEN_READONLY).use { database ->
            assertEquals(15, scalarInt(database, "PRAGMA user_version;"))
            assertEquals("ok", scalarString(database, "PRAGMA integrity_check;"))
            database.rawQuery("PRAGMA foreign_key_check;", null).use { assertFalse(it.moveToFirst()) }
        }
    }

    private fun logicalDump(databasePath: File): List<String> =
        SQLiteDatabase.openDatabase(databasePath.path, null, SQLiteDatabase.OPEN_READONLY).use { database ->
            val tables = database.rawQuery(
                "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;",
                null,
            ).use { cursor -> buildList { while (cursor.moveToNext()) add(cursor.getString(0)) } }
            buildList {
                tables.forEach { table ->
                    /* Table names originate from sqlite_master, not external input. */
                    database.rawQuery("SELECT * FROM `$table` ORDER BY rowid;", null).use { cursor ->
                        while (cursor.moveToNext()) {
                            add(buildString {
                                append(table)
                                repeat(cursor.columnCount) { column ->
                                    append('|')
                                    append(if (cursor.isNull(column)) "<NULL>" else cursor.getString(column))
                                }
                            })
                        }
                    }
                }
            }
        }

    /** Snapshot every original column so additive columns/tables are ignored
     * while every pre-migration user fact remains byte-for-byte comparable. */
    private fun historicalDump(databasePath: File, shape: Set<String>? = null): Map<String, List<String>> =
        SQLiteDatabase.openDatabase(databasePath.path, null, SQLiteDatabase.OPEN_READONLY).use { database ->
            val tables = database.rawQuery("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;", null)
                .use { c -> buildList { while(c.moveToNext()) add(c.getString(0)) } }
                .filter { it != "android_metadata" }
            val specifications = shape ?: tables.map { table ->
                val columns = database.rawQuery("PRAGMA table_info(`$table`);", null)
                    .use { c -> buildList { while(c.moveToNext()) add(c.getString(1)) } }
                "$table|${columns.joinToString(",")}"
            }.toSet()
            specifications.associateWith { specification ->
                val table = specification.substringBefore('|')
                val columns = specification.substringAfter('|').split(',')
                database.rawQuery("SELECT ${columns.joinToString(",") { "`$it`" }} FROM `$table` ORDER BY rowid;",null)
                    .use { c -> buildList { while(c.moveToNext()) add(buildString { repeat(c.columnCount){i->if(i>0)append('|');append(if(c.isNull(i))"<NULL>" else c.getString(i))} }) } }
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
