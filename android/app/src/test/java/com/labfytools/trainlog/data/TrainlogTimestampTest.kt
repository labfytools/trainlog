package com.labfytools.trainlog.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TrainlogTimestampTest {
    @Test fun acceptsSettledGrammarAndComparesEveryFractionDigit() {
        listOf(
            "0001-01-01T00:00Z", "2000-02-29t23:59:59.1z",
            "2026-09-05T18:34+23:59", "2026-09-05T18:34:12-00:00",
            "9999-12-31T23:59:59.9-23:59",
        ).forEach { assertNotNull(it, TrainlogTimestamp.parse(it)) }
        val low = TrainlogTimestamp.parse("2026-01-01T00:00:00.12345678901234567890Z")!!
        val high = TrainlogTimestamp.parse("2026-01-01T00:00:00.12345678901234567891Z")!!
        val equal = TrainlogTimestamp.parse("2026-01-01T00:00:00.123456789012345678900Z")!!
        assertTrue(low < high)
        assertEquals(0, low.compareTo(equal))
    }

    @Test fun rejectsPlatformOnlyAndOutOfRangeForms() {
        listOf(
            "0000-01-01T00:00:00Z", "2026-02-29T00:00:00Z",
            "2026-09-05 18:34:12+02:00", "20260905T183412+0200",
            "2026-W36-5T18:34:12+02:00", "2026-09-05T18:34:12,5+02:00",
            "2026-09-05T18:34:12+0200", "2026-09-05T18:34:12+02",
            "2026-09-05T18:34.5Z", "2026-09-05T18:34:60Z",
            "2026-09-05T18:34:12+24:00",
        ).forEach { assertNull(it, TrainlogTimestamp.parse(it)) }
    }
}
