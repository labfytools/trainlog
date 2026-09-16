/*
 * Android ExerciseScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.BasicText
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.BodyZone
import com.labfytools.trainlog.data.BodyZoneKind
import com.labfytools.trainlog.data.EditExerciseResult
import com.labfytools.trainlog.data.ExerciseKnowledge
import com.labfytools.trainlog.data.ExerciseKnowledgeStatus
import com.labfytools.trainlog.data.KnowledgeConfidence
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseEditInput
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography
import com.labfytools.trainlog.R

class ExerciseScreenState {
    val name = mutableStateOf("")
    val recordingMode = mutableStateOf(RecordingMode.SETS)
    val trackingMode = mutableStateOf(TrackingMode.REPS)
    val speed = mutableStateOf(false)
    val distance = mutableStateOf(false)
    val primaryZoneId = mutableStateOf<String?>(null)
    val secondaryZoneIds = mutableStateOf(emptySet<String>())
    val searchQuery = mutableStateOf("")
    val filterZoneId = mutableStateOf<String?>(null)
    val unclassifiedFilter = mutableStateOf(false)
    val expandedKnowledgeIds = mutableStateOf(emptySet<String>())
    val message = mutableStateOf<String?>(null)
    val editedExerciseId = mutableStateOf<String?>(null)
    var selectedExerciseId by mutableStateOf<String?>(null)
    var catalogueScroll by mutableStateOf(0)
    var detailScroll by mutableStateOf(0)
    private var cleanSignature = signature()
    val dirty: Boolean get() = signature() != cleanSignature
    fun markClean() { cleanSignature = signature() }
    fun prepareCreate() {
        if (editedExerciseId.value == null && !dirty) abandonEdits()
    }
    fun prepareEdit(exercise: ExerciseProfile) {
        if (editedExerciseId.value == exercise.exerciseId) return
        require(!dirty) { "dirty editor state cannot be replaced" }
        editedExerciseId.value = exercise.exerciseId
        name.value = exercise.name
        recordingMode.value = exercise.recordingMode
        trackingMode.value = exercise.trackingMode
        speed.value = exercise.dataFields and ExerciseDataFields.SPEED_KMH != 0
        distance.value = exercise.dataFields and ExerciseDataFields.DISTANCE_KM != 0
        primaryZoneId.value = exercise.primaryZoneId
        secondaryZoneIds.value = exercise.secondaryZoneIds.toSet()
        message.value = null
        markClean()
    }
    fun abandonEdits() {
        name.value = ""; recordingMode.value = RecordingMode.SETS; trackingMode.value = TrackingMode.REPS
        speed.value = false; distance.value = false; primaryZoneId.value = null
        secondaryZoneIds.value = emptySet(); editedExerciseId.value = null; message.value = null; markClean()
    }
    private fun signature(): String = listOf(name.value, recordingMode.value, trackingMode.value, speed.value,
        distance.value, primaryZoneId.value, secondaryZoneIds.value.sorted(), editedExerciseId.value).joinToString("|")
}

@Composable
fun ExerciseScreen(
    repository: TrainlogRepository,
    state: ExerciseScreenState,
    inline: Boolean,
    onBack: () -> Unit,
    onSaved: () -> Unit,
    onOpenMaxima: () -> Unit,
    catalogueVisible: Boolean = true,
) {
    val colors =
        LocalTrainlogColors.current
    val strings = localizedContext()

    var name by state.name
    var recordingMode by state.recordingMode
    var trackingMode by state.trackingMode
    var speed by state.speed
    var distance by state.distance
    var primaryZoneId by state.primaryZoneId
    var secondaryZoneIds by state.secondaryZoneIds
    var searchQuery by state.searchQuery
    var filterZoneId by state.filterZoneId
    var unclassifiedFilter by state.unclassifiedFilter
    var expandedKnowledgeIds by state.expandedKnowledgeIds
    val zones = repository.listBodyZones()

    var message by state.message
    var editedExerciseId by state.editedExerciseId

    val originalProfile = remember(editedExerciseId) {
        editedExerciseId?.let { id -> repository.listExercises().firstOrNull { it.exerciseId == id } }
    }
    var confirmedIncompatible by remember(editedExerciseId) { mutableStateOf(false) }
    var confirmingIncompatible by remember(editedExerciseId) { mutableStateOf(false) }
    val incompatibleProfileChange = originalProfile?.let {
        it.recordingMode != recordingMode || it.trackingMode != trackingMode ||
            it.dataFields != ((if (speed) ExerciseDataFields.SPEED_KMH else 0) or
                (if (distance) ExerciseDataFields.DISTANCE_KM else 0))
    } == true

    fun startEditing(exercise: ExerciseProfile) {
        /* WHY: edit state copies catalog metadata for presentation only. The
         * repository remains the sole owner of stable identity and SQLite. */
        editedExerciseId = exercise.exerciseId
        name = exercise.name
        recordingMode = exercise.recordingMode
        trackingMode = exercise.trackingMode
        speed = exercise.dataFields and ExerciseDataFields.SPEED_KMH != 0
        distance = exercise.dataFields and ExerciseDataFields.DISTANCE_KM != 0
        primaryZoneId = exercise.primaryZoneId
        secondaryZoneIds = exercise.secondaryZoneIds.toSet()
        message = null
        state.markClean()
    }

    TrainlogScreen(
        subtitle = strings.getString(if (editedExerciseId == null) R.string.route_exercise_create else R.string.route_exercise_edit)
    ) {
        TrainlogFrame(
            title =
                if (editedExerciseId == null) {
                    strings.getString(R.string.new_exercise)
                } else {
                    strings.getString(R.string.route_exercise_edit)
                }
        ) {
            TrainlogField(
                label = strings.getString(R.string.name),
                value = name,
                onValueChange = {
                    name = it
                    message = null
                },
            )

            TrainlogChoiceGroup(
                label = strings.getString(R.string.organization),
            ) {
                TrainlogChoice(
                    label = strings.getString(R.string.organization_sets),
                    selected =
                        recordingMode ==
                            RecordingMode.SETS,
                    enabled = true,
                    onClick = {
                        recordingMode =
                            RecordingMode.SETS

                        speed = false
                        distance = false
                        message = null
                    },
                )

                TrainlogChoice(
                    label = strings.getString(R.string.continuous),
                    selected =
                        recordingMode ==
                            RecordingMode.CONTINUOUS,
                    enabled = true,
                    onClick = {
                        recordingMode =
                            RecordingMode.CONTINUOUS

                        trackingMode =
                            TrackingMode.DURATION

                        message = null
                    },
                )
            }

            TrainlogChoiceGroup(
                label = strings.getString(R.string.primary_measurement),
            ) {
                if (
                    recordingMode ==
                    RecordingMode.SETS
                ) {
                    TrainlogChoice(
                        label =
                            strings.getString(R.string.field_reps),
                        selected =
                            trackingMode ==
                                TrackingMode.REPS,
                        enabled = true,
                        onClick = {
                            trackingMode =
                                TrackingMode.REPS

                            message = null
                        },
                    )
                }

                TrainlogChoice(
                    label = strings.getString(R.string.field_duration),
                    selected =
                        trackingMode ==
                            TrackingMode.DURATION,
                    enabled = true,
                    onClick = {
                        trackingMode =
                            TrackingMode.DURATION

                        message = null
                    },
                )
            }

            if (
                recordingMode ==
                RecordingMode.CONTINUOUS
            ) {
                TrainlogChoiceGroup(
                    label =
                        strings.getString(R.string.supplemental_data),
                ) {
                    TrainlogChoice(
                        label = strings.getString(R.string.field_speed_short),
                        selected = speed,
                        enabled = true,
                        onClick = {
                            speed = !speed
                            message = null
                        },
                    )

                    TrainlogChoice(
                        label = strings.getString(R.string.field_distance_short),
                        selected = distance,
                        enabled = true,
                        onClick = {
                            distance =
                                !distance

                            message = null
                        },
                    )
                }
            }

            TrainlogChoiceGroup(label = strings.getString(R.string.primary_zone)) {
                TrainlogChoice(
                    label = strings.getString(R.string.not_specified_feminine),
                    selected = primaryZoneId == null,
                    onClick = {
                        primaryZoneId = null
                        secondaryZoneIds = emptySet()
                        message = null
                    },
                )
                BodyZoneChoices(zones) { zone, indented ->
                    TrainlogChoice(
                        label = (if (indented) "  ↳ " else "") + localizedBodyZoneName(strings, zone.zoneId, zone.displayName),
                        selected = primaryZoneId == zone.zoneId,
                        enabled = zone.kind != BodyZoneKind.GROUP,
                        onClick = {
                            primaryZoneId = zone.zoneId
                            secondaryZoneIds = secondaryZoneIds - zone.zoneId
                            message = null
                        },
                    )
                }
            }

            TrainlogChoiceGroup(label = strings.getString(R.string.secondary_zones)) {
                BodyZoneChoices(zones) { zone, indented ->
                    TrainlogChoice(
                        label = (if (indented) "  ↳ " else "") + localizedBodyZoneName(strings, zone.zoneId, zone.displayName),
                        selected = zone.zoneId in secondaryZoneIds,
                        enabled = primaryZoneId != null &&
                            zone.kind != BodyZoneKind.GROUP && zone.zoneId != primaryZoneId,
                        onClick = {
                            secondaryZoneIds = if (zone.zoneId in secondaryZoneIds) {
                                secondaryZoneIds - zone.zoneId
                            } else {
                                secondaryZoneIds + zone.zoneId
                            }
                            message = null
                        },
                    )
                }
            }

            val fields =
                if (
                    recordingMode ==
                    RecordingMode.CONTINUOUS
                ) {
                    (
                        if (speed) {
                            ExerciseDataFields
                                .SPEED_KMH
                        } else {
                            ExerciseDataFields
                                .NONE
                        }
                    ) or
                        (
                            if (distance) {
                                ExerciseDataFields
                                    .DISTANCE_KM
                            } else {
                                ExerciseDataFields
                                    .NONE
                            }
                        )
                } else {
                    ExerciseDataFields.NONE
                }

            TrainlogInfo(
                text =
                    profilePreview(
                        recordingMode,
                        trackingMode,
                        fields,
                    ),
                color = colors.accent,
            )

            if (editedExerciseId != null && repository.canEditExerciseProfile(editedExerciseId!!).not()) {
                TrainlogInfo(
                    text =
                        strings.getString(R.string.profile_change_note),
                    color = colors.warning,
                )
            }

            TrainlogPrimaryAction(
                label =
                    if (editedExerciseId != null) {
                        strings.getString(R.string.save_changes)
                    } else if (inline) {
                        strings.getString(R.string.create_return_session)
                    } else {
                        strings.getString(R.string.exercise_save)
                    },
                description =
                    if (editedExerciseId == null) {
                        strings.getString(R.string.add_profile_description)
                    } else {
                        strings.getString(R.string.identity_update_description)
                    },
                onClick = save@{
                    if (editedExerciseId != null && incompatibleProfileChange && !confirmedIncompatible) {
                        confirmingIncompatible = true
                        return@save
                    }
                    if (editedExerciseId == null &&
                        recordingMode == RecordingMode.SETS && primaryZoneId == null) {
                        message = strings.getString(R.string.primary_zone_required)
                        return@save
                    }
                    val current = editedExerciseId
                    val result =
                        if (current == null) {
                            repository.createExercise(
                                NewExerciseProfile(
                                    name = name,
                                    recordingMode =
                                        recordingMode,
                                    trackingMode =
                                        trackingMode,
                                    dataFields =
                                        fields,
                                    primaryZoneId = primaryZoneId,
                                    secondaryZoneIds = secondaryZoneIds.toList(),
                                ),
                            )
                        } else {
                            repository.editExercise(
                                ExerciseEditInput(
                                    exerciseId = current,
                                    name = name,
                                    recordingMode = recordingMode,
                                    trackingMode = trackingMode,
                                    dataFields = fields,
                                    primaryZoneId = primaryZoneId,
                                    secondaryZoneIds = secondaryZoneIds.toList(),
                                ),
                            )
                        }
                    when (result) {
                        is CreateExerciseResult.Created -> {
                            message = null
                            state.abandonEdits()
                            onSaved()
                        }

                        CreateExerciseResult.Conflict -> {
                            message =
                                strings.getString(R.string.exercise_name_exists)
                        }

                        CreateExerciseResult.Invalid -> {
                            message =
                                strings.getString(R.string.invalid_profile_name)
                        }

                        is CreateExerciseResult.DatabaseError -> {
                            message = result.message
                        }

                        is EditExerciseResult.Saved -> {
                            message = null
                            editedExerciseId = null
                            state.abandonEdits()
                            onSaved()
                        }

                        EditExerciseResult.Conflict -> {
                            message = strings.getString(R.string.other_exercise_name_exists)
                        }

                        EditExerciseResult.InvalidNameOrProfile -> {
                            message = strings.getString(R.string.invalid_name_profile)
                        }

                        EditExerciseResult.IncompatibleProfileChange -> {
                            message =
                                strings.getString(R.string.profile_used_error)
                        }

                        EditExerciseResult.DatabaseError -> {
                            message = strings.getString(R.string.database_save_failed)
                        }
                    }
                },
            )

            if (editedExerciseId != null) {
                TrainlogAction(
                    label = strings.getString(R.string.dialog_cancel),
                    description = strings.getString(R.string.cancel_catalog_description),
                    accent = colors.muted,
                    onClick = {
                        editedExerciseId = null
                        name = ""
                        recordingMode = RecordingMode.SETS
                        trackingMode = TrackingMode.REPS
                        speed = false
                        distance = false
                        primaryZoneId = null
                        secondaryZoneIds = emptySet()
                        message = null
                    },
                )
            }

            if (message != null) {
                TrainlogInfo(
                    text =
                        message.orEmpty(),
                    color = colors.error,
                )
            }
        }

        if (catalogueVisible) TrainlogFrame(title = strings.getString(R.string.existing_exercises), active = false) {
            TrainlogInputField(
                label = strings.getString(R.string.search_prefix),
                value = searchQuery,
                onValueChange = { searchQuery = it },
            )
            TrainlogChoiceGroup(label = strings.getString(R.string.zone_filter)) {
                TrainlogChoice(
                    label = strings.getString(R.string.all_zones),
                    selected = filterZoneId == null && !unclassifiedFilter,
                    onClick = { filterZoneId = null; unclassifiedFilter = false },
                )
                BodyZoneChoices(zones, includeGroups = true) { zone, indented ->
                    TrainlogChoice(
                        label = (if (indented) "  ↳ " else "") + localizedBodyZoneName(strings, zone.zoneId, zone.displayName),
                        selected = filterZoneId == zone.zoneId && !unclassifiedFilter,
                        onClick = { filterZoneId = zone.zoneId; unclassifiedFilter = false },
                    )
                }
                TrainlogChoice(
                    label = strings.getString(R.string.not_specified_plural),
                    selected = unclassifiedFilter,
                    onClick = { filterZoneId = null; unclassifiedFilter = true },
                )
            }
            val exercises = repository.listExercises(
                query = searchQuery,
                zoneId = filterZoneId,
                includeDescendants = true,
                unclassifiedOnly = unclassifiedFilter,
            )
            if (exercises.isEmpty()) {
                TrainlogInfo(strings.getString(R.string.exercise_none_saved))
            } else {
                exercises.forEach { exercise ->
                    TrainlogAction(
                        label = strings.getString(R.string.modify_named, exercise.name),
                        description = exerciseZoneSummary(repository, exercise, strings),
                        accent = colors.accent,
                        onClick = { startEditing(exercise) },
                    )
                    repository.getExerciseKnowledge(exercise.exerciseId)?.let { knowledge ->
                        val expanded = exercise.exerciseId in expandedKnowledgeIds
                        TrainlogAction(
                            label = strings.getString(if (expanded) R.string.knowledge_collapse else R.string.knowledge_expand),
                            description = knowledgeSummary(knowledge, strings),
                            accent = colors.muted,
                            onClick = {
                                expandedKnowledgeIds = if (expanded) {
                                    expandedKnowledgeIds - exercise.exerciseId
                                } else {
                                    expandedKnowledgeIds + exercise.exerciseId
                                }
                            },
                        )
                        if (expanded) KnowledgePanel(repository, knowledge)
                    }
                    TrainlogAction(
                        label = strings.getString(R.string.view_latest_max),
                        description = strings.getString(R.string.view_latest_max_description),
                        accent = colors.warning,
                        onClick = onOpenMaxima,
                    )
                }
            }
        }

        TrainlogFrame(
            title = strings.getString(R.string.contract),
            active = false,
        ) {
            TrainlogInfo(
                "CONTINUOUS force DURATION."
            )

            TrainlogInfo(
                strings.getString(R.string.name_no_rules)
            )
        }
    }
    if (confirmingIncompatible) AlertDialog(
        onDismissRequest = { confirmingIncompatible = false },
        title = { Text(strings.getString(R.string.tracking_change_question)) },
        text = { Text(strings.getString(R.string.profile_change_dialog_detail)) },
        confirmButton = { TextButton(onClick = {
            confirmedIncompatible = true
            confirmingIncompatible = false
            message = strings.getString(R.string.profile_transition_confirmed)
        }) { Text(strings.getString(R.string.modify)) } },
        dismissButton = { TextButton(onClick = { confirmingIncompatible = false }) { Text(strings.getString(R.string.dialog_cancel)) } },
    )
}

