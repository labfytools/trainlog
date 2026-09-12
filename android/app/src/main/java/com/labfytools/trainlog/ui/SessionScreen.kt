package com.labfytools.trainlog.ui

/* TRAINLOG_ANDROID_MAX_TEST_SESSION_V1 */

/* TRAINLOG_ANDROID_SESSION_REMOVE */

/* TRAINLOG_VARIABLE_SET_REPS_V1 */

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CreateEquipmentResult
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.data.FinalizeActiveDraftResult
import com.labfytools.trainlog.data.ManualPercentMaxResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.SessionLoadMode
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography
import java.text.Normalizer
import java.util.Locale

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
        subtitle = "Séance en cours"
    ) {
        if (activeDraft == null) {
            TrainlogFrame(
                title = "Erreur"
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
            title = "Type de séance"
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

        ExercisePicker(
            exercises = exercises,
            selectedExercise = currentDraft.form.selectedExercise,
            onExerciseSelected = { exercise ->
                /* CONTRACT: only a tapped catalogue result changes the durable
                 * form identity. The transient query is never an exercise. */
                persistDraft(
                    currentDraft.copy(
                        form = currentDraft.form.copy(selectedExercise = exercise),
                    ),
                    null,
                )
            },
        )

        TrainlogFrame(
            title = "Séance en cours"
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
                        draft.plan?.let { plan ->
                            TrainlogInfo(
                                "Plan : ${plan.sets} × ${plan.reps ?: plan.durationSeconds} · " +
                                    "repos ${plan.restSeconds} s · " +
                                    (plan.weightKg?.let { "charge cible $it kg" }
                                        ?: "aucune charge numérique proposée"),
                                color = colors.muted,
                            )
                            if (draft.sets.isEmpty()) TrainlogInfo(
                                "Aucune série réalisée saisie : ajoutez les valeurs réellement effectuées.",
                                color = colors.warning,
                            )
                        }

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

                        if (currentDraft.sourceSessionId == null) {
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
        }

        val editingExercise =
            currentDraft.form.selectedExercise

        if (editingExercise != null) {
            SessionExerciseForm(
                key =
                    "${editingExercise.exerciseId}:${currentDraft.sessionType.wireValue}:" +
                        currentDraft.form.editingEntryId.orEmpty(),
                repository = repository,
                exercise =
                    editingExercise,
                sessionType = currentDraft.sessionType,
                initialForm =
                    currentDraft.form,
                initialPlan = currentDraft.form.editingExerciseIndex?.let {
                    currentDraft.exercises.getOrNull(it)?.plan
                },
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
                                        /* INVARIANT: normal performed-value edits
                                         * preserve generator planning metadata. */
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
            title = "Exercices"
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
            title = "Enregistrement",
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
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
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
                .then(modifier)
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
                .clickable(
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
                    if (selected) {
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
                        if (selected) {
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

/**
 * WHY: the session picker keeps query state outside [SessionDraftForm] so
 * typing and cancelling are harmless to the selected exercise and raw values.
 * CONTRACT: matching is presentation-only prefix matching; it never changes
 * stored names or treats query text as a catalogue identity.
 * INVARIANT: [exercises] is the complete, stable repository catalogue, so a
 * movement already used in this session remains selectable for another entry.
 */
@Composable
private fun ExercisePicker(
    exercises: List<ExerciseProfile>,
    selectedExercise: ExerciseProfile?,
    onExerciseSelected: (ExerciseProfile) -> Unit,
) {
    val colors = LocalTrainlogColors.current
    var searchOpen by remember(selectedExercise?.exerciseId) {
        mutableStateOf(selectedExercise == null)
    }
    var query by remember(selectedExercise?.exerciseId) { mutableStateOf("") }
    val results = remember(exercises, query) { exercisePrefixMatches(exercises, query) }

    TrainlogFrame(title = "Exercice", active = exercises.isNotEmpty()) {
        if (exercises.isEmpty()) {
            TrainlogInfo("Aucun exercice.")
            return@TrainlogFrame
        }

        TrainlogAction(
            label = selectedExercise?.name ?: "Choisir un exercice",
            description = "Exercice sélectionné. Appuyer pour afficher les suggestions.",
            accent = if (selectedExercise == null) colors.muted else colors.success,
            modifier = Modifier.testTag("exercise-picker-open"),
            onClick = { searchOpen = true },
        )

        TrainlogInputField(
            label = "Rechercher un exercice…",
            value = query,
            onValueChange = {
                query = it
                searchOpen = true
            },
            testTag = "exercise-search-input",
        )

        if (!searchOpen) {
            return@TrainlogFrame
        }

        if (results.isEmpty()) {
            TrainlogInfo("Aucun exercice trouvé", color = colors.muted)
        } else {
            /* The viewport stays keyboard-friendly while LazyColumn preserves
             * access to every prefix result instead of truncating it. */
            LazyColumn(
                modifier = Modifier.heightIn(max = 280.dp).testTag("exercise-search-results"),
            ) {
                items(results, key = { it.exerciseId }) { exercise ->
                    CatalogChoice(
                        exercise = exercise,
                        selected = selectedExercise?.exerciseId == exercise.exerciseId,
                        modifier = Modifier.testTag("exercise-search-result-${exercise.exerciseId}"),
                        onClick = {
                            onExerciseSelected(exercise)
                            query = ""
                            searchOpen = false
                        },
                    )
                }
            }
        }

        if (selectedExercise != null) {
            TrainlogAction(
                label = "Annuler la recherche",
                description = "Conserver ${selectedExercise.name} et les valeurs saisies.",
                accent = colors.muted,
                onClick = {
                    query = ""
                    searchOpen = false
                },
            )
        }
    }
}

internal fun exercisePrefixMatches(
    exercises: List<ExerciseProfile>,
    query: String,
): List<ExerciseProfile> {
    val normalizedQuery = normalizeExerciseSearchText(query)
    return exercises.filter { exercise ->
        normalizedQuery.isEmpty() ||
            normalizeExerciseSearchText(exercise.name).startsWith(normalizedQuery)
    }
}

private fun normalizeExerciseSearchText(value: String): String =
    Normalizer.normalize(value, Normalizer.Form.NFD)
        .replace("\\p{M}+".toRegex(), "")
        .lowercase(Locale.ROOT)
        .trim()
        .replace("\\s+".toRegex(), " ")

internal enum class ManualTargetChoice { KG, PERCENT_MAX, NONE }

/**
 * Build occurrence planning metadata separately from performed rows.
 * CONTRACT: an existing generated/manual dose keeps its sets/reps/duration/rest;
 * only the user-selected load target changes. A new manual occurrence derives
 * its initial dose shape from the confirmed performed-row form without copying
 * target weight into any performed set.
 */
internal fun buildManualTargetPlan(
    draft: SessionExerciseDraft,
    existing: SessionExercisePlan?,
    choice: ManualTargetChoice,
    directKgText: String,
    percentResult: ManualPercentMaxResult?,
    equipmentSemantics: EquipmentLoadSemantics?,
): Result<SessionExercisePlan?> {
    if (choice == ManualTargetChoice.NONE) return Result.success(null)
    val weight = when (choice) {
        ManualTargetChoice.KG -> directKgText.trim().replace(',', '.').toDoubleOrNull()
            ?.takeIf { it.isFinite() && it > 0.0 }
            ?: return Result.failure(IllegalArgumentException(
                "Saisissez une charge cible strictement positive."))
        ManualTargetChoice.PERCENT_MAX ->
            (percentResult as? ManualPercentMaxResult.Available)?.targetWeightKg
                ?: return Result.failure(IllegalArgumentException(
                    (percentResult as? ManualPercentMaxResult.Unavailable)?.message
                        ?: "MAX compatible indisponible."))
        ManualTargetChoice.NONE -> error("handled above")
    }
    val mode = when (equipmentSemantics) {
        EquipmentLoadSemantics.EXTERNAL -> SessionLoadMode.EXTERNAL
        EquipmentLoadSemantics.ASSISTANCE -> if (choice == ManualTargetChoice.PERCENT_MAX)
            return Result.failure(IllegalArgumentException(
                "Le %MAX est indisponible pour une assistance ; choisissez une résistance externe."))
        else SessionLoadMode.ASSISTANCE
        else -> return Result.failure(IllegalArgumentException(
            "Choisissez un équipement compatible avec une charge cible."))
    }
    val dose = existing ?: when (draft.exercise.trackingMode) {
        TrackingMode.REPS -> SessionExercisePlan(
            sets = draft.sets.size,
            reps = draft.sets.firstOrNull()?.reps,
        )
        TrackingMode.DURATION -> SessionExercisePlan(
            sets = draft.sets.size,
            durationSeconds = draft.sets.firstOrNull()?.durationSeconds,
        )
    }
    if (dose.sets !in 1..MAX_SESSION_SETS ||
        (draft.exercise.trackingMode == TrackingMode.REPS &&
            (dose.reps == null || dose.reps !in 1..MAX_REPS_PER_SET)) ||
        (draft.exercise.trackingMode == TrackingMode.DURATION &&
            (dose.durationSeconds == null || dose.durationSeconds <= 0)))
        return Result.failure(IllegalArgumentException(
            "Définissez une dose cible positive avant la charge cible."))
    return Result.success(dose.copy(weightKg = weight, loadMode = mode))
}

@Composable
private fun SessionExerciseForm(
    key: String,
    repository: TrainlogRepository,
    exercise: ExerciseProfile,
    sessionType: SessionType,
    initialForm: SessionDraftForm,
    initialPlan: SessionExercisePlan?,
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

    val initialSetRows =
        remember(key) {
            rawSetRowsFromForm(initialForm)
        }

    var repsText by
        remember(key) {
            mutableStateOf(
                encodeRawReps(initialSetRows)
            )
        }

    var weightText by
        remember(key) {
            mutableStateOf(
                encodeRawWeights(initialSetRows)
            )
        }

    /* CONTRACT: SETS + REPS is edited as occurrence-owned rows. The raw
     * strings (including blanks and invalid fragments) are mirrored into the
     * durable form after every mutation, while the saved occurrence is only
     * replaced when the user confirms with "Ajouter à la séance". */
    var setRows by
        remember(key) {
            mutableStateOf(
                initialSetRows
            )
        }

    var maxWeightText by
        remember(key) {
            mutableStateOf(initialForm.maxWeightText)
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
    val fixedEquipmentId = remember(exercise.exerciseId) {
        repository.machineExerciseLegacyEquipmentId(exercise.exerciseId)
    }
    var equipmentSearch by remember(key) { mutableStateOf("") }
    var customEquipmentName by remember(key) { mutableStateOf("") }
    var selectedEquipmentId by remember(key) {
        mutableStateOf(initialForm.selectedEquipmentId ?: fixedEquipmentId)
    }
    var targetChoice by remember(key) {
        mutableStateOf(if (initialPlan?.weightKg != null) ManualTargetChoice.KG
            else ManualTargetChoice.NONE)
    }
    var targetKgText by remember(key) {
        mutableStateOf(initialPlan?.weightKg?.let(::formatMaxWeight).orEmpty())
    }
    var targetPercentText by remember(key) { mutableStateOf("70") }

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
            text = if (sessionType == SessionType.MAX_TEST) "TEST MAX" else exerciseProfileLabel(exercise),
            color = colors.accent,
        )

        if (fixedEquipmentId == null) {
            /* Legacy/custom/unresolved rows retain the compatibility selector;
             * resolved fixed machines never require a second user choice. */
            TrainlogInputField(
                label = "Contexte historique (optionnel)",
                value = equipmentSearch,
                onValueChange = { equipmentSearch = it },
            )
            TrainlogInputField(
                label = "Nouveau contexte historique",
                value = customEquipmentName,
                onValueChange = { customEquipmentName = it },
            )
            TrainlogAction(
                label = "Créer le contexte",
                description = "Compatibilité temporaire pour un exercice custom ou non résolu.",
                accent = colors.success,
                onClick = {
                    when (val result = repository.createCustomEquipment(customEquipmentName)) {
                        is CreateEquipmentResult.Created -> {
                            customEquipmentName = ""
                            equipmentRevision += 1
                            selectedEquipmentId = result.equipment.equipmentId
                            onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, selectedEquipmentId, weightText, maxWeightText))
                        }
                        CreateEquipmentResult.Invalid -> error = "Donnez un nom de contexte valide."
                        CreateEquipmentResult.Conflict -> error = "Ce contexte existe déjà."
                        is CreateEquipmentResult.DatabaseError -> error = "Contexte non créé : ${result.message}"
                    }
                },
            )
        }
        val selectedEquipment = equipmentEntries.firstOrNull { it.equipmentId == selectedEquipmentId }
        val percentResult = remember(
            exercise.exerciseId, selectedEquipmentId, targetPercentText,
            targetChoice, equipmentRevision,
        ) {
            if (targetChoice == ManualTargetChoice.PERCENT_MAX)
                repository.calculateManualPercentMaxTarget(
                    exercise.exerciseId, selectedEquipmentId,
                    targetPercentText.toIntOrNull() ?: 0,
                )
            else null
        }
        if (selectedEquipment != null && fixedEquipmentId == null) {
            TrainlogAction(
                label = "✓ ${selectedEquipment.displayName}",
                description = selectedEquipment.labelName.ifBlank { "Équipement sélectionné." },
                accent = colors.success,
                onClick = {
                    selectedEquipmentId = null
                    onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, null, weightText, maxWeightText))
                },
            )
        }
        if (fixedEquipmentId == null) repository.searchEquipment(equipmentSearch).take(8).forEach { equipment ->
            TrainlogAction(
                label = if (equipment.equipmentId == selectedEquipmentId) "✓ ${equipment.displayName}" else equipment.displayName,
                description = equipment.labelName.ifBlank { equipment.type },
                accent = if (equipment.equipmentId == selectedEquipmentId) colors.success else colors.muted,
                onClick = {
                    selectedEquipmentId = equipment.equipmentId
                    onFormChanged(currentForm(exercise, setCountText, repsText, durationText, speedText, distanceText, equipment.equipmentId, weightText, maxWeightText))
                },
            )
        }

        if (sessionType == SessionType.MAX_TEST) {
            SessionNumberField(
                label = "Poids max (kg)",
                value = maxWeightText,
                onValueChange = {
                    maxWeightText = it
                    error = null
                    onFormChanged(
                        currentForm(
                            exercise, setCountText, repsText, durationText,
                            speedText, distanceText, selectedEquipmentId,
                            weightText, it,
                        )
                    )
                },
            )
            TrainlogInfo(
                text = "Valeur strictement positive, virgule française acceptée.",
                color = colors.muted,
            )
        } else if (
            exercise.recordingMode ==
            RecordingMode.SETS
        ) {
            TrainlogInfo("Charge cible prévue — séparée des charges réellement effectuées.", colors.muted)
            TrainlogChoiceChips(
                listOf(
                    ManualTargetChoice.KG.name to "Valeur en kg",
                    ManualTargetChoice.PERCENT_MAX.name to "% de mon MAX",
                    ManualTargetChoice.NONE.name to "Aucune",
                ),
                targetChoice.name,
            ) { selected -> targetChoice = ManualTargetChoice.valueOf(selected) }
            when (targetChoice) {
                ManualTargetChoice.KG -> SessionNumberField(
                    label = "Charge cible (kg)", value = targetKgText,
                    onValueChange = { targetKgText = it; error = null },
                )
                ManualTargetChoice.PERCENT_MAX -> {
                    SessionNumberField(
                        label = "Pourcentage de mon MAX (1 à 100)",
                        value = targetPercentText,
                        onValueChange = { targetPercentText = it; error = null },
                    )
                    when (val result = percentResult) {
                        is ManualPercentMaxResult.Available -> {
                            TrainlogInfo(
                                "MAX compatible : ${formatMaxWeight(result.maxWeightKg)} kg · ${result.maxStartedAt}",
                                colors.muted,
                            )
                            TrainlogInfo(
                                "Cible calculée : ${formatMaxWeight(result.targetWeightKg)} kg",
                                colors.success,
                            )
                        }
                        is ManualPercentMaxResult.Unavailable ->
                            TrainlogInfo(result.message, colors.error)
                        null -> Unit
                    }
                }
                ManualTargetChoice.NONE ->
                    TrainlogInfo("Aucune charge cible ne sera enregistrée.", colors.muted)
            }
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                TrainlogInfo(
                    text = "Chaque série conserve ses propres répétitions et sa propre charge.",
                    color = colors.muted,
                )
                val loadLabel =
                    if (selectedEquipment?.loadSemantics == EquipmentLoadSemantics.ASSISTANCE) {
                        "Assistance (kg)"
                    } else {
                        "Charge (kg)"
                    }
                setRows.forEachIndexed { index, row ->
                    TrainlogInfo(
                        text = "Série ${index + 1}",
                        color = colors.accent,
                    )
                    SessionNumberField(
                        label = "Série ${index + 1} — Répétitions",
                        value = row.repsText,
                        testTag = "session-set-$index-reps",
                        onValueChange = { value ->
                            val updated = setRows.replaceAt(index, row.copy(repsText = value))
                            setRows = updated
                            repsText = encodeRawReps(updated)
                            weightText = encodeRawWeights(updated)
                            error = null
                            onFormChanged(
                                currentForm(
                                    exercise, setCountText, repsText, durationText,
                                    speedText, distanceText, selectedEquipmentId, weightText,
                                )
                            )
                        },
                    )
                    SessionNumberField(
                        label = "Série ${index + 1} — $loadLabel",
                        value = row.weightText,
                        testTag = "session-set-$index-weight",
                        onValueChange = { value ->
                            val updated = setRows.replaceAt(index, row.copy(weightText = value))
                            setRows = updated
                            repsText = encodeRawReps(updated)
                            weightText = encodeRawWeights(updated)
                            error = null
                            onFormChanged(
                                currentForm(
                                    exercise, setCountText, repsText, durationText,
                                    speedText, distanceText, selectedEquipmentId, weightText,
                                )
                            )
                        },
                    )
                    TrainlogAction(
                        label = "Supprimer la série ${index + 1}",
                        description = "Retirer uniquement cette série.",
                        accent = colors.error,
                        onClick = {
                            val updated = setRows.filterIndexed { rowIndex, _ -> rowIndex != index }
                            setRows = updated
                            repsText = encodeRawReps(updated)
                            weightText = encodeRawWeights(updated)
                            error = null
                            onFormChanged(
                                currentForm(
                                    exercise, setCountText, repsText, durationText,
                                    speedText, distanceText, selectedEquipmentId, weightText,
                                )
                            )
                        },
                    )
                }
                TrainlogAction(
                    label = "Ajouter une série",
                    description = "Ajouter une ligne vide sans modifier les autres séries.",
                    accent = colors.success,
                    onClick = {
                        if (setRows.size < MAX_SESSION_SETS) {
                            val updated = setRows + RawSetRow()
                            setRows = updated
                            repsText = encodeRawReps(updated)
                            weightText = encodeRawWeights(updated)
                            error = null
                            onFormChanged(
                                currentForm(
                                    exercise, setCountText, repsText, durationText,
                                    speedText, distanceText, selectedEquipmentId, weightText,
                                )
                            )
                        }
                    },
                )
                TrainlogInfo(
                    text = "Charge facultative ; virgule française acceptée. Une case vide n'est pas zéro.",
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
                        sessionType = sessionType,
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
                        maxWeightText = maxWeightText,
                        entryId = initialForm.editingEntryId,
                        rawSetRows = setRows,
                    )

                if (draft == null) {
                    error =
                        if (sessionType == SessionType.MAX_TEST) {
                            "Saisissez un poids max strictement positif (ex. 100 ou 86,5)."
                        } else if (
                            exercise.recordingMode == RecordingMode.SETS &&
                            exercise.trackingMode == TrackingMode.REPS
                        ) {
                            setRowValidationError(setRows)
                        } else {
                            "Valeurs invalides."
                        }
                } else if (sessionType == SessionType.TRAINING &&
                    exercise.recordingMode == RecordingMode.SETS) {
                    val plan = buildManualTargetPlan(
                        draft, initialPlan, targetChoice, targetKgText,
                        percentResult, selectedEquipment?.loadSemantics,
                    )
                    plan.fold(
                        onSuccess = { onAdd(draft.copy(plan = it)) },
                        onFailure = { error = it.message ?: "Charge cible invalide." },
                    )
                } else {
                    onAdd(draft.copy(plan = null))
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
    maxWeightText: String = "",
): SessionDraftForm =
    SessionDraftForm(
        selectedExercise = exercise,
        selectedEquipmentId = equipmentId,
        setCountText = setCountText,
        repsText = repsText,
        weightText = weightText,
        maxWeightText = maxWeightText,
        durationText = durationText,
        speedText = speedText,
        distanceText = distanceText,
    )

@Composable
private fun SessionNumberField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    testTag: String? = null,
) {
    TrainlogInputField(
        label = label,
        value = value,
        onValueChange =
            onValueChange,
        testTag = testTag,
    )
}

private const val MAX_SESSION_SETS = 64
private const val MAX_REPS_PER_SET = 10000

internal data class RawSetRow(
    val repsText: String = "",
    val weightText: String = "",
)

internal fun List<RawSetRow>.replaceAt(index: Int, value: RawSetRow): List<RawSetRow> =
    mapIndexed { rowIndex, existing -> if (rowIndex == index) value else existing }

internal fun encodeRawReps(rows: List<RawSetRow>): String =
    rows.joinToString(";") { it.repsText }

internal fun encodeRawWeights(rows: List<RawSetRow>): String =
    rows.joinToString(";") { it.weightText }

/**
 * WHY: schema v9 already has durable raw form columns. Parallel token strings
 * preserve row order and interior blanks without inventing a schema migration.
 * Legacy compact rep expressions are expanded once when the row editor opens.
 */
internal fun rawSetRowsFromForm(form: SessionDraftForm): List<RawSetRow> {
    val repTokens =
        if (';' in form.repsText) {
            form.repsText.split(';')
        } else if (',' in form.repsText) {
            /* Legacy compact lists may end in an unfinished token. Keep that
             * blank row instead of normalizing it away during first reopen. */
            form.repsText.split(',')
        } else {
            val compact = parseRepSequence(form.repsText)
            if (compact != null) {
                compact.map(Int::toString)
            } else {
                listOf(form.repsText)
            }
        }
    val weightTokens =
        if (';' in form.weightText) {
            form.weightText.split(';')
        } else {
            listOf(form.weightText)
        }

    if (repTokens.isEmpty()) return listOf(RawSetRow())
    return repTokens.mapIndexed { index, reps ->
        RawSetRow(
            repsText = reps,
            /* Compatibility only: a legacy single compact load was broadcast
             * by the old editor. New edits always persist one token per row. */
            weightText =
                if (weightTokens.size == 1) weightTokens.single()
                else weightTokens.getOrElse(index) { "" },
        )
    }
}

internal fun parseRawSetRows(rows: List<RawSetRow>): List<SessionSetDraft>? {
    if (rows.isEmpty() || rows.size > MAX_SESSION_SETS) return null
    return rows.map { row ->
        val reps = row.repsText.trim().toIntOrNull()
        if (reps == null || reps !in 0..MAX_REPS_PER_SET) return null

        val rawWeight = row.weightText.trim()
        val weight =
            if (rawWeight.isEmpty()) {
                null
            } else {
                rawWeight.replace(',', '.').toDoubleOrNull()
                    ?.takeIf { it.isFinite() && it >= 0.0 }
                    ?: return null
            }
        SessionSetDraft(reps = reps, weightKg = weight)
    }
}

internal fun setRowValidationError(rows: List<RawSetRow>): String {
    if (rows.isEmpty()) return "Ajoutez au moins une série."
    rows.forEachIndexed { index, row ->
        val reps = row.repsText.trim().toIntOrNull()
        if (reps == null || reps !in 0..MAX_REPS_PER_SET) {
            return "Série ${index + 1} : saisissez des répétitions entre 0 et $MAX_REPS_PER_SET."
        }
        if (row.weightText.isNotBlank()) {
            val weight = row.weightText.trim().replace(',', '.').toDoubleOrNull()
            if (weight == null || !weight.isFinite() || weight < 0.0) {
                return "Série ${index + 1} : saisissez une charge non négative ou laissez la case vide."
            }
        }
    }
    return "Valeurs de séries invalides."
}

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
    sessionType: SessionType,
    setCountText: String,
    repsText: String,
    durationText: String,
    speedText: String,
    distanceText: String,
    equipmentId: String? = null,
    weightText: String = "",
    maxWeightText: String = "",
    entryId: String? = null,
    rawSetRows: List<RawSetRow>? = null,
): SessionExerciseDraft? {
    if (sessionType == SessionType.MAX_TEST) {
        val maxWeight = maxWeightText.trim().replace(',', '.').toDoubleOrNull()
        if (maxWeight == null || !maxWeight.isFinite() || maxWeight <= 0.0) {
            return null
        }
        return SessionExerciseDraft(
            entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
            exercise = exercise,
            equipmentId = equipmentId,
            maxWeightKg = maxWeight,
        )
    }

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
        val parsedSets =
            parseRawSetRows(
                rawSetRows ?: rawSetRowsFromForm(
                    SessionDraftForm(repsText = repsText, weightText = weightText)
                )
            ) ?: return null

        SessionExerciseDraft(
            entryId = entryId ?: "sxe_" + java.util.UUID.randomUUID().toString(),
            exercise = exercise,
            equipmentId = equipmentId,
            sets = parsedSets,
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
internal fun formForExistingExercise(
    draft: SessionExerciseDraft,
    index: Int,
): SessionDraftForm =
    SessionDraftForm(
        selectedExercise = draft.exercise,
        editingExerciseIndex = index,
        editingEntryId = draft.entryId,
        selectedEquipmentId = draft.equipmentId,
        maxWeightText = draft.maxWeightKg?.let(::formatMaxWeight).orEmpty(),
        setCountText = draft.sets.size.toString(),
        repsText = if (draft.exercise.trackingMode == TrackingMode.REPS) {
            draft.sets.joinToString(",") { it.reps.toString() }
        } else {
            "3x10"
        },
        /* INVARIANT: keep one load token per performed-set row. mapNotNull
         * would shift later weights left when an earlier row is blank. */
        weightText = draft.sets.joinToString(";") {
            it.weightKg?.let(::formatMaxWeight).orEmpty()
        },
        durationText = if (draft.exercise.recordingMode == RecordingMode.CONTINUOUS) {
            (draft.continuousDurationSeconds / 60).toString()
        } else {
            draft.sets.firstOrNull()?.durationSeconds?.toString() ?: "30"
        },
        speedText = draft.speedKmh?.toString().orEmpty(),
        distanceText = draft.distanceKm?.toString().orEmpty(),
    )

private fun draftSummary(
    draft: SessionExerciseDraft,
): String {
    return if (draft.maxWeightKg != null) {
        "${draft.exercise.name} · Max : ${formatMaxWeight(draft.maxWeightKg)} kg"
    } else if (
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
            draft.sets.all { it.weightKg == null } &&
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
                draft.sets.joinToString(separator = " ; ") { set ->
                    buildString {
                        append("${set.reps} reps")
                        set.weightKg?.let { append(" @ ${formatMaxWeight(it)} kg") }
                    }
                }
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

private fun formatMaxWeight(value: Double): String =
    java.math.BigDecimal.valueOf(value)
        .stripTrailingZeros()
        .toPlainString()
        .replace('.', ',')

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
