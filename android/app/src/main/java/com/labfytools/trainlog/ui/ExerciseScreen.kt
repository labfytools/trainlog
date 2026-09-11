package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
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
        require(!dirty) { "un éditeur sale ne peut pas être remplacé" }
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

    val profileLocked =
        editedExerciseId?.let {
            !repository.canEditExerciseProfile(it)
        } ?: false

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
        subtitle = if (editedExerciseId == null) "Créer un exercice" else "Modifier l'exercice"
    ) {
        TrainlogFrame(
            title =
                if (editedExerciseId == null) {
                    "Nouvel exercice"
                } else {
                    "Modifier l'exercice"
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

            TrainlogPrimaryAction(
                label =
                    if (editedExerciseId != null) {
                        "Enregistrer les modifications"
                    } else if (inline) {
                        "Créer et revenir à la séance"
                    } else {
                        "Enregistrer l'exercice"
                    },
                description =
                    if (editedExerciseId == null) {
                        "Ajouter ce profil au catalogue local."
                    } else {
                        "Conserver l'identité et mettre à jour le catalogue."
                    },
                onClick = save@{
                    if (editedExerciseId == null &&
                        recordingMode == RecordingMode.SETS && primaryZoneId == null) {
                        message = "Une zone principale est requise pour un nouvel exercice musculaire."
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
                            editedExerciseId = null
                            state.abandonEdits()
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

            if (editedExerciseId != null) {
                TrainlogAction(
                    label = "Annuler",
                    description = "Revenir au catalogue sans modification.",
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

        if (catalogueVisible) TrainlogFrame(title = "Exercices existants", active = false) {
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
                    TrainlogAction(
                        label = "Voir les derniers MAX",
                        description = "Ouvrir la liste agrégée existante dans Statistiques.",
                        accent = colors.warning,
                        onClick = onOpenMaxima,
                    )
                }
            }
        }

        TrainlogFrame(
            title = "Contrat",
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
fun ExerciseEditorRoute(
    repository: TrainlogRepository,
    state: ExerciseScreenState,
    exerciseId: String?,
    inline: Boolean,
    onSaved: () -> Unit,
) {
    val profile = remember(exerciseId) {
        exerciseId?.let { id -> repository.listExercises().firstOrNull { it.exerciseId == id } }
    }
    LaunchedEffect(exerciseId) {
        if (profile == null) state.prepareCreate() else state.prepareEdit(profile)
    }
    if (exerciseId != null && profile == null) {
        TrainlogScreen("Modifier l'exercice") { TrainlogInfo("Exercice introuvable.") }
    } else if (state.editedExerciseId.value != exerciseId) {
        TrainlogScreen("Modifier l'exercice") { TrainlogInfo("Chargement du profil…") }
    } else {
        ExerciseScreen(repository, state, inline, {}, onSaved, {}, catalogueVisible = false)
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
        TrainlogInfo("Confiance : ${confidenceLabel(knowledge.confidence)}", colors.muted)
    } else if (knowledge.resolutionStatus == ExerciseKnowledgeStatus.CONDITIONAL) {
        TrainlogInfo(
            "Interprétation conditionnelle · à confirmer : ${interpretation.requiredConfirmation.orEmpty()}",
            colors.warning,
        )
    }
    interpretation?.let { resolved ->
        val patterns = resolved.patternIds.mapNotNull(repository::getMovementPatternKnowledge)
        val primary = resolved.primaryMuscleIds.mapNotNull(repository::getMuscleKnowledge)
        val secondary = resolved.secondaryMuscleIds.mapNotNull(repository::getMuscleKnowledge)
        val primaryZone = repository.bodyZone(resolved.primaryZoneId)?.displayName ?: resolved.primaryZoneId
        val secondaryZones = resolved.secondaryZoneIds.map { repository.bodyZone(it)?.displayName ?: it }
        val runtimeEquipment = repository.listEquipment().associateBy { it.equipmentId }
        val equipment = knowledge.equipmentIds.map { runtimeEquipment[it]?.displayName ?: it }
        TrainlogInfo("Mouvement : ${patterns.joinToString { it.displayNameFr }.ifEmpty { "Non classé" }}")
        TrainlogInfo("Muscles principaux : ${primary.joinToString { it.displayNameFr }.ifEmpty { "Non classés" }}")
        TrainlogInfo("Muscles secondaires : ${secondary.joinToString { it.displayNameFr }.ifEmpty { "Aucun établi" }}")
        TrainlogInfo("Zones scientifiques : $primaryZone" + if (secondaryZones.isEmpty()) "" else " · secondaires : ${secondaryZones.joinToString()}")
        TrainlogInfo("Équipement compatible : ${equipment.joinToString().ifEmpty { "Non établi" }}")
        TrainlogInfo("Confiance : ${confidenceLabel(resolved.confidence)}", colors.muted)
    }
    knowledge.limitations.forEach { TrainlogInfo("Limite : $it", colors.muted) }
    knowledge.sourceRefs.mapNotNull(repository::getScienceReference).forEach { reference ->
        TrainlogInfo(
            "Source : ${reference.authorsOrOrganization} · ${reference.title}" +
                (reference.year?.let { " ($it)" } ?: ""),
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
    var query by state.searchQuery
    var zoneId by state.filterZoneId
    var unclassified by state.unclassifiedFilter
    val zones = repository.listBodyZones()
    val exercises = repository.listExercises(query, zoneId, true, false, unclassified)
    TrainlogScreen("Catalogue d'exercices", scrollKey = "exercise-catalogue") {
        TrainlogPrimaryAction("Créer un exercice", "Ajouter un profil au catalogue local.", onCreate)
        TrainlogInputField("Recherche par préfixe", query, { query = it })
        TrainlogChoiceGroup("Filtrer") {
            TrainlogChoice("Toutes les zones", zoneId == null && !unclassified) {
                zoneId = null; unclassified = false
            }
            zones.filter { it.parentZoneId == null }.forEach { zone ->
                TrainlogChoice(zone.displayName, zoneId == zone.zoneId && !unclassified) {
                    zoneId = zone.zoneId; unclassified = false
                }
            }
            TrainlogChoice("Non renseignés", unclassified) { zoneId = null; unclassified = true }
        }
        TrainlogFrame("Catalogue", active = exercises.isNotEmpty()) {
            if (exercises.isEmpty()) TrainlogInfo("Aucun exercice trouvé.")
            exercises.forEach { exercise ->
                TrainlogAction(exercise.name, exerciseZoneSummary(repository, exercise), {
                    state.selectedExerciseId = exercise.exerciseId
                    onOpenDetail(exercise.exerciseId)
                })
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
    val context = remember(exerciseId) { repository.getTrainingExerciseContext(exerciseId) }
    TrainlogScreen("Fiche exercice", scrollKey = "exercise-detail:$exerciseId") {
        if (context == null) {
            TrainlogInfo("Exercice introuvable.", colors.error)
            return@TrainlogScreen
        }
        TrainlogFrame("Profil") {
            TrainlogInfo(context.exercise.name, colors.accent)
            TrainlogInfo(exerciseZoneSummary(repository, context.exercise))
            TrainlogInfo(profilePreview(context.exercise.recordingMode, context.exercise.trackingMode, context.exercise.dataFields))
            TrainlogAction("Modifier", "Modifier le nom, le profil et les zones selon les règles existantes.", onModify)
        }
        TrainlogFrame("Connaissances", active = context.knowledge != null) {
            context.knowledge?.let { KnowledgePanel(repository, it) }
                ?: TrainlogInfo("Aucune connaissance scientifique liée à cet identifiant.", colors.muted)
        }
        TrainlogFrame("Équipements compatibles", active = context.compatibleEquipment.isNotEmpty()) {
            if (context.compatibleEquipment.isEmpty()) TrainlogInfo("Compatibilité non établie.")
            context.compatibleEquipment.forEach { equipment ->
                val name = listOfNotNull(equipment.manufacturer, equipment.model).joinToString(" ").ifBlank { equipment.equipmentId }
                TrainlogInfo(name)
            }
        }
        context.latestExplicitMax?.let { max ->
            TrainlogFrame("MAX") {
                TrainlogInfo("Dernier résultat : ${max.maxWeightKg} kg · ${max.startedAt.take(10)}", colors.warning)
            }
        }
        TrainlogAction("Voir les derniers MAX", "Ouvrir la liste agrégée existante dans Statistiques.", onOpenMaxima)
    }
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
