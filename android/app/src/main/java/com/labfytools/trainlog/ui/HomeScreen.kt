package com.labfytools.trainlog.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.asAndroidPath
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.zIndex
import com.labfytools.trainlog.data.BodyZoneHomeOverview
import com.labfytools.trainlog.data.BodyZoneHomeState
import com.labfytools.trainlog.data.BodyZoneHomeStatus
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.LatestExerciseMax
import com.labfytools.trainlog.model.SessionSummary
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography

@Composable
fun HomeScreen(
    activeDraft: ActiveSessionDraft?,
    draftError: String?,
    latestSession: SessionSummary?,
    latestMaximum: LatestExerciseMax?,
    bodyZoneOverview: BodyZoneHomeOverview,
    onSession: () -> Unit,
    onOpenExercises: (String) -> Unit,
    onOpenStatistics: () -> Unit,
    onBody: () -> Unit,
    onOpenLatestSession: (String) -> Unit,
    onSync: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    TrainlogScreen("Accueil", scrollKey = "home") {
        draftError?.let { TrainlogInfo(it, colors.error) }
        TrainlogFrame("Séance") {
            if (activeDraft != null) {
                val kind = if (activeDraft.sessionType == SessionType.MAX_TEST) "Test max" else "Entraînement"
                TrainlogPrimaryAction("Reprendre la séance en cours", "$kind · ${activeDraft.exercises.size} exercice(s)", onSession)
            } else {
                TrainlogPrimaryAction("Nouvelle séance manuelle", "Créer explicitement un brouillon durable.", onSession)
            }
        }
        BodyZoneHomeSection(bodyZoneOverview, onOpenExercises, onOpenStatistics)
        latestSession?.let { session ->
            TrainlogFrame("Dernière séance") {
                TrainlogAction(formatStartedAt(session.startedAt), "${sessionTypeLabel(session.sessionType)} · ${session.exerciseCount} exercice(s)", { onOpenLatestSession(session.sessionId) })
            }
        }
        latestMaximum?.let { maximum ->
            val weight = "%.2f".format(java.util.Locale.FRANCE, maximum.maxWeightKg).trimEnd('0').trimEnd(',')
            TrainlogInfo("Dernier MAX · ${maximum.exerciseName} · $weight kg · ${maximum.startedAt.take(10)}", colors.warning)
        }
        TrainlogFrame("Accès rapides", active = false) {
            TrainlogAction("Mensurations", "Ajouter ou consulter les relevés locaux.", onBody)
            TrainlogAction("Synchronisation", "Consulter l'état connu et lancer une action explicite.", onSync)
        }
    }
}

/**
 * CONTRACT: this section reports conservative recorded exposure only. Its
 * wording and actions must not imply recovery/readiness/fatigue or expose the
 * hidden session generator.
 */
