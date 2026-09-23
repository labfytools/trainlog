package com.labfytools.trainlog.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class HeartRateLiveStateTest {
    @Test
    fun liveStateDropsStaleBpmWithoutInventingMeasurement() {
        HeartRateLiveState.disconnected()
        val observed = mutableListOf<HeartRateLiveSnapshot>()
        val subscription = HeartRateLiveState.subscribe { observed += it }
        try {
            HeartRateLiveState.connecting()
            HeartRateLiveState.measurement(
                ParsedHeartRateMeasurement(
                    bpm = 82,
                    sensorContactDetected = null,
                    energyExpended = null,
                    rrIntervals1024 = listOf(768),
                ),
            )
            assertEquals(HeartRateLivePhase.CONNECTED, HeartRateLiveState.current().phase)
            assertEquals(82, HeartRateLiveState.current().bpm)
            assertEquals(listOf(768), HeartRateLiveState.current().rrIntervals1024)
            assertEquals(1L, HeartRateLiveState.current().notificationCount)

            HeartRateLiveState.stale()
            assertEquals(HeartRateLivePhase.STALE, HeartRateLiveState.current().phase)
            assertNull(HeartRateLiveState.current().bpm)
            assertEquals(listOf(768), HeartRateLiveState.current().rrIntervals1024)

            HeartRateLiveState.disconnected()
            assertEquals(HeartRateLivePhase.DISCONNECTED, HeartRateLiveState.current().phase)
            assertNull(HeartRateLiveState.current().bpm)
            assertEquals(5, observed.size)
        } finally {
            subscription.close()
            HeartRateLiveState.disconnected()
        }
    }
}
