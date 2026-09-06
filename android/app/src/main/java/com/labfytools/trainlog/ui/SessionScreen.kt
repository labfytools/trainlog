package com.labfytools.trainlog.ui

/* TRAINLOG_ANDROID_MAX_TEST_SESSION_V1 */

/* TRAINLOG_ANDROID_SESSION_REMOVE */

/* TRAINLOG_VARIABLE_SET_REPS_V1 */

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

    var sessionType by
        remember {
            mutableStateOf(
                SessionType.TRAINING
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
            title = "TYPE DE SEANCE"
        ) {
            TrainlogAction(
                label =
                    if (
                        sessionType ==
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
                        sessionType ==
                        SessionType.TRAINING
                    ) {
                        colors.success
                    } else {
                        colors.muted
                    },
                onClick = {
                    sessionType =
                        SessionType.TRAINING
                },
            )

            TrainlogAction(
                label =
                    if (
                        sessionType ==
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
                        sessionType ==
                        SessionType.MAX_TEST
                    ) {
                        colors.warning
                    } else {
                        colors.muted
                    },
                onClick = {
                    sessionType =
                        SessionType.MAX_TEST
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
                            sessionType ==
                            SessionType.MAX_TEST
                        ) {
                            "TEST MAX"
                        } else {
                            "ENTRAÎNEMENT"
                        },
                color =
                    if (
                        sessionType ==
                        SessionType.MAX_TEST
                    ) {
                        colors.warning
                    } else {
                        colors.accent
                    },
            )

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

                        TrainlogAction(
                            label =
                                "Retirer ${draft.exercise.name}",
                            description =
                                "Supprimer cet exercice de la séance en cours.",
                            accent =
                                colors.error,
                            onClick = {
                                draftExercises =
                                    draftExercises
                                        .filterIndexed {
                                                itemIndex,
                                                _ ->
                                            itemIndex !=
                                                index
                                        }

                                sessionRevision += 1

                                message =
                                    "Exercice retiré de la séance."
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
                                            draftExercises,
                                        sessionType =
                                            sessionType,
                                    )
                                )
                    ) {
                        is SaveSessionResult.Saved -> {
                            draftExercises =
                                emptyList()

                            selectedExercise =
                                null

                            sessionType =
                                SessionType.TRAINING

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
            mutableStateOf("3x10")
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
                    },
                )

                TrainlogInfo(
                    text =
                        "Formats : 5x10 · 4,5,6,7 · 4..10..4",
                    color =
                        colors.muted,
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
                    exercise = exercise,
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

        SessionExerciseDraft(
            exercise = exercise,
            sets =
                reps.map {
                    SessionSetDraft(
                        reps = it
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
                exercise = exercise,
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
