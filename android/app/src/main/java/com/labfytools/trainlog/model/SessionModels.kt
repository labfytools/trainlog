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
    val weightKg: Double? = null,
)

data class SessionExerciseDraft(
    /** Stable occurrence identity; exercise_id identifies only the catalogue movement. */
    val entryId: String = "sxe_" + java.util.UUID.randomUUID().toString(),
    val exercise: ExerciseProfile,
    /** Stable canonical equipment ID selected for this occurrence, if any. */
    val equipmentId: String? = null,
    /**
     * CONTRACT: a measured maximum is an occurrence result, not a synthetic
     * performed set. It is present only for an explicit MAX_TEST entry and is
     * mutually exclusive with sets/continuous data.
     */
    val maxWeightKg: Double? = null,
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
    /** Null means creation; otherwise replace this durable draft entry. */
    val editingExerciseIndex: Int? = null,
    val editingEntryId: String? = null,
    val selectedEquipmentId: String? = null,
    /** Raw max input is durable so a French decimal fragment survives restart. */
    val maxWeightText: String = "",
    val setCountText: String = "3",
    val repsText: String = "3x10",
    val weightText: String = "",
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
    /** Existing max_test session updated atomically on finalization, if any. */
    val sourceSessionId: String? = null,
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
    /** Stable completed-session occurrence identity, never catalogue identity. */
    val entryId: String,
    val exerciseId: String,
    val exerciseName: String,
    val equipmentId: String? = null,
    val equipmentDisplayName: String? = null,
    val recordingMode: RecordingMode,
    val trackingMode: TrackingMode,
    val dataFields: Int,
    /** Explicit max result; null also represents a preserved legacy max entry. */
    val maxWeightKg: Double? = null,
    val sets: List<SessionSetDraft> = emptyList(),
    val continuousDurationSeconds: Int = 0,
    val speedKmh: Double? = null,
    val distanceKm: Double? = null,
)

data class SessionDetail(
    val summary: SessionSummary,
    val exercises: List<SessionExerciseDetail>,
)

data class LatestExerciseMax(
    val exerciseId: String,
    val exerciseName: String,
    val maxWeightKg: Double,
    val startedAt: String,
    val equipmentDisplayName: String? = null,
)
