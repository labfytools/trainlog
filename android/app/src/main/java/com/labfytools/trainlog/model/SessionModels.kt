/*
 * Android SessionModels model.
 *
 * Defines in-memory capture semantics without owning persistence, synchronization, or presentation.
 */
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

enum class SessionLoadMode(val wireValue: String) {
    NONE("none"),
    EXTERNAL("external"),
    ASSISTANCE("assistance");

    companion object {
        fun fromWire(value: String): SessionLoadMode =
            entries.firstOrNull { it.wireValue == value }
                ?: error("Mode de charge inconnu: $value")
    }
}

/**
 * Planned occurrence metadata is deliberately separate from performed rows.
 * A generator may create a target-only draft, while normal completion still
 * requires actual work before the occurrence enters completed history.
 */
data class SessionExercisePlan(
    val sets: Int,
    val reps: Int? = null,
    val durationSeconds: Int? = null,
    val weightKg: Double? = null,
    val loadMode: SessionLoadMode = SessionLoadMode.NONE,
    val restSeconds: Int = 0,
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
    val plan: SessionExercisePlan? = null,
    val sets: List<SessionSetDraft> = emptyList(),
    val continuousDurationSeconds: Int = 0,
    val speedKmh: Double? = null,
    val distanceKm: Double? = null,
)

data class SessionDraft(
    val exercises: List<SessionExerciseDraft>,
    val sessionType: SessionType = SessionType.TRAINING,
)

data class SessionExerciseTiming(
    val sessionId: String,
    val entryId: String,
    val exerciseId: String,
    val startedAt: String,
    val endedAt: String?,
) {
    val active: Boolean get() = endedAt == null
}

data class ActiveSessionTimelineContext(
    val sessionId: String,
    val startedAt: String,
    val sessionType: SessionType,
    val activeEntryId: String?,
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
    /** Stable lifecycle identity, allocated once when first persisted. */
    val sessionId: String? = null,
    /** Causal draft revision; local UI-only persistence may retain its tip. */
    val revisionId: String = "dr_legacy_v1",
    val parentRevisionId: String? = null,
    /** Genuine lifecycle start, distinct from local form update time. */
    val startedAt: String? = null,
    val exercises: List<SessionExerciseDraft> = emptyList(),
    val sessionType: SessionType = SessionType.TRAINING,
    /** Existing max_test session updated atomically on finalization, if any. */
    val sourceSessionId: String? = null,
    /** Stable desktop-owned Program provenance; both values are present or absent. */
    val sourceProgramId: String? = null,
    val sourceProgramSessionId: String? = null,
    val form: SessionDraftForm = SessionDraftForm(),
    val updatedAt: String = "",
)

/**
 * A desktop-authored proposal is inert until the user explicitly starts it.
 * INVARIANT: entries contain targets only; performed values remain owned by
 * [ActiveSessionDraft] after an atomic start operation.
 */
data class AiSessionDraft(
    val draftId: String,
    val createdAt: String,
    val plannedFor: String? = null,
    val title: String? = null,
    val notes: String? = null,
    val entries: List<SessionExerciseDraft>,
)

/** Immutable desktop preparation delivery waiting for an explicit start. */
data class PreparedSession(
    val deliveryId: String,
    val preparationId: String,
    val revisionId: String,
    val executionSessionId: String,
    val plannedFor: String?,
    val title: String,
    val notes: String?,
    val entries: List<SessionExerciseDraft>,
)

data class SessionSummary(
    val sessionId: String,
    val startedAt: String,
    val exerciseCount: Int,
    val sessionType: SessionType = SessionType.TRAINING,
    /** Null is preserved for historical sessions without a trustworthy anchor. */
    val endedAt: String? = null,
)

data class ExerciseFeedback(
    val feedbackId: String,
    val sessionId: String,
    val entryId: String,
    val exerciseId: String,
    val observedAt: String,
    val rawText: String,
    val modified: Boolean = false,
)

/** Android-local feedback owned by an unfinished occurrence. */
data class DraftExerciseFeedback(
    val feedbackId: String,
    val entryId: String,
    val exerciseId: String,
    val observedAt: String,
    val rawText: String,
    val modified: Boolean = false,
)

data class SessionFollowUp(
    val followupId: String,
    val sessionId: String,
    val observedAt: String,
    val rawText: String,
    val modified: Boolean = false,
)

/** Immutable wording revision. The parent observation timestamp is deliberately
 * absent: correcting wording never creates a new physiological observation. */
data class FeedbackRevision(
    val revisionId: String,
    val createdAt: String,
    val rawText: String,
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
    val plan: SessionExercisePlan? = null,
    val sets: List<SessionSetDraft> = emptyList(),
    val continuousDurationSeconds: Int = 0,
    val speedKmh: Double? = null,
    val distanceKm: Double? = null,
    val feedback: List<ExerciseFeedback> = emptyList(),
)

data class SessionDetail(
    val summary: SessionSummary,
    val exercises: List<SessionExerciseDetail>,
    val followUps: List<SessionFollowUp> = emptyList(),
)

data class LatestExerciseMax(
    val exerciseId: String,
    val exerciseName: String,
    val maxWeightKg: Double,
    val startedAt: String,
    val equipmentDisplayName: String? = null,
)
