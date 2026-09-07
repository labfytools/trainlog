package com.labfytools.trainlog.ui

/* TRAINLOG_ANDROID_MAX_TEST_SESSION_V1 */

/* TRAINLOG_ANDROID_SESSION_REMOVE */

/* TRAINLOG_VARIABLE_SET_REPS_V1 */

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
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CreateEquipmentResult
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.data.FinalizeActiveDraftResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography

@Composable
fun SessionScreen(
    repository: TrainlogRepository,
    catalogRevision: Int,
    onBack: () -> Unit,
    onCreateExercise: () -> Unit,
    onSessionSaved: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    val exercises =
        remember(
            catalogRevision
        ) {
            repository.listExercises()
        }

    val initialLoad =
        remember(catalogRevision) {
            repository.loadActiveSessionDraft()
        }

    var activeDraft by
        remember(catalogRevision) {
            mutableStateOf(
                (initialLoad as?
                    ActiveDraftLoadResult.Loaded)
                    ?.draft
            )
        }

    var message by
        remember(catalogRevision) {
            mutableStateOf<
                String?
            >(
                when (initialLoad) {
                    is ActiveDraftLoadResult.Error ->
                        initialLoad.message

                    ActiveDraftLoadResult.None ->
                        "Aucune séance en cours."

                    is ActiveDraftLoadResult.Loaded ->
                        initialLoad.warning
                }
            )
        }

    var confirmingDiscard by
        remember {
            mutableStateOf(false)
        }

    val persistDraft:
        (ActiveSessionDraft, String?) -> Unit =
        { updated, successMessage ->
            when (
                val result =
                    repository
                        .saveActiveSessionDraft(
                            updated
                        )
            ) {
                ActiveDraftMutationResult.Saved -> {
                    activeDraft = updated
                    message = successMessage
                }

                is ActiveDraftMutationResult.Error -> {
                    message =
                        "Brouillon non sauvegardé : " +
                            result.message
                }
            }
        }

    TrainlogScreen(
        subtitle = "S E A N C E"
    ) {
        TrainlogAction(
            label = "< Retour",
            description =
                "Revenir à l'accueil sans supprimer la séance en cours.",
            /* CONTRACT: ordinary navigation never owns draft deletion. */
            onClick = onBack,
            accent = colors.muted,
        )

        if (activeDraft == null) {
            TrainlogFrame(
                title = "ERREUR"
            ) {
                TrainlogInfo(
                    text = message.orEmpty(),
                    color = colors.error,
                )
            }
            return@TrainlogScreen
        }

        val currentDraft = activeDraft!!

        val lastWriteFailed =
            message?.startsWith(
                "Brouillon non sauvegardé"
            ) == true
        TrainlogInfo(
            text =
                if (lastWriteFailed) {
                    "Dernière modification non sauvegardée."
                } else {
                    "Séance sauvegardée localement."
                },
            color =
                if (lastWriteFailed) {
                    colors.error
                } else {
                    colors.muted
                },
        )

        TrainlogFrame(
            title = "TYPE DE SEANCE"
        ) {
            TrainlogAction(
                label =
                    if (
                        currentDraft.sessionType ==
                        SessionType.TRAINING
                    ) {
                        "[✓] Entraînement"
                    } else {
                        "[ ] Entraînement"
                    },
                description =
                    "Séance normale de travail.",
                accent =
                    if (
                        currentDraft.sessionType ==
                        SessionType.TRAINING
                    ) {
                        colors.success
                    } else {
                        colors.muted
                    },
                onClick = {
                    persistDraft(
                        currentDraft.copy(
                            sessionType =
                                SessionType.TRAINING
                        ),
                        null,
                    )
                },
            )

            TrainlogAction(
                label =
                    if (
                        currentDraft.sessionType ==
                        SessionType.MAX_TEST
                    ) {
                        "[✓] Test max"
                    } else {
                        "[ ] Test max"
                    },
                description =
                    "Séance explicitement dédiée à une mesure de max.",
                accent =
                    if (
                        currentDraft.sessionType ==
                        SessionType.MAX_TEST
                    ) {
                        colors.warning
                    } else {
                        colors.muted
                    },
                onClick = {
                    persistDraft(
                        currentDraft.copy(
                            sessionType =
                                SessionType.MAX_TEST
                        ),
                        null,
                    )
                },
            )
        }

        TrainlogFrame(
            title = "SEANCE EN COURS"
        ) {
            TrainlogInfo(
                text =
                    "Type : " +
                        if (
                            currentDraft.sessionType ==
                            SessionType.MAX_TEST
                        ) {
                            "TEST MAX"
                        } else {
                            "ENTRAÎNEMENT"
                        },
                color =
                    if (
                        currentDraft.sessionType ==
                        SessionType.MAX_TEST
                    ) {
                        colors.warning
                    } else {
                        colors.accent
                    },
            )

            if (
                currentDraft.exercises.isEmpty()
            ) {
                TrainlogInfo(
                    "Aucun exercice ajouté."
                )
            } else {
                currentDraft.exercises
                    .forEachIndexed {
                            index,
                            draft ->

                        TrainlogInfo(
                            text =
                                "${index + 1}. " +
                                    draftSummary(
                                        draft
                                    ),
                            color =
                                colors.text,
                        )

                        TrainlogAction(
                            label = "Modifier ${draft.exercise.name}",
                            description = "Corriger cet exercice sans le supprimer de la séance.",
                            accent = colors.accent,
                            onClick = {
                                persistDraft(
                                    currentDraft.copy(
                                        form = formForExistingExercise(draft, index),
                                    ),
                                    null,
                                )
                            },
                        )

                        TrainlogAction(
                            label =
                                "Retirer ${draft.exercise.name}",
                            description =
                                "Supprimer cet exercice de la séance en cours.",
                            accent =
                                colors.error,
                            onClick = {
                                persistDraft(
                                    currentDraft.copy(
                                        exercises =
                                            currentDraft.exercises
                                                .filterIndexed {
                                                        itemIndex,
                                                        _ ->
                                                    itemIndex != index
                                                }
                                    ),
                                    "Exercice retiré de la séance.",
                                )
                            },
                        )
                    }
            }
        }

        TrainlogFrame(
            title = "CATALOGUE",
            active =
                exercises.isNotEmpty(),
        ) {
            if (
                exercises.isEmpty()
            ) {
                TrainlogInfo(
                    "Aucun exercice."
                )
            } else {
                exercises.forEach {
                        exercise ->

                    val alreadyAdded =
                        currentDraft.exercises.any {
                            it.exercise.exerciseId ==
                                exercise.exerciseId
                        }

                    CatalogChoice(
                        exercise =
                            exercise,
                        selected =
                            currentDraft.form
                                .selectedExercise
                                ?.exerciseId ==
                                exercise.exerciseId,
                    disabled = false,
                        onClick = {
                            if (true) {
                                persistDraft(
                                    currentDraft.copy(
                                        form =
                                            currentDraft.form.copy(
                                                selectedExercise =
                                                    exercise
                                            )
                                    ),
                                    null,
                                )
                            }
                        },
                    )
                }
            }
        }

        val editingExercise =
            currentDraft.form.selectedExercise

        if (editingExercise != null) {
            SessionExerciseForm(
                key =
                    editingExercise.exerciseId,
                repository = repository,
                exercise =
                    editingExercise,
                initialForm =
                    currentDraft.form,
                onFormChanged = {
                    form ->
                        persistDraft(
                            currentDraft.copy(
                                form = form.copy(
                                    editingExerciseIndex = currentDraft.form.editingExerciseIndex,
                                    editingEntryId = currentDraft.form.editingEntryId,
                                )
                            ),
                            null,
                        )
                },
                onCancel = {
                    persistDraft(
                        currentDraft.copy(
                            form =
                                SessionDraftForm()
                        ),
                        null,
                    )
                },
                onAdd = {
                    draft ->
                        val editIndex = currentDraft.form.editingExerciseIndex
                        persistDraft(
                            currentDraft.copy(
                                exercises = editIndex?.let { replacingIndex ->
                                    currentDraft.exercises.mapIndexed { index, existing ->
                                        if (index == replacingIndex) draft else existing
                                    }
                                } ?: (currentDraft.exercises + draft),
                                form =
                                    SessionDraftForm(),
                            ),
                            if (editIndex == null) "Exercice ajouté à la séance." else "Exercice modifié.",
                        )
                },
            )
        }

        TrainlogFrame(
            title = "EXERCICES"
        ) {
            TrainlogAction(
                label =
                    "Créer un nouvel exercice",
                description =
                    "Créer l'exercice sans quitter la saisie de séance.",
                onClick =
                    onCreateExercise,
                accent =
                    colors.success,
            )
        }

        TrainlogFrame(
            title = "ENREGISTREMENT",
            active =
                currentDraft.exercises.isNotEmpty(),
        ) {
            TrainlogAction(
                label =
                    "Enregistrer la séance",
                description =
                    "${currentDraft.exercises.size} exercice(s) dans la séance.",
                accent =
                    colors.success,
                onClick = {
                    when (
                        val result =
                            repository.finalizeActiveSessionDraft()
                    ) {
                        is FinalizeActiveDraftResult.Saved -> {
                            onSessionSaved()
                            onBack()
                        }

                        is FinalizeActiveDraftResult.Invalid -> {
                            message = result.message
                        }

                        is FinalizeActiveDraftResult.DatabaseError -> {
                            message =
                                "Échec de finalisation, brouillon conservé : " +
                                    result.message
                        }
                    }
                },
            )

            TrainlogAction(
                label = "Supprimer la séance en cours",
                description =
                    "Supprimer uniquement ce brouillon local.",
                accent = colors.error,
                onClick = {
                    confirmingDiscard = true
                },
            )

            if (confirmingDiscard) {
                TrainlogInfo(
                    text =
                        "Cette suppression n'ajoutera rien à l'historique.",
                    color = colors.error,
                )
                TrainlogAction(
                    label = "Confirmer la suppression",
                    description =
                        "Supprimer définitivement la séance en cours.",
                    accent = colors.error,
                    onClick = {
                        when (
                            val result =
                                repository
                                    .discardActiveSessionDraft()
                        ) {
                            ActiveDraftMutationResult.Saved -> {
                                confirmingDiscard = false
                                onBack()
                            }

                            is ActiveDraftMutationResult.Error -> {
                                message = result.message
                            }
                        }
                    },
                )
                TrainlogAction(
                    label = "Annuler",
                    description =
                        "Conserver la séance en cours.",
                    accent = colors.muted,
                    onClick = {
                        confirmingDiscard = false
                    },
                )
            }

            if (
                message != null
            ) {
                TrainlogInfo(
                    text =
                        message.orEmpty(),
                    color =
                        if (
                            message ==
                            "Exercice ajouté à la séance." ||
                            message ==
                            "Exercice retiré de la séance."
                        ) {
                            colors.success
                        } else {
                            colors.error
                        },
                )
            }
        }
    }
}

