package com.labfytools.trainlog.ui

import android.content.Context
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.hasClickAction
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.semantics.SemanticsActions
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.BodyZoneRecentExposure
import com.labfytools.trainlog.data.ExposureWindowSummary
import com.labfytools.trainlog.data.GenerationWarningLevel
import com.labfytools.trainlog.data.SessionGenerationPreview
import com.labfytools.trainlog.data.SessionGenerationPreviewExercise
import com.labfytools.trainlog.data.SessionGenerationRequest
import com.labfytools.trainlog.data.TrainingRecencyWarning
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import java.security.MessageDigest
import java.util.UUID
import org.junit.After
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class TrainlogAppNavigationCallbackTest {
    @Suppress("DEPRECATION")
    @get:Rule val compose = createComposeRule()

    private lateinit var context: Context
    private lateinit var databaseName: String
    private lateinit var repository: TrainlogRepository
    private lateinit var appState: TrainlogAppState

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "app-navigation-callback-${UUID.randomUUID()}.db"
        repository = TrainlogRepository(context, databaseName)
        assertEquals(ActiveDraftMutationResult.Saved, repository.startActiveSessionDraft())
        appState = TrainlogAppState().also {
            it.navigation.open(AppRoute.SessionGenerator)
            it.generator.preview.value = preview()
            it.generator.warningAcknowledged.value = true
            it.generator.setsText.value = "raw sets retained"
        }
    }

    @After fun tearDown() {
        repository.close()
        context.deleteDatabase(databaseName)
    }

    @Test fun existingDraftAcceptUsesProductionGuardAndRetainsPreviewUntilResolution() {
        compose.setContent {
            TrainlogTheme {
                TrainlogApp(
                    repository,
                    SyncExporter(context, repository),
                    SyncCatalogInbox(context, repository),
                    SyncRequestOutbox(context),
                    appState,
                )
            }
        }
        compose.waitForIdle()
        val before = databaseDigest()

        // CONTRACT: exercise the callback installed by TrainlogApp itself; a
        // controller-only call would not detect a root-composition bypass.
        compose.onNode(
            hasText("Accepter et saisir les valeurs réelles") and hasClickAction(),
        ).performSemanticsAction(SemanticsActions.OnClick)
        compose.waitUntil(5_000) { appState.navigationController.hasPendingNavigation }

        assertEquals(AppRoute.SessionGenerator, appState.navigation.route)
        assertTrue(appState.navigationController.hasPendingNavigation)
        assertNotNull(appState.generator.preview.value)
        assertEquals("raw sets retained", appState.generator.setsText.value)
        assertArrayEquals(before, databaseDigest())

        compose.onNodeWithText("Conserver et quitter")
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.waitForIdle()
        assertEquals(AppRoute.Sessions, appState.navigation.route)
        assertNotNull(appState.generator.preview.value)
        assertEquals("raw sets retained", appState.generator.setsText.value)
        assertFalse(appState.navigationController.hasPendingNavigation)
        assertArrayEquals(before, databaseDigest())
    }

    private fun databaseDigest(): ByteArray = MessageDigest.getInstance("SHA-256")
        .digest(context.getDatabasePath(databaseName).readBytes())

    private fun preview(): SessionGenerationPreview {
        val empty = ExposureWindowSummary(0, 0, 0, emptyList())
        return SessionGenerationPreview(
            SessionGenerationRequest("full_body", "general", 30, "2026-09-10T12:00:00Z"),
            listOf(
                SessionGenerationPreviewExercise(
                    exerciseId = "fixture_exercise",
                    exerciseName = "Exercice de contrôle",
                    equipmentId = "fixture_equipment",
                    equipmentName = "Équipement de contrôle",
                    primaryZoneId = "full_body",
                    primaryZoneName = "Corps entier",
                    patternIds = emptyList(),
                    patternNames = emptyList(),
                    plan = SessionExercisePlan(2, reps = 10, restSeconds = 90),
                    estimatedSeconds = 300,
                    recency = TrainingRecencyWarning(false, false),
                    rationaleCodes = emptyList(),
                    loadSourceSessionId = null,
                    loadSourceOccurrenceId = null,
                    loadSourceStartedAt = null,
                ),
            ),
            300,
            false,
            BodyZoneRecentExposure(
                empty, empty, false, false, GenerationWarningLevel.NONE,
                null, null, null, emptyList(), 0,
            ),
        )
    }
}
