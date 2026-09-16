package com.labfytools.trainlog.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class FieldInputRefinementTest {
    @Test fun `one continuous delta targets distant list positions`() {
        assertEquals(1, reorderTargetIndex(origin = 5, totalDragDistancePx = -4f * 48f,
            rowStepPx = 48f, itemCount = 6))
        assertEquals(5, reorderTargetIndex(origin = 0, totalDragDistancePx = 20f * 48f,
            rowStepPx = 48f, itemCount = 6))
        assertEquals(0, reorderTargetIndex(origin = 5, totalDragDistancePx = -20f * 48f,
            rowStepPx = 48f, itemCount = 6))

        val ids = listOf("A", "B", "C", "D", "E", "F")
        assertEquals(listOf("A", "F", "B", "C", "D", "E"),
            ids.toMutableList().apply { add(1, removeAt(5)) })
        /* A canceled gesture restores its stable origin permutation. */
        val moved = ids.toMutableList().apply { add(1, removeAt(5)) }
        assertEquals(ids, moved.toMutableList().apply { add(5, removeAt(1)) })
    }

    @Test fun `decimal editor accepts both separators and incomplete typing states`() {
        listOf("0.3", "0,3", "0.25", "0,25", "1", "1.0", "1,0",
            "0.", "0,", ".3", ",3", "").forEach {
            assertTrue("editor rejected $it", decimalEditorTextValid(it))
        }
        assertFalse(decimalEditorTextValid("1.2.3"))
        assertFalse(decimalEditorTextValid("1,2,3"))
        assertFalse(decimalEditorTextValid("NaN"))
        assertFalse(decimalEditorTextValid("inf"))
    }

    @Test fun `final decimal parse is locale independent finite and sign checked by domain`() {
        mapOf("0.3" to 0.3, "0,3" to 0.3, "0.25" to 0.25, "0,25" to 0.25,
            "1" to 1.0, "1.0" to 1.0, "1,0" to 1.0, ".3" to 0.3, ",3" to 0.3)
            .forEach { (raw, expected) -> assertEquals(expected, parseFiniteDecimal(raw)!!, 0.0) }
        listOf("", ".", ",", "NaN", "inf").forEach { assertNull(parseFiniteDecimal(it)) }
        assertTrue(parseFiniteDecimal("-0.3")!! < 0.0)
    }
}
