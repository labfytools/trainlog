package com.labfytools.trainlog.model

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
)

data class SessionSummary(
    val sessionId: String,
    val startedAt: String,
    val exerciseCount: Int,
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
