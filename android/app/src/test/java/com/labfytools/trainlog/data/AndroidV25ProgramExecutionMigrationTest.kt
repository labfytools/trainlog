package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.TrackingMode
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/** Additive v24 -> v25 provenance migration preserves the singleton draft. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV25ProgramExecutionMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v24FixturePreservesDraftAndInventsNoProgramProvenance() {
        val name = "program-execution-v24-${UUID.randomUUID()}.db"
        val first = TrainlogRepository(context, name)
        val exercise = (
            first.createExercise(
                NewExerciseProfile(
                    "Migration fixture",
                    RecordingMode.SETS,
                    TrackingMode.REPS,
                    0,
                ),
            ) as CreateExerciseResult.Created
        ).exercise
        assertEquals(
            ActiveDraftMutationResult.Saved,
            first.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = exercise,
                            plan = SessionExercisePlan(sets = 3, reps = 8),
                        ),
                    ),
                ),
            ),
        )
        first.close()

        val path = context.getDatabasePath(name).path
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            /* The published-unreleased fixture may already contain additive
             * columns; lowering only user_version proves migration replay is safe. */
            db.execSQL("PRAGMA user_version=24")
        }

        val reopened = TrainlogRepository(context, name)
        try {
            val draft = (reopened.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            assertEquals(null, draft.sourceProgramId)
            assertEquals(null, draft.sourceProgramSessionId)
        } finally {
            reopened.close()
        }
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            assertEquals(28, scalar(db, "PRAGMA user_version"))
            assertEquals(0, scalar(db, "SELECT COUNT(*) FROM sessions WHERE source_program_id IS NOT NULL"))
            assertEquals("ok", text(db, "PRAGMA integrity_check"))
            db.rawQuery("PRAGMA foreign_key_check", null).use { cursor ->
                assertFalse(cursor.moveToFirst())
            }
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
