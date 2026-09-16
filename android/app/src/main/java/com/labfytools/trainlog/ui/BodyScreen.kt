/*
 * Android BodyScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
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
import com.labfytools.trainlog.data.SaveBodyObservationResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.BodyObservationDraft
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.R

class BodyScreenState {
    val values = List(14) { mutableStateOf("") }
    val dirty: Boolean get() = values.any { it.value.isNotEmpty() }
    fun abandonEdits() { values.forEach { it.value = "" } }
}

@Composable
fun BodyScreen(
    repository: TrainlogRepository,
    state: BodyScreenState,
    onBodySaved: () -> Unit,
    onBack: () -> Unit,
) {
    val strings = localizedContext()
    val locale = presentationLocale()
    val colors =
        LocalTrainlogColors.current

    val focusManager =
        LocalFocusManager.current

    val keyboardController =
        LocalSoftwareKeyboardController.current

    var bodyWeight by state.values[0]; var neck by state.values[1]
    var shoulders by state.values[2]; var chest by state.values[3]
    var waist by state.values[4]; var hips by state.values[5]
    var leftArm by state.values[6]; var rightArm by state.values[7]
    var leftForearm by state.values[8]; var rightForearm by state.values[9]
    var leftThigh by state.values[10]; var rightThigh by state.values[11]
    var leftCalf by state.values[12]; var rightCalf by state.values[13]

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
        state.abandonEdits()
    }

    TrainlogScreen(
        subtitle = strings.getString(R.string.route_body_measurements)
    ) {
        TrainlogFrame(
            title = strings.getString(R.string.general)
        ) {
            BodyMetricField(
                label = strings.getString(R.string.weight),
                unit = "kg",
                value = bodyWeight,
                onValueChange = {
                    bodyWeight = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.neck),
                unit = "cm",
                value = neck,
                onValueChange = {
                    neck = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.shoulders),
                unit = "cm",
                value = shoulders,
                onValueChange = {
                    shoulders = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.chest),
                unit = "cm",
                value = chest,
                onValueChange = {
                    chest = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.waist),
                unit = "cm",
                value = waist,
                onValueChange = {
                    waist = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.hips),
                unit = "cm",
                value = hips,
                onValueChange = {
                    hips = it
                    message = null
                },
            )
        }

        TrainlogFrame(
            title = strings.getString(R.string.limbs)
        ) {
            BodyMetricField(
                label = strings.getString(R.string.left_arm),
                unit = "cm",
                value = leftArm,
                onValueChange = {
                    leftArm = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.right_arm),
                unit = "cm",
                value = rightArm,
                onValueChange = {
                    rightArm = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.left_forearm),
                unit = "cm",
                value = leftForearm,
                onValueChange = {
                    leftForearm = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.right_forearm),
                unit = "cm",
                value = rightForearm,
                onValueChange = {
                    rightForearm = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.left_thigh),
                unit = "cm",
                value = leftThigh,
                onValueChange = {
                    leftThigh = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.right_thigh),
                unit = "cm",
                value = rightThigh,
                onValueChange = {
                    rightThigh = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.left_calf),
                unit = "cm",
                value = leftCalf,
                onValueChange = {
                    leftCalf = it
                    message = null
                },
            )

            BodyMetricField(
                label = strings.getString(R.string.right_calf),
                unit = "cm",
                value = rightCalf,
                onValueChange = {
                    rightCalf = it
                    message = null
                },
            )
        }

        TrainlogFrame(
            title = strings.getString(R.string.recording)
        ) {
            TrainlogAction(
                label =
                    strings.getString(R.string.body_save),
                description =
                    strings.getString(R.string.empty_fields_ignored),
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
                            strings.getString(R.string.invalid_value)
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
                                    strings.getString(R.string.body_saved)

                                onBodySaved()
                            }

                            SaveBodyObservationResult.Invalid -> {
                                message =
                                    strings.getString(R.string.positive_measure_required)
                            }

                            SaveBodyObservationResult.DatabaseError -> {
                                message =
                                    strings.getString(R.string.local_database_error)
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
                            strings.getString(R.string.body_saved)
                        ) {
                            colors.success
                        } else {
                            colors.error
                        },
                )
            }
        }

        TrainlogFrame(
            title = strings.getString(R.string.latest_measurements),
            active =
                recent.isNotEmpty(),
        ) {
            if (recent.isEmpty()) {
                TrainlogInfo(
                    strings.getString(R.string.body_none)
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
                                    " · " + strings.resources.getQuantityString(R.plurals.measurement_count, item.metricCount, item.metricCount)
                                )

                                item.bodyWeightKg
                                    ?.let {
                                        append(
                                            " · %.1f kg".format(locale, it)
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
    TrainlogInputField(
        label = "$label ($unit)",
        value = value,
        onValueChange =
            onValueChange,
        keyboardOptions =
            KeyboardOptions(
                keyboardType =
                    KeyboardType.Decimal,
                imeAction =
                    ImeAction.Next,
            ),
    )
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
