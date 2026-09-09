package com.labfytools.trainlog.ui

import android.content.Context
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.junit4.v2.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performScrollToIndex
import androidx.compose.ui.test.performTextClearance
import androidx.compose.ui.test.performTextInput
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.ExternalResource
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SessionDraftUiInstrumentedTest {
    private lateinit var marcheExerciseId: String
    private lateinit var elevationExerciseId: String
    private lateinit var marcheTenExerciseId: String

    private val context: Context =
        ApplicationProvider.getApplicationContext()

    @get:Rule(order = 0)
    val isolatedDatabase =
        object : ExternalResource() {
            override fun before() {
                seedDraft()
            }

            override fun after() {
                context.deleteDatabase(
                    DRAFT_UI_TEST_DATABASE_NAME
                )
            }
        }

    @get:Rule(order = 1)
    val compose =
        createAndroidComposeRule<
            DraftUiTestActivity
        >()

    @Before
    fun confirmIsolatedDatabaseName() {
        assertTrue(
            DRAFT_UI_TEST_DATABASE_NAME !=
                "trainlog-android.db"
        )
    }

    @After
    fun closeAnyTestConnection() {
        /* Activity and rule cleanup own their respective connections. */
    }

    @Test
    fun resumeRestoresSetRowsAfterActivityRecreation() {
        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertIsDisplayed()
            .performClick()
        compose.onNodeWithTag("session-set-0-reps").performScrollTo().assertTextEquals("4")
        compose.onNodeWithTag("session-set-2-reps").performScrollTo().assertTextEquals("6")

        compose.activityRule.scenario.recreate()

        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertIsDisplayed()
            .performClick()
        compose.onNodeWithTag("session-set-0-reps").performScrollTo().assertTextEquals("4")
        compose.onNodeWithTag("session-set-2-reps").performScrollTo().assertTextEquals("6")
        compose.onNodeWithText(
            "Retirer Test UI"
        ).performScrollTo()
            .assertIsDisplayed()
    }

    @Test
    fun confirmedDiscardRemovesResumeWithoutHistory() {
        compose.onNodeWithText(
            "Supprimer la séance en cours"
        ).performClick()
        compose.onNodeWithText(
            "Confirmer la suppression"
        ).assertIsDisplayed()
        compose.onNodeWithText(
            "Annuler"
        ).performClick()
        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertIsDisplayed()

        compose.onNodeWithText(
            "Supprimer la séance en cours"
        ).performClick()
        compose.onNodeWithText(
            "Confirmer la suppression"
        ).assertIsDisplayed()
            .performClick()
        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertDoesNotExist()

        compose.activityRule.scenario.recreate()

        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertDoesNotExist()

        TrainlogRepository(
            context,
            DRAFT_UI_TEST_DATABASE_NAME,
        ).useForTest { repository ->
            assertTrue(repository.listSessions().isEmpty())
        }
    }

    @Test
    fun finalizeReturnsHomeWithOneCompletedSessionAndNoResume() {
        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).performClick()
        compose.onNodeWithText(
            "Enregistrer la séance"
        ).performScrollTo()
            .performClick()
        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertDoesNotExist()

        compose.activityRule.scenario.recreate()

        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertDoesNotExist()

        TrainlogRepository(
            context,
            DRAFT_UI_TEST_DATABASE_NAME,
        ).useForTest { repository ->
            assertEquals(1, repository.listSessions().size)
        }
    }

    @Test
    fun exerciseSearchFiltersPrefixIgnoringCaseAccentsAndExtraSpaces() {
        resumeAndOpenExerciseSearch()

        compose.onNodeWithTag("exercise-search-input")
            .performTextInput("m")
        compose.onNodeWithTag("exercise-search-result-$marcheExerciseId")
            .performScrollTo()

        compose.onNodeWithTag("exercise-search-input")
            .performTextClearance()
        compose.onNodeWithTag("exercise-search-input")
            .performTextInput("  MAR  ")
        compose.onNodeWithTag("exercise-search-result-$marcheExerciseId")
            .performScrollTo()

        compose.onNodeWithTag("exercise-search-input")
            .performTextClearance()
        compose.onNodeWithTag("exercise-search-input")
            .performTextInput("ele")
        compose.onNodeWithTag("exercise-search-result-$elevationExerciseId")
            .performScrollTo()
    }

    @Test
    fun exerciseSearchShowsFullCatalogueAndNoResultState() {
        resumeAndOpenExerciseSearch()

        compose.onNodeWithTag("exercise-search-input")
            .performTextInput("marche")
        compose.onNodeWithTag("exercise-search-results")
            .performScrollToIndex(10)

        compose.onNodeWithTag("exercise-search-result-$marcheTenExerciseId")
            .performScrollTo()

        compose.onNodeWithTag("exercise-search-input")
            .performTextClearance()
        compose.onNodeWithTag("exercise-search-input")
            .performTextInput("inexistant")
        compose.onNodeWithText("Aucun exercice trouvé").assertIsDisplayed()
    }

    @Test
    fun cancellingSearchKeepsTheConfirmedExerciseAndRawValues() {
        resumeAndOpenExerciseSearch()
        compose.onNodeWithTag("exercise-search-input")
            .performTextInput("mar")
        compose.onNodeWithText("Annuler la recherche").performClick()

        compose.onNodeWithTag("exercise-picker-open").assertIsDisplayed()
        compose.onNodeWithTag("session-set-0-reps").performScrollTo().assertTextEquals("4")
        compose.onNodeWithTag("session-set-2-reps").performScrollTo().assertTextEquals("6")
    }

    @Test
    fun rowEditFrenchWeightDeleteAndAddPersistAcrossRecreation() {
        compose.onNodeWithText("Reprendre la séance en cours").performClick()
        compose.onNodeWithTag("session-set-1-reps")
            .performScrollTo().performTextClearance()
        compose.onNodeWithTag("session-set-1-reps").performTextInput("9")
        compose.onNodeWithTag("session-set-1-weight")
            .performScrollTo().performTextInput("32,5")
        compose.onNodeWithText("Supprimer la série 1").performScrollTo().performClick()
        compose.onNodeWithText("Ajouter une série").performScrollTo().performClick()

        compose.onNodeWithTag("session-set-0-reps").performScrollTo().assertTextEquals("9")
        compose.onNodeWithTag("session-set-0-weight").performScrollTo().assertTextEquals("32,5")
        compose.onNodeWithTag("session-set-3-reps").performScrollTo().assertTextEquals("")

        compose.activityRule.scenario.recreate()
        compose.onNodeWithText("Reprendre la séance en cours").performClick()
        compose.onNodeWithTag("session-set-0-reps").performScrollTo().assertTextEquals("9")
        compose.onNodeWithTag("session-set-0-weight").performScrollTo().assertTextEquals("32,5")
        compose.onNodeWithTag("session-set-3-reps").performScrollTo().assertTextEquals("")
    }

    @Test
    fun selectingAnExerciseUsesItsIdentityAndAllowsASecondPassage() {
        resumeAndOpenExerciseSearch()
        selectMarche()
        assertMarcheFormSelected()
        compose.onNodeWithText("Ajouter à la séance")
            .performScrollTo()
            .performClick()

        compose.onNodeWithTag("exercise-picker-open").performClick()
        selectMarche()
        assertMarcheFormSelected()
        compose.onNodeWithText("Ajouter à la séance")
            .performScrollTo()
            .performClick()

        TrainlogRepository(context, DRAFT_UI_TEST_DATABASE_NAME).useForTest { repository ->
            val draft = (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            val marches = draft.exercises.filter { it.exercise.name == "Marche" }
            assertEquals(2, marches.size)
            assertTrue(marches.map { it.entryId }.distinct().size == 2)
        }
    }

    @Test
    fun changingAnEditedEntryReplacesOnlyThatEntry() {
        compose.onNodeWithText("Reprendre la séance en cours").performClick()
        compose.onNodeWithText("Modifier Test UI")
            .performScrollTo()
            .performClick()
        resumeAndOpenExerciseSearch(alreadyResumed = true)
        selectMarche()
        assertMarcheFormSelected()
        compose.onNodeWithText("Ajouter à la séance")
            .performScrollTo()
            .performClick()

        TrainlogRepository(context, DRAFT_UI_TEST_DATABASE_NAME).useForTest { repository ->
            val draft = (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            assertEquals(1, draft.exercises.size)
            assertEquals("Marche", draft.exercises.single().exercise.name)
            assertTrue(draft.exercises.single().entryId.startsWith("sxe_"))
        }
    }

    private fun resumeAndOpenExerciseSearch(alreadyResumed: Boolean = false) {
        if (!alreadyResumed) {
            compose.onNodeWithText("Reprendre la séance en cours").performClick()
        }
        compose.onNodeWithTag("exercise-picker-open")
            .performScrollTo()
            .performClick()
    }

    private fun selectMarche() {
        compose.onNodeWithTag("exercise-search-input")
            .performTextClearance()
        compose.onNodeWithTag("exercise-search-input")
            .performTextInput("marche")
        compose.onNodeWithTag("exercise-search-result-$marcheExerciseId")
            .performScrollTo()
            .performClick()
    }

    private fun assertMarcheFormSelected() {
        compose.waitForIdle()
        compose.onNodeWithText("SAISIE — Marche")
            .performScrollTo()
            .assertIsDisplayed()
    }

    private fun seedDraft() {
        context.deleteDatabase(
            DRAFT_UI_TEST_DATABASE_NAME
        )
        TrainlogRepository(
            context,
            DRAFT_UI_TEST_DATABASE_NAME,
        ).useForTest { repository ->
            val created =
                repository.createExercise(
                    NewExerciseProfile(
                        name = "Test UI",
                        recordingMode =
                            RecordingMode.SETS,
                        trackingMode =
                            TrackingMode.REPS,
                        dataFields = 0,
                    )
                ) as CreateExerciseResult.Created
            marcheExerciseId =
                createExercise(repository, "Marche", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
                    .exerciseId
            elevationExerciseId =
                createExercise(repository, "Élévation", RecordingMode.SETS, TrackingMode.REPS)
                    .exerciseId
            (1..10).forEach { number ->
                val exercise =
                    createExercise(
                        repository,
                        "Marche %02d".format(number),
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                    )
                if (number == 10) {
                    marcheTenExerciseId = exercise.exerciseId
                }
            }
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    ActiveSessionDraft(
                        exercises = listOf(
                            SessionExerciseDraft(
                                exercise = created.exercise,
                                sets = listOf(
                                    SessionSetDraft(reps = 4),
                                    SessionSetDraft(reps = 5),
                                    SessionSetDraft(reps = 6),
                                ),
                            )
                        ),
                        form = SessionDraftForm(
                            selectedExercise =
                                created.exercise,
                            repsText = "4,5,6,",
                        ),
                    )
                ),
            )
        }
    }

    private fun createExercise(
        repository: TrainlogRepository,
        name: String,
        recordingMode: RecordingMode,
        trackingMode: TrackingMode,
    ): ExerciseProfile {
        val result =
            repository.createExercise(
                NewExerciseProfile(
                    name = name,
                    recordingMode = recordingMode,
                    trackingMode = trackingMode,
                    dataFields = 0,
                ),
            )
        assertTrue(result is CreateExerciseResult.Created)
        return (result as CreateExerciseResult.Created).exercise
    }
}

private inline fun TrainlogRepository.useForTest(
    block: (TrainlogRepository) -> Unit,
) {
    try {
        block(this)
    } finally {
        close()
    }
}
