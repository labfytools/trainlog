package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Column
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
import androidx.compose.ui.focus.FocusManager
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.SaveBodyObservationResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.BodyObservationDraft
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography

@Composable
fun BodyScreen(
    repository: TrainlogRepository,
    onBack: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    val focusManager =
        LocalFocusManager.current

    val keyboardController =
        LocalSoftwareKeyboardController.current

    var bodyWeight by
        remember {
            mutableStateOf("")
        }

    var neck by
        remember {
            mutableStateOf("")
        }

    var shoulders by
        remember {
            mutableStateOf("")
        }

    var chest by
        remember {
            mutableStateOf("")
        }

    var waist by
        remember {
            mutableStateOf("")
        }

    var hips by
        remember {
            mutableStateOf("")
        }

    var leftArm by
        remember {
            mutableStateOf("")
        }

    var rightArm by
        remember {
            mutableStateOf("")
        }

    var leftForearm by
        remember {
            mutableStateOf("")
        }

    var rightForearm by
        remember {
            mutableStateOf("")
        }

    var leftThigh by
        remember {
            mutableStateOf("")
        }

    var rightThigh by
        remember {
            mutableStateOf("")
        }

    var leftCalf by
        remember {
            mutableStateOf("")
        }

    var rightCalf by
        remember {
            mutableStateOf("")
        }

    var message by
        remember {
            mutableStateOf<String?>(
                null
            )
        }

    var revision by
        remember {
            mutableIntStateOf(0)
        }

    val recent =
        remember(revision) {
            repository
                .listBodyObservations(
                    limit = 5
                )
        }

    fun clearForm() {
        bodyWeight = ""
        neck = ""
        shoulders = ""
        chest = ""
        waist = ""
        hips = ""
        leftArm = ""
        rightArm = ""
        leftForearm = ""
        rightForearm = ""
        leftThigh = ""
        rightThigh = ""
        leftCalf = ""
        rightCalf = ""
    }

    TrainlogScreen(
        subtitle = "M E N S U R A T I O N S"
    ) {
        TrainlogAction(
            label = "< Retour",
            description =
                "Revenir à l'accueil.",
            onClick = onBack,
            accent = colors.muted,
        )

        TrainlogFrame(
            title = "GENERAL"
        ) {
            BodyMetricField(
                label = "Poids",
                unit = "kg",
                value = bodyWeight,
                onValueChange = {
                    bodyWeight = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Cou",
                unit = "cm",
                value = neck,
                onValueChange = {
                    neck = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Épaules",
                unit = "cm",
                value = shoulders,
                onValueChange = {
                    shoulders = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Poitrine",
                unit = "cm",
                value = chest,
                onValueChange = {
                    chest = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Tour de taille",
                unit = "cm",
                value = waist,
                onValueChange = {
                    waist = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Hanches",
                unit = "cm",
                value = hips,
                onValueChange = {
                    hips = it
                    message = null
                },
            )
        }

        TrainlogFrame(
            title = "MEMBRES"
        ) {
            BodyMetricField(
                label = "Bras gauche",
                unit = "cm",
                value = leftArm,
                onValueChange = {
                    leftArm = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Bras droit",
                unit = "cm",
                value = rightArm,
                onValueChange = {
                    rightArm = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Avant-bras gauche",
                unit = "cm",
                value = leftForearm,
                onValueChange = {
                    leftForearm = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Avant-bras droit",
                unit = "cm",
                value = rightForearm,
                onValueChange = {
                    rightForearm = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Cuisse gauche",
                unit = "cm",
                value = leftThigh,
                onValueChange = {
                    leftThigh = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Cuisse droite",
                unit = "cm",
                value = rightThigh,
                onValueChange = {
                    rightThigh = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Mollet gauche",
                unit = "cm",
                value = leftCalf,
                onValueChange = {
                    leftCalf = it
                    message = null
                },
            )

            BodyMetricField(
                label = "Mollet droit",
                unit = "cm",
                value = rightCalf,
                onValueChange = {
                    rightCalf = it
                    message = null
                },
            )
        }

        TrainlogFrame(
            title = "ENREGISTREMENT"
        ) {
            TrainlogAction(
                label =
                    "Enregistrer les mensurations",
                description =
                    "Les champs vides sont ignorés.",
                accent =
                    colors.success,
                onClick = {
                    focusManager.clearFocus(
                        force = true
                    )

                    keyboardController?.hide()

                    val values =
                        listOf(
                            bodyWeight,
                            neck,
                            shoulders,
                            chest,
                            waist,
                            hips,
                            leftArm,
                            rightArm,
                            leftForearm,
                            rightForearm,
                            leftThigh,
                            rightThigh,
                            leftCalf,
                            rightCalf,
                        )

                    val parsed =
                        values.map {
                            parseOptionalMetric(
                                it
                            )
                        }

                    if (
                        parsed.any {
                            it is MetricParse.Invalid
                        }
                    ) {
                        message =
                            "Une valeur est invalide."
                    } else {
                        val doubles =
                            parsed.map {
                                when (it) {
                                    is MetricParse.Value ->
                                        it.value

                                    MetricParse.Empty ->
                                        null

                                    MetricParse.Invalid ->
                                        null
                                }
                            }

                        val draft =
                            BodyObservationDraft(
                                bodyWeightKg =
                                    doubles[0],
                                neckCm =
                                    doubles[1],
                                shouldersCm =
                                    doubles[2],
                                chestCm =
                                    doubles[3],
                                waistCm =
                                    doubles[4],
                                hipsCm =
                                    doubles[5],
                                leftArmCm =
                                    doubles[6],
                                rightArmCm =
                                    doubles[7],
                                leftForearmCm =
                                    doubles[8],
                                rightForearmCm =
                                    doubles[9],
                                leftThighCm =
                                    doubles[10],
                                rightThighCm =
                                    doubles[11],
                                leftCalfCm =
                                    doubles[12],
                                rightCalfCm =
                                    doubles[13],
                            )

                        when (
                            repository
                                .saveBodyObservation(
                                    draft
                                )
                        ) {
                            is SaveBodyObservationResult.Saved -> {
                                clearForm()
                                revision += 1
                                message =
                                    "Mensurations enregistrées."
                            }

                            SaveBodyObservationResult.Invalid -> {
                                message =
                                    "Ajoutez au moins une mesure positive."
                            }

                            SaveBodyObservationResult.DatabaseError -> {
                                message =
                                    "Erreur base locale."
                            }
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
                            "Mensurations enregistrées."
                        ) {
                            colors.success
                        } else {
                            colors.error
                        },
                )
            }
        }

        TrainlogFrame(
            title = "DERNIERS RELEVES",
            active =
                recent.isNotEmpty(),
        ) {
            if (recent.isEmpty()) {
                TrainlogInfo(
                    "Aucun relevé enregistré."
                )
            } else {
                recent.forEach {
                    item ->

                    TrainlogInfo(
                        text =
                            buildString {
                                append(
                                    formatStartedAt(
                                        item.observedAt
                                    )
                                )

                                append(
                                    " · ${item.metricCount} mesure(s)"
                                )

                                item.bodyWeightKg
                                    ?.let {
                                        append(
                                            " · %.1f kg"
                                                .format(it)
                                        )
                                    }
                            },
                        color =
                            colors.text,
                    )
                }
            }
        }
    }
}

@Composable
private fun BodyMetricField(
    label: String,
    unit: String,
    value: String,
    onValueChange: (String) -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    Column(
        modifier =
            Modifier.padding(
                bottom = 11.dp
            )
    ) {
        BasicText(
            text =
                "$label ($unit)"
                    .uppercase(),
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
            keyboardOptions =
                KeyboardOptions(
                    keyboardType =
                        KeyboardType.Decimal,
                    imeAction =
                        ImeAction.Next,
                ),
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

private sealed interface MetricParse {
    data object Empty :
        MetricParse

    data object Invalid :
        MetricParse

    data class Value(
        val value: Double,
    ) : MetricParse
}

private fun parseOptionalMetric(
    text: String,
): MetricParse {
    if (text.isBlank()) {
        return MetricParse.Empty
    }

    val value =
        text.trim()
            .replace(
                ',',
                '.'
            )
            .toDoubleOrNull()
            ?: return MetricParse.Invalid

    return if (value > 0.0) {
        MetricParse.Value(
            value
        )
    } else {
        MetricParse.Invalid
    }
}
