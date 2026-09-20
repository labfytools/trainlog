/*
 * Regression coverage for AndroidUiRedesignTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.ui

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.hasClickAction
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.semantics.SemanticsActions
import com.labfytools.trainlog.data.BodyZoneHomeOverview
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidUiRedesignTest {
    @Suppress("DEPRECATION")
    @get:Rule val compose = createComposeRule()

    @Test
    fun homeQuickActionsShareARowAndKeepTheirCallbacks() {
        var syncCalls = 0
        var bodyCalls = 0
        compose.setContent {
            TrainlogTheme {
                HomeScreen(
                    activeDraft = null,
                    draftError = null,
                    bodyZoneOverview = BodyZoneHomeOverview(emptyList(), emptyList(), false, false),
                    onSession = {},
                    onOpenExercises = {},
                    onBody = { bodyCalls++ },
                    onSync = { syncCalls++ },
                )
            }
        }

        val sync = compose.onNodeWithTag("home-sync-action").fetchSemanticsNode().boundsInRoot
        val body = compose.onNodeWithTag("home-body-action").fetchSemanticsNode().boundsInRoot
        assertEquals(sync.top, body.top, 1f)
        assertEquals(sync.height, body.height, 1f)
        assertEquals(sync.width, body.width, 1f)
        compose.onNode(hasText("Synchronisation") and hasClickAction())
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.onNode(hasText("Mensurations") and hasClickAction())
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.runOnIdle {
            assertEquals(1, syncCalls)
            assertEquals(1, bodyCalls)
        }
    }
}