@Composable
fun ExerciseEditorRoute(
    repository: TrainlogRepository,
    state: ExerciseScreenState,
    exerciseId: String?,
    inline: Boolean,
    onSaved: () -> Unit,
) {
    val strings = localizedContext()
    val profile = remember(exerciseId) {
        exerciseId?.let { id -> repository.listExercises().firstOrNull { it.exerciseId == id } }
    }
    LaunchedEffect(exerciseId) {
        if (profile == null) state.prepareCreate() else state.prepareEdit(profile)
    }
    if (exerciseId != null && profile == null) {
        TrainlogScreen(strings.getString(R.string.route_exercise_edit)) { TrainlogInfo(strings.getString(R.string.exercise_not_found)) }
    } else if (state.editedExerciseId.value != exerciseId) {
        TrainlogScreen(strings.getString(R.string.route_exercise_edit)) { TrainlogInfo(strings.getString(R.string.profile_loading)) }
    } else {
        ExerciseScreen(repository, state, inline, {}, onSaved, {}, catalogueVisible = false)
    }
}

@Composable
private fun KnowledgePanel(repository: TrainlogRepository, knowledge: ExerciseKnowledge) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    /* WHY: conditional content is opened only under an explicit uncertainty
     * label; it cannot be mistaken for an ordinary resolved classification. */
    val interpretation = when (knowledge.resolutionStatus) {
        ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED -> knowledge.interpretation
        ExerciseKnowledgeStatus.CONDITIONAL -> knowledge.conditionalInterpretation
        ExerciseKnowledgeStatus.UNRESOLVED -> null
    }
    if (interpretation == null) {
        TrainlogInfo(strings.getString(R.string.science_unresolved), colors.warning)
        TrainlogInfo(strings.getString(R.string.confidence_inline, confidenceLabel(knowledge.confidence, strings)), colors.muted)
    } else if (knowledge.resolutionStatus == ExerciseKnowledgeStatus.CONDITIONAL) {
        TrainlogInfo(
            strings.getString(R.string.conditional_interpretation, interpretation.requiredConfirmation.orEmpty()),
            colors.warning,
        )
    }
    interpretation?.let { resolved ->
        val patterns = resolved.patternIds.mapNotNull(repository::getMovementPatternKnowledge)
        val primary = resolved.primaryMuscleIds.mapNotNull(repository::getMuscleKnowledge)
        val secondary = resolved.secondaryMuscleIds.mapNotNull(repository::getMuscleKnowledge)
        val stabilizers = resolved.stabilizerMuscleIds.mapNotNull(repository::getMuscleKnowledge)
        val actions = resolved.actionIds.mapNotNull(repository::getJointActionKnowledge)
        val primaryZone = repository.bodyZone(resolved.primaryZoneId)?.let { localizedBodyZoneName(strings, it.zoneId, it.displayName) } ?: resolved.primaryZoneId
        val secondaryZones = resolved.secondaryZoneIds.map { id -> repository.bodyZone(id)?.let { localizedBodyZoneName(strings, it.zoneId, it.displayName) } ?: id }
        val runtimeEquipment = repository.listEquipment().associateBy { it.equipmentId }
        val equipment = repository.listEquipmentForKnownExercise(knowledge.exerciseId)
            .map { runtimeEquipment[it.equipmentId]?.displayName ?: it.equipmentId }
        val french = LocalLanguagePresentation.current.language == AppLanguage.FRENCH
        TrainlogInfo(strings.getString(R.string.movements_value, actions.joinToString { scientificCatalogLabel(it.actionId, it.displayNameFr, null, french) }.ifEmpty { patterns.joinToString { scientificCatalogLabel(it.patternId, it.displayNameFr, null, french) }.ifEmpty { strings.getString(R.string.unclassified) } }))
        TrainlogInfo(strings.getString(R.string.primary_muscles, primary.joinToString { scientificCatalogLabel(it.muscleId, it.displayNameFr, it.displayName, french) }.ifEmpty { strings.getString(R.string.unclassified) }))
        TrainlogInfo(strings.getString(R.string.secondary_muscles, secondary.joinToString { scientificCatalogLabel(it.muscleId, it.displayNameFr, it.displayName, french) }.ifEmpty { strings.getString(R.string.none_established) }))
        TrainlogInfo(strings.getString(R.string.stabilizers, stabilizers.joinToString { scientificCatalogLabel(it.muscleId, it.displayNameFr, it.displayName, french) }.ifEmpty { strings.getString(R.string.none_established) }))
        TrainlogInfo(strings.getString(R.string.scientific_zones, primaryZone,
            if (secondaryZones.isEmpty()) "" else strings.getString(R.string.secondary_suffix, secondaryZones.joinToString())))
        TrainlogInfo(strings.getString(R.string.compatible_equipment_value, equipment.joinToString().ifEmpty { strings.getString(R.string.not_established) }))
        TrainlogInfo(strings.getString(R.string.confidence_inline, confidenceLabel(resolved.confidence, strings)), colors.muted)
    }
    knowledge.limitations.forEach { TrainlogInfo(strings.getString(R.string.limitation_value, it), colors.muted) }
    knowledge.sourceRefs.mapNotNull(repository::getScienceReference).forEach { reference ->
        TrainlogInfo(
            strings.getString(R.string.source_value, reference.authorsOrOrganization, reference.title,
                reference.year?.let { " ($it)" } ?: ""),
            colors.muted,
        )
    }
}

