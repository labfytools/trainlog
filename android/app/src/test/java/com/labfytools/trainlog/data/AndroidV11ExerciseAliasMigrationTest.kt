package com.labfytools.trainlog.data

import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.BodyObservationDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV11ExerciseAliasMigrationTest {
    private lateinit var context: Context
    private lateinit var name: String

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        name = "exercise-alias-v11-${UUID.randomUUID()}.db"
    }

    @After fun tearDown() { context.deleteDatabase(name) }

    @Test fun exactVersionElevenFixtureOnlyAddsEmptyAliasTable() {
        val repository = TrainlogRepository(context, name)
        val created = repository.createExercise(NewExerciseProfile(
            "V11 fixture", RecordingMode.SETS, TrackingMode.REPS, 0,
        )) as CreateExerciseResult.Created
        assertTrue(repository.saveSession(SessionDraft(listOf(SessionExerciseDraft(
            entryId = "sxe_11111111-1111-4111-8111-111111111111",
            exercise = created.exercise,
            equipmentId = "leg_press",
            sets = listOf(SessionSetDraft(reps = 7, weightKg = 42.5)),
        )))) is SaveSessionResult.Saved)
        assertTrue(repository.saveBodyObservation(BodyObservationDraft(
            bodyWeightKg = 72.25,
        )) is SaveBodyObservationResult.Saved)
        repository.close()

        val path = context.getDatabasePath(name).path
        lateinit var before: Map<String, List<List<Any?>>>
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("DROP TABLE exercise_aliases")
            db.execSQL("PRAGMA user_version=11")
            before = snapshot(db)
        }

        TrainlogRepository(context, name).let { migrated ->
            migrated.listSessions()
            migrated.close()
        }
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            assertEquals(12, scalar(db, "PRAGMA user_version"))
            assertEquals(before, snapshot(db).filterKeys { it != "exercise_aliases" })
            assertEquals(0, scalar(db, "SELECT COUNT(*) FROM exercise_aliases"))
            db.rawQuery("PRAGMA foreign_key_check", null).use { assertFalse(it.moveToFirst()) }
        }
    }

    private fun snapshot(db: SQLiteDatabase): Map<String, List<List<Any?>>> {
        val tables = db.rawQuery(
            "SELECT name FROM sqlite_master WHERE type='table' " +
                "AND name NOT LIKE 'sqlite_%' AND name<>'android_metadata' ORDER BY name",
            null,
        ).use { cursor -> buildList { while (cursor.moveToNext()) add(cursor.getString(0)) } }
        return tables.associateWith { table ->
            db.rawQuery("SELECT * FROM `$table` ORDER BY rowid", null).use { cursor ->
                buildList {
                    while (cursor.moveToNext()) add(List(cursor.columnCount) { column -> cursor.value(column) })
                }
            }
        }
    }

    private fun Cursor.value(column: Int): Any? = when (getType(column)) {
        Cursor.FIELD_TYPE_NULL -> null
        Cursor.FIELD_TYPE_INTEGER -> getLong(column)
        Cursor.FIELD_TYPE_FLOAT -> getDouble(column)
        Cursor.FIELD_TYPE_STRING -> getString(column)
        Cursor.FIELD_TYPE_BLOB -> getBlob(column).toList()
        else -> error("Unsupported SQLite value type")
    }

    private fun scalar(db: SQLiteDatabase, sql: String): Int =
        db.rawQuery(sql, null).use { cursor ->
            assertTrue(cursor.moveToFirst())
            cursor.getInt(0)
        }
}
