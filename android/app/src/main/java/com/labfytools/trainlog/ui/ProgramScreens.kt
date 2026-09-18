package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import com.labfytools.trainlog.R
import com.labfytools.trainlog.data.StartProgramSessionResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ProgramSessionExecutionState

/** Presentation-only list for the desktop-owned Programs projection. */
@Composable
fun ProgramsScreen(repository: TrainlogRepository, onOpen: (String) -> Unit) {
    val strings = localizedContext()
    val programs = repository.listSyncedPrograms()
    TrainlogScreen(strings.getString(R.string.route_programs)) {
        if (programs.isEmpty()) TrainlogInfo(strings.getString(R.string.programs_none))
        programs.forEach { program ->
            TrainlogFrame(program.title, active = program.state == "active") {
                TrainlogInfo(
                    listOfNotNull(
                        strings.getString(if (program.state == "active") R.string.program_active else R.string.program_archived),
                        program.startDate?.let { strings.getString(R.string.program_start_date, formatDate(it)) },
                        program.endDate?.let { strings.getString(R.string.program_end_date, formatDate(it)) },
                        strings.resources.getQuantityString(
                            R.plurals.program_session_count,
                            program.sessionCount,
                            program.sessionCount,
                        ),
                    ).joinToString(" · "),
                )
                TrainlogButton(
                    strings.getString(R.string.program_open),
                    { onOpen(program.programId) },
                    Modifier.testTag("program-${program.programId}"),
                )
            }
        }
    }
}

/** Program definitions remain read-only; only the local execution draft mutates. */
@Composable
fun ProgramDetailScreen(
    repository: TrainlogRepository,
    programId: String,
    onOpenActiveSession: () -> Unit,
) {
    val strings = localizedContext()
    var revision by remember { mutableIntStateOf(0) }
    var message by remember { mutableStateOf<String?>(null) }
    val program = remember(programId, revision) { repository.getSyncedProgram(programId) }

    fun start(programSessionId: String) {
        when (val result = repository.startSyncedProgramSession(programId, programSessionId)) {
            StartProgramSessionResult.Started,
            StartProgramSessionResult.AlreadyActive -> onOpenActiveSession()
            StartProgramSessionResult.AlreadyCompleted -> {
                message = strings.getString(R.string.program_session_completed_detail)
                revision++
            }
            StartProgramSessionResult.ExistingUnrelatedDraft -> {
                message = strings.getString(R.string.existing_draft_warning)
            }
            StartProgramSessionResult.NotAvailable -> {
                message = strings.getString(R.string.program_session_unavailable)
            }
            is StartProgramSessionResult.Error -> message = result.message
        }
    }

    TrainlogScreen(strings.getString(R.string.route_program_detail)) {
        if (program == null) {
            TrainlogInfo(strings.getString(R.string.program_not_found))
            return@TrainlogScreen
        }
        TrainlogFrame(program.title, active = program.state == "active") {
            TrainlogInfo(strings.getString(if (program.state == "active") R.string.program_active else R.string.program_archived))
            program.startDate?.let { TrainlogInfo(strings.getString(R.string.program_start_date, formatDate(it))) }
            program.endDate?.let { TrainlogInfo(strings.getString(R.string.program_end_date, formatDate(it))) }
            program.note?.let { TrainlogInfo(it) }
        }
        message?.let { TrainlogInfo(it) }
        program.sessions.forEachIndexed { index, session ->
            var expanded by remember(session.programSessionId) { mutableStateOf(false) }
            val completed = session.executionState == ProgramSessionExecutionState.COMPLETED
            TrainlogFrame("${index + 1}. ${session.title}", active = completed) {
                val stateLabel = when (session.executionState) {
                    ProgramSessionExecutionState.TODO -> R.string.program_session_todo
                    ProgramSessionExecutionState.IN_PROGRESS -> R.string.program_session_in_progress
                    ProgramSessionExecutionState.COMPLETED -> R.string.program_session_completed
                }
                TrainlogInfo(strings.getString(stateLabel))
                TrainlogInfo(
                    listOfNotNull(
                        strings.getString(if (session.sessionType == "max_test") R.string.session_max_test else R.string.session_training),
                        session.plannedFor?.let { strings.getString(R.string.planned_for, formatDate(it)) },
                    ).joinToString(" · "),
                )
                session.note?.let { TrainlogInfo(it) }
                TrainlogButton(
                    strings.getString(
                        if (expanded) R.string.program_session_hide_occurrences
                        else R.string.program_session_show_occurrences,
                    ),
                    { expanded = !expanded },
                    Modifier.testTag("program-session-toggle-${session.programSessionId}"),
                )
                when (session.executionState) {
                    ProgramSessionExecutionState.TODO -> TrainlogButton(
                        strings.getString(R.string.program_session_start),
                        { start(session.programSessionId) },
                        Modifier.testTag("program-session-start-${session.programSessionId}"),
                    )
                    ProgramSessionExecutionState.IN_PROGRESS -> TrainlogButton(
                        strings.getString(R.string.program_session_resume),
                        onOpenActiveSession,
                        Modifier.testTag("program-session-resume-${session.programSessionId}"),
                    )
                    ProgramSessionExecutionState.COMPLETED -> TrainlogInfo(
                        strings.getString(R.string.program_session_completed_detail),
                    )
                }
                if (expanded) session.occurrences.forEachIndexed { occurrenceIndex, occurrence ->
                    val target = occurrence.targetReps?.let { strings.getString(R.string.program_target_reps, it) }
                        ?: occurrence.targetDurationSeconds?.let { strings.getString(R.string.program_target_duration, it) }.orEmpty()
                    val sets = occurrence.targetSets?.let { strings.getString(R.string.program_target_sets, it) }.orEmpty()
                    val weight = occurrence.targetWeightKg?.let { strings.getString(R.string.program_target_weight, it) }.orEmpty()
                    TrainlogInfo(
                        "${occurrenceIndex + 1}. ${occurrence.exerciseName}" +
                            listOf(sets, target, weight).filter { it.isNotEmpty() }.joinToString(" · ", prefix = " · "),
                    )
                    occurrence.notes?.let { TrainlogInfo(it) }
                }
            }
        }
    }
}