@Composable
internal fun BodyZoneHomeSection(
    overview: BodyZoneHomeOverview,
    onOpenExercises: (String) -> Unit,
    onOpenStatistics: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    var showExposureInfo by remember { mutableStateOf(false) }
    var selectedId by remember(overview.zones) {
        mutableStateOf(overview.recommendations.firstOrNull()?.zoneId ?: overview.zones.firstOrNull()?.zoneId)
    }
    val selected = overview.zones.firstOrNull { it.zoneId == selectedId }
    TrainlogFrame("Zones à travailler", active = overview.recommendations.isNotEmpty()) {
        TextButton(onClick = { showExposureInfo = true }) {
            Text("Exposition récente  ⓘ")
        }
        if (!overview.hasTrainingHistory) {
            TrainlogInfo("Pas encore d’historique d’entraînement.")
        }
        if (overview.hasInvalidHistoryTimestamp) {
            TrainlogInfo("Données insuffisantes : un horodatage historique ne peut pas être interprété.", colors.warning)
        }
        BodyZoneMap(overview.zones, selectedId) { selectedId = it }
        selected?.let { zone ->
            BasicText(
                "Sélection : ${zone.displayName}",
                modifier = Modifier.padding(bottom = 8.dp),
                style = TrainlogTypography.normal.copy(color = colors.lavender),
            )
        }
        TrainlogInfo("Priorités", colors.accent)
        overview.recommendations.take(3).forEachIndexed { index, zone ->
            TrainlogAction(
                "${index + 1}. ${zone.displayName} · ${homeStateLabel(zone)}",
                recommendationReason(zone),
                { selectedId = zone.zoneId },
            )
        }
        if (overview.recommendations.isEmpty()) {
            TrainlogInfo("Aucune zone disposant d’un exercice résolu n’est disponible.")
        }
        selected?.let { zone ->
            Column(
                Modifier.fillMaxWidth().padding(top = 8.dp).background(colors.surface, RoundedCornerShape(8.dp)).padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                BasicText(zone.displayName, style = TrainlogTypography.section.copy(color = colors.text))
                BasicText(homeStateLabel(zone), style = TrainlogTypography.small.copy(color = stateColor(zone.state, colors)))
                BasicText(
                    "Dernier travail principal : ${ageLabel(zone.primaryExposureAgeSeconds)}",
                    style = TrainlogTypography.small.copy(color = colors.text),
                )
                BasicText(
                    recentWorkLabel(zone.primaryWork7Days, "principal") + " · " +
                        recentWorkLabel(zone.secondaryWork7Days, "secondaire"),
                    style = TrainlogTypography.small.copy(color = colors.text),
                )
                BasicText(
                    availableExerciseLabel(zone.availableExerciseCount),
                    style = TrainlogTypography.small.copy(color = colors.text),
                )
                if (zone.availableExerciseCount == 0) TrainlogInfo(BodyZoneHomeState.UNSUPPORTED.label, colors.muted)
                TrainlogAction("Voir les exercices", "", { onOpenExercises(zone.zoneId) })
                TrainlogAction("Voir les statistiques", "", onOpenStatistics)
            }
        }
    }
    if (showExposureInfo) {
        AlertDialog(
            onDismissRequest = { showExposureInfo = false },
            title = { Text("Exposition récente") },
            text = {
                Text("Ces couleurs décrivent le travail enregistré récemment. Elles ne mesurent pas la récupération physiologique.")
            },
            confirmButton = {
                TextButton(onClick = { showExposureInfo = false }) { Text("Compris") }
            },
        )
    }
}