/** CONTRACT: the section root is a catalogue; creation and detail are explicit routes. */
@Composable
fun ExerciseCatalogueScreen(
    repository: TrainlogRepository,
    state: ExerciseScreenState,
    onCreate: () -> Unit,
    onOpenDetail: (String) -> Unit,
) {
    val strings = localizedContext()
    var query by state.searchQuery
    var zoneId by state.filterZoneId
    var unclassified by state.unclassifiedFilter
    val zones = repository.listBodyZones()
    val exercises = repository.listExercises(query, zoneId, true, false, unclassified)
    TrainlogScreen(strings.getString(R.string.exercise_catalog), scrollKey = "exercise-catalogue") {
        TrainlogPrimaryAction(strings.getString(R.string.route_exercise_create), strings.getString(R.string.create_exercise_description), onCreate)
        TrainlogInputField(strings.getString(R.string.search_prefix), query, { query = it })
        TrainlogChoiceGroup(strings.getString(R.string.filter)) {
            TrainlogChoice(strings.getString(R.string.all_zones), zoneId == null && !unclassified) {
                zoneId = null; unclassified = false
            }
            zones.filter { it.parentZoneId == null }.forEach { zone ->
                TrainlogChoice(localizedBodyZoneName(strings, zone.zoneId, zone.displayName), zoneId == zone.zoneId && !unclassified) {
                    zoneId = zone.zoneId; unclassified = false
                }
            }
            TrainlogChoice(strings.getString(R.string.not_specified_plural), unclassified) { zoneId = null; unclassified = true }
        }
        TrainlogFrame(strings.getString(R.string.catalog), active = exercises.isNotEmpty()) {
            if (exercises.isEmpty()) TrainlogInfo(strings.getString(R.string.exercise_none_found))
            if (exercises.isNotEmpty()) {
                /* CONTRACT: filtering and ordering stay repository-owned. This
                 * fixed viewport bounds presentation only; its child owns the
                 * catalogue scroll so filters remain visible above it. */
                Column(
                    Modifier
                        .fillMaxWidth()
                        .height(240.dp)
                        .verticalScroll(rememberScrollState())
                        .testTag("exercise-catalogue-scroll"),
                ) {
                    exercises.forEach { exercise ->
                        TrainlogAction(exercise.name, exerciseZoneSummary(repository, exercise, strings), {
                            state.selectedExerciseId = exercise.exerciseId
                            onOpenDetail(exercise.exerciseId)
                        })
                    }
                }
            }
        }
    }
}

