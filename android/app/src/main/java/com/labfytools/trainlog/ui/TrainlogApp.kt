/*
 * Android TrainlogApp.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

/* TRAINLOG_PC_CATALOG_AUTO_APPLY */

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.R
import androidx.compose.runtime.saveable.rememberSaveableStateHolder
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CatalogInboxResult
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.data.TrainlogRepository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun TrainlogApp(repository: TrainlogRepository, exporter: SyncExporter, inbox: SyncCatalogInbox, requestOutbox: SyncRequestOutbox, appState: TrainlogAppState) {
    val localized = localizedContext()
    val strings = localized.resources
    val navigation = appState.navigation
    val generatorState = appState.generator
    val equipmentState = appState.equipment
    val exerciseState = appState.exercise
    val bodyState = appState.body
    var catalogRevision by remember { mutableIntStateOf(0) }
    var draftRevision by remember { mutableIntStateOf(0) }
    var draftMessage by remember { mutableStateOf<String?>(null) }
    val navigationController = appState.navigationController
    val coroutineScope = rememberCoroutineScope()
    fun exportSnapshot() {
        coroutineScope.launch { exporter.exportMobileBundle() }
    }

    /* CONTRACT: automatic exchange work belongs to the application lifecycle.
     * Re-entering a route must never trigger a second import or export. */
    LaunchedEffect(Unit) {
        when (withContext(Dispatchers.IO) { inbox.importPcCatalog() }) {
            is CatalogInboxResult.Imported -> catalogRevision++
            CatalogInboxResult.FolderNotAuthorized, CatalogInboxResult.FileNotFound,
            is CatalogInboxResult.Error -> Unit
        }
    }

    val draftLoad = remember(draftRevision, catalogRevision) { repository.loadActiveSessionDraft() }
    val activeDraft = (draftLoad as? ActiveDraftLoadResult.Loaded)?.draft
    val pendingAiDraftCount = remember(draftRevision, catalogRevision) {
        repository.listAiSessionDrafts().size
    }

    fun open(route: AppRoute) { navigationController.open(route) }
    fun openSection(section: AppSection) { navigationController.openSection(section) }
    fun back(): Boolean = navigationController.back()
    fun openManualSession() {
        when (draftLoad) {
            is ActiveDraftLoadResult.Loaded -> open(AppRoute.SessionEditor)
            ActiveDraftLoadResult.None -> when (val result = repository.startActiveSessionDraft()) {
                ActiveDraftMutationResult.Saved -> { draftRevision++; open(AppRoute.SessionEditor) }
                is ActiveDraftMutationResult.Error -> draftMessage = localizedRepositoryMessage(localized, result.message)
            }
            is ActiveDraftLoadResult.Error -> draftMessage = localizedRepositoryMessage(localized, draftLoad.message)
        }
    }

    val routeStateHolder = rememberSaveableStateHolder()
    val retainedRouteKeys = remember { ArrayDeque<String>() }
    val routeKey = navigation.route.stateKey()
    LaunchedEffect(routeKey) {
        retainedRouteKeys.remove(routeKey)
        retainedRouteKeys.addLast(routeKey)
        while (retainedRouteKeys.size > 16) routeStateHolder.removeState(retainedRouteKeys.removeFirst())
    }

    AndroidAppShell(navigation.route, ::openSection, ::back) {
        routeStateHolder.SaveableStateProvider(routeKey) {
        when (val route = navigation.route) {
            AppRoute.Home -> HomeScreen(
                activeDraft, draftMessage ?: (draftLoad as? ActiveDraftLoadResult.Error)?.let { localizedRepositoryMessage(localized, it.message) } ?: (draftLoad as? ActiveDraftLoadResult.Loaded)?.warning?.let { localizedRepositoryMessage(localized, it) },
                repository.listSessions().firstOrNull(), repository.listLatestExerciseMaxima().firstOrNull(),
                repository.getBodyZoneHomeOverview(),
                ::openManualSession,
                { zoneId ->
                    exerciseState.filterZoneId.value = zoneId
                    exerciseState.unclassifiedFilter.value = false
                    exerciseState.searchQuery.value = ""
                    open(AppRoute.Exercises)
                },
                { open(AppRoute.Statistics) },
                { open(AppRoute.BodyMeasurements) }, { open(AppRoute.SessionDetail(it)) }, { open(AppRoute.Sync) },
            )
            AppRoute.Sessions -> SessionsHub(activeDraft, pendingAiDraftCount, { open(AppRoute.SessionEditor) }, ::openManualSession,
                { open(AppRoute.AiSessionDrafts) }, { open(AppRoute.CompletedSessions) })
            AppRoute.SessionEditor -> SessionScreen(repository, catalogRevision, { draftRevision++; back() }, { open(AppRoute.ExerciseCreate(AppRoute.SessionEditor)) }, { exportSnapshot(); draftRevision++ })
            AppRoute.SessionGenerator -> SessionGeneratorScreen(repository, generatorState, { back() }, { generatorState.abandon(); draftRevision++; open(AppRoute.SessionEditor) }, {
                draftMessage = strings.getString(R.string.existing_draft_warning)
                draftRevision++
                /* WHY: ExistingActiveDraft is discovered only after the accept
                 * action, while the proposal is still dirty. CONTRACT: this
                 * route request uses the same owner and keep/discard guard as
                 * Back and drawer navigation. */
                openSection(AppSection.SESSIONS)
            })
            AppRoute.AiSessionDrafts -> AiSessionDraftsScreen(
                repository = repository,
                externalRevision = catalogRevision,
                onPendingChanged = { draftRevision++ },
                onStarted = { open(AppRoute.SessionEditor) },
            )
            AppRoute.CompletedSessions -> HistoryScreen(repository, { back() }) { open(AppRoute.SessionDetail(it)) }
            is AppRoute.SessionDetail -> SessionDetailScreen(repository, route.sessionId, { back() }, { open(AppRoute.SessionCorrection(route.sessionId)) }) { draftRevision++; open(AppRoute.SessionEditor) }
            is AppRoute.SessionCorrection -> CompletedSessionCorrectionScreen(repository, route.sessionId) { saved ->
                if (saved) exportSnapshot()
                back()
            }
            AppRoute.Exercises -> ExerciseCatalogueScreen(repository, exerciseState, { open(AppRoute.ExerciseCreate(AppRoute.Exercises)) }, { open(AppRoute.ExerciseDetail(it)) })
            is AppRoute.ExerciseDetail -> ExerciseDetailScreen(repository, route.exerciseId, { open(AppRoute.ExerciseEdit(route.exerciseId, route)) }, { open(AppRoute.LatestMaxima) })
            is AppRoute.ExerciseCreate -> ExerciseEditorRoute(repository, exerciseState, null, route.caller == AppRoute.SessionEditor) { exportSnapshot(); catalogRevision++; back() }
            is AppRoute.ExerciseEdit -> ExerciseEditorRoute(repository, exerciseState, route.exerciseId, false) { exportSnapshot(); catalogRevision++; back() }
            AppRoute.Equipment -> EquipmentScreen(repository, equipmentState, { open(AppRoute.EquipmentCreate(AppRoute.Equipment)) }, { open(AppRoute.EquipmentDetail(it)) })
            is AppRoute.EquipmentDetail -> EquipmentDetailScreen(repository, route.equipmentId) { open(AppRoute.ExerciseDetail(it)) }
            is AppRoute.EquipmentCreate -> EquipmentCreateScreen(repository, equipmentState) { exportSnapshot(); catalogRevision++; back() }
            AppRoute.Statistics -> StatisticsDashboard(repository)
            AppRoute.BodyMeasurements -> BodyScreen(repository, bodyState, { exportSnapshot() }, { back() })
            AppRoute.LatestMaxima -> LatestMaximaScreen(repository)
            AppRoute.Sync -> SyncScreen(repository, inbox, exporter, requestOutbox, { exportSnapshot(); catalogRevision++ }, { back() })
            AppRoute.Settings -> SettingsScreen(repository, inbox) { exportSnapshot(); catalogRevision++ }
        }}
    }

    if (navigationController.hasPendingNavigation) {
        val guardedRoute = navigationController.pendingRoute
        val generator = guardedRoute == AppRoute.SessionGenerator
        AlertDialog(
            onDismissRequest = navigationController::cancelPending,
            title = { Text(strings.getString(if (generator) R.string.dialog_unaccepted_proposal else R.string.dialog_unsaved_changes)) },
            text = { Text(strings.getString(if (generator) R.string.dialog_proposal_retained else R.string.dialog_fields_retained)) },
            confirmButton = { TextButton(onClick = navigationController::keepAndNavigate) { Text(strings.getString(R.string.dialog_keep_leave)) } },
            dismissButton = {
                TextButton(onClick = {
                    navigationController.discardAndNavigate()
                }) { Text(strings.getString(if (generator) R.string.dialog_discard_proposal else R.string.dialog_discard_changes)) }
            },
        )
    }
}

private fun AppRoute.stateKey(): String = when (this) {
    is AppRoute.SessionDetail -> "session:${sessionId}"
    is AppRoute.SessionCorrection -> "session-correction:${sessionId}"
    is AppRoute.ExerciseDetail -> "exercise:${exerciseId}"
    is AppRoute.ExerciseEdit -> "exercise-edit:${exerciseId}:${caller.section}"
    is AppRoute.ExerciseCreate -> "exercise-create:${caller.section}"
    is AppRoute.EquipmentDetail -> "equipment:${equipmentId}"
    is AppRoute.EquipmentCreate -> "equipment-create:${caller.section}"
    else -> this::class.qualifiedName.orEmpty()
}
