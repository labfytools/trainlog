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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.rememberSaveableStateHolder
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.CatalogInboxResult
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.data.TrainlogRepository

@Composable
fun TrainlogApp(repository: TrainlogRepository, exporter: SyncExporter, inbox: SyncCatalogInbox, requestOutbox: SyncRequestOutbox, appState: TrainlogAppState) {
    val navigation = appState.navigation
    val generatorState = appState.generator
    val equipmentState = appState.equipment
    val exerciseState = appState.exercise
    val bodyState = appState.body
    var catalogRevision by remember { mutableIntStateOf(0) }
    var draftRevision by remember { mutableIntStateOf(0) }
    var draftMessage by remember { mutableStateOf<String?>(null) }
    val navigationController = appState.navigationController

    /* CONTRACT: automatic exchange work belongs to the application lifecycle.
     * Re-entering a route must never trigger a second import or export. */
    LaunchedEffect(Unit) {
        when (inbox.importPcCatalog()) {
            is CatalogInboxResult.Imported -> catalogRevision++
            CatalogInboxResult.FolderNotAuthorized, CatalogInboxResult.FileNotFound,
            is CatalogInboxResult.Error -> Unit
        }
        exporter.exportMobileBundle()
    }

    val draftLoad = remember(draftRevision, catalogRevision) { repository.loadActiveSessionDraft() }
    val activeDraft = (draftLoad as? ActiveDraftLoadResult.Loaded)?.draft

    fun open(route: AppRoute) { navigationController.open(route) }
    fun openSection(section: AppSection) { navigationController.openSection(section) }
    fun back(): Boolean = navigationController.back()
    fun openManualSession() {
        when (draftLoad) {
            is ActiveDraftLoadResult.Loaded -> open(AppRoute.SessionEditor)
            ActiveDraftLoadResult.None -> when (val result = repository.startActiveSessionDraft()) {
                ActiveDraftMutationResult.Saved -> { draftRevision++; open(AppRoute.SessionEditor) }
                is ActiveDraftMutationResult.Error -> draftMessage = result.message
            }
            is ActiveDraftLoadResult.Error -> draftMessage = draftLoad.message
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
                activeDraft, draftMessage ?: (draftLoad as? ActiveDraftLoadResult.Error)?.message ?: (draftLoad as? ActiveDraftLoadResult.Loaded)?.warning,
                repository.listSessions().firstOrNull(), repository.listLatestExerciseMaxima().firstOrNull(),
                ::openManualSession,
                { open(AppRoute.BodyMeasurements) }, { open(AppRoute.SessionDetail(it)) }, { open(AppRoute.Sync) },
            )
            AppRoute.Sessions -> SessionsHub(activeDraft, { open(AppRoute.SessionEditor) }, ::openManualSession, { open(AppRoute.CompletedSessions) })
            AppRoute.SessionEditor -> SessionScreen(repository, catalogRevision, { draftRevision++; back() }, { open(AppRoute.ExerciseCreate(AppRoute.SessionEditor)) }, { exporter.exportMobileBundle(); draftRevision++ })
            AppRoute.SessionGenerator -> SessionGeneratorScreen(repository, generatorState, { back() }, { generatorState.abandon(); draftRevision++; open(AppRoute.SessionEditor) }, {
                draftMessage = "Une séance est déjà en cours. Reprenez-la ou revenez à la proposition conservée."
                draftRevision++
                /* WHY: ExistingActiveDraft is discovered only after the accept
                 * action, while the proposal is still dirty. CONTRACT: this
                 * route request uses the same owner and keep/discard guard as
                 * Back and drawer navigation. */
                openSection(AppSection.SESSIONS)
            })
            AppRoute.CompletedSessions -> HistoryScreen(repository, { back() }) { open(AppRoute.SessionDetail(it)) }
            is AppRoute.SessionDetail -> SessionDetailScreen(repository, route.sessionId, { back() }) { draftRevision++; open(AppRoute.SessionEditor) }
            AppRoute.Exercises -> ExerciseCatalogueScreen(repository, exerciseState, { open(AppRoute.ExerciseCreate(AppRoute.Exercises)) }, { open(AppRoute.ExerciseDetail(it)) })
            is AppRoute.ExerciseDetail -> ExerciseDetailScreen(repository, route.exerciseId, { open(AppRoute.ExerciseEdit(route.exerciseId, route)) }, { open(AppRoute.LatestMaxima) })
            is AppRoute.ExerciseCreate -> ExerciseEditorRoute(repository, exerciseState, null, route.caller == AppRoute.SessionEditor) { exporter.exportMobileBundle(); catalogRevision++; back() }
            is AppRoute.ExerciseEdit -> ExerciseEditorRoute(repository, exerciseState, route.exerciseId, false) { exporter.exportMobileBundle(); catalogRevision++; back() }
            AppRoute.Equipment -> EquipmentScreen(repository, equipmentState, { open(AppRoute.EquipmentCreate(AppRoute.Equipment)) }, { open(AppRoute.EquipmentDetail(it)) })
            is AppRoute.EquipmentDetail -> EquipmentDetailScreen(repository, route.equipmentId)
            is AppRoute.EquipmentCreate -> EquipmentCreateScreen(repository, equipmentState) { exporter.exportMobileBundle(); catalogRevision++; back() }
            AppRoute.Statistics -> StatisticsHub({ open(AppRoute.BodyMeasurements) }, { open(AppRoute.LatestMaxima) })
            AppRoute.BodyMeasurements -> BodyScreen(repository, bodyState, { exporter.exportMobileBundle() }, { back() })
            AppRoute.LatestMaxima -> LatestMaximaScreen(repository)
            AppRoute.Sync -> SyncScreen(inbox, requestOutbox, { exporter.exportMobileBundle(); catalogRevision++ }, { back() })
            AppRoute.Settings -> SettingsScreen(inbox) { exporter.exportMobileBundle(); catalogRevision++ }
        }}
    }

    if (navigationController.hasPendingNavigation) {
        val guardedRoute = navigationController.pendingRoute
        val generator = guardedRoute == AppRoute.SessionGenerator
        AlertDialog(
            onDismissRequest = navigationController::cancelPending,
            title = { Text(if (generator) "Proposition non acceptée" else "Modifications non enregistrées") },
            text = { Text(if (generator) "La proposition peut rester en mémoire pendant que vous changez de rubrique." else "Les champs restent en mémoire tant que vous ne les abandonnez pas explicitement.") },
            confirmButton = { TextButton(onClick = navigationController::keepAndNavigate) { Text("Conserver et quitter") } },
            dismissButton = {
                TextButton(onClick = {
                    navigationController.discardAndNavigate()
                }) { Text(if (generator) "Abandonner la proposition" else "Abandonner les modifications") }
            },
        )
    }
}

private fun AppRoute.stateKey(): String = when (this) {
    is AppRoute.SessionDetail -> "session:${sessionId}"
    is AppRoute.ExerciseDetail -> "exercise:${exerciseId}"
    is AppRoute.ExerciseEdit -> "exercise-edit:${exerciseId}:${caller.section}"
    is AppRoute.ExerciseCreate -> "exercise-create:${caller.section}"
    is AppRoute.EquipmentDetail -> "equipment:${equipmentId}"
    is AppRoute.EquipmentCreate -> "equipment-create:${caller.section}"
    else -> this::class.qualifiedName.orEmpty()
}