@Composable
private fun CatalogChoice(
    exercise: ExerciseProfile,
    selected: Boolean,
    disabled: Boolean,
    onClick: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    val profile =
        exerciseProfileLabel(
            exercise
        )

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
                    enabled = !disabled,
                    onClick = onClick,
                )
                .padding(
                    horizontal = 10.dp,
                    vertical = 9.dp,
                )
    ) {
        BasicText(
            text =
                (
                    if (disabled) {
                        "✓ "
                    } else if (selected) {
                        "▌ "
                    } else {
                        "  "
                    }
                ) +
                    exercise.name +
                    "  ·  " +
                    profile,
            style =
                TrainlogTypography.normal.copy(
                    color =
                        if (disabled) {
                            colors.muted
                        } else if (selected) {
                            colors.warning
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

@Composable
private fun SessionExerciseForm(
    key: String,
    repository: TrainlogRepository,
    exercise: ExerciseProfile,
    initialForm: SessionDraftForm,
    onFormChanged: (SessionDraftForm) -> Unit,
    onCancel: () -> Unit,
    onAdd:
        (SessionExerciseDraft) ->
            Unit,
) {
    val colors =
        LocalTrainlogColors.current

    var setCountText by
        remember(key) {
            mutableStateOf(
                initialForm.setCountText
            )
        }

    var repsText by
        remember(key) {
            mutableStateOf(
                initialForm.repsText
            )
        }

    var weightText by
        remember(key) {
            mutableStateOf(
                initialForm.weightText
            )
        }

    var durationText by
        remember(key) {
            mutableStateOf(
                initialForm.durationText
            )
        }

    var speedText by
        remember(key) {
            mutableStateOf(
                initialForm.speedText
            )
        }

    var distanceText by
        remember(key) {
            mutableStateOf(
                initialForm.distanceText
            )
        }

    var equipmentRevision by remember(key) { mutableStateOf(0) }
    val equipmentEntries = remember(equipmentRevision) { repository.listEquipment() }
    var equipmentSearch by remember(key) { mutableStateOf("") }
    var customEquipmentName by remember(key) { mutableStateOf("") }
    var selectedEquipmentId by remember(key) {
        mutableStateOf(initialForm.selectedEquipmentId)
    }

    var error by
        remember(key) {
            mutableStateOf<
                String?
            >(null)
        }

    TrainlogFrame(
        title =
            "SAISIE — ${exercise.name}"
    ) {
        TrainlogInfo(
            text =
                exerciseProfileLabel(
                    exercise
                ),
            color = colors.accent,
        )

        TrainlogInputField(
            label = "Machine / équipement (optionnel)",
            value = equipmentSearch,
            onValueChange = { equipmentSearch = it },
        )
        TrainlogInputField(
            label = "Nouvelle machine",
            value = customEquipmentName,
            onValueChange = { customEquipmentName = it },
        )
        TrainlogAction(
            label = "Créer la machine",
            description = "L'ajouter à votre catalogue puis la sélectionner pour cette entrée.",
            accent = colors.success,
            onClick = {
                when (val result = repository.createCustomEquipment(customEquipmentName)) {
                    is CreateEquipmentResult.Created -> {
                        customEquipmentName = ""
                        equipmentRevision += 1
                        selectedEquipmentId = result.equipment.equipmentId
                        onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, selectedEquipmentId, weightText))
                    }
                    CreateEquipmentResult.Invalid -> error = "Donnez un nom de machine valide."
                    CreateEquipmentResult.Conflict -> error = "Cette machine existe déjà."
                    is CreateEquipmentResult.DatabaseError -> error = "Machine non créée : ${result.message}"
                }
            },
        )
        val selectedEquipment = equipmentEntries.firstOrNull { it.equipmentId == selectedEquipmentId }
        if (selectedEquipment != null) {
            TrainlogAction(
                label = "✓ ${selectedEquipment.displayName}",
                description = selectedEquipment.labelName.ifBlank { "Équipement sélectionné." },
                accent = colors.success,
                onClick = {
                    selectedEquipmentId = null
                    onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, null, weightText))
                },
            )
        }
        repository.searchEquipment(equipmentSearch).take(8).forEach { equipment ->
            TrainlogAction(
                label = if (equipment.equipmentId == selectedEquipmentId) "✓ ${equipment.displayName}" else equipment.displayName,
                description = equipment.labelName.ifBlank { equipment.type },
                accent = if (equipment.equipmentId == selectedEquipmentId) colors.success else colors.muted,
                onClick = {
                    selectedEquipmentId = equipment.equipmentId
                    onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, equipment.equipmentId, weightText))
                },
            )
        }

        if (
            exercise.recordingMode ==
            RecordingMode.SETS
        ) {
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                SessionNumberField(
                    label =
                        "Séries / répétitions",
                    value =
                        repsText,
                    onValueChange = {
                        repsText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                setCountText,
                                it,
                                durationText,
                                speedText,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )

                TrainlogInfo(
                    text =
                        "Formats : 5x10 · 4,5,6,7 · 4..10..4",
                    color =
                        colors.muted,
                )

                SessionNumberField(
                    label = if (selectedEquipment?.loadSemantics == EquipmentLoadSemantics.ASSISTANCE) {
                        "Assistance (kg)"
                    } else {
                        "Charge (kg)"
                    },
                    value = weightText,
                    onValueChange = {
                        weightText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise, setCountText, repsText, durationText,
                                speedText, distanceText, selectedEquipmentId, it,
                            )
                        )
                    },
                )
                TrainlogInfo(
                    text = "Une valeur par série séparée par ; (ex. 12,5;15). Une seule valeur s'applique à toutes les séries.",
                    color = colors.muted,
                )
            } else {
                SessionNumberField(
                    label =
                        "Nombre de séries",
                    value =
                        setCountText,
                    onValueChange = {
                        setCountText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                it,
                                repsText,
                                durationText,
                                speedText,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )

                SessionNumberField(
                    label =
                        "Durée par série (secondes)",
                    value =
                        durationText,
                    onValueChange = {
                        durationText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                setCountText,
                                repsText,
                                it,
                                speedText,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )
            }
        } else {
            SessionNumberField(
                label =
                    "Durée (minutes)",
                value =
                    durationText,
                onValueChange = {
                    durationText = it
                    error = null
                    onFormChanged(
                        currentForm(
                            exercise,
                            setCountText,
                            repsText,
                            it,
                                speedText,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                        )
                    )
                },
            )

            if (
                exercise.dataFields and
                    ExerciseDataFields
                        .SPEED_KMH != 0
            ) {
                SessionNumberField(
                    label =
                        "Vitesse km/h",
                    value =
                        speedText,
                    onValueChange = {
                        speedText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                setCountText,
                                repsText,
                                durationText,
                                it,
                                distanceText,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )
            }

            if (
                exercise.dataFields and
                    ExerciseDataFields
                        .DISTANCE_KM != 0
            ) {
                SessionNumberField(
                    label =
                        "Distance km",
                    value =
                        distanceText,
                    onValueChange = {
                        distanceText = it
                        error = null
                        onFormChanged(
                            currentForm(
                                exercise,
                                setCountText,
                                repsText,
                                durationText,
                                speedText,
                                it,
                                selectedEquipmentId,
                                weightText,
                            )
                        )
                    },
                )
            }
        }

        TrainlogAction(
            label =
                "Ajouter à la séance",
            description =
                "Ajouter cette saisie au brouillon.",
            accent =
                colors.success,
            onClick = {
                val draft =
                    buildSessionExerciseDraft(
                        exercise =
                            exercise,
                        setCountText =
                            setCountText,
                        repsText =
                            repsText,
                        durationText =
                            durationText,
                        speedText =
                            speedText,
                        distanceText =
                            distanceText,
                        equipmentId = selectedEquipmentId,
                        weightText = weightText,
                        entryId = initialForm.editingEntryId,
                    )

                if (draft == null) {
                    error =
                        "Valeurs invalides."
                } else {
                    onAdd(draft)
                }
            },
        )

        TrainlogAction(
            label =
                "Annuler la saisie",
            description =
                "Revenir au catalogue.",
            accent =
                colors.muted,
            onClick =
                onCancel,
        )

        if (
            error != null
        ) {
            TrainlogInfo(
                text =
                    error.orEmpty(),
                color = colors.error,
            )
        }
    }
}

