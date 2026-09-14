package com.labfytools.trainlog.ui

import android.content.Context
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.test.performKeyInput
import androidx.compose.ui.test.pressKey
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.click
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.AiSessionDraftImportResult
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject
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
class AiSessionDraftUiWiringTest {
    @Suppress("DEPRECATION")
    @get:Rule val compose = createComposeRule()
    private lateinit var context: Context
    private lateinit var databaseName: String
    private lateinit var repository: TrainlogRepository

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "ai-draft-ui-${UUID.randomUUID()}.db"
        repository = TrainlogRepository(context, databaseName)
    }

    @After
    fun tearDown() {
        repository.close()
        context.deleteDatabase(databaseName)
    }

    @Test
    fun sessionsHubAlwaysExposesBrouillonsAndZeroCount() {
        compose.setContent { TrainlogTheme {
            SessionsHub(null, 0, {}, {}, {}, {})
        } }

        compose.onNodeWithText("Brouillons").assertIsDisplayed()
        compose.onNodeWithText("0 séance(s) préparée(s)", substring = true).assertIsDisplayed()
    }

    @Test
    fun sessionsHubShowsPendingCountAndPreservesManualAndHistoryActions() {
        var manualCalls = 0
        var historyCalls = 0
        compose.setContent { TrainlogTheme {
            SessionsHub(null, 2, {}, { manualCalls++ }, {}, { historyCalls++ })
        } }

        compose.onNodeWithText("2 séance(s) préparée(s)", substring = true).assertIsDisplayed()
        compose.onNodeWithText("Nouvelle séance manuelle").performClick()
        compose.onNodeWithText("Séances effectuées").performClick()
        compose.runOnIdle {
            assertEquals(1, manualCalls)
            assertEquals(1, historyCalls)
        }
    }

    @Test
    fun draftScreenShowsDosBicepsAndTargetDetails() {
        importDraft()

        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision = 0, onStarted = {})
        } }

        compose.onNodeWithText("Dos + biceps").assertIsDisplayed()
        compose.onNodeWithText("Tractions supination · 3 × 10 répétitions · repos 90 s")
            .assertIsDisplayed()
        compose.onNodeWithText("Accent sur le dos et les biceps.").assertIsDisplayed()
    }

    @Test
    fun companionImportAppearsAfterExternalRevisionChanges() {
        val externalRevision = mutableIntStateOf(0)
        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision.intValue, onStarted = {})
        } }
        compose.onNodeWithText("Aucun brouillon importé.").assertIsDisplayed()

        importDraft()
        compose.runOnIdle { externalRevision.intValue++ }

        compose.onNodeWithText("Dos + biceps").assertIsDisplayed()
    }

    @Test
    fun renderingPendingDraftDoesNotCreateActiveSession() {
        importDraft()

        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision = 0, onStarted = {})
        } }

        compose.onNodeWithText("Dos + biceps").assertIsDisplayed()
        assertTrue(repository.loadActiveSessionDraft() is ActiveDraftLoadResult.None)
    }

    @Test
    fun startActionUsesRepositoryTransactionAndNotifiesOwner() {
        importDraft()
        var pendingChanges = 0
        var starts = 0
        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(
                repository,
                externalRevision = 0,
                onPendingChanged = { pendingChanges++ },
                onStarted = { starts++ },
            )
        } }

        compose.onNodeWithText("Démarrer").performClick()

        compose.runOnIdle {
            assertEquals(1, pendingChanges)
            assertEquals(1, starts)
            val active = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(3, active.draft.exercises.single().plan?.sets)
            assertEquals(10, active.draft.exercises.single().plan?.reps)
            assertTrue(active.draft.exercises.single().sets.isEmpty())
            assertTrue(repository.listAiSessionDrafts().isEmpty())
        }
    }

    @Test
    fun deleteActionOnlyOpensConfirmationWithoutMutation() {
        importDraft()
        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision = 0, onStarted = {})
        } }

        compose.onNodeWithText("Supprimer").performClick()

        compose.onNodeWithText("Supprimer ce brouillon ?").assertIsDisplayed()
        compose.onNodeWithText("Le brouillon « Dos + biceps » sera supprimé.", substring = true)
            .assertIsDisplayed()
        compose.runOnIdle { assertEquals(1, repository.listAiSessionDrafts().size) }
    }

    @Test
    fun cancelDeleteKeepsPendingDraft() {
        importDraft()
        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision = 0, onStarted = {})
        } }

        compose.onNodeWithText("Supprimer").performClick()
        compose.onNodeWithText("Annuler").performClick()

        compose.onNodeWithText("Dos + biceps").assertIsDisplayed()
        compose.runOnIdle { assertEquals(1, repository.listAiSessionDrafts().size) }
    }

    @Test
    fun backDismissesDeleteAndKeepsPendingDraft() {
        importDraft()
        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision = 0, onStarted = {})
        } }

        compose.onNodeWithText("Supprimer").performClick()
        compose.onNodeWithText("Supprimer ce brouillon ?").performKeyInput {
            pressKey(Key.Back)
        }

        compose.onNodeWithText("Dos + biceps").assertIsDisplayed()
        compose.runOnIdle { assertEquals(1, repository.listAiSessionDrafts().size) }
    }

    @Test
    fun outsideTapDismissesDeleteAndKeepsPendingDraft() {
        importDraft()
        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision = 0, onStarted = {})
        } }

        compose.onNodeWithText("Supprimer").performClick()
        compose.onNodeWithText("Supprimer ce brouillon ?").performTouchInput {
            click(Offset(-40f, -40f))
        }

        compose.onNodeWithText("Dos + biceps").assertIsDisplayed()
        compose.runOnIdle { assertEquals(1, repository.listAiSessionDrafts().size) }
    }

    @Test
    fun confirmedDeleteCreatesTombstoneAndRemovesPendingDraft() {
        val payload = importDraft()
        var pendingChanges = 0
        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(
                repository,
                externalRevision = 0,
                onPendingChanged = { pendingChanges++ },
                onStarted = {},
            )
        } }

        compose.onNodeWithText("Supprimer").performClick()
        compose.onNodeWithText("Supprimer ce brouillon ?").assertIsDisplayed()
        compose.onAllNodesWithText("Supprimer")[1].performClick()

        compose.onNodeWithText("Aucun brouillon importé.").assertIsDisplayed()
        compose.runOnIdle {
            assertEquals(1, pendingChanges)
            assertTrue(repository.listAiSessionDrafts().isEmpty())
            assertTrue(repository.applyAiSessionDraftsJson(payload) is AiSessionDraftImportResult.Applied)
            assertTrue(repository.listAiSessionDrafts().isEmpty())
        }
    }

    private fun importDraft(): String {
        val exercise = (repository.createExercise(NewExerciseProfile(
            name = "Tractions supination",
            recordingMode = RecordingMode.SETS,
            trackingMode = TrackingMode.REPS,
            dataFields = 0,
        )) as CreateExerciseResult.Created).exercise
        val payload = artifact(exercise.exerciseId)
        assertTrue(repository.applyAiSessionDraftsJson(payload)
            is AiSessionDraftImportResult.Applied)
        return payload
    }

    private fun artifact(exerciseId: String): String {
        val target = JSONObject().put("sets", 3).put("reps", 10)
            .put("duration_seconds", JSONObject.NULL).put("weight_kg", JSONObject.NULL)
        val entry = JSONObject()
            .put("entry_id", "sxe_abcdefab-cdef-4abc-8abc-abcdefabcdef")
            .put("position", 0).put("exercise_id", exerciseId)
            .put("recording_mode", "sets").put("tracking_mode", "reps")
            .put("data_fields", 0).put("equipment_id", JSONObject.NULL)
            .put("load_mode", "none").put("rest_seconds", 90).put("target", target)
        val draft = JSONObject()
            .put("draft_id", "aid_12345678-1234-4abc-8abc-123456789abc")
            .put("created_at", "2026-09-14T08:00:00Z").put("planned_for", "2026-09-15")
            .put("session_type", "training").put("title", "Dos + biceps")
            .put("notes", "Accent sur le dos et les biceps.").put("entries", JSONArray().put(entry))
        return JSONObject().put("format", "trainlog-ai-session-drafts").put("version", 1)
            .put("generated_at", "2026-09-14T08:00:00Z").put("drafts", JSONArray().put(draft))
            .toString()
    }
}
