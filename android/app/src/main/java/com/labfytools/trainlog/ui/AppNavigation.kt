package com.labfytools.trainlog.ui

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel

enum class AppSection(val label: String) {
    HOME("Accueil"), SESSIONS("Séances"), EXERCISES("Exercices"),
    EQUIPMENT("Équipements"), STATISTICS("Statistiques"), SYNC("Synchronisation"), SETTINGS("Paramètres"),
}

sealed interface AppRoute {
    val section: AppSection
    data object Home : AppRoute { override val section = AppSection.HOME }
    data object Sessions : AppRoute { override val section = AppSection.SESSIONS }
    data object SessionEditor : AppRoute { override val section = AppSection.SESSIONS }
    data object SessionGenerator : AppRoute { override val section = AppSection.SESSIONS }
    data object CompletedSessions : AppRoute { override val section = AppSection.SESSIONS }
    data class SessionDetail(val sessionId: String) : AppRoute { override val section = AppSection.SESSIONS }
    data object Exercises : AppRoute { override val section = AppSection.EXERCISES }
    data class ExerciseDetail(val exerciseId: String) : AppRoute { override val section = AppSection.EXERCISES }
    data class ExerciseEdit(val exerciseId: String, val caller: AppRoute) : AppRoute { override val section = caller.section }
    data class ExerciseCreate(val caller: AppRoute) : AppRoute { override val section = caller.section }
    data object Equipment : AppRoute { override val section = AppSection.EQUIPMENT }
    data class EquipmentDetail(val equipmentId: String) : AppRoute { override val section = AppSection.EQUIPMENT }
    data class EquipmentCreate(val caller: AppRoute) : AppRoute { override val section = caller.section }
    data object Statistics : AppRoute { override val section = AppSection.STATISTICS }
    data object BodyMeasurements : AppRoute { override val section = AppSection.STATISTICS }
    data object LatestMaxima : AppRoute { override val section = AppSection.STATISTICS }
    data object Sync : AppRoute { override val section = AppSection.SYNC }
    data object Settings : AppRoute { override val section = AppSection.SETTINGS }
}

fun AppSection.rootRoute(): AppRoute = when (this) {
    AppSection.HOME -> AppRoute.Home
    AppSection.SESSIONS -> AppRoute.Sessions
    AppSection.EXERCISES -> AppRoute.Exercises
    AppSection.EQUIPMENT -> AppRoute.Equipment
    AppSection.STATISTICS -> AppRoute.Statistics
    AppSection.SYNC -> AppRoute.Sync
    AppSection.SETTINGS -> AppRoute.Settings
}

/** CONTRACT: one owner performs every route transaction; routes never mutate persistence. */
class AppNavigationState(initial: AppRoute = AppRoute.Home, private val limit: Int = 16) {
    private val history = ArrayDeque<AppRoute>()
    var route: AppRoute by mutableStateOf(initial)
        private set

    fun open(destination: AppRoute) {
        if (destination == route) return
        history.addLast(route)
        while (history.size > limit) history.removeFirst()
        route = destination
    }

    fun openSection(section: AppSection) {
        history.clear()
        route = section.rootRoute()
    }

    fun back(): Boolean {
        val caller = when (val current = route) {
            is AppRoute.ExerciseCreate -> current.caller
            is AppRoute.ExerciseEdit -> current.caller
            is AppRoute.EquipmentCreate -> current.caller
            else -> null
        }
        /* INVARIANT: inline creation returns to its declared caller exactly
         * once. When open() recorded that caller, consume the matching entry
         * so the following Back continues beyond it instead of becoming a
         * no-op on the same route. */
        if (caller != null && history.lastOrNull() == caller) history.removeLast()
        route = caller ?: history.removeLastOrNull() ?: when (route) {
            AppRoute.Home -> return false
            else -> route.section.rootRoute().takeUnless { it == route } ?: AppRoute.Home
        }
        return true
    }
}

/**
 * WHY: root navigation guards are product behavior, so their owner must be
 * callable by both [TrainlogApp] and host-side regression tests.
 * CONTRACT: route requests never save, synchronize, finalize, or mutate the
 * repository; explicit discard clears only the transient state for the route
 * that raised the guard.
 */
