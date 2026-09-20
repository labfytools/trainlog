package com.labfytools.trainlog.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.width
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.junit4.v2.createComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class BodyMetricGridTest {
    @get:Rule val compose = createComposeRule()

    private val items = listOf(
        BodyMetricItem("POIDS", "kg", "", {}),
        BodyMetricItem("COU", "cm", "", {}),
        BodyMetricItem("TOUR DE TAILLE", "cm", "", {}),
        BodyMetricItem("HANCHES", "cm", "", {}),
    )

    @Test
    fun deployedPhoneContentWidthUsesTwoColumns() {
        // 1080 px / 480 dpi = 360 dp; the screen frame leaves about 312 dp.
        assertEquals(2, bodyMetricColumnCount(312f, 0.9f))
    }

    @Test
    fun genuinelyNarrowWidthStacksFields() {
        assertEquals(1, bodyMetricColumnCount(280f, 1f))
    }

    @Test
    fun largeAccessibilityTextStacksFields() {
        assertEquals(1, bodyMetricColumnCount(312f, 1.5f))
    }

    @Test
    fun deployedPhoneWidthRendersPairedFieldsWithoutClippingUnits() {
        compose.setContent {
            TrainlogTheme {
                Column(Modifier.width(312.dp)) { BodyMetricGrid(items) }
            }
        }
        val weight = compose.onNodeWithText("POIDS").fetchSemanticsNode().boundsInRoot
        val neck = compose.onNodeWithText("COU").fetchSemanticsNode().boundsInRoot
        assertTrue(neck.left > weight.left)
        assertTrue(kotlin.math.abs(neck.top - weight.top) < 2f)
        compose.onNodeWithText("kg").fetchSemanticsNode()
        assertTrue(compose.onAllNodesWithText("cm", useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty())
        compose.onNodeWithText("TOUR DE TAILLE").fetchSemanticsNode()
    }

    @Test
    fun trulyNarrowLayoutStacksPairedFields() {
        compose.setContent {
            TrainlogTheme {
                Column(Modifier.width(280.dp)) { BodyMetricGrid(items.take(2)) }
            }
        }
        val weight = compose.onNodeWithText("POIDS").fetchSemanticsNode().boundsInRoot
        val neck = compose.onNodeWithText("COU").fetchSemanticsNode().boundsInRoot
        assertTrue(neck.top > weight.top)
    }
}
