/*
 * Android HistoryScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.R
import java.time.OffsetDateTime
import java.time.LocalDate
import java.time.format.DateTimeFormatter

@Composable
fun HistoryScreen(
    repository: TrainlogRepository,
    onBack: () -> Unit,
    onOpenSession: (String) -> Unit,
) {
    val locale = presentationLocale()
    val strings = localizedContext()
    val colors =
        LocalTrainlogColors.current

    val sessions =
        remember {
            repository.listSessions()
        }
    val latestMaxima =
        remember {
            repository.listLatestExerciseMaxima()
        }

    TrainlogScreen(
        subtitle = strings.getString(R.string.route_completed_sessions)
    ) {
        TrainlogFrame(
            title = strings.getString(R.string.nav_sessions),
            active =
                sessions.isNotEmpty(),
        ) {
            if (sessions.isEmpty()) {
                TrainlogInfo(
                    strings.getString(R.string.history_none)
                )
            } else {
                sessions.forEach {
                    session ->

                    TrainlogAction(
                        label =
                            formatStartedAt(
                                session.startedAt
                            ),
                        description =
                            sessionTypeLabel(
                                session.sessionType
                            ) +
                                " · " + strings.resources.getQuantityString(
                                    R.plurals.exercise_count, session.exerciseCount, session.exerciseCount),
                        onClick = {
                            onOpenSession(
                                session.sessionId
                            )
                        },
                        accent =
                            if (
                                session.sessionType ==
                                SessionType.MAX_TEST
                            ) {
                                colors.warning
                            } else {
                                colors.accent
                            },
                    )
                }
            }
        }

        TrainlogFrame(
            title = strings.getString(R.string.route_latest_maxima),
            active = latestMaxima.isNotEmpty(),
        ) {
            if (latestMaxima.isEmpty()) {
                TrainlogInfo(strings.getString(R.string.max_none))
            } else {
                latestMaxima.forEach { max ->
                    val weight = "%.2f".format(locale, max.maxWeightKg)
                        .trimEnd('0').trimEnd(',')
                    TrainlogInfo(
                        text = "${max.exerciseName} · $weight kg · ${formatDate(max.startedAt)}",
                        color = colors.warning,
                    )
                    TrainlogInfo(
                        text = strings.getString(R.string.equipment_value,
                            max.equipmentDisplayName ?: strings.getString(R.string.value_none)),
                        color = colors.muted,
                    )
                }
            }
        }
    }
}

@Composable
internal fun sessionTypeLabel(
    value: SessionType,
): String =
    if (
        value ==
        SessionType.MAX_TEST
    ) {
        uiString(R.string.session_max_test)
    } else {
        uiString(R.string.session_training)
    }

@Composable
internal fun formatStartedAt(
    value: String,
): String {
    val locale = presentationLocale()
    val pattern = uiString(R.string.date_time_pattern)
    return runCatching {
        OffsetDateTime.parse(value).format(DateTimeFormatter.ofPattern(pattern, locale))
    }.getOrElse { value.replace('T', ' ').take(16) }
}

@Composable
internal fun formatDate(value: String): String {
    val locale = presentationLocale()
    val pattern = uiString(R.string.date_pattern)
    val formatter = DateTimeFormatter.ofPattern(pattern, locale)
    return runCatching { LocalDate.parse(value.take(10)).format(formatter) }
        .recoverCatching { OffsetDateTime.parse(value).format(formatter) }
        .getOrElse { value.take(10) }
}
