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
import com.labfytools.trainlog.R
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import kotlinx.coroutines.launch

private val drawerIcons = mapOf(
    AppSection.HOME to R.drawable.ic_home,
    AppSection.SESSIONS to R.drawable.ic_sessions,
    AppSection.EXERCISES to R.drawable.ic_exercises,
    AppSection.EQUIPMENT to R.drawable.ic_equipment,
    AppSection.STATISTICS to R.drawable.ic_statistics,
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
                AppSection.entries.forEach { section ->
                    NavigationDrawerItem(
                        icon = { Icon(painterResource(drawerIcons.getValue(section)), contentDescription = null) },
                        label = { Text(section.label) },
                        selected = route.section == section,
                        onClick = {
                            onOpenSection(section)
                            scope.launch { drawerState.close() }
                        },
                        modifier = Modifier.padding(horizontal = 12.dp),
                    )
                }
            }
        },
    ) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text(if (root) "TRAINLOG" else routeTitle(route)) },
                    navigationIcon = {
                        IconButton(
                            onClick = {
                                if (root) scope.launch { drawerState.open() } else onBack()
                            },
                            modifier = Modifier.semantics {
                                contentDescription = if (root) "Ouvrir la navigation" else "Retour"
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
                                modifier = Modifier.semantics { contentDescription = "Navigation" },
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