@Composable
fun ExerciseDetailScreen(
    repository: TrainlogRepository,
    exerciseId: String,
    onModify: () -> Unit,
    onOpenMaxima: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    val context = remember(exerciseId) { repository.getTrainingExerciseContext(exerciseId) }
    TrainlogScreen(strings.getString(R.string.route_exercise_detail), scrollKey = "exercise-detail:$exerciseId") {
        if (context == null) {
            TrainlogInfo(strings.getString(R.string.exercise_not_found), colors.error)
            return@TrainlogScreen
        }
        TrainlogFrame(strings.getString(R.string.profile)) {
            TrainlogInfo(context.exercise.name, colors.accent)
            TrainlogInfo(exerciseZoneSummary(repository, context.exercise, strings))
            TrainlogInfo(profilePreview(context.exercise.recordingMode, context.exercise.trackingMode, context.exercise.dataFields))
            TrainlogAction(strings.getString(R.string.modify), strings.getString(R.string.exercise_modify_description), onModify)
        }
        TrainlogFrame(strings.getString(R.string.knowledge), active = context.knowledge != null) {
            context.knowledge?.let { KnowledgePanel(repository, it) }
                ?: TrainlogInfo(strings.getString(R.string.knowledge_none), colors.muted)
        }
        TrainlogFrame(strings.getString(R.string.compatible_equipment), active = context.compatibleEquipment.isNotEmpty()) {
            if (context.compatibleEquipment.isEmpty()) TrainlogInfo(strings.getString(R.string.compatibility_none))
            context.compatibleEquipment.forEach { equipment ->
                val name = repository.listEquipment().firstOrNull { it.equipmentId == equipment.equipmentId }?.displayName
                    ?: listOfNotNull(equipment.manufacturer, equipment.model).joinToString(" ").ifBlank { equipment.equipmentId }
                TrainlogInfo(name)
            }
        }
        context.latestExplicitMax?.let { max ->
            TrainlogFrame("MAX") {
                TrainlogInfo(strings.getString(R.string.latest_result, max.maxWeightKg, formatDate(max.startedAt)), colors.warning)
            }
        }
        TrainlogAction(strings.getString(R.string.view_latest_max), strings.getString(R.string.view_latest_max_description), onOpenMaxima)
    }
}

