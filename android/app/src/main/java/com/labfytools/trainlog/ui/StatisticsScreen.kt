package com.labfytools.trainlog.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.data.StatisticsFrequency
import com.labfytools.trainlog.data.StatisticsOverview
import com.labfytools.trainlog.data.StatisticsPeriod
import com.labfytools.trainlog.data.StatisticsPoint
import com.labfytools.trainlog.data.StatisticsSeries
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography
import java.time.LocalDate
import java.time.temporal.WeekFields
import java.util.Locale
import kotlin.math.max

private enum class StatisticsDetail { PROGRESS, BODY, FREQUENCY, DISTRIBUTION, SUMMARY }

internal data class DashboardWeek(val label: String, val sessions: Int, val maxima: Int)

internal fun sixWeekFrequency(
    frequency: List<StatisticsFrequency>,
    today: LocalDate = LocalDate.now(),
): List<DashboardWeek> {
    val fields = WeekFields.ISO
    val byWeek = frequency.associateBy { it.week }
    val currentMonday = today.with(fields.dayOfWeek(), 1)
    return (5L downTo 0L).map { offset ->
        val date = currentMonday.minusWeeks(offset)
        val key = "%04d-W%02d".format(
            Locale.ROOT,
            date.get(fields.weekBasedYear()),
            date.get(fields.weekOfWeekBasedYear()),
        )
        byWeek[key]?.let { DashboardWeek(key, it.all, it.maxima) }
            ?: DashboardWeek(key, 0, 0)
    }
}

@Composable
fun StatisticsDashboard(repository: TrainlogRepository) {
    var period by remember { mutableStateOf(StatisticsPeriod.DAYS_30) }
    var detail by remember { mutableStateOf<StatisticsDetail?>(null) }
    val overview = remember(period) { repository.loadStatistics(period) }
    if (detail != null) return StatisticsDetailScreen(detail!!, overview) { detail = null }
    TrainlogScreen("Statistiques") {
        TrainlogChoiceChips(StatisticsPeriod.entries.map { it.name to it.label }, period.name) {
            period = StatisticsPeriod.valueOf(it)
        }
        if (overview.hasInvalidData) TrainlogInfo(
            "Certaines données historiques invalides ont été ignorées; les statistiques valides restent disponibles.",
        )
        SummaryGrid(overview)
        PerformanceCard(overview) { detail = StatisticsDetail.PROGRESS }
        MeasurementsCard(overview) { detail = StatisticsDetail.BODY }
        FrequencyCard(overview) { detail = StatisticsDetail.FREQUENCY }
        TrainlogFrame("EXPLORER") {
            TrainlogAction("Répartition", "Zones et utilisation des exercices, uniquement quand elles sont classifiées.", { detail = StatisticsDetail.DISTRIBUTION })
            TrainlogAction("Résumé global", "Indicateurs prudents dérivés des données disponibles.", { detail = StatisticsDetail.SUMMARY })
        }
    }
}

@Composable
private fun SummaryGrid(overview: StatisticsOverview) {
    TrainlogFrame("RÉSUMÉ · ${overview.period.label}") {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Metric("Séances", overview.summary.sessions.toString(), Modifier.weight(1f))
            Metric("Séries", overview.summary.performedSets.toString(), Modifier.weight(1f))
        }
        Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Metric("Exercices", overview.summary.distinctExercises.toString(), Modifier.weight(1f))
            Metric("MAX explicites", overview.summary.explicitMaxima.toString(), Modifier.weight(1f))
        }
    }
}

@Composable
private fun Metric(label: String, value: String, modifier: Modifier) {
    val colors = LocalTrainlogColors.current
    Column(modifier.background(colors.surface, RoundedCornerShape(10.dp)).padding(horizontal = 12.dp, vertical = 8.dp)) {
        Text(label, style = TrainlogTypography.small.copy(color = colors.muted))
        Text(value, style = TrainlogTypography.value.copy(color = colors.text))
    }
}

@Composable
internal fun PerformanceCard(overview: StatisticsOverview, click: () -> Unit) {
    val colors = LocalTrainlogColors.current
    val work = overview.performanceEvents.sumOf { it.workingImprovements }
    val maxima = overview.performanceEvents.sumOf { it.maxImprovements }
    TrainlogFrame("PROGRESSION", active = true) {
        Column(Modifier.fillMaxWidth().clickable(onClick = click).padding(vertical = 2.dp)) {
            PerformanceTimeline(overview)
            Text(
                if (work + maxima == 0) "Données insuffisantes pour établir une progression comparable."
                else "$work progression(s) travail · $maxima record(s) MAX",
                style = TrainlogTypography.small.copy(color = if (work + maxima == 0) colors.muted else colors.text),
                modifier = Modifier.padding(top = 6.dp),
            )
            Text(
                "${overview.summary.explicitMaxima} MAX explicite(s) · ouvrir le détail",
                style = TrainlogTypography.small.copy(color = colors.notice),
                modifier = Modifier.padding(top = 4.dp),
            )
        }
    }
}

