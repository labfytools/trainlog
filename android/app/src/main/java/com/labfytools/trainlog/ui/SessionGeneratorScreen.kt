package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.AcceptGeneratedSessionResult
import com.labfytools.trainlog.data.GenerationWarningLevel
import com.labfytools.trainlog.data.GeneratorLoadChoice
import com.labfytools.trainlog.data.SessionGenerationPreview
import com.labfytools.trainlog.data.SessionGenerationRequest
import com.labfytools.trainlog.data.SessionGenerationResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import java.time.OffsetDateTime
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

internal object SessionGeneratorPreviewController {
    fun remove(preview: SessionGenerationPreview, index: Int): SessionGenerationPreview {
        val removed = preview.exercises.getOrNull(index) ?: return preview
        return preview.copy(
            exercises = preview.exercises.filterIndexed { at, _ -> at != index },
            estimatedDurationSeconds = preview.estimatedDurationSeconds - removed.estimatedSeconds,
        )
    }

    fun move(preview: SessionGenerationPreview, index: Int, offset: Int): SessionGenerationPreview {
        val destination = index + offset
        if (index !in preview.exercises.indices || destination !in preview.exercises.indices) return preview
        val changed = preview.exercises.toMutableList()
        val value = changed.removeAt(index)
        changed.add(destination, value)
        return preview.copy(exercises = changed)
    }
}

internal object SessionGeneratorFormController {
    val goals = listOf(
        "general" to "Général",
        "strength" to "Force",
        "hypertrophy" to "Hypertrophie",
        "endurance" to "Endurance locale",
    )
    fun duration(text: String, allowed: IntRange): Int? = text.toIntOrNull()?.takeIf { it in allowed }

    fun manualWeight(text: String): Result<Double?> {
        val normalized = text.trim().replace(',', '.')
        if (normalized.isEmpty()) return Result.success(null)
        val value = normalized.toDoubleOrNull()
        return if (value != null && value.isFinite() && value > 0.0) Result.success(value)
        else Result.failure(IllegalArgumentException("La charge cible doit être un nombre positif fini."))
    }
}

/**
 * INVARIANT: a generated proposal is transient application state. Keeping this
 * holder above the route preserves edits across drawer navigation without ever
 * representing the proposal as the repository-owned active draft.
 */
class SessionGeneratorUiState {
    val zoneId = mutableStateOf("full_body")
    val goalId = mutableStateOf("general")
    val durationText = mutableStateOf("30")
    val preview = mutableStateOf<SessionGenerationPreview?>(null)
    val message = mutableStateOf<String?>(null)
    val warningAcknowledged = mutableStateOf(false)
    val editingIndex = mutableStateOf<Int?>(null)
    val setsText = mutableStateOf("")
    val repsText = mutableStateOf("")
    val restText = mutableStateOf("")
    val loadText = mutableStateOf("")
    val loadChoice = mutableStateOf(GeneratorLoadChoice.AUTOMATIC)
    val maxPercentText = mutableStateOf("70")
    val busy = mutableStateOf(false)
    var requestIdentity: Long = 0

    val hasUnacceptedWork: Boolean
        get() = preview.value != null || zoneId.value != "full_body" ||
            goalId.value != "general" || durationText.value != "30"

    fun abandon() {
        zoneId.value = "full_body"
        goalId.value = "general"
        durationText.value = "30"
        preview.value = null
        message.value = null
        warningAcknowledged.value = false
        editingIndex.value = null
        setsText.value = ""
        repsText.value = ""
        restText.value = ""
        loadText.value = ""
        loadChoice.value = GeneratorLoadChoice.AUTOMATIC
        maxPercentText.value = "70"
    }
}