private fun knowledgeSummary(knowledge: ExerciseKnowledge, context: android.content.Context): String = when (knowledge.resolutionStatus) {
    ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED ->
        context.getString(R.string.science_classification_confidence, confidenceLabel(knowledge.confidence, context))
    ExerciseKnowledgeStatus.CONDITIONAL ->
        context.getString(R.string.conditional_classification)
    ExerciseKnowledgeStatus.UNRESOLVED -> context.getString(R.string.science_unresolved)
}

private fun confidenceLabel(confidence: KnowledgeConfidence, context: android.content.Context): String = when (confidence) {
    KnowledgeConfidence.HIGH -> context.getString(R.string.confidence_high)
    KnowledgeConfidence.MODERATE -> context.getString(R.string.confidence_moderate)
    KnowledgeConfidence.UNCERTAIN -> context.getString(R.string.confidence_uncertain)
}

@Composable
private fun BodyZoneChoices(
    zones: List<BodyZone>,
    includeGroups: Boolean = false,
    content: @Composable (BodyZone, Boolean) -> Unit,
) {
    val strings = localizedContext()
    zones.forEach { zone ->
        if (includeGroups || zone.kind != BodyZoneKind.GROUP) {
            content(zone, zone.parentZoneId != null)
        } else {
            TrainlogInfo(localizedBodyZoneName(strings, zone.zoneId, zone.displayName))
        }
    }
}

