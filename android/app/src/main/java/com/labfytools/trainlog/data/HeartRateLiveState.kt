package com.labfytools.trainlog.data

import java.util.concurrent.CopyOnWriteArraySet

internal enum class HeartRateLivePhase {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    STALE,
}

internal data class HeartRateLiveSnapshot(
    val phase: HeartRateLivePhase = HeartRateLivePhase.DISCONNECTED,
    val bpm: Int? = null,
    val rrIntervals1024: List<Int> = emptyList(),
    val sensorContactDetected: Boolean? = null,
    val energyExpended: Int? = null,
    val notificationCount: Long = 0,
)

internal object HeartRateLiveState {
    private val listeners =
        CopyOnWriteArraySet<(HeartRateLiveSnapshot) -> Unit>()

    @Volatile
    private var snapshot = HeartRateLiveSnapshot()

    fun current(): HeartRateLiveSnapshot = snapshot

    fun publish(next: HeartRateLiveSnapshot) {
        snapshot = next
        listeners.forEach { it(next) }
    }

    fun subscribe(listener: (HeartRateLiveSnapshot) -> Unit): AutoCloseable {
        listeners += listener
        listener(snapshot)
        return AutoCloseable { listeners -= listener }
    }

    fun disconnected() {
        publish(HeartRateLiveSnapshot(phase = HeartRateLivePhase.DISCONNECTED))
    }

    fun connecting() {
        publish(
            snapshot.copy(
                phase = HeartRateLivePhase.CONNECTING,
                bpm = null,
                rrIntervals1024 = emptyList(),
                sensorContactDetected = null,
                energyExpended = null,
            ),
        )
    }

    fun measurement(measurement: ParsedHeartRateMeasurement) {
        publish(
            HeartRateLiveSnapshot(
                phase = HeartRateLivePhase.CONNECTED,
                bpm = measurement.bpm,
                rrIntervals1024 = measurement.rrIntervals1024,
                sensorContactDetected = measurement.sensorContactDetected,
                energyExpended = measurement.energyExpended,
                notificationCount = snapshot.notificationCount + 1,
            ),
        )
    }

    fun stale() {
        if (snapshot.phase == HeartRateLivePhase.CONNECTED) {
            publish(
                snapshot.copy(
                    phase = HeartRateLivePhase.STALE,
                    bpm = null,
                ),
            )
        }
    }
}
