package com.labfytools.trainlog.model

/** Read-only desktop-owned program projection exposed to Android presentation. */
data class SyncedProgramSummary(
    val programId: String,
    val title: String,
    val state: String,
    val startDate: String?,
    val endDate: String?,
    val sessionCount: Int,
)

data class SyncedProgramDetail(
    val programId: String,
    val title: String,
    val note: String?,
    val state: String,
    val startDate: String?,
    val endDate: String?,
    val sessions: List<SyncedProgramSession>,
)

data class SyncedProgramSession(
    val programSessionId: String,
    val title: String,
    val sessionType: String,
    val plannedFor: String?,
    val note: String?,
    val executionState: ProgramSessionExecutionState,
    val executionSessionId: String?,
    val occurrences: List<SyncedProgramOccurrence>,
)

enum class ProgramSessionExecutionState {
    TODO,
    IN_PROGRESS,
    COMPLETED,
}

data class SyncedProgramOccurrence(
    val entryId: String,
    val exerciseId: String,
    val exerciseName: String,
    val equipmentId: String?,
    val loadMode: String,
    val restSeconds: Int,
    val targetSets: Int?,
    val targetReps: Int?,
    val targetDurationSeconds: Int?,
    val targetWeightKg: Double?,
    val notes: String?,
)
