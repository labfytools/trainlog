package com.labfytools.trainlog.model

enum class RecordingMode(
    val wireValue: String,
) {
    SETS("sets"),
    CONTINUOUS("continuous"),
}

enum class TrackingMode(
    val wireValue: String,
) {
    REPS("reps"),
    DURATION("duration"),
}

object ExerciseDataFields {
    const val NONE: Int = 0
    const val SPEED_KMH: Int = 1
    const val DISTANCE_KM: Int = 2
    const val KNOWN_MASK: Int =
        SPEED_KMH or DISTANCE_KM
}

data class ExerciseProfile(
    val exerciseId: String,
    val name: String,
    val normalizedName: String,
    val recordingMode: RecordingMode,
    val trackingMode: TrackingMode,
    val dataFields: Int,
    /** Stable manifest IDs; display names are presentation-only metadata. */
    val primaryZoneId: String? = null,
    val secondaryZoneIds: List<String> = emptyList(),
)

data class NewExerciseProfile(
    val name: String,
    val recordingMode: RecordingMode,
    val trackingMode: TrackingMode,
    val dataFields: Int,
    /** Null is the explicit no-relations state; validation rejects groups and orphan secondaries. */
    val primaryZoneId: String? = null,
    val secondaryZoneIds: List<String> = emptyList(),
) {
    fun validate(): Boolean {
        if (name.isBlank()) {
            return false
        }

        if (
            dataFields and
                ExerciseDataFields.KNOWN_MASK.inv() != 0
        ) {
            return false
        }

        if (
            recordingMode ==
                RecordingMode.CONTINUOUS &&
            trackingMode !=
                TrackingMode.DURATION
        ) {
            return false
        }

        if (
            recordingMode ==
                RecordingMode.SETS &&
            dataFields !=
                ExerciseDataFields.NONE
        ) {
            return false
        }

        return true
    }
}

/**
 * CONTRACT: an edit addresses the existing stable identity.  `name` is
 * presentation metadata, not a replacement identity, so callers must never
 * create a second exercise merely to rename one. Name/profile/zones are saved
 * transactionally; cancelling before this call has no persistence effect.
 */
data class ExerciseEditInput(
    val exerciseId: String,
    val name: String,
    val recordingMode: RecordingMode,
    val trackingMode: TrackingMode,
    val dataFields: Int,
    val primaryZoneId: String? = null,
    val secondaryZoneIds: List<String> = emptyList(),
) {
    fun validateProfile(): Boolean =
        NewExerciseProfile(
            name = name,
            recordingMode = recordingMode,
            trackingMode = trackingMode,
            dataFields = dataFields,
        ).validate()
}
