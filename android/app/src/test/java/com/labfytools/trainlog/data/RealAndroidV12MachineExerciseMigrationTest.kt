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
            assertEquals(13, scalarInt(database, "PRAGMA user_version;"))
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
