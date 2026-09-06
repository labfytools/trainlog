package com.labfytools.trainlog.ui

import androidx.activity.compose.BackHandler
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.TrainlogRepository

private enum class TrainlogScreenId {
    HOME,
    SESSION,
    EXERCISE,
    BODY,
    HISTORY,
    SESSION_DETAIL,
}

@Composable
fun TrainlogApp(
    repository: TrainlogRepository,
) {
    var screen by
        remember {
            mutableStateOf(
                TrainlogScreenId.HOME
            )
        }

    var exerciseReturnTarget by
        remember {
            mutableStateOf(
                TrainlogScreenId.HOME
            )
        }

    var catalogRevision by
        remember {
            mutableIntStateOf(0)
        }

    var selectedSessionId by
        remember {
            mutableStateOf<String?>(
                null
            )
        }

    if (
        screen !=
        TrainlogScreenId.HOME
    ) {
        BackHandler {
            screen =
                when (screen) {
                    TrainlogScreenId.EXERCISE ->
                        exerciseReturnTarget

                    TrainlogScreenId.SESSION_DETAIL ->
                        TrainlogScreenId.HISTORY

                    else ->
                        TrainlogScreenId.HOME
                }
        }
    }

    when (screen) {
        TrainlogScreenId.HOME ->
            HomeScreen(
                onSession = {
                    screen =
                        TrainlogScreenId.SESSION
                },
                onExercise = {
                    exerciseReturnTarget =
                        TrainlogScreenId.HOME

                    screen =
                        TrainlogScreenId.EXERCISE
                },
                onBody = {
                    screen =
                        TrainlogScreenId.BODY
                },
                onHistory = {
                    screen =
                        TrainlogScreenId.HISTORY
                },
            )

        TrainlogScreenId.SESSION ->
            SessionScreen(
                repository = repository,
                catalogRevision =
                    catalogRevision,
                onBack = {
                    screen =
                        TrainlogScreenId.HOME
                },
                onCreateExercise = {
                    exerciseReturnTarget =
                        TrainlogScreenId.SESSION

                    screen =
                        TrainlogScreenId.EXERCISE
                },
            )

        TrainlogScreenId.EXERCISE ->
            ExerciseScreen(
                repository = repository,
                inline =
                    exerciseReturnTarget ==
                        TrainlogScreenId.SESSION,
                onBack = {
                    screen =
                        exerciseReturnTarget
                },
                onSaved = {
                    catalogRevision += 1

                    screen =
                        exerciseReturnTarget
                },
            )

        TrainlogScreenId.BODY ->
            BodyScreen(
                repository = repository,
                onBack = {
                    screen =
                        TrainlogScreenId.HOME
                },
            )

        TrainlogScreenId.HISTORY ->
            HistoryScreen(
                repository = repository,
                onBack = {
                    screen =
                        TrainlogScreenId.HOME
                },
                onOpenSession = {
                    sessionId ->
                        selectedSessionId =
                            sessionId

                        screen =
                            TrainlogScreenId.SESSION_DETAIL
                },
            )

        TrainlogScreenId.SESSION_DETAIL ->
            SessionDetailScreen(
                repository = repository,
                sessionId =
                    selectedSessionId,
                onBack = {
                    screen =
                        TrainlogScreenId.HISTORY
                },
            )
    }
}
