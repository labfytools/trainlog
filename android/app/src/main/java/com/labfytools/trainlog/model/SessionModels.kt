package com.labfytools.trainlog.model

enum class SessionType(
    val wireValue: String,
) {
    TRAINING("training"),
    MAX_TEST("max_test");

    companion object {
        fun fromWire(
            value: String,
        ): SessionType =
            if (value == "max_test") {
                MAX_TEST
            } else {
                TRAINING
            }
    }
}

data class SessionSetDraft(
    val reps: Int = 0,
    val durationSeconds: Int = 0,
)

data class SessionExerciseDraft(
    val exercise: ExerciseProfile,
    val sets: List<SessionSetDraft> = emptyList(),
    val continuousDurationSeconds: Int = 0,
    val speedKmh: Double? = null,
    val distanceKm: Double? = null,
)

data class SessionDraft(
    val exercises: List<SessionExerciseDraft>,
    val sessionType: SessionType = SessionType.TRAINING,
)

data class SessionSummary(
    val sessionId: String,
    val startedAt: String,
    val exerciseCount: Int,
    val sessionType: SessionType = SessionType.TRAINING,
)

data class SessionExerciseDetail(
    val exerciseName: String,
    val recordingMode: RecordingMode,
    val trackingMode: TrackingMode,
    val dataFields: Int,
    val sets: List<SessionSetDraft> = emptyList(),
    val continuousDurationSeconds: Int = 0,
    val speedKmh: Double? = null,
    val distanceKm: Double? = null,
)

data class SessionDetail(
    val summary: SessionSummary,
    val exercises: List<SessionExerciseDetail>,
)
