package com.labfytools.trainlog.ui

import android.content.Context
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.FinalizeActiveDraftResult
import com.labfytools.trainlog.data.SaveFeedbackResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import java.util.UUID
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class DraftFeedbackUiReadbackTest {
    @Suppress("DEPRECATION")
    @get:Rule val compose = createComposeRule()
    private lateinit var context: Context
    private lateinit var databaseName: String
    private lateinit var repository: TrainlogRepository

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "draft-feedback-ui-${UUID.randomUUID()}.db"
        repository = TrainlogRepository(context, databaseName)
    }

    @After fun tearDown() {
        repository.close()
        context.deleteDatabase(databaseName)
    }

    @Test fun activeAndCompletedScreensReadBackTheSameFeedback() {
        val exercise = (repository.createExercise(NewExerciseProfile(
            "Tirage synthétique", RecordingMode.SETS, TrackingMode.REPS, 0,
        )) as CreateExerciseResult.Created).exercise
        val entry = SessionExerciseDraft(exercise = exercise, sets = listOf(SessionSetDraft(reps = 8)))
        assertEquals(ActiveDraftMutationResult.Saved,
            repository.saveActiveSessionDraft(ActiveSessionDraft(exercises = listOf(entry))))
        assertTrue(repository.saveDraftExerciseFeedback(
            entry.entryId, "texte UI durable", "2026-09-01T10:00:00+02:00") is SaveFeedbackResult.Saved)

        val completedSessionId = mutableStateOf<String?>(null)
        compose.setContent { TrainlogTheme {
            completedSessionId.value?.let { SessionDetailScreen(repository, it, {}, {}) }
                ?: SessionScreen(repository, 0, {}, {}, {})
        } }
        compose.onNodeWithTag("draft-feedback-${entry.entryId}").fetchSemanticsNode()
        compose.onNodeWithText("Ressenti · texte UI durable").fetchSemanticsNode()

        val saved = repository.finalizeActiveSessionDraft() as FinalizeActiveDraftResult.Saved
        compose.runOnIdle { completedSessionId.value = saved.sessionId }
        compose.onNodeWithText("Pendant la séance · texte UI durable").fetchSemanticsNode()
    }
}
