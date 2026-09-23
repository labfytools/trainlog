/*
 * Bluetooth Heart Rate Measurement parser.
 *
 * This file deliberately has no Android Bluetooth dependency so protocol
 * decoding can be exhaustively unit-tested independently of GATT transport.
 */
package com.labfytools.trainlog.data

internal data class ParsedHeartRateMeasurement(
    val bpm: Int,
    val sensorContactDetected: Boolean?,
    val energyExpended: Int?,
    /** Raw Bluetooth Heart Rate Service units: 1/1024 second. */
    val rrIntervals1024: List<Int>,
)

internal sealed interface HeartRateMeasurementParseResult {
    data class Parsed(val measurement: ParsedHeartRateMeasurement) :
        HeartRateMeasurementParseResult
    data class Invalid(val reason: String) : HeartRateMeasurementParseResult
}

internal object HeartRateMeasurementParser {
    private const val FLAG_HEART_RATE_16_BIT = 1 shl 0
    private const val FLAG_CONTACT_DETECTED = 1 shl 1
    private const val FLAG_CONTACT_SUPPORTED = 1 shl 2
    private const val FLAG_ENERGY_PRESENT = 1 shl 3
    private const val FLAG_RR_PRESENT = 1 shl 4
    private const val KNOWN_FLAGS =
        FLAG_HEART_RATE_16_BIT or FLAG_CONTACT_DETECTED or FLAG_CONTACT_SUPPORTED or
            FLAG_ENERGY_PRESENT or FLAG_RR_PRESENT

    fun parse(payload: ByteArray): HeartRateMeasurementParseResult {
        if (payload.isEmpty()) return HeartRateMeasurementParseResult.Invalid("missing flags")
        val flags = payload[0].toInt() and 0xff
        if ((flags and KNOWN_FLAGS.inv()) != 0) {
            return HeartRateMeasurementParseResult.Invalid("reserved flags are set")
        }

        var offset = 1
        fun uint8(): Int? =
            payload.getOrNull(offset)?.let {
                offset += 1
                it.toInt() and 0xff
            }
        fun uint16(): Int? {
            if (offset + 2 > payload.size) return null
            val value =
                (payload[offset].toInt() and 0xff) or
                    ((payload[offset + 1].toInt() and 0xff) shl 8)
            offset += 2
            return value
        }

        val bpm =
            if ((flags and FLAG_HEART_RATE_16_BIT) != 0) {
                uint16() ?: return HeartRateMeasurementParseResult.Invalid("truncated 16-bit BPM")
            } else {
                uint8() ?: return HeartRateMeasurementParseResult.Invalid("missing BPM")
            }

        val contact =
            if ((flags and FLAG_CONTACT_SUPPORTED) == 0) {
                null
            } else {
                (flags and FLAG_CONTACT_DETECTED) != 0
            }

        val energy =
            if ((flags and FLAG_ENERGY_PRESENT) != 0) {
                uint16()
                    ?: return HeartRateMeasurementParseResult.Invalid(
                        "truncated energy-expended value",
                    )
            } else {
                null
            }

        val rr = mutableListOf<Int>()
        if ((flags and FLAG_RR_PRESENT) != 0) {
            if (offset == payload.size) {
                return HeartRateMeasurementParseResult.Invalid("RR flag has no interval")
            }
            if ((payload.size - offset) % 2 != 0) {
                return HeartRateMeasurementParseResult.Invalid("truncated RR interval")
            }
            while (offset < payload.size) {
                rr += checkNotNull(uint16())
            }
        } else if (offset != payload.size) {
            return HeartRateMeasurementParseResult.Invalid("unexpected trailing bytes")
        }

        return HeartRateMeasurementParseResult.Parsed(
            ParsedHeartRateMeasurement(
                bpm = bpm,
                sensorContactDetected = contact,
                energyExpended = energy,
                rrIntervals1024 = rr,
            ),
        )
    }
}
