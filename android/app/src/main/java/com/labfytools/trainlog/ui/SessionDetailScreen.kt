package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.SaveFeedbackResult
import com.labfytools.trainlog.data.exerciseFeedbackElapsedLabel
import com.labfytools.trainlog.data.sessionFollowUpElapsedLabel
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionExerciseDetail
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun SessionDetailScreen(
    repository: TrainlogRepository,
    sessionId: String?,
    onBack: () -> Unit,
    onResumeMaxTest: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    var revision by remember(sessionId) { mutableStateOf(0) }
    var editingEntryId by remember(sessionId) { mutableStateOf<String?>(null) }
    var pendingEquipmentChange by remember(sessionId) { mutableStateOf<Pair<String, String?>?>(null) }
    var equipmentQuery by remember(sessionId) { mutableStateOf("") }
    var resumeMessage by remember(sessionId) { mutableStateOf<String?>(null) }
    var feedbackEntryId by remember(sessionId) { mutableStateOf<String?>(null) }
    var editingFeedbackId by remember(sessionId) { mutableStateOf<String?>(null) }
    var editingFollowUpId by remember(sessionId) { mutableStateOf<String?>(null) }
    var addingFollowUp by remember(sessionId) { mutableStateOf(false) }

    val detail =
        remember(sessionId, revision) {
            sessionId?.let {
                repository.getSessionDetail(
                    it
                )
            }
        }
    val equipmentEntries =
        remember(sessionId, revision) {
            repository.listEquipment()
        }

    TrainlogScreen(
        subtitle = "Détail de la séance"
    ) {
        if (detail == null) {
            TrainlogFrame(
                title = "Erreur"
            ) {
                TrainlogInfo(
                    text =
                        "Séance introuvable.",
                    color =
                        colors.error,
                )
            }

            return@TrainlogScreen
        }

        TrainlogFrame(
            title = "Séance"
        ) {
            TrainlogInfo(
                formatStartedAt(
                    detail.summary.startedAt
                )
            )

            TrainlogInfo(
                text =
                    "Type : " +
                        sessionTypeLabel(
                            detail.summary
                                .sessionType
                        ),
                color =
                    if (
                        detail.summary
                            .sessionType ==
                        SessionType.MAX_TEST
                    ) {
                        colors.warning
                    } else {
                        colors.accent
                    },
            )

            TrainlogInfo(
                "${detail.summary.exerciseCount} exercice(s)"
            )

            if (detail.summary.sessionType == SessionType.MAX_TEST) {
                TrainlogAction(
                    label = "Reprendre ce Test max",
                    description = "Continuer la même séance en conservant son identifiant et sa date.",
                    accent = colors.success,
                    onClick = {
                        when (val result = repository.resumeMaxTestSession(detail.summary.sessionId)) {
                            ActiveDraftMutationResult.Saved -> onResumeMaxTest()
                            is ActiveDraftMutationResult.Error -> resumeMessage = result.message
                        }
                    },
                )
                resumeMessage?.let { TrainlogInfo(it, color = colors.error) }
            }
        }

        detail.exercises
            .forEachIndexed {
                    index,
                    exercise ->

                TrainlogFrame(
                    title =
                        "${index + 1}. ${exercise.exerciseName}"
                ) {
                    exercise.equipmentDisplayName?.let { equipment ->
                        TrainlogInfo("Équipement : $equipment", color = colors.muted)
                    }
                    exercise.plan?.let { plan ->
                        TrainlogInfo(
                            "Plan : ${plan.sets} × ${plan.reps ?: plan.durationSeconds} · " +
                                "repos ${plan.restSeconds} s · " +
                                (plan.weightKg?.let { "charge cible $it kg" }
                                    ?: "sans charge numérique"),
                            color = colors.muted,
                        )
                    }
                    if (editingEntryId == exercise.entryId) {
                        TrainlogInputField(
                            label = "Rechercher une machine",
                            value = equipmentQuery,
                            onValueChange = { equipmentQuery = it },
                        )
                        TrainlogIconAction(
                            icon = TrainlogIcons.DeleteOutline,
                            contentDescription = "Retirer la machine de cet exercice",
                            accent = colors.warning,
                            modifier = Modifier.testTag("delete-completed-equipment-${exercise.entryId}"),
                            onClick = {
                                pendingEquipmentChange = exercise.entryId to null
                            },
                        )
                        repository.searchEquipment(equipmentQuery).take(8).forEach { equipment ->
                            TrainlogAction(
                                label = equipment.displayName,
                                description = equipment.labelName.ifBlank { equipment.type },
                                accent = colors.success,
                                onClick = {
                                    if (exercise.equipmentId == null) {
                                        if (repository.setCompletedSessionEquipment(detail.summary.sessionId, exercise.entryId, equipment.equipmentId)) {
                                            revision++; editingEntryId = null; equipmentQuery = ""
                                        }
                                    } else pendingEquipmentChange = exercise.entryId to equipment.equipmentId
                                },
                            )
                        }
                    } else {
                        TrainlogAction(
                            label = "Modifier l'équipement",
                            description = "Choisir, remplacer ou retirer la machine de cette entrée.",
                            accent = colors.muted,
                            onClick = { editingEntryId = exercise.entryId },
                        )
                    }
                    if (exercise.maxWeightKg != null) {
                        val rendered = "%.2f".format(java.util.Locale.FRANCE, exercise.maxWeightKg)
                            .trimEnd('0').trimEnd(',')
                        TrainlogInfo(
                            text = "Max : $rendered kg",
                            color = colors.warning,
                        )
                    } else if (
                        exercise.recordingMode ==
                        RecordingMode.CONTINUOUS
                    ) {
                        ContinuousDetail(
                            exercise
                        )
                    } else {
                        SetsDetail(
                            exercise = exercise,
                            loadSemantics = equipmentEntries
                                .firstOrNull { it.equipmentId == exercise.equipmentId }
                                ?.loadSemantics,
                        )
                    }
                    exercise.feedback.forEach { item ->
                        TrainlogInfo("${exerciseFeedbackElapsedLabel(detail.summary.endedAt, item.observedAt)} · ${item.rawText}${if (item.modified) " · Modifié" else ""}")
                        TrainlogAction("Modifier", "Corriger le texte sans déplacer l’observation.",
                            onClick = { editingFeedbackId = item.feedbackId }, accent = colors.muted)
                        if (editingFeedbackId == item.feedbackId) FeedbackEditor(
                            initialText = item.rawText, editing = true,
                            onSave = { text -> when (val result = repository.reviseExerciseFeedback(item.feedbackId, text)) {
                                is SaveFeedbackResult.Saved -> { editingFeedbackId = null; revision++; null }
                                is SaveFeedbackResult.Invalid -> result.message
                                is SaveFeedbackResult.DatabaseError -> result.message
                            } }, onCancel = { editingFeedbackId = null })
                    }
                    if (feedbackEntryId == exercise.entryId) {
                        FeedbackEditor(onSave = { text ->
                            when (val result = repository.saveExerciseFeedback(detail.summary.sessionId, exercise.entryId, text)) {
                                is SaveFeedbackResult.Saved -> { feedbackEntryId = null; revision++; null }
                                is SaveFeedbackResult.Invalid -> result.message
                                is SaveFeedbackResult.DatabaseError -> result.message
                            }
                        }, onCancel = { feedbackEntryId = null })
                    } else {
                        TrainlogAction(
                            label = if (exercise.feedback.isEmpty()) "🎙 Ressenti" else "+ Ajouter un ressenti",
                            description = "Ajouter une observation subjective à cette occurrence.",
                            accent = colors.accent,
                            onClick = { feedbackEntryId = exercise.entryId },
                        )
                    }
                }
            }

        TrainlogFrame(title = "Suivi après séance") {
            detail.followUps.forEach { item ->
                TrainlogInfo(sessionFollowUpElapsedLabel(detail.summary.endedAt, item.observedAt), color = colors.accent)
                TrainlogInfo(item.rawText + if (item.modified) " · Modifié" else "")
                TrainlogAction("Modifier", "Corriger le texte sans changer le repère H+.",
                    onClick = { editingFollowUpId = item.followupId }, accent = colors.muted)
                if (editingFollowUpId == item.followupId) FeedbackEditor(
                    initialText = item.rawText, editing = true,
                    onSave = { text -> when (val result = repository.reviseSessionFollowUp(item.followupId, text)) {
                        is SaveFeedbackResult.Saved -> { editingFollowUpId = null; revision++; null }
                        is SaveFeedbackResult.Invalid -> result.message
                        is SaveFeedbackResult.DatabaseError -> result.message
                    } }, onCancel = { editingFollowUpId = null })
            }
            if (addingFollowUp) {
                FeedbackEditor(onSave = { text ->
                    when (val result = repository.saveSessionFollowUp(detail.summary.sessionId, text)) {
                        is SaveFeedbackResult.Saved -> { addingFollowUp = false; revision++; null }
                        is SaveFeedbackResult.Invalid -> result.message
                        is SaveFeedbackResult.DatabaseError -> result.message
                    }
                }, onCancel = { addingFollowUp = false })
            } else TrainlogAction("+ Ajouter un enregistrement",
                "Ajouter une observation chronologique sans l’attribuer à un exercice.",
                onClick = { addingFollowUp = true }, accent = colors.success)
        }
        pendingEquipmentChange?.let { (entryId, equipmentId) ->
            DestructiveConfirmationDialog(
                title = if (equipmentId == null) "Retirer la machine de cette entrée ?" else "Remplacer la machine associée ?",
                detail = "L’association de machine actuellement enregistrée sera perdue. Les séries, MAX et ressentis restent inchangés.",
                confirmLabel = if (equipmentId == null) "Retirer" else "Remplacer",
                onCancel = { pendingEquipmentChange = null },
                onConfirm = {
                    if (repository.setCompletedSessionEquipment(detail.summary.sessionId,entryId,equipmentId)) {
                        revision++;editingEntryId=null;equipmentQuery="";pendingEquipmentChange=null
                    }
                },
            )
        }
    }
}

