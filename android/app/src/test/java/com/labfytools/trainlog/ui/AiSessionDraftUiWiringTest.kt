/*
 * Regression coverage for AiSessionDraftUiWiringTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.ui

import android.content.Context
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextClearance
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.test.performKeyInput
import androidx.compose.ui.test.pressKey
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.click
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.AiSessionDraftImportResult
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.SessionLoadMode
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
        compose.onNodeWithText("0 séance préparée", substring = true).assertIsDisplayed()
    }

    @Test
    fun sessionsHubShowsPendingCountAndPreservesManualAndHistoryActions() {
        var manualCalls = 0
        var draftCalls = 0
        var historyCalls = 0
        compose.setContent { TrainlogTheme {
            SessionsHub(null, 2, {}, { manualCalls++ }, { draftCalls++ }, { historyCalls++ })
        } }

        compose.onNodeWithText("2 séances préparées", substring = true).assertIsDisplayed()
        compose.onNodeWithText("Nouvelle séance manuelle").performClick()
        compose.onNodeWithText("Brouillons").performClick()
        compose.onNodeWithText("Séances effectuées").performClick()
        compose.runOnIdle {
            assertEquals(1, manualCalls)
            assertEquals(1, draftCalls)
            assertEquals(1, historyCalls)
        }
    }

    @Test
    fun sessionsHubKeepsPrepareActionsTogetherAndHistoryBelow() {
        compose.setContent { TrainlogTheme { SessionsHub(null, 2, {}, {}, {}, {}) } }

        val manual = compose.onNodeWithTag("sessions-manual-action").fetchSemanticsNode().boundsInRoot
        val drafts = compose.onNodeWithTag("sessions-drafts-action").fetchSemanticsNode().boundsInRoot
        val history = compose.onNodeWithTag("sessions-history-action").fetchSemanticsNode().boundsInRoot
        assertEquals(manual.top, drafts.top, 1f)
        assertEquals(manual.height, drafts.height, 1f)
        assertEquals(manual.width, drafts.width, 1f)
        assertTrue(history.top > manual.bottom)
    }

    @Test
    fun activeSessionActionsAreEqualHeightButtonsAndKeepCreateCallback() {
        assertEquals(ActiveDraftMutationResult.Saved, repository.startActiveSessionDraft())
        var createCalls = 0
        compose.setContent { TrainlogTheme {
            SessionScreen(repository, 0, {}, { createCalls++ }, {})
        } }

        val create = compose.onNodeWithTag("active-session-create-exercise")
            .fetchSemanticsNode().boundsInRoot
        val save = compose.onNodeWithTag("active-session-save")
            .fetchSemanticsNode().boundsInRoot
        assertEquals(create.top, save.top, 1f)
        assertEquals(create.height, save.height, 1f)
        assertEquals(create.width, save.width, 1f)
        compose.onNodeWithTag("active-session-create-exercise")
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.runOnIdle { assertEquals(1, createCalls) }
    }

    @Test
    fun modifyingTargetOnlyOccurrenceRendersItsPrefilledEditor() {
        val walk = createExercise("Marche ciblée", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        val press = createExercise("Presse ciblée", RecordingMode.SETS, TrackingMode.REPS)
        val walkEntry = SessionExerciseDraft(
            entryId = "spe_11111111-1111-4111-8111-111111111111",
            exercise = walk,
            plan = SessionExercisePlan(sets = 0, durationSeconds = 600),
        )
        val pressEntry = SessionExerciseDraft(
            entryId = "spe_22222222-2222-4222-8222-222222222222",
            exercise = press,
            plan = SessionExercisePlan(
                sets = 3,
                reps = 10,
                weightKg = 52.0,
                loadMode = SessionLoadMode.EXTERNAL,
                restSeconds = 90,
            ),
        )
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repository.saveActiveSessionDraft(
                ActiveSessionDraft(exercises = listOf(walkEntry, pressEntry)),
            ),
        )
        val loaded = (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repository.saveActiveSessionDraft(
                loaded.copy(form = formForExistingExercise(loaded.exercises[0], 0)),
            ),
        )
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repository.saveActiveSessionDraft(loaded),
        )
        compose.setContent {
            TrainlogTheme { SessionScreen(repository, 0, {}, {}, {}) }
        }

        compose.onNodeWithTag("edit-draft-exercise-${walkEntry.entryId}")
            .performSemanticsAction(SemanticsActions.OnClick)

        compose.runOnIdle {
            val active = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(walk.exerciseId, active.draft.form.selectedExercise?.exerciseId)
        }
        compose.onNodeWithText("SAISIE — Marche ciblée").assertExists()
        compose.onNodeWithTag("session-continuous-duration").assertExists()
        compose.onNodeWithText("Ajouter à la séance").assertExists()

        compose.onNodeWithTag("session-continuous-duration").performTextClearance()
        compose.onNodeWithTag("session-continuous-duration").performTextInput("12")
        compose.onNodeWithTag("add-to-session")
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.runOnIdle {
            val active = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(2, active.draft.exercises.size)
            assertEquals(walkEntry.entryId, active.draft.exercises[0].entryId)
            assertEquals(720, active.draft.exercises[0].continuousDurationSeconds)
            assertEquals(600, active.draft.exercises[0].plan?.durationSeconds)
            assertEquals(pressEntry, active.draft.exercises[1])
        }

        compose.onNodeWithTag("edit-draft-exercise-${pressEntry.entryId}")
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.onNodeWithText("SAISIE — Presse ciblée").assertExists()
        compose.onNodeWithTag("cancel-session-exercise-editor")
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.runOnIdle {
            val active = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
            assertEquals(720, active.draft.exercises[0].continuousDurationSeconds)
            assertEquals(pressEntry, active.draft.exercises[1])
            assertEquals(null, active.draft.form.selectedExercise)
        }
    }

    @Test
    fun draftScreenShowsDosBicepsAndTargetDetails() {
        importDraft()

        compose.setContent { TrainlogTheme {
            AiSessionDraftsScreen(repository, externalRevision = 0, onStarted = {})
        } }

        compose.onNodeWithText("Dos + biceps").assertIsDisplayed()
        compose.onNodeWithText("Tractions supination").assertIsDisplayed()
        compose.onNodeWithText("3 × 10 · repos 90 s").assertIsDisplayed()
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

        compose.onNodeWithText("Supprimer").assertDoesNotExist()
        compose.onNodeWithContentDescription("Supprimer ce brouillon").performClick()

        compose.onNodeWithText("Supprimer ce brouillon ?").assertIsDisplayed()
        compose.onNodeWithTag("confirm-destructive-delete").assertIsDisplayed()
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

        compose.onNodeWithContentDescription("Supprimer ce brouillon").performClick()
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

        compose.onNodeWithContentDescription("Supprimer ce brouillon").performClick()
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

        compose.onNodeWithContentDescription("Supprimer ce brouillon").performClick()
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

        compose.onNodeWithContentDescription("Supprimer ce brouillon").performClick()
        compose.onNodeWithText("Supprimer ce brouillon ?").assertIsDisplayed()
        compose.onNodeWithTag("confirm-destructive-delete").performClick()

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

    private fun createExercise(
        name: String,
        recordingMode: RecordingMode,
        trackingMode: TrackingMode,
    ) =
        (repository.createExercise(
            NewExerciseProfile(name, recordingMode, trackingMode, 0),
        ) as CreateExerciseResult.Created).exercise

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
