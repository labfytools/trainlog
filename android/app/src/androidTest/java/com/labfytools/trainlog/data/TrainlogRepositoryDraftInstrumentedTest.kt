package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class TrainlogRepositoryDraftInstrumentedTest {
    private lateinit var context: Context
    private lateinit var databaseName: String
    private var repository: TrainlogRepository? = null

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        /* INVARIANT: instrumentation never opens the user's production DB. */
        databaseName = "draft-instrumentation-${UUID.randomUUID()}.db"
    }

    @After
    fun tearDown() {
        repository?.close()
        context.deleteDatabase(databaseName)
    }

    @Test
    fun realAndroidSqliteRestoresRawDraftAfterRepositoryRecreation() {
        val first = openRepository()
        val created = first.createExercise(
            NewExerciseProfile(
                name = "Test isolé",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.REPS,
                dataFields = 0,
            )
        ) as CreateExerciseResult.Created
        val expected = ActiveSessionDraft(
            exercises = listOf(
                SessionExerciseDraft(
                    exercise = created.exercise,
                    sets = listOf(3, 4, 5).map { SessionSetDraft(reps = it) },
                )
            ),
            form = SessionDraftForm(
                selectedExercise = created.exercise,
                repsText = "4,5,6,",
            ),
        )
        assertEquals(ActiveDraftMutationResult.Saved, first.saveActiveSessionDraft(expected))
        first.close()
        repository = null

        val restored = openRepository().loadActiveSessionDraft()
        assertTrue(restored is ActiveDraftLoadResult.Loaded)
        restored as ActiveDraftLoadResult.Loaded
        assertEquals(expected.exercises, restored.draft.exercises)
        assertEquals("4,5,6,", restored.draft.form.repsText)
    }

    @Test
    fun isolatedFinalizationCreatesOneExportedSessionAndClearsDraft() {
        val repo = openRepository()
        val created = repo.createExercise(
            NewExerciseProfile(
                name = "Finalisation isolée",
                recordingMode = RecordingMode.SETS,
                trackingMode = TrackingMode.DURATION,
                dataFields = 0,
            )
        ) as CreateExerciseResult.Created
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repo.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = created.exercise,
                            sets = listOf(
                                SessionSetDraft(durationSeconds = 20),
                                SessionSetDraft(durationSeconds = 35),
                            ),
                        )
                    )
                )
            ),
        )
        assertTrue(repo.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved)
        assertEquals(ActiveDraftLoadResult.None, repo.loadActiveSessionDraft())
        assertTrue(repo.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Invalid)
        assertEquals(1, repo.listSessions().size)
        assertEquals(
            1,
            JSONObject(repo.buildMobileExportJson())
                .getJSONArray("sessions")
                .length(),
        )
    }

    private fun openRepository(): TrainlogRepository =
        TrainlogRepository(context, databaseName).also {
            repository = it
        }
}