private fun currentForm(
    exercise: ExerciseProfile,
    setCountText: String,
    repsText: String,
    durationText: String,
    speedText: String,
    distanceText: String,
    equipmentId: String? = null,
    weightText: String = "",
): SessionDraftForm =
    SessionDraftForm(
        selectedExercise = exercise,
        selectedEquipmentId = equipmentId,
        setCountText = setCountText,
        repsText = repsText,
        weightText = weightText,
        durationText = durationText,
        speedText = speedText,
        distanceText = distanceText,
    )

@Composable
private fun SessionNumberField(
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

private const val MAX_SESSION_SETS = 64
private const val MAX_REPS_PER_SET = 10000

private fun parseRepSequence(
    text: String,
): List<Int>? {
    val normalized =
        text.trim()
            .lowercase()
            .replace(
                '×',
                'x'
            )

    if (normalized.isEmpty()) {
        return null
    }

    val repeated =
        Regex(
            """^(\d+)\s*x\s*(\d+)$"""
        ).matchEntire(
            normalized
        )

    if (repeated != null) {
        val count =
            repeated.groupValues[1]
                .toIntOrNull()

        val reps =
            repeated.groupValues[2]
                .toIntOrNull()

        if (
            count == null ||
            reps == null ||
            count !in 1..MAX_SESSION_SETS ||
            reps !in 0..MAX_REPS_PER_SET
        ) {
            return null
        }

        return List(count) {
            reps
        }
    }

    val pyramid =
        Regex(
            """^(\d+)\s*\.\.\s*(\d+)\s*\.\.\s*(\d+)$"""
        ).matchEntire(
            normalized
        )

    if (pyramid != null) {
        val start =
            pyramid.groupValues[1]
                .toIntOrNull()

        val peak =
            pyramid.groupValues[2]
                .toIntOrNull()

        val end =
            pyramid.groupValues[3]
                .toIntOrNull()

        if (
            start == null ||
            peak == null ||
            end == null ||
            start !in 0..MAX_REPS_PER_SET ||
            peak !in 0..MAX_REPS_PER_SET ||
            end !in 0..MAX_REPS_PER_SET ||
            start > peak ||
            end > peak
        ) {
            return null
        }

        val values =
            mutableListOf<Int>()

        for (value in start..peak) {
            values += value

            if (
                values.size >
                MAX_SESSION_SETS
            ) {
                return null
            }
        }

        if (peak > end) {
            for (
                value in
                    (peak - 1) downTo end
            ) {
                values += value

                if (
                    values.size >
                    MAX_SESSION_SETS
                ) {
                    return null
                }
            }
        }

        return values
    }

    val parts =
        normalized
            .split(
                Regex(
                    """[\s,;]+"""
                )
            )
            .filter {
                it.isNotEmpty()
            }

    if (
        parts.isEmpty() ||
        parts.size >
        MAX_SESSION_SETS
    ) {
        return null
    }

    val values =
        mutableListOf<Int>()

    for (part in parts) {
        val reps =
            part.toIntOrNull()
                ?: return null

        if (
            reps !in
            0..MAX_REPS_PER_SET
        ) {
            return null
        }

        values += reps
    }

    return values
}

private fun buildSessionExerciseDraft(
    exercise: ExerciseProfile,
    setCountText: String,
    repsText: String,
    durationText: String,
    speedText: String,
    distanceText: String,
    equipmentId: String? = null,
    weightText: String = "",
    entryId: String? = null,
): SessionExerciseDraft? {
    return if (
        exercise.recordingMode ==
        RecordingMode.CONTINUOUS
    ) {
        val minutes =
            durationText.toIntOrNull()

        if (
            minutes == null ||
            minutes <= 0 ||
            minutes > 1440
        ) {
            null
        } else {
            val wantsSpeed =
                exercise.dataFields and
                    ExerciseDataFields.SPEED_KMH != 0

            val wantsDistance =
                exercise.dataFields and
                    ExerciseDataFields.DISTANCE_KM != 0

            val speed =
                if (wantsSpeed) {
                    speedText
                        .replace(',', '.')
                        .toDoubleOrNull()
                } else {
                    null
                }

            val distance =
                if (wantsDistance) {
                    distanceText
                        .replace(',', '.')
                        .toDoubleOrNull()
                } else {
                    null
                }

            if (
                (
                    wantsSpeed &&
                    (
                        speed == null ||
                        speed <= 0.0
                    )
                ) ||
                (
                    wantsDistance &&
                    (
                        distance == null ||
                        distance <= 0.0
                    )
                )
            ) {
                null
            } else {
                SessionExerciseDraft(
                    entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
                    exercise = exercise,
                    equipmentId = equipmentId,
                    continuousDurationSeconds =
                        minutes * 60,
                    speedKmh = speed,
                    distanceKm = distance,
                )
            }
        }
    } else if (
        exercise.trackingMode ==
        TrackingMode.REPS
    ) {
        val reps =
            parseRepSequence(
                repsText
            ) ?: return null

        val weights = parseWeightSequence(weightText, reps.size) ?: return null

        SessionExerciseDraft(
            entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
            exercise = exercise,
            equipmentId = equipmentId,
            sets =
                reps.mapIndexed { index, rep ->
                    SessionSetDraft(
                        reps = rep,
                        weightKg = weights[index],
                    )
                },
        )
    } else {
        val count =
            setCountText.toIntOrNull()

        val seconds =
            durationText.toIntOrNull()

        if (
            count == null ||
            count <= 0 ||
            count > MAX_SESSION_SETS ||
            seconds == null ||
            seconds <= 0 ||
            seconds > 86400
        ) {
            null
        } else {
            SessionExerciseDraft(
                entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
                exercise = exercise,
                equipmentId = equipmentId,
                sets =
                    List(count) {
                        SessionSetDraft(
                            durationSeconds =
                                seconds
                        )
                    },
            )
        }
    }
}

/** Reconstruct editable text from the entry itself; editing never mutates a
 * different entry or the global exercise definition. */
private fun formForExistingExercise(
    draft: SessionExerciseDraft,
    index: Int,
): SessionDraftForm =
    SessionDraftForm(
        selectedExercise = draft.exercise,
        editingExerciseIndex = index,
        editingEntryId = draft.entryId,
        selectedEquipmentId = draft.equipmentId,
        setCountText = draft.sets.size.toString(),
        repsText = if (draft.exercise.trackingMode == TrackingMode.REPS) {
            draft.sets.joinToString(",") { it.reps.toString() }
        } else {
            "3x10"
        },
        weightText = draft.sets.mapNotNull { it.weightKg }.joinToString(";") { "%g".format(java.util.Locale.FRANCE, it) },
        durationText = if (draft.exercise.recordingMode == RecordingMode.CONTINUOUS) {
            (draft.continuousDurationSeconds / 60).toString()
        } else {
            draft.sets.firstOrNull()?.durationSeconds?.toString() ?: "30"
        },
        speedText = draft.speedKmh?.toString().orEmpty(),
        distanceText = draft.distanceKm?.toString().orEmpty(),
    )

/** Accept French decimal commas without confusing them with the set separator.
 * CONTRACT: blank means no load recorded; zero is a real explicit value. */
private fun parseWeightSequence(text: String, count: Int): List<Double?>? {
    if (text.trim().isEmpty()) return List(count) { null }
    val values = text.split(';').map { token ->
        token.trim().replace(',', '.').toDoubleOrNull()
    }
    if (values.any { it == null || !it.isFinite() || it < 0.0 }) return null
    @Suppress("UNCHECKED_CAST")
    val parsed = values as List<Double>
    return when {
        parsed.size == 1 -> List(count) { parsed.single() }
        parsed.size == count -> parsed
        else -> null
    }
}

private fun draftSummary(
    draft: SessionExerciseDraft,
): String {
    return if (
        draft.exercise.recordingMode ==
        RecordingMode.CONTINUOUS
    ) {
        buildString {
            append(
                draft.exercise.name
            )

            append(
                " · ${draft.continuousDurationSeconds / 60} min"
            )

            draft.speedKmh?.let {
                append(
                    " · %.1f km/h"
                        .format(it)
                )
            }

            draft.distanceKm?.let {
                append(
                    " · %.2f km"
                        .format(it)
                )
            }
        }
    } else if (
        draft.exercise.trackingMode ==
        TrackingMode.REPS
    ) {
        val reps =
            draft.sets.map {
                it.reps
            }

        if (
            reps.isNotEmpty() &&
            reps.all {
                it == reps.first()
            }
        ) {
            (
                "${draft.exercise.name} · " +
                "${reps.size} × " +
                "${reps.first()} reps"
            )
        } else {
            (
                "${draft.exercise.name} · " +
                "${reps.size} séries · " +
                reps.joinToString(
                    separator = ","
                ) +
                " reps"
            )
        }
    } else {
        val first =
            draft.sets.firstOrNull()

        (
            "${draft.exercise.name} · " +
                "${draft.sets.size} × " +
                "${first?.durationSeconds ?: 0} s"
        )
    }
}

private fun exerciseProfileLabel(
    exercise: ExerciseProfile,
): String {
    return buildString {
        append(
            if (
                exercise.recordingMode ==
                RecordingMode.CONTINUOUS
            ) {
                "CONTINU"
            } else {
                "SERIES"
            }
        )

        append(" · ")

        append(
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                "REPS"
            } else {
                "DUREE"
            }
        )

        if (
            exercise.dataFields and
                ExerciseDataFields
                    .SPEED_KMH != 0
        ) {
            append(" · VITESSE")
        }

        if (
            exercise.dataFields and
                ExerciseDataFields
                    .DISTANCE_KM != 0
        ) {
            append(" · DISTANCE")
        }
    }
}
