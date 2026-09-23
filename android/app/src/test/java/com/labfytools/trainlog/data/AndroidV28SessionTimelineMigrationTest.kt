package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV28SessionTimelineMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v27SeedsOnlyAuthoritativeActiveSessionBoundary() {
        val name = "timeline-v27-${UUID.randomUUID()}.db"
        var repository = TrainlogRepository(context, name)
        try {
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.startActiveSessionDraft(),
            )
            val exercise =
                (repository.createExercise(
                    NewExerciseProfile(
                        "Timeline migration exercise",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise
            val loaded =
                repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            val sessionId = checkNotNull(loaded.draft.sessionId)
            val startedAt = checkNotNull(loaded.draft.startedAt)
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    loaded.draft.copy(
                        exercises =
                            listOf(
                                SessionExerciseDraft(
                                    exercise = exercise,
                                    sets = listOf(SessionSetDraft(reps = 1)),
                                ),
                            ),
                    ),
                ),
            )
            repository.close()

            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READWRITE,
            ).use { db ->
                db.execSQL("DROP TABLE session_timeline_generation_sessions")
                db.execSQL("DROP TABLE session_timeline_exercises")
                db.execSQL("DROP TABLE session_timeline_sessions")
                db.version = 27
            }

            repository = TrainlogRepository(context, name)
            assertNotNull(repository.activeSessionTimelineContext())
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { db ->
                assertEquals(30, db.version)
                db.rawQuery(
                    "SELECT session_id,started_at,ended_at FROM session_timeline_sessions",
                    null,
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(sessionId, cursor.getString(0))
                    assertEquals(startedAt, cursor.getString(1))
                    assertTrue(cursor.isNull(2))
                }
                db.rawQuery(
                    "SELECT COUNT(*) FROM session_timeline_exercises",
                    null,
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals(0, cursor.getInt(0))
                }
            }
            assertNotNull(repository.activeSessionTimelineContext())
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
