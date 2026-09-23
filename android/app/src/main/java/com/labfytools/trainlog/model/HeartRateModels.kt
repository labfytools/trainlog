/*
 * Android HeartRateModels model.
 *
 * Android owns sensor acquisition. Desktop/Web only consume synchronized
 * measured facts and never connect to the heart-rate sensor.
 */
package com.labfytools.trainlog.model

enum class HeartRateContextKind(val wireValue: String) {
    SESSION("session"),
    CARDIO("cardio"),
    SLEEP("sleep");

    companion object {
        fun fromWire(value: String): HeartRateContextKind =
            entries.firstOrNull { it.wireValue == value }
                ?: error("Unknown heart-rate context: $value")
    }
}

data class HeartRateRrInterval(
    val index: Int,
    /** Raw Bluetooth Heart Rate Service unit: 1/1024 second. */
    val value1024: Int,
)

data class HeartRateSample(
    val sequence: Long,
    val observedAt: String,
    val bpm: Int,
    val exerciseEntryId: String? = null,
    val sensorContactDetected: Boolean? = null,
    val energyExpended: Int? = null,
    val rrIntervals: List<HeartRateRrInterval> = emptyList(),
)

data class HeartRateCapture(
    val captureId: String,
    val contextKind: HeartRateContextKind,
    val contextId: String,
    val startedAt: String,
    val endedAt: String?,
    val activeExerciseEntryId: String?,
    val sensorName: String?,
    val samples: List<HeartRateSample> = emptyList(),
)
