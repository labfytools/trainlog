package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
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
            val historical = historicalDump(verified.database)
            val activeDraft = historical.entries.single { it.key.startsWith("active_session_draft|") }
            val hadActiveDraft = activeDraft.value.isNotEmpty()
            assertTrue(service.restore(repository, verified, name) is AndroidBackupResult.Success)
            repository = TrainlogRepository(context, name)
            assertEquals(
                hadActiveDraft,
                repository.loadActiveSessionDraft() is ActiveDraftLoadResult.Loaded,
            )
            val migrated = context.getDatabasePath(name)
            assertEquals(historical, historicalDump(migrated, historical.keys))
            SQLiteDatabase.openDatabase(
                    migrated.path,
                    null,
                    android.database.sqlite.SQLiteDatabase.OPEN_READONLY,
                )
                .use { assertEquals(29, it.version) }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    /**
     * CONTRACT: every pre-migration column and value is compared exactly;
     * additive v18-v20 tables/columns are outside the historical shape.
     * INVARIANT: the supplied archive is opened read-only and only the
     * test-owned restored database is migrated.
     */
    private fun historicalDump(
        databasePath: File,
        shape: Set<String>? = null,
    ): Map<String, List<List<String>>> =
        SQLiteDatabase.openDatabase(
                databasePath.path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            )
            .use { database ->
                val tables =
                    database
                        .rawQuery(
                            "SELECT name FROM sqlite_master WHERE type='table' " +
                                "AND name NOT LIKE 'sqlite_%' AND name<>'android_metadata' " +
                                "ORDER BY name;",
                            null,
                        )
                        .use { cursor ->
                            buildList {
                                while (cursor.moveToNext()) add(cursor.getString(0))
                            }
                        }
                val specifications =
                    shape
                        ?: tables
                            .map { table ->
                                val columns =
                                    database.rawQuery("PRAGMA table_info(`$table`);", null).use {
                                        cursor ->
                                        buildList {
                                            while (cursor.moveToNext()) add(cursor.getString(1))
                                        }
                                    }
                                "$table|${columns.joinToString(",")}"
                            }
                            .toSet()
                specifications.associateWith { specification ->
                    val table = specification.substringBefore('|')
                    val columns = specification.substringAfter('|').split(',')
                    val selection = columns.joinToString(",") { "`$it`" }
                    database
                        .rawQuery(
                            "SELECT $selection FROM `$table` ORDER BY rowid;",
                            null,
                        )
                        .use { cursor ->
                            buildList {
                                while (cursor.moveToNext()) {
                                    add(
                                        (0 until cursor.columnCount).map { column ->
                                            when (cursor.getType(column)) {
                                                android.database.Cursor.FIELD_TYPE_NULL -> "null"
                                                android.database.Cursor.FIELD_TYPE_INTEGER ->
                                                    "integer:${cursor.getLong(column)}"
                                                android.database.Cursor.FIELD_TYPE_FLOAT ->
                                                    "float:${java.lang.Double.toHexString(cursor.getDouble(column))}"
                                                android.database.Cursor.FIELD_TYPE_STRING ->
                                                    "string:${cursor.getString(column)}"
                                                android.database.Cursor.FIELD_TYPE_BLOB ->
                                                    "blob:${cursor.getBlob(column).contentToString()}"
                                                else -> error("Unknown SQLite field type")
                                            }
                                        }
                                    )
                                }
                            }
                        }
                }
            }
}
