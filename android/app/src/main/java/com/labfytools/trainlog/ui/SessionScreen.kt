package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.SaveSessionResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
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

    var selectedExercise by
        remember(
            catalogRevision
        ) {
            mutableStateOf<
                ExerciseProfile?
            >(null)
        }

    var draftExercises by
        remember {
            mutableStateOf(
                emptyList<
                    SessionExerciseDraft
                >()
            )
        }

    var sessionRevision by
        remember {
            mutableIntStateOf(0)
        }

    var message by
        remember {
            mutableStateOf<
                String?
            >(null)
        }

    TrainlogScreen(
        subtitle = "S E A N C E"
    ) {
        TrainlogAction(
            label = "< Retour",
            description =
                "Revenir à l'accueil.",
            onClick = onBack,
            accent = colors.muted,
        )

        TrainlogFrame(
            title = "SEANCE EN COURS"
        ) {
            if (
                draftExercises.isEmpty()
            ) {
                TrainlogInfo(
                    "Aucun exercice ajouté."
                )
            } else {
                draftExercises
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
                        draftExercises.any {
                            it.exercise.exerciseId ==
                                exercise.exerciseId
                        }

                    CatalogChoice(
                        exercise =
                            exercise,
                        selected =
                            selectedExercise
                                ?.exerciseId ==
                                exercise.exerciseId,
                        disabled =
                            alreadyAdded,
                        onClick = {
                            if (
                                !alreadyAdded
                            ) {
                                selectedExercise =
                                    exercise

                                message = null
                            }
                        },
                    )
                }
            }
        }

        if (
            selectedExercise != null
        ) {
            SessionExerciseForm(
                key =
                    selectedExercise!!
                        .exerciseId,
                exercise =
                    selectedExercise!!,
                onCancel = {
                    selectedExercise =
                        null
                },
                onAdd = {
                    draft ->
                        draftExercises =
                            draftExercises +
                                draft

                        selectedExercise =
                            null

                        sessionRevision += 1

                        message =
                            "Exercice ajouté à la séance."
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
                draftExercises.isNotEmpty(),
        ) {
            TrainlogAction(
                label =
                    "Enregistrer la séance",
                description =
                    "${draftExercises.size} exercice(s) dans la séance.",
                accent =
                    colors.success,
                onClick = {
                    when (
                        val result =
                            repository
                                .saveSession(
                                    SessionDraft(
                                        exercises =
                                            draftExercises
                                    )
                                )
                    ) {
                        is SaveSessionResult.Saved -> {
                            draftExercises =
                                emptyList()

                            selectedExercise =
                                null

                            sessionRevision += 1

                            message =
                                "Séance enregistrée."

                            onSessionSaved()
                        }

                        SaveSessionResult.Invalid -> {
                            message =
                                "Séance invalide."
                        }

                        SaveSessionResult.DatabaseError -> {
                            message =
                                "Erreur base locale."
                        }
                    }
                },
            )

            if (
                message != null
            ) {
                TrainlogInfo(
                    text =
                        message.orEmpty(),
                    color =
                        if (
                            message ==
                            "Séance enregistrée." ||
                            message ==
                            "Exercice ajouté à la séance."
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
                .padding(
                    vertical = 3.dp
                )
                .border(
                    width = 1.dp,
                    color =
                        when {
                            selected ->
                                colors.warning

                            disabled ->
                                colors.muted

                            else ->
                                colors.surfaceAlt
                        },
                )
                .background(
                    if (selected) {
                        colors.surfaceAlt
                    } else {
                        colors.surface
                    }
                )
                .clickable(
                    enabled =
                        !disabled,
                    onClick =
                        onClick,
                )
                .padding(11.dp)
    ) {
        BasicText(
            text =
                (
                    if (disabled) {
                        "[✓] "
                    } else if (selected) {
                        "[>] "
                    } else {
                        "[ ] "
                    }
                ) +
                    exercise.name +
                    "  [$profile]",
            style =
                TrainlogTypography.normal
                    .copy(
                        color =
                            if (disabled) {
                                colors.muted
                            } else if (
                                selected
                            ) {
                                colors.warning
                            } else {
                                colors.text
                            },
                    ),
        )
    }
}

@Composable
private fun SessionExerciseForm(
    key: String,
    exercise: ExerciseProfile,
    onCancel: () -> Unit,
    onAdd:
        (SessionExerciseDraft) ->
            Unit,
) {
    val colors =
        LocalTrainlogColors.current

    var setCountText by
        remember(key) {
            mutableStateOf("3")
        }

    var repsText by
        remember(key) {
            mutableStateOf("10")
        }

    var durationText by
        remember(key) {
            mutableStateOf("30")
        }

    var speedText by
        remember(key) {
            mutableStateOf("")
        }

    var distanceText by
        remember(key) {
            mutableStateOf("")
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

        if (
            exercise.recordingMode ==
            RecordingMode.SETS
        ) {
            SessionNumberField(
                label =
                    "Nombre de séries",
                value =
                    setCountText,
                onValueChange = {
                    setCountText = it
                    error = null
                },
            )

            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                SessionNumberField(
                    label =
                        "Répétitions par série",
                    value =
                        repsText,
                    onValueChange = {
                        repsText = it
                        error = null
                    },
                )
            } else {
                SessionNumberField(
                    label =
                        "Durée par série (secondes)",
                    value =
                        durationText,
                    onValueChange = {
                        durationText = it
                        error = null
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

@Composable
private fun SessionNumberField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    Column(
        modifier =
            Modifier.padding(
                bottom = 12.dp
            )
    ) {
        BasicText(
            text =
                label.uppercase(),
            modifier =
                Modifier.padding(
                    bottom = 5.dp
                ),
            style =
                TrainlogTypography.small
                    .copy(
                        color =
                            colors.muted,
                    ),
        )

        BasicTextField(
            value = value,
            onValueChange =
                onValueChange,
            singleLine = true,
            cursorBrush =
                SolidColor(
                    colors.accent
                ),
            textStyle =
                TrainlogTypography.normal
                    .copy(
                        color =
                            colors.text,
                    ),
            modifier =
                Modifier
                    .fillMaxWidth()
                    .border(
                        width = 1.dp,
                        color =
                            colors.surfaceAlt,
                    )
                    .background(
                        colors.surface
                    )
                    .padding(12.dp),
        )
    }
}

private fun buildSessionExerciseDraft(
    exercise: ExerciseProfile,
    setCountText: String,
    repsText: String,
    durationText: String,
    speedText: String,
    distanceText: String,
): SessionExerciseDraft? {
    return if (
        exercise.recordingMode ==
        RecordingMode.CONTINUOUS
    ) {
        val minutes =
            durationText
                .toIntOrNull()

        if (
            minutes == null ||
            minutes <= 0 ||
            minutes > 1440
        ) {
            null
        } else {
            val wantsSpeed =
                exercise.dataFields and
                    ExerciseDataFields
                        .SPEED_KMH != 0

            val wantsDistance =
                exercise.dataFields and
                    ExerciseDataFields
                        .DISTANCE_KM != 0

            val speed =
                if (wantsSpeed) {
                    speedText
                        .replace(
                            ',',
                            '.'
                        )
                        .toDoubleOrNull()
                } else {
                    null
                }

            val distance =
                if (
                    wantsDistance
                ) {
                    distanceText
                        .replace(
                            ',',
                            '.'
                        )
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
                    exercise =
                        exercise,
                    continuousDurationSeconds =
                        minutes * 60,
                    speedKmh =
                        speed,
                    distanceKm =
                        distance,
                )
            }
        }
    } else {
        val count =
            setCountText
                .toIntOrNull()

        if (
            count == null ||
            count <= 0 ||
            count > 64
        ) {
            null
        } else if (
            exercise.trackingMode ==
            TrackingMode.REPS
        ) {
            val reps =
                repsText
                    .toIntOrNull()

            if (
                reps == null ||
                reps < 0 ||
                reps > 10000
            ) {
                null
            } else {
                SessionExerciseDraft(
                    exercise =
                        exercise,
                    sets =
                        List(count) {
                            SessionSetDraft(
                                reps = reps
                            )
                        },
                )
            }
        } else {
            val seconds =
                durationText
                    .toIntOrNull()

            if (
                seconds == null ||
                seconds <= 0 ||
                seconds > 86400
            ) {
                null
            } else {
                SessionExerciseDraft(
                    exercise =
                        exercise,
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
}

private fun draftSummary(
    draft:
        SessionExerciseDraft,
): String {
    val exercise =
        draft.exercise

    return if (
        exercise.recordingMode ==
        RecordingMode.CONTINUOUS
    ) {
        buildString {
            append(
                exercise.name
            )

            append(" · ")

            append(
                draft
                    .continuousDurationSeconds /
                    60
            )

            append(" min")

            draft.speedKmh
                ?.let {
                    append(
                        " · %.1f km/h"
                            .format(it)
                    )
                }

            draft.distanceKm
                ?.let {
                    append(
                        " · %.2f km"
                            .format(it)
                    )
                }
        }
    } else {
        val metric =
            if (
                exercise.trackingMode ==
                TrackingMode.REPS
            ) {
                "${draft.sets.firstOrNull()?.reps ?: 0} reps"
            } else {
                "${draft.sets.firstOrNull()?.durationSeconds ?: 0} s"
            }

        "${exercise.name} · ${draft.sets.size} × $metric"
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