@Composable
private fun SetsDetail(
    exercise: SessionExerciseDetail,
    loadSemantics: EquipmentLoadSemantics?,
) {
    val colors =
        LocalTrainlogColors.current

    TrainlogInfo(
        text =
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                "Mode : séries · répétitions"
            } else {
                "Mode : séries · durée"
            },
        color = colors.accent,
    )

    if (exercise.trackingMode == TrackingMode.REPS) {
        val loadHeading =
            if (loadSemantics == EquipmentLoadSemantics.ASSISTANCE) {
                "Assistance (kg)"
            } else {
                "Charge (kg)"
            }
        /* Readable row table: history must expose every persisted value and
         * distinguish an absent load from an explicit zero. */
        TrainlogInfo("Série | Répétitions | $loadHeading", color = colors.muted)
    }

    exercise.sets
        .forEachIndexed {
                index,
                set ->

            TrainlogInfo(
                if (
                    exercise.trackingMode ==
                    TrackingMode.REPS
                ) {
                    val renderedWeight = set.weightKg?.let {
                        "%.2f".format(java.util.Locale.FRANCE, it).trimEnd('0').trimEnd(',')
                    } ?: "—"
                    "${index + 1} | ${set.reps} | $renderedWeight"
                } else {
                    "Série ${index + 1} : ${formatDuration(set.durationSeconds)}"
                }
            )
        }
}

@Composable
private fun ContinuousDetail(
    exercise: SessionExerciseDetail,
) {
    val colors =
        LocalTrainlogColors.current

    TrainlogInfo(
        text =
            "Mode : continu",
        color = colors.accent,
    )

    TrainlogInfo(
        "Durée : ${formatDuration(exercise.continuousDurationSeconds)}"
    )

    if (
        exercise.dataFields and
            ExerciseDataFields.SPEED_KMH != 0
    ) {
        TrainlogInfo(
            "Vitesse : %.1f km/h".format(
                exercise.speedKmh ?: 0.0
            )
        )
    }

    if (
        exercise.dataFields and
            ExerciseDataFields.DISTANCE_KM != 0
    ) {
        TrainlogInfo(
            "Distance : %.2f km".format(
                exercise.distanceKm ?: 0.0
            )
        )
    }
}

private fun formatDuration(
    seconds: Int,
): String {
    if (
        seconds > 0 &&
        seconds % 60 == 0
    ) {
        return "${seconds / 60} min"
    }

    return "$seconds s"
}
