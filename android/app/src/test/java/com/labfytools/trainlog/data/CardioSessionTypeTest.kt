package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import java.util.UUID
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CardioSessionTypeTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun cardioIsCanonicalLocallyAndExcludedFromFrozenMobileHistory() {
        val name = "cardio-kind-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            val exercise =
                (repository.createExercise(
                    NewExerciseProfile(
                        "Cardio kind exercise",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    ),
                ) as CreateExerciseResult.Created).exercise
            val occurrence =
                SessionExerciseDraft(
                    exercise = exercise,
                    sets = listOf(SessionSetDraft(reps = 1)),
                )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    ActiveSessionDraft(
                        sessionType = SessionType.CARDIO,
                        exercises = listOf(occurrence),
                    ),
                ),
            )
            val loaded =
                repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(SessionType.CARDIO, loaded.draft.sessionType)

            val db = context.openOrCreateDatabase(name, Context.MODE_PRIVATE, null)
            db.rawQuery(
                "SELECT session_type,session_kind FROM active_session_draft WHERE id=1",
                null,
            ).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals("training", cursor.getString(0))
                assertEquals("cardio", cursor.getString(1))
            }
            db.close()

            assertTrue(
                repository.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved,
            )
            assertEquals(
                SessionType.CARDIO,
                repository.listSessions().single().sessionType,
            )

            val frozen = JSONObject(repository.buildMobileExportV4Json())
            assertEquals(0, frozen.getJSONArray("sessions").length())

            val verify = context.openOrCreateDatabase(name, Context.MODE_PRIVATE, null)
            verify.rawQuery(
                "SELECT session_type,session_kind FROM sessions",
                null,
            ).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals("training", cursor.getString(0))
                assertEquals("cardio", cursor.getString(1))
            }
            verify.close()
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
