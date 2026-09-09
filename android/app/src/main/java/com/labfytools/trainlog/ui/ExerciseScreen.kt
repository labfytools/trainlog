package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
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

@Composable
fun ExerciseScreen(
    repository: TrainlogRepository,
    inline: Boolean,
    onBack: () -> Unit,
    onSaved: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    var name by
        remember {
            mutableStateOf("")
        }

    var recordingMode by
        remember {
            mutableStateOf(
                RecordingMode.SETS
            )
        }

    var trackingMode by
        remember {
            mutableStateOf(
                TrackingMode.REPS
            )
        }

    var speed by
        remember {
            mutableStateOf(false)
        }

    var distance by
        remember {
            mutableStateOf(false)
        }

    var primaryZoneId by remember { mutableStateOf<String?>(null) }
    var secondaryZoneIds by remember { mutableStateOf(emptySet<String>()) }
    var searchQuery by remember { mutableStateOf("") }
    var filterZoneId by remember { mutableStateOf<String?>(null) }
    var unclassifiedFilter by remember { mutableStateOf(false) }
    var expandedKnowledgeIds by remember { mutableStateOf(emptySet<String>()) }
    val zones = repository.listBodyZones()

    var message by
        remember {
            mutableStateOf<String?>(
                null
            )
        }

    var editedExercise by
        remember {
            mutableStateOf<ExerciseProfile?>(null)
        }

    val profileLocked =
        editedExercise?.let {
            !repository.canEditExerciseProfile(it.exerciseId)
        } ?: false

    fun startEditing(exercise: ExerciseProfile) {
        /* WHY: edit state copies catalog metadata for presentation only. The
         * repository remains the sole owner of stable identity and SQLite. */
        editedExercise = exercise
        name = exercise.name
        recordingMode = exercise.recordingMode
        trackingMode = exercise.trackingMode
        speed = exercise.dataFields and ExerciseDataFields.SPEED_KMH != 0
        distance = exercise.dataFields and ExerciseDataFields.DISTANCE_KM != 0
        primaryZoneId = exercise.primaryZoneId
        secondaryZoneIds = exercise.secondaryZoneIds.toSet()
        message = null
    }

    TrainlogScreen(
        subtitle = "E X E R C I C E"
    ) {
        TrainlogAction(
            label =
                if (inline) {
                    "< Retour à la séance"
                } else {
                    "< Retour"
                },
            description =
                if (inline) {
                    "Retourner dans la séance en cours."
                } else {
                    "Revenir à l'accueil."
                },
            onClick = onBack,
            accent = colors.muted,
        )

        TrainlogFrame(
            title =
                if (editedExercise == null) {
                    "NOUVEL EXERCICE"
                } else {
                    "MODIFIER L'EXERCICE"
                }
        ) {
            TrainlogField(
                label = "Nom",
                value = name,
                onValueChange = {
                    name = it
                    message = null
                },
            )

            TrainlogChoiceGroup(
                label = "Organisation",
            ) {
                TrainlogChoice(
                    label = "Séries",
                    selected =
                        recordingMode ==
                            RecordingMode.SETS,
                    enabled = !profileLocked,
                    onClick = {
                        if (profileLocked) return@TrainlogChoice
                        recordingMode =
                            RecordingMode.SETS

                        speed = false
                        distance = false
                        message = null
                    },
                )

                TrainlogChoice(
                    label = "Continu",
                    selected =
                        recordingMode ==
                            RecordingMode.CONTINUOUS,
                    enabled = !profileLocked,
                    onClick = {
                        if (profileLocked) return@TrainlogChoice
                        recordingMode =
                            RecordingMode.CONTINUOUS

                        trackingMode =
                            TrackingMode.DURATION

                        message = null
                    },
                )
            }

            TrainlogChoiceGroup(
                label = "Mesure principale",
            ) {
                if (
                    recordingMode ==
                    RecordingMode.SETS
                ) {
                    TrainlogChoice(
                        label =
                            "Répétitions",
                        selected =
                            trackingMode ==
                                TrackingMode.REPS,
                        enabled = !profileLocked,
                        onClick = {
                            if (profileLocked) return@TrainlogChoice
                            trackingMode =
                                TrackingMode.REPS

                            message = null
                        },
                    )
                }

                TrainlogChoice(
                    label = "Durée",
                    selected =
                        trackingMode ==
                            TrackingMode.DURATION,
                    enabled = !profileLocked,
                    onClick = {
                        if (profileLocked) return@TrainlogChoice
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
                        "Données complémentaires",
                ) {
                    TrainlogChoice(
                        label = "Vitesse",
                        selected = speed,
                        enabled = !profileLocked,
                        onClick = {
                            if (profileLocked) return@TrainlogChoice
                            speed = !speed
                            message = null
                        },
                    )

                    TrainlogChoice(
                        label = "Distance",
                        selected = distance,
                        enabled = !profileLocked,
                        onClick = {
                            if (profileLocked) return@TrainlogChoice
                            distance =
                                !distance

                            message = null
                        },
                    )
                }
            }

            TrainlogChoiceGroup(label = "Zone principale") {
                TrainlogChoice(
                    label = "Non renseignée",
                    selected = primaryZoneId == null,
                    onClick = {
                        primaryZoneId = null
                        secondaryZoneIds = emptySet()
                        message = null
                    },
                )
                BodyZoneChoices(zones) { zone, indented ->
                    TrainlogChoice(
                        label = (if (indented) "  ↳ " else "") + zone.displayName,
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

            TrainlogChoiceGroup(label = "Zones secondaires") {
                BodyZoneChoices(zones) { zone, indented ->
                    TrainlogChoice(
                        label = (if (indented) "  ↳ " else "") + zone.displayName,
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

            if (profileLocked) {
                TrainlogInfo(
                    text =
                        "Profil verrouillé : cet exercice est déjà référencé " +
                            "par une séance terminée ou le brouillon actif. " +
                            "Le nom reste modifiable.",
                    color = colors.warning,
                )
            }

            TrainlogAction(
                label =
                    if (editedExercise != null) {
                        "Enregistrer les modifications"
                    } else if (inline) {
                        "Créer et revenir à la séance"
                    } else {
                        "Enregistrer l'exercice"
                    },
                description =
                    if (editedExercise == null) {
                        "Ajouter ce profil au catalogue local."
                    } else {
                        "Conserver l'identité et mettre à jour le catalogue."
                    },
                accent =
                    colors.success,
                onClick = save@{
                    if (editedExercise == null &&
                        recordingMode == RecordingMode.SETS && primaryZoneId == null) {
                        message = "Une zone principale est requise pour un nouvel exercice musculaire."
                        return@save
                    }
                    val current = editedExercise
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
                                    exerciseId = current.exerciseId,
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
                            onSaved()
                        }

                        CreateExerciseResult.Conflict -> {
                            message =
                                "Un exercice portant ce nom existe déjà."
                        }

                        CreateExerciseResult.Invalid -> {
                            message =
                                "Profil ou nom invalide."
                        }

                        is CreateExerciseResult.DatabaseError -> {
                            message = result.message
                        }

                        is EditExerciseResult.Saved -> {
                            message = null
                            editedExercise = null
                            onSaved()
                        }

                        EditExerciseResult.Conflict -> {
                            message = "Un autre exercice porte déjà ce nom."
                        }

                        EditExerciseResult.InvalidNameOrProfile -> {
                            message = "Nom ou profil invalide."
                        }

                        EditExerciseResult.IncompatibleProfileChange -> {
                            message =
                                "Le profil ne peut pas changer après utilisation."
                        }

                        EditExerciseResult.DatabaseError -> {
                            message = "Enregistrement en base impossible."
                        }
                    }
                },
            )

            if (editedExercise != null) {
                TrainlogAction(
                    label = "Annuler",
                    description = "Revenir au catalogue sans modification.",
                    accent = colors.muted,
                    onClick = {
                        editedExercise = null
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

        TrainlogFrame(title = "EXERCICES EXISTANTS", active = false) {
            TrainlogInputField(
                label = "Recherche par préfixe",
                value = searchQuery,
                onValueChange = { searchQuery = it },
            )
            TrainlogChoiceGroup(label = "Filtre par zone") {
                TrainlogChoice(
                    label = "Toutes les zones",
                    selected = filterZoneId == null && !unclassifiedFilter,
                    onClick = { filterZoneId = null; unclassifiedFilter = false },
                )
                BodyZoneChoices(zones, includeGroups = true) { zone, indented ->
                    TrainlogChoice(
                        label = (if (indented) "  ↳ " else "") + zone.displayName,
                        selected = filterZoneId == zone.zoneId && !unclassifiedFilter,
                        onClick = { filterZoneId = zone.zoneId; unclassifiedFilter = false },
                    )
                }
                TrainlogChoice(
                    label = "Non renseignés",
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
                TrainlogInfo("Aucun exercice enregistré.")
            } else {
                exercises.forEach { exercise ->
                    TrainlogAction(
                        label = "Modifier · ${exercise.name}",
                        description = exerciseZoneSummary(repository, exercise),
                        accent = colors.accent,
                        onClick = { startEditing(exercise) },
                    )
                    repository.getExerciseKnowledge(exercise.exerciseId)?.let { knowledge ->
                        val expanded = exercise.exerciseId in expandedKnowledgeIds
                        TrainlogAction(
                            label = if (expanded) "− Connaissances" else "+ Connaissances",
                            description = knowledgeSummary(knowledge),
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
                }
            }
        }

        TrainlogFrame(
            title = "CONTRAT",
            active = false,
        ) {
            TrainlogInfo(
                "CONTINUOUS force DURATION."
            )

            TrainlogInfo(
                "Aucune règle ne dépend du nom."
            )
        }
    }
}

@Composable
private fun KnowledgePanel(repository: TrainlogRepository, knowledge: ExerciseKnowledge) {
    val colors = LocalTrainlogColors.current
    /* WHY: conditional content is opened only under an explicit uncertainty
     * label; it cannot be mistaken for an ordinary resolved classification. */
    val interpretation = when (knowledge.resolutionStatus) {
        ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED -> knowledge.interpretation
        ExerciseKnowledgeStatus.CONDITIONAL -> knowledge.conditionalInterpretation
        ExerciseKnowledgeStatus.UNRESOLVED -> null
    }
    if (interpretation == null) {
        TrainlogInfo("Classification scientifique non résolue.", colors.warning)
        return
    }
    if (knowledge.resolutionStatus == ExerciseKnowledgeStatus.CONDITIONAL) {
        TrainlogInfo(
            "Interprétation conditionnelle · à confirmer : ${interpretation.requiredConfirmation.orEmpty()}",
            colors.warning,
        )
    }
    val patterns = interpretation.patternIds.mapNotNull(repository::getMovementPatternKnowledge)
    val primary = interpretation.primaryMuscleIds.mapNotNull(repository::getMuscleKnowledge)
    val secondary = interpretation.secondaryMuscleIds.mapNotNull(repository::getMuscleKnowledge)
    val primaryZone = repository.bodyZone(interpretation.primaryZoneId)?.displayName
        ?: interpretation.primaryZoneId
    val secondaryZones = interpretation.secondaryZoneIds.map { repository.bodyZone(it)?.displayName ?: it }
    val runtimeEquipment = repository.listEquipment().associateBy { it.equipmentId }
    val equipment = knowledge.equipmentIds.map { runtimeEquipment[it]?.displayName ?: it }
    TrainlogInfo("Mouvement : ${patterns.joinToString { it.displayNameFr }.ifEmpty { "Non classé" }}")
    TrainlogInfo("Muscles principaux : ${primary.joinToString { it.displayNameFr }.ifEmpty { "Non classés" }}")
    TrainlogInfo("Muscles secondaires : ${secondary.joinToString { it.displayNameFr }.ifEmpty { "Aucun établi" }}")
    TrainlogInfo(
        "Zones scientifiques : $primaryZone" +
            if (secondaryZones.isEmpty()) "" else " · secondaires : ${secondaryZones.joinToString()}",
    )
    TrainlogInfo("Équipement compatible : ${equipment.joinToString().ifEmpty { "Non établi" }}")
    TrainlogInfo("Confiance : ${confidenceLabel(interpretation.confidence)}", colors.muted)
}

private fun knowledgeSummary(knowledge: ExerciseKnowledge): String = when (knowledge.resolutionStatus) {
    ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED ->
        "Classification scientifique · confiance ${confidenceLabel(knowledge.confidence)}"
    ExerciseKnowledgeStatus.CONDITIONAL ->
        "Classification conditionnelle · incertitude explicite"
    ExerciseKnowledgeStatus.UNRESOLVED -> "Classification scientifique non résolue"
}

private fun confidenceLabel(confidence: KnowledgeConfidence): String = when (confidence) {
    KnowledgeConfidence.HIGH -> "élevée"
    KnowledgeConfidence.MODERATE -> "modérée"
    KnowledgeConfidence.UNCERTAIN -> "incertaine"
}

@Composable
private fun BodyZoneChoices(
    zones: List<BodyZone>,
    includeGroups: Boolean = false,
    content: @Composable (BodyZone, Boolean) -> Unit,
) {
    zones.forEach { zone ->
        if (includeGroups || zone.kind != BodyZoneKind.GROUP) {
            content(zone, zone.parentZoneId != null)
        } else {
            TrainlogInfo(zone.displayName)
        }
    }
}

private fun exerciseZoneSummary(
    repository: TrainlogRepository,
    exercise: ExerciseProfile,
): String {
    val primary = exercise.primaryZoneId?.let(repository::bodyZone)
        ?: return "Zone : Non renseignée"
    val secondary = exercise.secondaryZoneIds.mapNotNull(repository::bodyZone)
    val group = repository.bodyZoneAncestors(primary.zoneId).firstOrNull()
    return buildString {
        append("Zone principale : ${primary.displayName}")
        append(" · Zones secondaires : ")
        append(if (secondary.isEmpty()) "Aucune" else secondary.joinToString { it.displayName })
        group?.let { append(" · Groupe : ${it.displayName}") }
    }
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
                .background(
                    if (selected) {
                        colors.surfaceAlt
                    } else {
                        colors.surface
                    }
                )
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
                            colors.warning
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