private fun exerciseZoneSummary(
    repository: TrainlogRepository,
    exercise: ExerciseProfile,
    context: android.content.Context,
): String {
    val primary = exercise.primaryZoneId?.let(repository::bodyZone)
        ?: return context.getString(R.string.zone_not_specified)
    val secondary = exercise.secondaryZoneIds.mapNotNull(repository::bodyZone)
    val group = repository.bodyZoneAncestors(primary.zoneId).firstOrNull()
    return buildString {
        append(context.getString(R.string.primary_zone_value, localizedBodyZoneName(context, primary.zoneId, primary.displayName)))
        append(context.getString(R.string.secondary_zones_value,
            if (secondary.isEmpty()) context.getString(R.string.value_none_feminine) else secondary.joinToString { localizedBodyZoneName(context, it.zoneId, it.displayName) }))
        group?.let { append(context.getString(R.string.group_value, localizedBodyZoneName(context, it.zoneId, it.displayName))) }
    }
}

/** INVARIANT: scientific catalog IDs/names are never rewritten; this derives presentation text only. */
private fun scientificCatalogLabel(id: String, frenchLabel: String, englishLabel: String?, french: Boolean): String =
    if (french) frenchLabel else englishLabel ?: id.split('_').joinToString(" ") { token ->
        token.replaceFirstChar { character -> character.uppercase() }
    }

