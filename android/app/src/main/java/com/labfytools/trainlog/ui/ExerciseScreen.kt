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
import com.labfytools.trainlog.data.EditExerciseResult
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
                onClick = {
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
            val exercises = repository.listExercises()
            if (exercises.isEmpty()) {
                TrainlogInfo("Aucun exercice enregistré.")
            } else {
                exercises.forEach { exercise ->
                    TrainlogAction(
                        label = "Modifier · ${exercise.name}",
                        description = "Modifier le nom ou le profil si disponible.",
                        accent = colors.accent,
                        onClick = { startEditing(exercise) },
                    )
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
