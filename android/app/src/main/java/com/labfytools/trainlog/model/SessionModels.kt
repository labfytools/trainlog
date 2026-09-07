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

data class SessionDraftForm(
    val selectedExercise: ExerciseProfile? = null,
    val setCountText: String = "3",
    val repsText: String = "3x10",
    val durationText: String = "30",
    val speedText: String = "",
    val distanceText: String = "",
)

/**
 * INVARIANT: this is the one Android-local in-progress workout. It is stored
 * separately from [SessionDraft] completion rows so history and sync can never
 * mistake unfinished capture for a completed session.
 */
data class ActiveSessionDraft(
    val exercises: List<SessionExerciseDraft> = emptyList(),
    val sessionType: SessionType = SessionType.TRAINING,
    val form: SessionDraftForm = SessionDraftForm(),
    val updatedAt: String = "",
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