@Composable
private fun TrainlogField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
) {
    TrainlogInputField(
        label = label,
        value = value,
        onValueChange =
            onValueChange,
    )
}

@Composable
private fun TrainlogChoiceGroup(
    label: String,
    content: @Composable () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    Column(
        modifier =
            Modifier.padding(
                bottom = 14.dp
            )
    ) {
        BasicText(
            text = label.uppercase(),
            modifier =
                Modifier.padding(
                    bottom = 5.dp
                ),
            style =
                TrainlogTypography.small.copy(
                    color = colors.muted,
                ),
        )

        content()
    }
}

@Composable
private fun TrainlogChoice(
    label: String,
    selected: Boolean,
    enabled: Boolean = true,
    onClick: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(vertical = 2.dp)
                .heightIn(min = 48.dp)
                .background(
                    if (selected) {
                        colors.surfaceAlt
                    } else {
                        colors.surface
                    }
                )
                .semantics { this.selected = selected }
                .clickable(
                    enabled = enabled,
                    onClick = onClick,
                )
                .padding(
                    horizontal = 10.dp,
                    vertical = 9.dp,
                )
    ) {
        BasicText(
            text =
                if (selected) {
                    "▌ $label"
                } else {
                    "  $label"
                },
            style =
                TrainlogTypography.normal.copy(
                    color =
                        if (selected) {
                            colors.accent
                        } else if (!enabled) {
                            colors.muted
                        } else {
                            colors.text
                        },
                    fontWeight =
                        if (selected) {
                            FontWeight.Bold
                        } else {
                            FontWeight.Normal
                        },
                ),
        )
    }
}


private fun profilePreview(
    recordingMode: RecordingMode,
    trackingMode: TrackingMode,
    dataFields: Int,
): String {
    val extras =
        buildList {
            if (
                dataFields and
                    ExerciseDataFields.SPEED_KMH != 0
            ) {
                add("VITESSE")
            }

            if (
                dataFields and
                    ExerciseDataFields.DISTANCE_KM != 0
            ) {
                add("DISTANCE")
            }
        }

    return buildString {
        append(recordingMode.name)
        append(" + ")
        append(trackingMode.name)

        for (extra in extras) {
            append(" + ")
            append(extra)
        }
    }
}