@Composable
private fun BodyZoneMap(zones: List<BodyZoneHomeStatus>, selectedId: String?, select: (String) -> Unit) {
    val byId = zones.associateBy { it.zoneId }
    Row(
        Modifier.fillMaxWidth().padding(vertical = 8.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        BodyMapPanel(BodyView.FRONT, byId, selectedId, select, Modifier.weight(1f))
        BodyMapPanel(BodyView.BACK, byId, selectedId, select, Modifier.weight(1f))
    }
}

@Composable
private fun BodyMapPanel(
    view: BodyView,
    zones: Map<String, BodyZoneHomeStatus>,
    selectedId: String?,
    select: (String) -> Unit,
    modifier: Modifier,
) {
    val colors = LocalTrainlogColors.current
    val visibleZones = view.zoneIds.mapNotNull(zones::get)
    Column(modifier.semantics { contentDescription = "Carte corporelle, ${view.label}" }) {
        BasicText(view.label, style = TrainlogTypography.small.copy(color = colors.muted))
        BoxWithConstraints(Modifier.fillMaxWidth().height(236.dp)) {
            val panelWidth = maxWidth
            val panelHeight = maxHeight
            Canvas(
                Modifier
                    .matchParentSize()
                    .zIndex(1f)
                    .testTag("body-map-${view.testName}")
                    .pointerInput(view, visibleZones) {
                        detectTapGestures { tap ->
                            bodyZoneAtPoint(
                                viewName = view.testName,
                                width = size.width.toFloat(),
                                height = size.height.toFloat(),
                                point = tap,
                                visibleZoneIds = visibleZones.mapTo(mutableSetOf()) { it.zoneId },
                            )?.let(select)
                        }
                    },
            ) {
                drawOval(
                    color = colors.surfaceAlt,
                    topLeft = Offset(size.width * .405f, size.height * .015f),
                    size = Size(size.width * .19f, size.height * .135f),
                )
                drawPath(neckPath(size.width, size.height), colors.surfaceAlt)
                bodyRegions(view, size.width, size.height).forEach { region ->
                    val zone = zones[region.zoneId] ?: return@forEach
                    val chosen = selectedId == region.zoneId
                    region.paths.forEach { path ->
                        drawPath(path, stateColor(zone.state, colors).copy(alpha = .62f))
                        if (chosen) {
                            drawPath(
                                path,
                                color = colors.lavender.copy(alpha = .34f),
                                style = Stroke(width = 8f),
                            )
                        }
                        drawPath(
                            path,
                            color = if (chosen) colors.lavender else colors.crust.copy(alpha = .72f),
                            style = Stroke(width = if (chosen) 4.5f else 1.25f),
                        )
                    }
                }
            }

            // Accessibility controls deliberately have no pointer modifier: direct
            // touch is resolved against the Path geometry above, while TalkBack gets
            // one explicit, comfortably sized Button target per visible zone.
            visibleZones.forEach { zone ->
                val target = view.accessibilityBounds.getValue(zone.zoneId)
                val chosen = selectedId == zone.zoneId
                Box(
                    Modifier
                        .offset(panelWidth * target.left, panelHeight * target.top)
                        .size(
                            width = maxOf(panelWidth * target.width, 48.dp),
                            height = maxOf(panelHeight * target.height, 48.dp),
                        )
                        .testTag("body-zone-${view.testName}-${zone.zoneId}")
                        .semantics {
                            role = Role.Button
                            selected = chosen
                            contentDescription = buildString {
                                append(view.label)
                                append(", ")
                                append(zone.displayName)
                                append(", ")
                                append(homeStateLabel(zone))
                                append(if (chosen) ", sélectionnée" else ", non sélectionnée")
                            }
                            onClick(label = "Sélectionner ${zone.displayName}") {
                                select(zone.zoneId)
                                true
                            }
                        },
                )
            }
        }
    }
}

private enum class BodyView(
    val label: String,
    val testName: String,
    val zoneIds: List<String>,
    val accessibilityBounds: Map<String, Rect>,
) {
    FRONT(
        label = "Face",
        testName = "front",
        zoneIds = listOf("shoulders", "chest", "arms", "core", "thighs", "calves"),
        accessibilityBounds = mapOf(
            "shoulders" to Rect(.18f, .14f, .82f, .32f),
            "chest" to Rect(.27f, .24f, .73f, .40f),
            "arms" to Rect(.02f, .30f, .32f, .59f),
            "core" to Rect(.31f, .39f, .69f, .59f),
            "thighs" to Rect(.25f, .58f, .75f, .81f),
            "calves" to Rect(.27f, .79f, .73f, .99f),
        ),
    ),
    BACK(
        label = "Dos",
        testName = "back",
        zoneIds = listOf("shoulders", "back", "arms", "glutes", "thighs", "calves"),
        accessibilityBounds = mapOf(
            "shoulders" to Rect(.18f, .14f, .82f, .32f),
            "back" to Rect(.29f, .26f, .71f, .53f),
            "arms" to Rect(.68f, .30f, .98f, .59f),
            "glutes" to Rect(.29f, .52f, .71f, .67f),
            "thighs" to Rect(.25f, .65f, .75f, .83f),
            "calves" to Rect(.27f, .81f, .73f, .99f),
        ),
    ),
}

private data class BodyRegion(val zoneId: String, val paths: List<Path>)

private fun Path.containsPoint(point: Offset): Boolean {
    val approximation = asAndroidPath().approximate(.5f)
    val pointCount = approximation.size / 3
    if (pointCount < 3) return false
    var inside = false
    var previous = pointCount - 1
    for (current in 0 until pointCount) {
        val currentX = approximation[current * 3 + 1]
        val currentY = approximation[current * 3 + 2]
        val previousX = approximation[previous * 3 + 1]
        val previousY = approximation[previous * 3 + 2]
        if ((currentY > point.y) != (previousY > point.y)) {
            val crossingX = (previousX - currentX) * (point.y - currentY) /
                (previousY - currentY) + currentX
            if (point.x < crossingX) inside = !inside
        }
        previous = current
    }
    return inside
}

internal fun bodyZoneAtPoint(
    viewName: String,
    width: Float,
    height: Float,
    point: Offset,
    visibleZoneIds: Set<String>,
): String? {
    val view = BodyView.entries.firstOrNull { it.testName == viewName } ?: return null
    return bodyRegions(view, width, height)
        .lastOrNull { it.zoneId in visibleZoneIds && it.paths.any { path -> path.containsPoint(point) } }
        ?.zoneId
}

/**
 * CONTRACT: these stylized shapes represent only Trainlog BODY ZONES UX. They
 * are not medical anatomy and never communicate recovery, fatigue or readiness.
 * Every coordinate is normalized; drawing and hit testing must both call this
 * function with the same Canvas size so their Path geometry remains identical.
 */
private fun bodyRegions(view: BodyView, width: Float, height: Float): List<BodyRegion> {
    fun path(build: Path.(Float, Float) -> Unit) = Path().apply { build(width, height) }
    fun paired(zoneId: String, left: Path, right: Path) = BodyRegion(zoneId, listOf(left, right))

    val shoulders = paired(
        "shoulders",
        path { w, h ->
            moveTo(.48f * w, .175f * h); cubicTo(.40f * w, .17f * h, .30f * w, .165f * h, .22f * w, .215f * h)
            cubicTo(.18f * w, .24f * h, .18f * w, .285f * h, .23f * w, .30f * h)
            cubicTo(.30f * w, .275f * h, .38f * w, .25f * h, .48f * w, .245f * h); close()
        },
        path { w, h ->
            moveTo(.52f * w, .175f * h); cubicTo(.60f * w, .17f * h, .70f * w, .165f * h, .78f * w, .215f * h)
            cubicTo(.82f * w, .24f * h, .82f * w, .285f * h, .77f * w, .30f * h)
            cubicTo(.70f * w, .275f * h, .62f * w, .25f * h, .52f * w, .245f * h); close()
        },
    )
    val arms = paired(
        "arms",
        path { w, h ->
            moveTo(.215f * w, .255f * h); cubicTo(.16f * w, .285f * h, .145f * w, .365f * h, .13f * w, .43f * h)
            lineTo(.105f * w, .56f * h); cubicTo(.12f * w, .59f * h, .17f * w, .595f * h, .19f * w, .555f * h)
            lineTo(.255f * w, .305f * h); close()
        },
        path { w, h ->
            moveTo(.785f * w, .255f * h); cubicTo(.84f * w, .285f * h, .855f * w, .365f * h, .87f * w, .43f * h)
            lineTo(.895f * w, .56f * h); cubicTo(.88f * w, .59f * h, .83f * w, .595f * h, .81f * w, .555f * h)
            lineTo(.745f * w, .305f * h); close()
        },
    )
    val thighs = paired(
        "thighs",
        path { w, h ->
            moveTo(.35f * w, .565f * h); cubicTo(.31f * w, .63f * h, .30f * w, .71f * h, .315f * w, .805f * h)
            cubicTo(.34f * w, .83f * h, .405f * w, .825f * h, .435f * w, .795f * h)
            lineTo(.475f * w, .57f * h); close()
        },
        path { w, h ->
            moveTo(.65f * w, .565f * h); cubicTo(.69f * w, .63f * h, .70f * w, .71f * h, .685f * w, .805f * h)
            cubicTo(.66f * w, .83f * h, .595f * w, .825f * h, .565f * w, .795f * h)
            lineTo(.525f * w, .57f * h); close()
        },
    )
    val calves = paired(
        "calves",
        path { w, h ->
            moveTo(.315f * w, .805f * h); cubicTo(.285f * w, .86f * h, .315f * w, .91f * h, .335f * w, .98f * h)
            lineTo(.405f * w, .98f * h); cubicTo(.41f * w, .91f * h, .45f * w, .855f * h, .435f * w, .795f * h); close()
        },
        path { w, h ->
            moveTo(.685f * w, .805f * h); cubicTo(.715f * w, .86f * h, .685f * w, .91f * h, .665f * w, .98f * h)
            lineTo(.595f * w, .98f * h); cubicTo(.59f * w, .91f * h, .55f * w, .855f * h, .565f * w, .795f * h); close()
        },
    )

    return if (view == BodyView.FRONT) {
        val chest = paired(
            "chest",
            path { w, h ->
                moveTo(.27f * w, .285f * h); cubicTo(.32f * w, .245f * h, .40f * w, .24f * h, .49f * w, .265f * h)
                lineTo(.49f * w, .38f * h); cubicTo(.42f * w, .395f * h, .34f * w, .38f * h, .29f * w, .345f * h); close()
            },
            path { w, h ->
                moveTo(.73f * w, .285f * h); cubicTo(.68f * w, .245f * h, .60f * w, .24f * h, .51f * w, .265f * h)
                lineTo(.51f * w, .38f * h); cubicTo(.58f * w, .395f * h, .66f * w, .38f * h, .71f * w, .345f * h); close()
            },
        )
        val core = BodyRegion("core", listOf(path { w, h ->
            moveTo(.30f * w, .36f * h); cubicTo(.35f * w, .40f * h, .38f * w, .43f * h, .37f * w, .55f * h)
            cubicTo(.41f * w, .58f * h, .59f * w, .58f * h, .63f * w, .55f * h)
            cubicTo(.62f * w, .43f * h, .65f * w, .40f * h, .70f * w, .36f * h)
            cubicTo(.60f * w, .39f * h, .40f * w, .39f * h, .30f * w, .36f * h); close()
        }))
        listOf(shoulders, chest, arms, core, thighs, calves)
    } else {
        val back = BodyRegion("back", listOf(path { w, h ->
            moveTo(.28f * w, .285f * h); cubicTo(.35f * w, .25f * h, .43f * w, .245f * h, .50f * w, .265f * h)
            cubicTo(.57f * w, .245f * h, .65f * w, .25f * h, .72f * w, .285f * h)
            cubicTo(.68f * w, .38f * h, .64f * w, .47f * h, .62f * w, .55f * h)
            cubicTo(.56f * w, .57f * h, .44f * w, .57f * h, .38f * w, .55f * h)
            cubicTo(.36f * w, .47f * h, .32f * w, .38f * h, .28f * w, .285f * h); close()
        }))
        val glutes = paired(
            "glutes",
            path { w, h ->
                moveTo(.37f * w, .535f * h); cubicTo(.31f * w, .57f * h, .32f * w, .64f * h, .39f * w, .665f * h)
                cubicTo(.43f * w, .675f * h, .475f * w, .645f * h, .49f * w, .595f * h); lineTo(.49f * w, .55f * h); close()
            },
            path { w, h ->
                moveTo(.63f * w, .535f * h); cubicTo(.69f * w, .57f * h, .68f * w, .64f * h, .61f * w, .665f * h)
                cubicTo(.57f * w, .675f * h, .525f * w, .645f * h, .51f * w, .595f * h); lineTo(.51f * w, .55f * h); close()
            },
        )
        listOf(shoulders, back, arms, glutes, thighs, calves)
    }
}

private fun neckPath(width: Float, height: Float) = Path().apply {
    moveTo(.445f * width, .13f * height)
    lineTo(.43f * width, .19f * height)
    cubicTo(.46f * width, .205f * height, .54f * width, .205f * height, .57f * width, .19f * height)
    lineTo(.555f * width, .13f * height)
    close()
}

private fun stateColor(state: BodyZoneHomeState, colors: TrainlogColors): Color {
    return when (state) {
        BodyZoneHomeState.PRIORITIZE -> colors.lavender
        BodyZoneHomeState.LITTLE_RECENT_WORK -> colors.info
        BodyZoneHomeState.RECENT_WORK -> colors.success
        BodyZoneHomeState.HIGH_RECENT_EXPOSURE -> colors.notice
        BodyZoneHomeState.INSUFFICIENT_DATA -> colors.warning
        BodyZoneHomeState.UNSUPPORTED -> colors.muted
    }
}

internal fun recommendationReason(zone: BodyZoneHomeStatus): String = when {
    zone.lastPrimaryExposure == null -> "Aucune exposition principale enregistrée"
    zone.primaryWork7Days == 0 -> "Dernier travail principal : ${ageLabel(zone.primaryExposureAgeSeconds)}"
    else -> recentWorkLabel(zone.primaryWork7Days, "principal")
}

internal fun homeStateLabel(zone: BodyZoneHomeStatus): String =
    if (zone.state == BodyZoneHomeState.PRIORITIZE && zone.primaryWork7Days > 0) {
        "Priorité relative"
    } else {
        zone.state.label
    }

internal fun recentWorkLabel(count: Int, kind: String): String =
    "Travail $kind sur 7 j : $count"

private fun availableExerciseLabel(count: Int): String =
    "$count ${if (count == 1) "exercice disponible" else "exercices disponibles"}"

internal fun ageLabel(seconds: Long?): String = when {
    seconds == null -> "jamais enregistrée"
    seconds < 86400L -> "aujourd’hui"
    seconds < 2L * 86400L -> "il y a 1 jour"
    else -> "il y a ${seconds / 86400L} jours"
}
