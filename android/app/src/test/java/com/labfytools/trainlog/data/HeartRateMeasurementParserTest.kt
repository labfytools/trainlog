package com.labfytools.trainlog.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class HeartRateMeasurementParserTest {
    private fun parsed(vararg bytes: Int): ParsedHeartRateMeasurement {
        val result =
            HeartRateMeasurementParser.parse(
                ByteArray(bytes.size) { index -> bytes[index].toByte() },
            )
        return (result as HeartRateMeasurementParseResult.Parsed).measurement
    }

    private fun invalid(vararg bytes: Int): HeartRateMeasurementParseResult.Invalid =
        HeartRateMeasurementParser.parse(
            ByteArray(bytes.size) { index -> bytes[index].toByte() },
        ) as HeartRateMeasurementParseResult.Invalid

    @Test
    fun parsesEightBitBpm() {
        val value = parsed(0x00, 72)
        assertEquals(72, value.bpm)
        assertNull(value.sensorContactDetected)
        assertNull(value.energyExpended)
        assertTrue(value.rrIntervals1024.isEmpty())
    }

    @Test
    fun parsesSixteenBitBpmLittleEndian() {
        assertEquals(300, parsed(0x01, 0x2c, 0x01).bpm)
        assertEquals(65535, parsed(0x01, 0xff, 0xff).bpm)
    }

    @Test
    fun contactIsNullableAndOnlyMeaningfulWhenSupported() {
        assertNull(parsed(0x02, 80).sensorContactDetected)
        assertEquals(false, parsed(0x04, 80).sensorContactDetected)
        assertEquals(true, parsed(0x06, 80).sensorContactDetected)
    }

    @Test
    fun parsesEnergyExpended() {
        val value = parsed(0x08, 90, 0x34, 0x12)
        assertEquals(0x1234, value.energyExpended)
    }

    @Test
    fun parsesOneAndMultipleRawRrIntervals() {
        assertEquals(listOf(1024), parsed(0x10, 60, 0x00, 0x04).rrIntervals1024)
        assertEquals(
            listOf(1024, 1000, 65535),
            parsed(0x10, 60, 0x00, 0x04, 0xe8, 0x03, 0xff, 0xff).rrIntervals1024,
        )
    }

    @Test
    fun parsesCombinedKnownFlagsInWireOrder() {
        val value =
            parsed(
                0x1f,
                0x96,
                0x00,
                0x07,
                0x00,
                0xf0,
                0x03,
                0x00,
                0x04,
            )
        assertEquals(150, value.bpm)
        assertEquals(true, value.sensorContactDetected)
        assertEquals(7, value.energyExpended)
        assertEquals(listOf(1008, 1024), value.rrIntervals1024)
    }

    @Test
    fun rejectsTruncatedAndIncoherentPayloads() {
        assertEquals("missing flags", invalid().reason)
        assertEquals("truncated 16-bit BPM", invalid(0x01, 0x2c).reason)
        assertEquals("truncated energy-expended value", invalid(0x08, 70, 0x01).reason)
        assertEquals("RR flag has no interval", invalid(0x10, 70).reason)
        assertEquals("truncated RR interval", invalid(0x10, 70, 0x01).reason)
        assertEquals("unexpected trailing bytes", invalid(0x00, 70, 0x01).reason)
        assertEquals("reserved flags are set", invalid(0x20, 70).reason)
    }

    @Test
    fun preservesBoundaryBpmValues() {
        assertEquals(0, parsed(0x00, 0).bpm)
        assertEquals(255, parsed(0x00, 255).bpm)
    }
}
