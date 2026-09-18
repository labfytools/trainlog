package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import com.labfytools.trainlog.R
import com.labfytools.trainlog.data.TrainlogRepository

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

/** CONTRACT: this route exposes no start, edit, archive, or delete mutation. */
@Composable
fun ProgramDetailScreen(repository: TrainlogRepository, programId: String) {
    val strings = localizedContext()
    val program = repository.getSyncedProgram(programId)
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
        program.sessions.forEachIndexed { index, session ->
            TrainlogFrame("${index + 1}. ${session.title}") {
                TrainlogInfo(
                    listOfNotNull(
                        strings.getString(if (session.sessionType == "max_test") R.string.session_max_test else R.string.session_training),
                        session.plannedFor?.let { strings.getString(R.string.planned_for, formatDate(it)) },
                    ).joinToString(" · "),
                )
                session.note?.let { TrainlogInfo(it) }
                session.occurrences.forEachIndexed { occurrenceIndex, occurrence ->
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