class AppNavigationController(
    val navigation: AppNavigationState,
    private val generator: SessionGeneratorUiState,
    private val equipment: EquipmentScreenState,
    private val exercise: ExerciseScreenState,
    private val body: BodyScreenState,
) {
    private var pendingAction: (() -> Unit)? by mutableStateOf(null)
    private var guardedRoute: AppRoute? = null
    private var replacementBlocked = false
    val hasPendingNavigation: Boolean get() = pendingAction != null
    val pendingRoute: AppRoute? get() = guardedRoute

    private fun isDirty(route: AppRoute): Boolean =
        route == AppRoute.SessionGenerator && generator.hasUnacceptedWork ||
            route is AppRoute.EquipmentCreate && equipment.dirty ||
            (route is AppRoute.ExerciseCreate || route is AppRoute.ExerciseEdit) && exercise.dirty ||
            route == AppRoute.BodyMeasurements && body.dirty

    private fun request(replacesEditor: Boolean = false, action: () -> Unit): Boolean {
        val source = navigation.route
        return if (isDirty(source)) {
            guardedRoute = source
            replacementBlocked = replacesEditor
            pendingAction = action
            false
        } else {
            action()
            true
        }
    }

    fun open(route: AppRoute): Boolean {
        val source = navigation.route
        val replacement =
            (source is AppRoute.ExerciseCreate || source is AppRoute.ExerciseEdit) &&
                (route is AppRoute.ExerciseCreate || route is AppRoute.ExerciseEdit) && route != source
        return request(replacement) { navigation.open(route) }
    }
    fun openSection(section: AppSection): Boolean = request { navigation.openSection(section) }
    fun back(): Boolean = request { navigation.back() }
    fun cancelPending() { pendingAction = null; guardedRoute = null; replacementBlocked = false }
    fun keepAndNavigate() {
        /* INVARIANT: retaining a dirty form also retains its route identity;
         * a different target may only replace it after explicit discard. */
        if (replacementBlocked) cancelPending() else resolve(discard = false)
    }
    fun discardAndNavigate() = resolve(discard = true)

    private fun resolve(discard: Boolean) {
        val action = pendingAction ?: return
        val source = guardedRoute
        if (discard) when (source) {
            AppRoute.SessionGenerator -> generator.abandon()
            is AppRoute.EquipmentCreate -> equipment.abandonEdits()
            AppRoute.BodyMeasurements -> body.abandonEdits()
            is AppRoute.ExerciseCreate, is AppRoute.ExerciseEdit -> exercise.abandonEdits()
            else -> Unit
        }
        cancelPending()
        action()
    }
}

internal fun routeTitle(route: AppRoute): String = when (route) {
    AppRoute.Home -> "Accueil"
    AppRoute.Sessions -> "Séances"
    AppRoute.SessionEditor -> "Séance en cours"
    AppRoute.SessionGenerator -> "Programmer une séance"
    AppRoute.CompletedSessions -> "Séances effectuées"
    is AppRoute.SessionDetail -> "Détail de la séance"
    AppRoute.Exercises -> "Exercices"
    is AppRoute.ExerciseDetail -> "Fiche exercice"
    is AppRoute.ExerciseEdit -> "Modifier l'exercice"
    is AppRoute.ExerciseCreate -> "Créer un exercice"
    AppRoute.Equipment -> "Équipements"
    is AppRoute.EquipmentDetail -> "Fiche équipement"
    is AppRoute.EquipmentCreate -> "Créer un équipement"
    AppRoute.Statistics -> "Statistiques"
    AppRoute.BodyMeasurements -> "Mensurations"
    AppRoute.LatestMaxima -> "Derniers MAX"
    AppRoute.Sync -> "Synchronisation"
    AppRoute.Settings -> "Paramètres"
}

/**
 * WHY: configuration recreation must retain small navigation and form state,
 * while process death deliberately restores only the repository-owned draft.
 * No session or proposal is serialized into a Bundle.
 */
class TrainlogAppState : ViewModel() {
    val navigation = AppNavigationState()
    val generator = SessionGeneratorUiState()
    val equipment = EquipmentScreenState()
    val exercise = ExerciseScreenState()
    val body = BodyScreenState()
    val navigationController = AppNavigationController(navigation, generator, equipment, exercise, body)
}
