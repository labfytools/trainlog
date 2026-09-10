package com.labfytools.trainlog.data

import com.labfytools.trainlog.model.SessionExercisePlan

data class SessionGenerationFormOptions(
    val zoneIds: List<String>,
    val goalIds: List<String>,
    val durationPresets: List<Int>,
    val customMinutes: IntRange,
)

data class SessionGenerationRequest(
    val zoneId: String,
    val goalId: String,
    val durationMinutes: Int,
    val referenceTime: String,
    /** Null means every compatible local context; empty means none available. */
    val availableEquipmentIds: Set<String>? = null,
    val preferredExerciseIds: Set<String> = emptySet(),
    val excludedExerciseIds: Set<String> = emptySet(),
    val excludedPatternIds: Set<String> = emptySet(),
)

data class SessionGenerationPreviewExercise(
    val exerciseId: String,
    val exerciseName: String,
    val equipmentId: String,
    val equipmentName: String,
    val primaryZoneId: String,
    val primaryZoneName: String,
    val patternIds: List<String>,
    val patternNames: List<String>,
    val plan: SessionExercisePlan,
    val estimatedSeconds: Int,
    val recency: TrainingRecencyWarning,
    val rationaleCodes: List<String>,
    val loadSourceSessionId: String?,
    val loadSourceOccurrenceId: String?,
    val loadSourceStartedAt: String?,
)

data class SessionGenerationPreview(
    val request: SessionGenerationRequest,
    val exercises: List<SessionGenerationPreviewExercise>,
    val estimatedDurationSeconds: Int,
    val insufficientResolvedCandidates: Boolean,
    val exposure: BodyZoneRecentExposure,
    val shortageCodes: List<String> = emptyList(),
)

sealed interface SessionGenerationResult {
    data class Generated(val preview: SessionGenerationPreview) : SessionGenerationResult
    data class Invalid(val message: String) : SessionGenerationResult
    data class DatabaseError(val message: String) : SessionGenerationResult
}

sealed interface AcceptGeneratedSessionResult {
    data object Accepted : AcceptGeneratedSessionResult
    data object ExistingActiveDraft : AcceptGeneratedSessionResult
    data class Invalid(val message: String) : AcceptGeneratedSessionResult
    data class DatabaseError(val message: String) : AcceptGeneratedSessionResult
}
