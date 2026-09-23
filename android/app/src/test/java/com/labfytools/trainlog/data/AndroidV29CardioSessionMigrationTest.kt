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
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV29CardioSessionMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v28BackfillsCanonicalSessionKindFromFrozenLegacyType() {
        val name = "cardio-v28-" + UUID.randomUUID() + ".db"
        var repository = TrainlogRepository(context, name)
        try {
            val exercise =
                (repository.createExercise(
                    NewExerciseProfile(
                        "Cardio migration exercise",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise
            assertTrue(
                repository.saveSession(
                    SessionDraft(
                        listOf(
                            SessionExerciseDraft(
                                exercise = exercise,
                                sets = listOf(SessionSetDraft(reps = 5)),
                            ),
                        ),
                    ),
                ) is SaveSessionResult.Saved,
            )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    ActiveSessionDraft(
                        exercises =
                            listOf(
                                SessionExerciseDraft(
                                    exercise = exercise,
                                    sets = listOf(SessionSetDraft(reps = 6)),
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
                db.execSQL("UPDATE sessions SET session_kind='cardio'")
                db.execSQL("UPDATE active_session_draft SET session_kind='cardio'")
                db.version = 28
            }

            repository = TrainlogRepository(context, name)
            assertEquals(1, repository.listSessions().size)
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { db ->
                assertEquals(32, db.version)
                db.rawQuery(
                    "SELECT session_type,session_kind FROM sessions",
                    null,
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals("training", cursor.getString(0))
                    assertEquals("training", cursor.getString(1))
                }
                db.rawQuery(
                    "SELECT session_type,session_kind FROM active_session_draft WHERE id=1",
                    null,
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    assertEquals("training", cursor.getString(0))
                    assertEquals("training", cursor.getString(1))
                }
            }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
