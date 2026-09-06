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
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.CreateExerciseResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ExerciseDataFields
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
            title = "NOUVEL EXERCICE"
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
                    onClick = {
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
                        onClick = {
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
                        "Données complémentaires",
                ) {
                    TrainlogChoice(
                        label = "Vitesse",
                        selected = speed,
                        onClick = {
                            speed = !speed
                            message = null
                        },
                    )

                    TrainlogChoice(
                        label = "Distance",
                        selected = distance,
                        onClick = {
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

            TrainlogAction(
                label =
                    if (inline) {
                        "Créer et revenir à la séance"
                    } else {
                        "Enregistrer l'exercice"
                    },
                description =
                    "Ajouter ce profil au catalogue local.",
                accent =
                    colors.success,
                onClick = {
                    when (
                        repository.createExercise(
                            NewExerciseProfile(
                                name = name,
                                recordingMode =
                                    recordingMode,
                                trackingMode =
                                    trackingMode,
                                dataFields =
                                    fields,
                            )
                        )
                    ) {
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
                    }
                },
            )

            if (message != null) {
                TrainlogInfo(
                    text =
                        message.orEmpty(),
                    color = colors.error,
                )
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
                    bottom = 6.dp
                ),
            style =
                TrainlogTypography.small.copy(
                    color = colors.muted,
                ),
        )

        BasicTextField(
            value = value,
            onValueChange = onValueChange,
            singleLine = true,
            cursorBrush =
                SolidColor(
                    colors.accent
                ),
            textStyle =
                TrainlogTypography.normal
                    .copy(
                        color = colors.text
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
    onClick: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

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
                        if (selected) {
                            colors.warning
                        } else {
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
                    onClick = onClick
                )
                .padding(11.dp)
    ) {
        BasicText(
            text =
                if (selected) {
                    "[X] $label"
                } else {
                    "[ ] $label"
                },
            style =
                TrainlogTypography.normal.copy(
                    color =
                        if (selected) {
                            colors.warning
                        } else {
                            colors.text
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
