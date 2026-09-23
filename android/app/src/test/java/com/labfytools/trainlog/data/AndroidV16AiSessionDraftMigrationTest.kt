/*
 * Regression coverage for AndroidV16AiSessionDraftMigrationTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
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

/** Physical v16 fixture: v17 adds only the separate inert AI proposal tables. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV16AiSessionDraftMigrationTest {
    private lateinit var context: Context
    private lateinit var name: String

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        name = "ai-drafts-v16-${UUID.randomUUID()}.db"
    }

    @After fun tearDown() { context.deleteDatabase(name) }

    @Test fun v16FixtureAddsEmptyAiTablesWithoutChangingHistoryOrActiveDraft() {
        val repository = TrainlogRepository(context, name)
        val exercise = (repository.createExercise(NewExerciseProfile(
            "V16 migration fixture", RecordingMode.SETS, TrackingMode.REPS, 0,
        )) as CreateExerciseResult.Created).exercise
        assertTrue(repository.saveSession(SessionDraft(listOf(SessionExerciseDraft(
            entryId = "sxe_11111111-1111-4111-8111-111111111111", exercise = exercise,
            sets = listOf(SessionSetDraft(reps = 8, weightKg = 50.0)),
        )))) is SaveSessionResult.Saved)
        assertEquals(ActiveDraftMutationResult.Saved, repository.saveActiveSessionDraft(
            ActiveSessionDraft(exercises = listOf(SessionExerciseDraft(
                entryId = "sxe_22222222-2222-4222-8222-222222222222", exercise = exercise,
                sets = listOf(SessionSetDraft(reps = 5, weightKg = 30.0)),
            ))),
        ))
        repository.close()

        val path = context.getDatabasePath(name).path
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("DROP TABLE ai_session_draft_entries")
            db.execSQL("DROP TABLE ai_session_drafts")
            db.execSQL("PRAGMA user_version=16")
        }
        val before = SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            listOf(
                scalar(db, "SELECT COUNT(*) FROM sessions"),
                scalar(db, "SELECT COUNT(*) FROM performed_sets"),
                scalar(db, "SELECT COUNT(*) FROM active_session_draft"),
                scalar(db, "SELECT COUNT(*) FROM draft_session_exercises"),
                scalar(db, "SELECT COUNT(*) FROM draft_performed_sets"),
            )
        }

        TrainlogRepository(context, name).let { migrated ->
            migrated.listSessions()
            migrated.close()
        }
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            assertEquals(27, scalar(db, "PRAGMA user_version"))
            assertEquals(before, listOf(
                scalar(db, "SELECT COUNT(*) FROM sessions"),
                scalar(db, "SELECT COUNT(*) FROM performed_sets"),
                scalar(db, "SELECT COUNT(*) FROM active_session_draft"),
                scalar(db, "SELECT COUNT(*) FROM draft_session_exercises"),
                scalar(db, "SELECT COUNT(*) FROM draft_performed_sets"),
            ))
            assertEquals(0, scalar(db, "SELECT COUNT(*) FROM ai_session_drafts"))
            assertEquals(0, scalar(db, "SELECT COUNT(*) FROM ai_session_draft_entries"))
            assertEquals("ok", string(db, "PRAGMA integrity_check"))
            db.rawQuery("PRAGMA foreign_key_check", null).use { assertFalse(it.moveToFirst()) }
        }
    }

    private fun scalar(db: SQLiteDatabase, sql: String) = db.rawQuery(sql, null).use {
        assertTrue(it.moveToFirst()); it.getInt(0)
    }

    private fun string(db: SQLiteDatabase, sql: String) = db.rawQuery(sql, null).use {
        assertTrue(it.moveToFirst()); it.getString(0)
    }
}
