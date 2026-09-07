package com.labfytools.trainlog.ui

import android.content.Context
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.v2.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
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
    fun resumeRestoresRawFormAfterActivityRecreation() {
        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertIsDisplayed()
            .performClick()
        compose.onNodeWithText(
            "4,5,6,"
        ).performScrollTo()
            .assertIsDisplayed()

        compose.activityRule.scenario.recreate()

        compose.onNodeWithText(
            "Reprendre la séance en cours"
        ).assertIsDisplayed()
            .performClick()
        compose.onNodeWithText(
            "4,5,6,"
        ).performScrollTo()
            .assertIsDisplayed()
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
