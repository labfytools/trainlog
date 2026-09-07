package com.labfytools.trainlog.ui

/* TRAINLOG_PC_CATALOG_AUTO_APPLY */

import androidx.activity.compose.BackHandler
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.CatalogInboxResult
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncRequestOutbox

private enum class TrainlogScreenId {
    HOME,
    SESSION,
    EXERCISE,
    BODY,
    HISTORY,
    SESSION_DETAIL,
    SYNC,
}

@Composable
fun TrainlogApp(
    repository: TrainlogRepository,
    exporter: SyncExporter,
    inbox: SyncCatalogInbox,
    requestOutbox: SyncRequestOutbox,
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

    var draftRevision by
        remember {
            mutableIntStateOf(0)
        }

    var draftMessage by
        remember {
            mutableStateOf<String?>(null)
        }

    var selectedSessionId by
        remember {
            mutableStateOf<String?>(
                null
            )
        }

    LaunchedEffect(Unit) {
        when (
            inbox.importPcCatalog()
        ) {
            is CatalogInboxResult.Imported -> {
                catalogRevision += 1
            }

            CatalogInboxResult.FolderNotAuthorized,
            CatalogInboxResult.FileNotFound,
            is CatalogInboxResult.Error -> {
                /* Nothing to import yet. */
            }
        }
    }

    LaunchedEffect(Unit) {
        exporter.exportMobileBundle()
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
        TrainlogScreenId.HOME -> {
            val draftLoad =
                remember(
                    draftRevision,
                    catalogRevision,
                ) {
                    repository.loadActiveSessionDraft()
                }

            HomeScreen(
                activeDraft =
                    (draftLoad as?
                        ActiveDraftLoadResult.Loaded)
                        ?.draft,
                draftError =
                    draftMessage
                        ?: (draftLoad as?
                            ActiveDraftLoadResult.Error)
                            ?.message
                        ?: (draftLoad as?
                            ActiveDraftLoadResult.Loaded)
                            ?.warning,
                onSession = {
                    when (draftLoad) {
                        is ActiveDraftLoadResult.Loaded -> {
                            draftMessage = null
                            screen = TrainlogScreenId.SESSION
                        }

                        ActiveDraftLoadResult.None -> {
                            when (
                                val result =
                                    repository
                                        .startActiveSessionDraft()
                            ) {
                                ActiveDraftMutationResult.Saved -> {
                                    draftMessage = null
                                    draftRevision += 1
                                    screen =
                                        TrainlogScreenId.SESSION
                                }

                                is ActiveDraftMutationResult.Error -> {
                                    draftMessage = result.message
                                }
                            }
                        }

                        is ActiveDraftLoadResult.Error -> {
                            draftMessage = draftLoad.message
                        }
                    }
                },
                onDiscardDraft = {
                    when (
                        val result =
                            repository
                                .discardActiveSessionDraft()
                    ) {
                        ActiveDraftMutationResult.Saved -> {
                            draftMessage = null
                            draftRevision += 1
                        }

                        is ActiveDraftMutationResult.Error -> {
                            draftMessage = result.message
                        }
                    }
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
                onSync = {
                    screen =
                        TrainlogScreenId.SYNC
                },
            )
        }

        TrainlogScreenId.SESSION ->
            SessionScreen(
                repository = repository,
                catalogRevision =
                    catalogRevision,
                onBack = {
                    /* WHY: Back changes routing only; the repository remains
                     * the canonical owner of the in-progress workout. */
                    draftRevision += 1
                    screen =
                        TrainlogScreenId.HOME
                },
                onCreateExercise = {
                    exerciseReturnTarget =
                        TrainlogScreenId.SESSION

                    screen =
                        TrainlogScreenId.EXERCISE
                },
                onSessionSaved = {
                    exporter.exportMobileBundle()
                    draftRevision += 1
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
                    exporter.exportMobileBundle()

                    catalogRevision += 1

                    screen =
                        exerciseReturnTarget
                },
            )

        TrainlogScreenId.BODY ->
            BodyScreen(
                repository = repository,
                onBodySaved = {
                    exporter.exportMobileBundle()
                },
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

        TrainlogScreenId.SYNC ->
            SyncScreen(
                inbox = inbox,
                requestOutbox =
                    requestOutbox,
                onCatalogChanged = {
                    exporter.exportMobileBundle()

                    catalogRevision += 1
                },
                onBack = {
                    screen =
                        TrainlogScreenId.HOME
                },
            )
    }
}