@Composable
fun SessionGeneratorScreen(
    repository: TrainlogRepository,
    state: SessionGeneratorUiState,
    onBack: () -> Unit,
    onAccepted: () -> Unit,
    onExistingDraft: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    val scope = rememberCoroutineScope()
    val options = remember { repository.sessionGenerationFormOptions() }
    val zones = remember { repository.listBodyZones().filter { it.zoneId in options.zoneIds } }
    var zoneId by state.zoneId
    var goalId by state.goalId
    var durationText by state.durationText
    var preview by state.preview
    var message by state.message
    var warningAcknowledged by state.warningAcknowledged
    var editingIndex by state.editingIndex
    var setsText by state.setsText
    var repsText by state.repsText
    var restText by state.restText
    var loadText by state.loadText
    var loadChoice by state.loadChoice
    var maxPercentText by state.maxPercentText
    var busy by state.busy

    fun generate(request: SessionGenerationRequest) {
        if (busy) return
        val requestIdentity = ++state.requestIdentity
        scope.launch {
            busy = true
            try {
                when (val result = withContext(Dispatchers.IO) { repository.generateSessionPreview(request) }) {
                    is SessionGenerationResult.Generated -> {
                        /* INVARIANT: an obsolete async generation request may
                         * not replace a newer proposal after navigation. */
                        if (requestIdentity != state.requestIdentity) return@launch
                        preview = result.preview
                        warningAcknowledged = result.preview.exposure.warningLevel == GenerationWarningLevel.NONE
                        message = null
                    }
                    is SessionGenerationResult.Invalid -> if (requestIdentity == state.requestIdentity) message = result.message
                    is SessionGenerationResult.DatabaseError -> if (requestIdentity == state.requestIdentity) message = result.message
                }
            } finally {
                busy = false
            }
        }
    }

    TrainlogScreen(subtitle = "Programmer une séance") {
        val current = preview
        if (current == null) {
            TrainlogFrame("Zone corporelle") {
                TrainlogChoiceChips(zones.map { it.zoneId to it.displayName }, zoneId) { zoneId = it }
            }
            TrainlogFrame("Objectif") {
                TrainlogChoiceChips(SessionGeneratorFormController.goals.filter { it.first in options.goalIds }, goalId) { goalId = it }
            }
            TrainlogFrame("Durée") {
                TrainlogChoiceChips(options.durationPresets.map { it.toString() to "$it min" }, durationText) { durationText = it }
                TrainlogInputField(
                    "Durée personnalisée (${options.customMinutes.first} à ${options.customMinutes.last} min)",
                    durationText,
                    onValueChange = { durationText = it },
                )
            }
            TrainlogAction("Générer", "Analyser l'historique en lecture seule et préparer la proposition.", accent = colors.success, onClick = {
                val minutes = SessionGeneratorFormController.duration(durationText, options.customMinutes)
                if (minutes == null) message = "La durée doit être comprise entre ${options.customMinutes.first} et ${options.customMinutes.last} minutes."
                else generate(SessionGenerationRequest(zoneId, goalId, minutes, OffsetDateTime.now().toString()))
            })
        } else {
            TrainlogFrame("Proposition") {
                TrainlogInfo("Durée cible : ${current.request.durationMinutes} min · estimation : ${current.estimatedDurationSeconds / 60} min")
                if (current.request.durationMinutes * 60 - current.estimatedDurationSeconds >= 300)
                    TrainlogInfo("La proposition est nettement plus courte que la cible ; aucun exercice n'est ajouté pour remplir artificiellement le temps.", colors.warning)
                TrainlogInfo("Échauffement et retour au calme ne sont pas générés en V1.", colors.muted)
                if (current.insufficientResolvedCandidates) TrainlogInfo(
                    "La couverture est incomplète faute de contextes résolus disponibles" +
                        current.shortageCodes.takeIf { it.isNotEmpty() }
                            ?.joinToString(prefix = " : ", postfix = ".") { generationShortageLabel(it) }
                            .orEmpty(),
                    colors.warning,
                )
                val exposure = current.exposure
                TrainlogInfo("Exposition observée : ${exposure.within24h.primarySetCount} séries principales et ${exposure.within24h.secondarySetCount} secondaires sur 24 h ; ${exposure.within72h.primarySetCount} principales et ${exposure.within72h.secondarySetCount} secondaires sur 72 h.")
                exposure.latestStartedAt?.let { TrainlogInfo("Dernière exposition observée : $it", colors.muted) }
                if (exposure.warningLevel != GenerationWarningLevel.NONE) {
                    TrainlogInfo("Attention informative : l'historique montre une exposition récente. Cela ne mesure pas la récupération physiologique.", colors.warning)
                    if (!warningAcknowledged) TrainlogAction("Continuer malgré l'avertissement", "Conserver la proposition et autoriser son acceptation.", accent = colors.warning, onClick = {
                        if (!busy) warningAcknowledged = true
                    })
                }
            }
            current.exercises.forEachIndexed { index, item ->
                TrainlogFrame("${index + 1}. ${item.exerciseName}") {
                    TrainlogInfo("Équipement : ${item.equipmentName}")
                    TrainlogInfo("Cible : ${item.plan.sets} × ${item.plan.reps} · repos ${item.plan.restSeconds} s")
                    TrainlogInfo(if (item.plan.weightKg == null) "Charge cible : aucune charge numérique proposée."
                        else "Charge cible : ${item.plan.weightKg} kg")
                    TrainlogInfo("Zone principale : ${item.primaryZoneName}")
                    TrainlogInfo("Mouvement : ${item.patternNames.joinToString()}")
                    if (item.recency.recentSameExercise || item.recency.recentSamePattern)
                        TrainlogInfo("Historique récent similaire détecté ; information uniquement.", colors.warning)
                    item.loadSourceStartedAt?.let {
                        TrainlogInfo(
                            "Charge issue d'une dose réellement observée le $it ; " +
                                "son applicabilité aujourd'hui reste incertaine.",
                            colors.muted,
                        )
                    }
                    TrainlogInfo(
                        "Raisons : ${item.rationaleCodes.joinToString { generationReasonLabel(it) }}",
                        colors.muted,
                    )
                    if (editingIndex == index) {
                        TrainlogInputField("Séries", setsText, onValueChange = { setsText = it })
                        TrainlogInputField("Répétitions", repsText, onValueChange = { repsText = it })
                        TrainlogInputField("Repos (secondes)", restText, onValueChange = { restText = it })
                        TrainlogInfo("Charge : choix utilisateur, jamais une recommandation.", colors.muted)
                        TrainlogChoiceChips(
                            listOf("AUTOMATIC" to "Automatique", "PERCENT_MAX" to "% MAX", "NONE" to "Aucune"),
                            loadChoice.name,
                        ) { selected ->
                            loadChoice = GeneratorLoadChoice.valueOf(selected)
                            if (loadChoice != GeneratorLoadChoice.AUTOMATIC) loadText = ""
                        }
                        if (loadChoice == GeneratorLoadChoice.PERCENT_MAX)
                            TrainlogInputField("Pourcentage du MAX (1 à 100)", maxPercentText,
                                onValueChange = { maxPercentText = it })
                        if (loadChoice == GeneratorLoadChoice.AUTOMATIC)
                            TrainlogInputField(
                                "Charge cible manuelle (vide = automatique)",
                                loadText,
                                onValueChange = { loadText = it },
                            )
                        TrainlogAction("Appliquer", "Réestimer la durée et requalifier la charge observée.", accent = colors.success, onClick = {
                            val parsedWeight = if (loadChoice == GeneratorLoadChoice.AUTOMATIC)
                                SessionGeneratorFormController.manualWeight(loadText)
                            else Result.success(null)
                            if (parsedWeight.isFailure) {
                                message = parsedWeight.exceptionOrNull()?.message
                            } else {
                                val weight = parsedWeight.getOrNull()
                                if (!busy) scope.launch {
                                    busy = true
                                    try {
                                        when (val result = withContext(Dispatchers.IO) {
                                            repository.editGeneratedDose(current, index,
                                                setsText.toIntOrNull() ?: -1, repsText.toIntOrNull() ?: -1,
                                                restText.toIntOrNull() ?: -1, weight, loadChoice,
                                                maxPercentText.toIntOrNull())
                                        }) {
                                            is SessionGenerationResult.Generated -> { preview = result.preview; editingIndex = null; message = null }
                                            is SessionGenerationResult.Invalid -> message = result.message
                                            is SessionGenerationResult.DatabaseError -> message = result.message
                                        }
                                    } finally {
                                        busy = false
                                    }
                                }
                            }
                        })
                    } else TrainlogAction("Modifier la cible", "Modifier séries, répétitions, repos ou charge.", onClick = {
                        if (!busy) {
                            editingIndex = index; setsText = item.plan.sets.toString(); repsText = item.plan.reps.toString()
                            restText = item.plan.restSeconds.toString()
                            // Preserve an explicit user value across later edits. An
                            // automatic observed value stays display-only: empty asks
                            // the engine to qualify it again for the changed dose.
                            loadChoice = when {
                                "user_selected_max_percentage" in item.rationaleCodes ||
                                    "compatible_max_unavailable" in item.rationaleCodes -> GeneratorLoadChoice.PERCENT_MAX
                                "numeric_load_absent" in item.rationaleCodes -> GeneratorLoadChoice.NONE
                                else -> GeneratorLoadChoice.AUTOMATIC
                            }
                            loadText = if ("manual_target_load" in item.rationaleCodes)
                                item.plan.weightKg?.toString().orEmpty() else ""
                        }
                    })
                    TrainlogAction("Monter", "Déplacer cet exercice avant le précédent.", onClick = {
                        if (!busy) preview = SessionGeneratorPreviewController.move(current, index, -1)
                    }, accent = colors.muted)
                    TrainlogAction("Descendre", "Déplacer cet exercice après le suivant.", onClick = {
                        if (!busy) preview = SessionGeneratorPreviewController.move(current, index, 1)
                    }, accent = colors.muted)
                    TrainlogAction("Retirer", "Retirer cet exercice de la proposition uniquement.", onClick = {
                        if (!busy) preview = SessionGeneratorPreviewController.remove(current, index)
                    }, accent = colors.error)
                }
            }
            TrainlogAction("Régénérer", "Relancer les mêmes entrées et le même instant de référence.", onClick = { generate(current.request) })
            TrainlogAction("Accepter et saisir les valeurs réelles", "Créer le brouillon normal sans préremplir les séries réalisées.", accent = colors.success, onClick = {
                if (!warningAcknowledged) message = "Confirmez d'abord l'avertissement d'exposition récente."
                else if (!busy) scope.launch {
                    busy = true
                    try {
                        when (val result = withContext(Dispatchers.IO) { repository.acceptGeneratedSession(current) }) {
                            AcceptGeneratedSessionResult.Accepted -> onAccepted()
                            AcceptGeneratedSessionResult.ExistingActiveDraft -> onExistingDraft()
                            is AcceptGeneratedSessionResult.Invalid -> message = result.message
                            is AcceptGeneratedSessionResult.DatabaseError -> message = result.message
                        }
                    } finally {
                        busy = false
                    }
                }
            })
            TrainlogAction("Annuler la proposition", "Revenir sans écrire de brouillon.", onClick = {
                if (!busy) onBack()
            }, accent = colors.muted)
        }
        if (busy) TrainlogFrame("Traitement") { TrainlogInfo("Analyse en cours…", colors.muted) }
        message?.let { TrainlogFrame("Message") { TrainlogInfo(it, colors.error) } }
    }
}