@Composable
private fun PerformanceTimeline(overview: StatisticsOverview) {
    val colors = LocalTrainlogColors.current
    val recent = overview.performanceEvents.takeLast(6).map { it.workingImprovements + it.maxImprovements }
    val values = List(6 - recent.size) { 0 } + recent
    val description = if (values.all { it == 0 })
        "Progression sur six semaines : aucun événement comparable, six points vides"
    else "Progression sur six semaines : ${values.joinToString()} événements"
    Canvas(Modifier.fillMaxWidth().height(54.dp).semantics { contentDescription = description }) {
        val step = size.width / values.size
        val peak = max(1, values.max())
        values.forEachIndexed { index, value ->
            val center = Offset(step * (index + .5f), size.height / 2f)
            if (value == 0) drawCircle(colors.muted, radius = 3.dp.toPx(), center = center)
            else drawCircle(colors.info, radius = (4 + 4f * value / peak).dp.toPx(), center = center)
        }
    }
}

@Composable
internal fun MeasurementsCard(overview: StatisticsOverview, click: () -> Unit) {
    val colors = LocalTrainlogColors.current
    TrainlogFrame("MENSURATIONS", active = overview.body.isNotEmpty()) {
        Column(Modifier.fillMaxWidth().clickable(onClick = click)) {
            if (overview.body.isEmpty()) {
                Text("Aucune mensuration dans cette période.", style = TrainlogTypography.normal.copy(color = colors.muted))
            } else overview.body.chunked(2).forEachIndexed { rowIndex, row ->
                Row(
                    Modifier.fillMaxWidth().padding(top = if (rowIndex == 0) 0.dp else 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    row.forEach { series -> MeasurementTile(series, Modifier.weight(1f)) }
                    if (row.size == 1) Box(Modifier.weight(1f))
                }
            }
        }
    }
}

@Composable
private fun MeasurementTile(series: StatisticsSeries, modifier: Modifier) {
    val colors = LocalTrainlogColors.current
    val latest = series.points.last().value
    val delta = series.points.takeIf { it.size >= 2 }?.let { latest - it[it.lastIndex - 1].value }
    Column(modifier.background(colors.surface, RoundedCornerShape(10.dp)).padding(10.dp)) {
        Text(series.label, style = TrainlogTypography.small.copy(color = colors.muted))
        Text("$latest ${series.unit}", style = TrainlogTypography.section.copy(color = colors.text))
        if (delta == null) Text("1 relevé · tendance indisponible", style = TrainlogTypography.small.copy(color = colors.muted))
        else {
            Text("Δ ${if (delta >= 0) "+" else ""}$delta", style = TrainlogTypography.small.copy(color = colors.info))
            LineChart(listOf(series), Modifier.height(42.dp))
        }
    }
}

@Composable
internal fun FrequencyCard(overview: StatisticsOverview, today: LocalDate = LocalDate.now(), click: () -> Unit) {
    val colors = LocalTrainlogColors.current
    val weeks = sixWeekFrequency(overview.frequency, today)
    val current = weeks.last()
    TrainlogFrame("FRÉQUENCE", active = overview.frequency.isNotEmpty()) {
        Column(Modifier.fillMaxWidth().clickable(onClick = click)) {
            FrequencyBars(weeks)
            Text(
                "Semaine actuelle · ${current.sessions} séance(s) · ${current.maxima} MAX inclus",
                style = TrainlogTypography.small.copy(color = colors.accent, fontWeight = FontWeight.Bold),
                modifier = Modifier.padding(top = 6.dp),
            )
            Text(
                "Les MAX signalent des séances; ils ne sont jamais additionnés une seconde fois.",
                style = TrainlogTypography.small.copy(color = colors.muted),
                modifier = Modifier.padding(top = 2.dp),
            )
        }
    }
}

@Composable
private fun FrequencyBars(weeks: List<DashboardWeek>) {
    val colors = LocalTrainlogColors.current
    val description = "Fréquence sur six semaines : " + weeks.mapIndexed { index, week ->
        "${if (index == weeks.lastIndex) "actuelle" else week.label} ${week.sessions} séances dont ${week.maxima} MAX"
    }.joinToString("; ")
    Canvas(Modifier.fillMaxWidth().height(116.dp).semantics { contentDescription = description }) {
        val peak = max(1, weeks.maxOf { it.sessions })
        val slot = size.width / weeks.size
        val barWidth = slot * .44f
        weeks.forEachIndexed { index, week ->
            val barHeight = if (week.sessions == 0) 3.dp.toPx() else size.height * .82f * week.sessions / peak
            val left = slot * index + (slot - barWidth) / 2f
            val top = size.height - barHeight
            drawRoundRect(
                color = if (index == weeks.lastIndex) colors.accent else colors.info,
                topLeft = Offset(left, top), size = Size(barWidth, barHeight), cornerRadius = CornerRadius(5.dp.toPx()),
            )
            /* MAX is a marker within the session bar, never stacked onto it. */
            if (week.maxima > 0) drawCircle(
                colors.notice, radius = 4.dp.toPx(), center = Offset(left + barWidth / 2f, top + 7.dp.toPx()),
            )
        }
    }
}

@Composable
private fun DashboardCard(title: String, subtitle: String, series: List<StatisticsSeries>, click: () -> Unit) {
    val colors = LocalTrainlogColors.current
    TrainlogFrame(title, active = series.isNotEmpty()) {
        Column(Modifier.fillMaxWidth().clickable(onClick = click).padding(vertical = 4.dp)) {
            Text(subtitle, style = TrainlogTypography.small.copy(color = colors.muted))
            if (series.isEmpty()) Text("Pas encore assez de données pour une courbe.", style = TrainlogTypography.normal.copy(color = colors.muted), modifier = Modifier.padding(top = 12.dp))
            else {
                LineChart(series.take(3))
                Text("${series.size} série(s) comparable(s) · ouvrir le détail", style = TrainlogTypography.small.copy(color = colors.accent), modifier = Modifier.padding(top = 6.dp))
            }
        }
    }
}

@Composable
private fun LineChart(series: List<StatisticsSeries>, modifier: Modifier = Modifier.height(108.dp)) {
    val colors = LocalTrainlogColors.current
    Canvas(modifier.fillMaxWidth().padding(top = 6.dp)) {
        val palette = listOf(colors.lavender, colors.graph, colors.success)
        series.forEachIndexed { seriesIndex, item ->
            val values = item.points.map { it.value }
            if (values.size < 2) return@forEachIndexed
            val minimum = values.min()
            val span = (values.max() - minimum).takeIf { it > 0.0 } ?: 1.0
            val path = androidx.compose.ui.graphics.Path()
            values.forEachIndexed { index, value ->
                val point = Offset(
                    size.width * index / (values.size - 1),
                    size.height - (size.height * ((value - minimum) / span)).toFloat(),
                )
                if (index == 0) path.moveTo(point.x, point.y) else path.lineTo(point.x, point.y)
            }
            drawPath(path, palette[seriesIndex % palette.size], style = Stroke(3.dp.toPx(), cap = StrokeCap.Round))
        }
    }
}

@Composable
private fun StatisticsDetailScreen(detail: StatisticsDetail, overview: StatisticsOverview, back: () -> Unit) {
    val (title, series, description) = when (detail) {
        StatisticsDetail.PROGRESS -> Triple("Progression", overview.performance, "Chaque ligne de détail conserve un exercice canonique, un équipement et un mode comparables; le dashboard ne somme jamais des kg.")
        StatisticsDetail.BODY -> Triple("Mensurations", overview.body, "Aucune interpolation : seuls les relevés réellement enregistrés sont affichés.")
        StatisticsDetail.FREQUENCY -> Triple("Fréquence", listOf(StatisticsSeries("f", "Séances", "séances", overview.frequency.map { x -> StatisticsPoint(x.week, x.all.toDouble(), x.week) })), "Nombre de séances avec travail réel par semaine; MAX est un marqueur, pas une seconde séance.")
        StatisticsDetail.DISTRIBUTION -> Triple("Répartition", emptyList(), "La répartition détaillée arrive lorsque les occurrences classifiées sont disponibles dans cette vue.")
        StatisticsDetail.SUMMARY -> Triple("Résumé global", emptyList(), "${overview.summary.sessions} séances · ${overview.summary.performedSets} séries · ${overview.summary.distinctExercises} exercices · ${overview.summary.explicitMaxima} MAX explicites")
    }
    TrainlogScreen(title) {
        TrainlogAction("← Retour au dashboard", "", back)
        TrainlogInfo(description)
        if (series.isEmpty()) TrainlogInfo("Aucune donnée comparable dans cette période.")
        else series.forEach { DashboardCard(it.label, "${it.points.size} point(s) · ${it.unit}", listOf(it), {}) }
    }
}
