package com.labfytools.trainlog.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import com.labfytools.trainlog.data.StatisticsFrequency
import com.labfytools.trainlog.data.StatisticsOverview
import com.labfytools.trainlog.data.StatisticsPeriod
import com.labfytools.trainlog.data.StatisticsPoint
import com.labfytools.trainlog.data.StatisticsSeries
import com.labfytools.trainlog.data.StatisticsSummary
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import java.time.LocalDate
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class StatisticsScreenPresentationTest {
    @Suppress("DEPRECATION")
    @get:Rule val compose = createComposeRule()

    @Test fun frequencyPresentationPadsSixIsoWeeksWithoutAddingMaxima() {
        val weeks = sixWeekFrequency(
            listOf(
                StatisticsFrequency("2026-W36", 2, 1),
                StatisticsFrequency("2026-W37", 3, 1),
            ),
            LocalDate.of(2026, 9, 11),
        )

        assertEquals(6, weeks.size)
        assertEquals(DashboardWeek("2026-W32", 0, 0), weeks.first())
        assertEquals(DashboardWeek("2026-W37", 3, 1), weeks.last())
        assertEquals(5, weeks.sumOf { it.sessions })
    }

    @Test fun emptyProgressAndSingleMeasurementRemainExplicitAndNonSynthetic() {
        val overview = overview(
            body = listOf(
                StatisticsSeries(
                    "body_weight_kg", "Poids", "kg",
                    listOf(StatisticsPoint("2026-09-11T08:00:00Z", 82.5)),
                ),
            ),
        )
        compose.setContent {
            TrainlogTheme {
                Column {
                    PerformanceCard(overview) {}
                    MeasurementsCard(overview) {}
                }
            }
        }

        compose.onNodeWithContentDescription(
            "Progression sur six semaines : aucun événement comparable, six points vides",
        ).fetchSemanticsNode()
        compose.onNodeWithText("Données insuffisantes pour établir une progression comparable.").fetchSemanticsNode()
        compose.onNodeWithText("2 MAX explicite(s) · ouvrir le détail").fetchSemanticsNode()
        compose.onNodeWithText("82.5 kg").fetchSemanticsNode()
        compose.onNodeWithText("1 relevé · tendance indisponible").fetchSemanticsNode()
        assertTrue(compose.onAllNodesWithText("Δ", substring = true).fetchSemanticsNodes().isEmpty())
    }

    @Test fun frequencyChartExposesSixBucketsAndCurrentWeekSummary() {
        val overview = overview(
            frequency = listOf(
                StatisticsFrequency("2026-W36", 2, 1),
                StatisticsFrequency("2026-W37", 3, 1),
            ),
        )
        compose.setContent {
            TrainlogTheme {
                FrequencyCard(overview, LocalDate.of(2026, 9, 11)) {}
            }
        }

        compose.onNodeWithContentDescription("Fréquence sur six semaines", substring = true).fetchSemanticsNode()
        compose.onNodeWithText("Semaine actuelle · 3 séance(s) · 1 MAX inclus").fetchSemanticsNode()
        compose.onNodeWithText(
            "Les MAX signalent des séances; ils ne sont jamais additionnés une seconde fois.",
        ).fetchSemanticsNode()
    }

    private fun overview(
        body: List<StatisticsSeries> = emptyList(),
        frequency: List<StatisticsFrequency> = emptyList(),
    ) = StatisticsOverview(
        period = StatisticsPeriod.DAYS_30,
        hasInvalidData = false,
        summary = StatisticsSummary(5, 8, 3, 2),
        performanceEvents = emptyList(),
        performance = emptyList(),
        body = body,
        frequency = frequency,
        sessionsLast7Days = 3,
        sessionsLast30Days = 5,
    )
}