private fun generationReasonLabel(code: String): String = when (code) {
    "observed_repeated_dose_anchor" -> "dose répétée observée"
    "explicit_max_present_no_numeric_prescription" -> "maximum observé sans prescription numérique"
    "user_selected_max_percentage" -> "pourcentage de MAX choisi par l'utilisateur"
    "compatible_max_unavailable" -> "MAX compatible indisponible"
    "assistance_numeric_load_omitted" -> "charge d'assistance omise"
    "numeric_load_absent" -> "charge numérique absente"
    "manual_target_load" -> "charge saisie manuellement"
    "preferred_exercise" -> "exercice préféré"
    "requested_primary_zone" -> "zone principale demandée"
    "requested_secondary_zone" -> "zone secondaire demandée"
    "new_exact_pattern" -> "mouvement distinct"
    "recent_same_exercise_penalty" -> "même exercice observé récemment"
    "recent_same_pattern_penalty" -> "mouvement similaire observé récemment"
    else -> code.replace('_', ' ')
}

private fun generationShortageLabel(code: String): String = when (code) {
    "missing_upper_region" -> "région supérieure manquante"
    "missing_lower_region" -> "région inférieure manquante"
    "missing_core_region" -> "tronc manquant"
    "missing_upper_push" -> "poussée du haut du corps manquante"
    "missing_upper_pull" -> "tirage du haut du corps manquant"
    "missing_lower_extension" -> "extension du bas du corps manquante"
    "missing_lower_flexion" -> "flexion du bas du corps manquante"
    "fewer_than_max_exercises" -> "moins d'exercices distincts disponibles"
    else -> code.replace('_', ' ')
}
