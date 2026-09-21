/*
 * Android AndroidAppShell.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.NavigationDrawerItem
import androidx.compose.material3.NavigationDrawerItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.shape.RoundedCornerShape
import com.labfytools.trainlog.R
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import kotlinx.coroutines.launch

private val drawerIcons = mapOf(
    AppSection.HOME to R.drawable.ic_home,
    AppSection.SESSIONS to R.drawable.ic_sessions,
    AppSection.EXERCISES to R.drawable.ic_exercises,
    AppSection.EQUIPMENT to R.drawable.ic_equipment,
    AppSection.SLEEP to R.drawable.ic_statistics,
    AppSection.SYNC to R.drawable.ic_sync,
    AppSection.SETTINGS to R.drawable.ic_settings,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AndroidAppShell(
    route: AppRoute,
    onOpenSection: (AppSection) -> Unit,
    onBack: () -> Boolean,
    content: @Composable () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext().resources
    fun sectionLabel(section: AppSection): String = strings.getString(when (section) {
        AppSection.HOME -> R.string.nav_home; AppSection.SESSIONS -> R.string.nav_sessions
        AppSection.EXERCISES -> R.string.nav_exercises; AppSection.EQUIPMENT -> R.string.nav_equipment
        AppSection.SLEEP -> R.string.nav_sleep
        AppSection.SYNC -> R.string.nav_sync; AppSection.SETTINGS -> R.string.nav_settings
    })
    fun title(): String = strings.getString(when (route) {
        AppRoute.Home -> R.string.nav_home; AppRoute.Sessions -> R.string.nav_sessions
        AppRoute.SessionEditor -> R.string.route_session_editor; AppRoute.SessionGenerator -> R.string.route_session_generator
        AppRoute.AiSessionDrafts -> R.string.route_ai_drafts; AppRoute.CompletedSessions -> R.string.route_completed_sessions
        AppRoute.Programs -> R.string.route_programs; is AppRoute.ProgramDetail -> R.string.route_program_detail
        is AppRoute.SessionDetail -> R.string.route_session_detail; is AppRoute.SessionCorrection -> R.string.route_session_correction
        AppRoute.Exercises -> R.string.nav_exercises; is AppRoute.ExerciseDetail -> R.string.route_exercise_detail
        is AppRoute.ExerciseEdit -> R.string.route_exercise_edit; is AppRoute.ExerciseCreate -> R.string.route_exercise_create
        AppRoute.Equipment -> R.string.nav_equipment; is AppRoute.EquipmentDetail -> R.string.route_equipment_detail
        is AppRoute.EquipmentCreate -> R.string.route_equipment_create
        AppRoute.BodyMeasurements -> R.string.route_body_measurements; AppRoute.LatestMaxima -> R.string.route_latest_maxima
        AppRoute.SleepDiary -> R.string.nav_sleep
        AppRoute.Sync -> R.string.nav_sync; AppRoute.Settings -> R.string.nav_settings
    })
    val drawerState = rememberDrawerState(DrawerValue.Closed)
    val scope = rememberCoroutineScope()
    val root = route == route.section.rootRoute()
    val configuration = LocalConfiguration.current
    val endMargin = if (configuration.fontScale >= 1.8f) 24.dp else 56.dp
    val drawerWidth = minOf(360.dp, configuration.screenWidthDp.dp - endMargin)

    BackHandler(enabled = drawerState.isOpen || route != AppRoute.Home) {
        if (drawerState.isOpen) scope.launch { drawerState.close() } else onBack()
    }

    ModalNavigationDrawer(
        drawerState = drawerState,
        drawerContent = {
            ModalDrawerSheet(
                /* WHY: the seven destinations and brand must remain reachable
                 * in short landscape windows and at large system font scales. */
                modifier = Modifier.width(drawerWidth).verticalScroll(rememberScrollState()),
                drawerContainerColor = colors.mantle,
            ) {
                Text("TRAINLOG", modifier = Modifier.padding(horizontal = 20.dp, vertical = 24.dp))
                /* Equipment remains a compatibility store in schema v13, but
                 * it is no longer a normal product destination. */
                AppSection.entries.filterNot { it == AppSection.EQUIPMENT }.forEach { section ->
                    NavigationDrawerItem(
                        icon = { Icon(painterResource(drawerIcons.getValue(section)), contentDescription = null) },
                        label = { Text(sectionLabel(section)) },
                        selected = route.section == section,
                        onClick = {
                            onOpenSection(section)
                            scope.launch { drawerState.close() }
                        },
                        modifier = Modifier.padding(horizontal = 12.dp, vertical = 2.dp),
                        shape = RoundedCornerShape(10.dp),
                        colors = NavigationDrawerItemDefaults.colors(
                            selectedContainerColor = colors.surfaceAlt,
                            selectedIconColor = colors.accent,
                            selectedTextColor = colors.text,
                            unselectedContainerColor = colors.mantle,
                            unselectedIconColor = colors.muted,
                            unselectedTextColor = colors.muted,
                        ),
                    )
                }
            }
        },
    ) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text(if (root) "TRAINLOG" else title()) },
                    navigationIcon = {
                        IconButton(
                            onClick = {
                                if (root) scope.launch { drawerState.open() } else onBack()
                            },
                            modifier = Modifier.semantics {
                                contentDescription = strings.getString(if (root) R.string.a11y_open_navigation else R.string.a11y_back)
                            },
                        ) {
                            Icon(
                                painterResource(if (root) R.drawable.ic_menu else R.drawable.ic_back),
                                contentDescription = null,
                            )
                        }
                    },
                    actions = {
                        if (!root) {
                            IconButton(
                                onClick = { scope.launch { drawerState.open() } },
                                modifier = Modifier.semantics { contentDescription = strings.getString(R.string.a11y_navigation) },
                            ) { Icon(painterResource(R.drawable.ic_menu), contentDescription = null) }
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(containerColor = colors.crust),
                )
            },
        ) { padding ->
            Box(Modifier.fillMaxSize().padding(padding)) { content() }
        }
    }
}
