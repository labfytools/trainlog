package com.labfytools.trainlog.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.assertHasClickAction
import androidx.compose.ui.test.assertIsNotSelected
import androidx.compose.ui.test.assertIsSelected
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.click
import androidx.compose.ui.test.junit4.v2.createComposeRule
import androidx.compose.ui.test.onChildren
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.BodyZoneHomeOverview
import com.labfytools.trainlog.data.BodyZoneHomeState
import com.labfytools.trainlog.data.BodyZoneHomeStatus
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import org.junit.Rule
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class BodyZoneHomeSemanticsTest {
    @get:Rule val compose = createComposeRule()

    @Test fun compactLargeFontMapExposesEveryViewSpecificZoneAndChangesSelection() {
        setSection(widthDp = 288, fontScale = 1.6f)

        compose.onNodeWithContentDescription("Carte corporelle, Face").assertExists()
        compose.onNodeWithContentDescription("Carte corporelle, Dos").assertExists()
        expectedFrontZones.forEach { (id, name) ->
            val state = if (id == "chest") "Priorité relative" else "Travaillé récemment"
            val selection = if (id == "chest") "sélectionnée" else "non sélectionnée"
            compose.onNodeWithContentDescription("Face, $name, $state, $selection")
                .assertExists()
                .assertHasClickAction()
        }
        expectedBackZones.forEach { (_, name) ->
            compose.onNodeWithContentDescription("Dos, $name, Travaillé récemment, non sélectionnée")
                .assertExists()
                .assertHasClickAction()
        }

        compose.onNodeWithContentDescription("Face, Pectoraux, Priorité relative, sélectionnée").assertIsSelected()
        compose.onNodeWithText("Sélection : Pectoraux").assertExists()
        compose.onNodeWithContentDescription("Dos, Fessiers, Travaillé récemment, non sélectionnée")
            .assertIsNotSelected()
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.onNodeWithContentDescription("Dos, Fessiers, Travaillé récemment, sélectionnée")
            .assertIsSelected()
        compose.onNodeWithText("Sélection : Fessiers").assertExists()
        compose.onNodeWithText("Fessiers").assertTextEquals("Fessiers")

        // Anatomical labels are not rendered over either vector: each tagged
        // Canvas is a leaf and textual state/detail remains below the maps.
        compose.onNodeWithTag("body-map-front", useUnmergedTree = true).onChildren().assertCountEquals(0)
        compose.onNodeWithTag("body-map-back", useUnmergedTree = true).onChildren().assertCountEquals(0)
        compose.onNodeWithText("1. Pectoraux · Priorité relative").assertExists()
        compose.onNodeWithText("1 exposition principale sur 7 j").assertExists()
        listOf("fait(s) primaire(s)", "fait(s) secondaire(s)", "secondaire pondéré plus faiblement").forEach {
            compose.onAllNodesWithText(it, substring = true).assertCountEquals(0)
        }
    }

    @Test fun compactExposureInfoOpensAccessibleExplanation() {
        setSection(widthDp = 380, fontScale = 1f)

        compose.onNodeWithText("Exposition récente  ⓘ")
            .assertHasClickAction()
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.onNodeWithText(
            "Ces couleurs décrivent le travail enregistré récemment. Elles ne mesurent pas la récupération physiologique.",
        ).assertExists()
        compose.onNodeWithText("Compris").assertHasClickAction()
    }

    @Test fun wideLayoutKeepsBothMapsSelectionAndCompactPriorities() {
        setSection(widthDp = 720, fontScale = 1f)

        compose.onNodeWithTag("body-map-front").assertExists()
        compose.onNodeWithTag("body-map-back").assertExists()
        compose.onNodeWithText("Priorités").assertExists()
        compose.onNodeWithText("1. Pectoraux · Priorité relative")
            .assertHasClickAction()
            .performSemanticsAction(SemanticsActions.OnClick)
        compose.onNodeWithText("Sélection : Pectoraux").assertExists()
    }

    @Test fun practicalWidthMapSelectionFeedsExistingNavigationActions() {
        var openedZone: String? = null
        var statisticsOpened = false
        setSection(
            widthDp = 380,
            fontScale = 1f,
            onOpenExercises = { openedZone = it },
            onOpenStatistics = { statisticsOpened = true },
        )

        assertEquals("core", bodyZoneAtPoint("front", 180f, 236f, Offset(90f, 113.28f), allZoneIds.toSet()))
        compose.onNodeWithTag("body-map-front")
            .performTouchInput { click(percentOffset(.5f, .48f)) }
        compose.onNodeWithContentDescription("Face, Abdominaux / tronc, Travaillé récemment, sélectionnée")
            .assertIsSelected()
        compose.onNodeWithText("Voir les exercices").assertHasClickAction()
            .performSemanticsAction(SemanticsActions.OnClick)
        assertEquals("core", openedZone)
        compose.onNodeWithText("Voir les statistiques").assertHasClickAction()
            .performSemanticsAction(SemanticsActions.OnClick)
        assertTrue(statisticsOpened)
    }

    private fun setSection(
        widthDp: Int,
        fontScale: Float,
        onOpenExercises: (String) -> Unit = {},
        onOpenStatistics: () -> Unit = {},
    ) {
        val zones = allZoneIds.map { id ->
            status(id, if (id == "chest") BodyZoneHomeState.PRIORITIZE else BodyZoneHomeState.RECENT_WORK)
        }
        compose.setContent {
            TrainlogTheme {
                CompositionLocalProvider(LocalDensity provides Density(1f, fontScale)) {
                    Box(Modifier.width(widthDp.dp)) {
                        BodyZoneHomeSection(
                            BodyZoneHomeOverview(zones, listOf(zones.first()), true, false),
                            onOpenExercises,
                            onOpenStatistics,
                        )
                    }
                }
            }
        }
    }

    private fun status(id: String, state: BodyZoneHomeState) = BodyZoneHomeStatus(
        zoneId = id,
        displayName = mapOf(
            "chest" to "Pectoraux", "back" to "Dos", "shoulders" to "Épaules", "arms" to "Bras",
            "core" to "Abdominaux / tronc", "glutes" to "Fessiers", "thighs" to "Cuisses", "calves" to "Mollets",
        ).getValue(id),
        lastPrimaryExposure = "2026-09-10T12:00:00Z",
        lastSecondaryExposure = null,
        primaryExposureAgeSeconds = 86400L,
        secondaryExposureAgeSeconds = null,
        primaryWork7Days = 1,
        secondaryWork7Days = 0,
        primaryWork30Days = 1,
        secondaryWork30Days = 0,
        recentSessionCount = 1,
        availableExerciseCount = 1,
        state = state,
        reasons = listOf(state.label),
    )

    private companion object {
        val allZoneIds = listOf("chest", "back", "shoulders", "arms", "core", "glutes", "thighs", "calves")
        val expectedFrontZones = listOf(
            "shoulders" to "Épaules",
            "chest" to "Pectoraux",
            "arms" to "Bras",
            "core" to "Abdominaux / tronc",
            "thighs" to "Cuisses",
            "calves" to "Mollets",
        )
        val expectedBackZones = listOf(
            "shoulders" to "Épaules",
            "back" to "Dos",
            "arms" to "Bras",
            "glutes" to "Fessiers",
            "thighs" to "Cuisses",
            "calves" to "Mollets",
        )
    }
}
