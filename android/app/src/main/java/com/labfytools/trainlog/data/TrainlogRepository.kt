package com.labfytools.trainlog.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteConstraintException
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import android.util.JsonReader
import android.util.JsonToken
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.BodyObservationDraft
import com.labfytools.trainlog.model.BodyObservationSummary
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.ExerciseEditInput
import com.labfytools.trainlog.model.LatestExerciseMax
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSummary
import com.labfytools.trainlog.model.SessionDetail
import com.labfytools.trainlog.model.SessionExerciseDetail
import com.labfytools.trainlog.model.SessionExercisePlan
import com.labfytools.trainlog.model.SessionLoadMode
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONArray
import org.json.JSONObject
import java.text.Normalizer
import java.io.StringReader
import java.time.LocalDate
import java.time.OffsetDateTime
import java.time.temporal.WeekFields
import java.util.Locale
import java.util.UUID

/* WHY: ended_at is absent on valid imported/history rows and present lifecycle
 * metadata cannot prove actual work. CONTRACT: observable history requires at
 * least one occurrence-owned performed set, continuous activity, or explicit
 * MAX. INVARIANT: plans and targets alone never satisfy this predicate. */
private const val ACTUAL_SESSION_PREDICATE = """
    EXISTS (SELECT 1 FROM session_exercises AS actual_se
            JOIN performed_sets AS ps ON ps.session_exercise_row_id=actual_se.id
            WHERE actual_se.session_row_id=s.id)
    OR EXISTS (SELECT 1 FROM session_exercises AS actual_se
               JOIN continuous_activity AS ca ON ca.session_exercise_row_id=actual_se.id
               WHERE actual_se.session_row_id=s.id)
    OR EXISTS (SELECT 1 FROM session_exercises AS actual_se
               JOIN max_results AS mr ON mr.session_exercise_row_id=actual_se.id
               WHERE actual_se.session_row_id=s.id)
"""

sealed interface CreateExerciseResult {
    data class Created(
        val exercise: ExerciseProfile,
    ) : CreateExerciseResult

    data object Conflict :
        CreateExerciseResult

    data object Invalid :
        CreateExerciseResult

    data class DatabaseError(val message: String) : CreateExerciseResult
}

sealed interface EditExerciseResult {
    data class Saved(
        val exercise: ExerciseProfile,
    ) : EditExerciseResult

    data object InvalidNameOrProfile : EditExerciseResult
    data object Conflict : EditExerciseResult
    data object IncompatibleProfileChange : EditExerciseResult
    data object DatabaseError : EditExerciseResult
}


sealed interface PcCatalogImportResult {
    data class Applied(
        val imported: Int,
        val reconciled: Int,
        val skipped: Int,
    ) : PcCatalogImportResult

    data class Invalid(
        val message: String,
    ) : PcCatalogImportResult

    data object DatabaseError :
        PcCatalogImportResult
}

/** Read-only diagnostic emitted by the production PC-catalog importer. */
data class PcCatalogExerciseDecision(
    val exerciseId: String,
    val name: String,
    val recordingMode: String,
    val trackingMode: String,
    val dataFields: Int,
    val lookup: String,
    val decision: String,
)

sealed interface EquipmentAssociationImportResult {
    data class Applied(val updated: Int) : EquipmentAssociationImportResult
    data class Invalid(val message: String) : EquipmentAssociationImportResult
    data object DatabaseError : EquipmentAssociationImportResult
}

sealed interface EquipmentDefinitionImportResult {
    data class Applied(val imported: Int, val skipped: Int) : EquipmentDefinitionImportResult
    data class Invalid(val message: String) : EquipmentDefinitionImportResult
    data object DatabaseError : EquipmentDefinitionImportResult
}

sealed interface ExerciseBodyZoneImportResult {
    data class Applied(
        val updated: Int,
        val skipped: Int,
        val keptLocal: Int,
    ) : ExerciseBodyZoneImportResult
    data class Invalid(val message: String) : ExerciseBodyZoneImportResult
    data class Conflict(val exerciseId: String) : ExerciseBodyZoneImportResult
    data object DatabaseError : ExerciseBodyZoneImportResult
}

sealed interface ExerciseAliasImportResult {
    data class Applied(val added: Int, val skipped: Int) : ExerciseAliasImportResult
    data class Invalid(val message: String) : ExerciseAliasImportResult
    data class Conflict(val sourceExerciseId: String) : ExerciseAliasImportResult
    data object DatabaseError : ExerciseAliasImportResult
}

sealed interface MobileSessionImportResult {
    /** CONTRACT: counters describe persistent mutations, not artifact size. */
    data class Applied(
        val sessionsAdded: Int,
        val sessionsSkipped: Int,
        val bodyObservationsAdded: Int,
        val bodyObservationsSkipped: Int,
        val sessionsUpdated: Int = 0,
    ) : MobileSessionImportResult
    data class Invalid(val message: String) : MobileSessionImportResult
    data object DatabaseError : MobileSessionImportResult
}

sealed interface CreateEquipmentResult {
    data class Created(val equipment: EquipmentCatalogEntry) : CreateEquipmentResult
    data object Invalid : CreateEquipmentResult
    data object Conflict : CreateEquipmentResult
    data class DatabaseError(val message: String) : CreateEquipmentResult
}

sealed interface SaveBodyObservationResult {
    data class Saved(
        val observationId: String,
    ) : SaveBodyObservationResult

    data object Invalid :
        SaveBodyObservationResult

    data object DatabaseError :
        SaveBodyObservationResult
}

sealed interface SaveSessionResult {
    data class Saved(
        val sessionId: String,
    ) : SaveSessionResult

    data object Invalid :
        SaveSessionResult

    data object DatabaseError :
        SaveSessionResult
}

sealed interface ActiveDraftLoadResult {
    data class Loaded(
        val draft: ActiveSessionDraft,
        val warning: String? = null,
    ) : ActiveDraftLoadResult

    data object None : ActiveDraftLoadResult

    data class Error(
        val message: String,
    ) : ActiveDraftLoadResult
}

sealed interface ActiveDraftMutationResult {
    data object Saved : ActiveDraftMutationResult

    data class Error(
        val message: String,
    ) : ActiveDraftMutationResult
}

sealed interface FinalizeActiveDraftResult {
    data class Saved(
        val sessionId: String,
    ) : FinalizeActiveDraftResult

    data class Invalid(
        val message: String,
    ) : FinalizeActiveDraftResult

    data class DatabaseError(
        val message: String,
    ) : FinalizeActiveDraftResult
}

data class ExerciseOccurrenceCursor(
    val startedAt: String,
    val sessionId: String,
    val entryId: String,
)

data class ExerciseSetContext(
    val position: Int,
    val reps: Int?,
    val durationSeconds: Int?,
    /** Null and 0.0 are intentionally distinct actual observations. */
    val weightKg: Double?,
)

data class ExerciseSetPage(
    val sets: List<ExerciseSetContext>,
    val nextPosition: Int?,
)

data class ExerciseOccurrenceContext(
    val sessionId: String,
    val entryId: String,
    val startedAt: String,
    val sessionType: SessionType,
    val equipmentId: String?,
    val equipmentDisplayName: String?,
    val loadSemantics: EquipmentLoadSemantics?,
    val recordingMode: RecordingMode,
    val trackingMode: TrackingMode,
    val dataFields: Int,
    val setPreview: ExerciseSetPage?,
    val continuousDurationSeconds: Int?,
    val speedKmh: Double?,
    val distanceKm: Double?,
)

data class ExerciseOccurrencePage(
    val occurrences: List<ExerciseOccurrenceContext>,
    val nextCursor: ExerciseOccurrenceCursor?,
)

data class ExplicitMaxContext(
    val sessionId: String,
    val entryId: String,
    val startedAt: String,
    val maxWeightKg: Double,
    val equipmentId: String?,
    val equipmentDisplayName: String?,
    val loadSemantics: EquipmentLoadSemantics?,
)

data class TrainingExerciseContext(
    val exercise: ExerciseProfile,
    /** Persisted user classification; never replaced by scientific projection. */
    val persistedDirectZoneIds: List<String>,
    val persistedZoneIdsWithAncestors: List<String>,
    val knowledge: ExerciseKnowledge?,
    val compatibleEquipment: List<EquipmentKnowledge>,
    val latestExplicitMax: ExplicitMaxContext?,
    val recentPerformance: ExerciseOccurrencePage,
)

sealed interface ManualPercentMaxResult {
    data class Available(
        val maxWeightKg: Double,
        val maxStartedAt: String,
        val targetWeightKg: Double,
    ) : ManualPercentMaxResult
    data class Unavailable(val message: String) : ManualPercentMaxResult
}

class TrainlogRepository(
    context: Context,
    databaseName: String =
        ANDROID_DATABASE_NAME,
) {
    private val applicationContext = context.applicationContext
    private val canonicalExerciseNames = ExerciseNameCatalog.load(applicationContext)
    private val bodyZones = BodyZoneCatalog.load(applicationContext)
    private val trainingKnowledge = TrainingKnowledgeCatalog.load(applicationContext, bodyZones)
    private val sessionGenerationPolicy =
        SessionGenerationPolicyLoader.load(applicationContext, trainingKnowledge, bodyZones)
    private val sessionGenerationEngine =
        SessionGenerationEngine(sessionGenerationPolicy, trainingKnowledge, bodyZones)
    private val database =
        TrainlogDatabaseHelper(
            applicationContext,
            databaseName,
        )

    fun close() {
        database.close()
    }

    /**
     * WHY: dashboard comparisons must remain conservative. CONTRACT: rows are
     * actual observable-history facts only; external working loads are grouped
     * by canonical exercise and equipment, assistance is deliberately omitted
     * from kg trends, and missing metrics create gaps rather than zeroes.
     * Merges repoint occurrences transactionally, so the stored exercise row
     * is already the canonical identity at this read boundary.
     */
    fun loadStatistics(period: StatisticsPeriod): StatisticsOverview =
        loadStatistics(period, checkNotNull(TrainlogTimestamp.parse(OffsetDateTime.now().toString())))

    /** Visible to repository tests so exact inclusive boundaries do not depend on wall time. */
    internal fun loadStatistics(period: StatisticsPeriod, now: TrainlogTimestampKey): StatisticsOverview {
        val db = database.readableDatabase
        val cutoff = period.days?.let { TrainlogTimestampKey(now.utcSecond - it * 86400L, now.fraction) }
        var hasInvalidData = false
        fun timestamp(value: String): TrainlogTimestampKey? =
            TrainlogTimestamp.parse(value).also { if (it == null) hasInvalidData = true }
        fun included(key: TrainlogTimestampKey): Boolean = key <= now && (cutoff == null || key >= cutoff)
        fun query(sql: String, block: (android.database.Cursor) -> Unit) =
            db.rawQuery(sql, null).use { cursor -> while (cursor.moveToNext()) block(cursor) }
        fun week(key: TrainlogTimestampKey): String {
            val date = LocalDate.of(key.localYear, key.localMonth, key.localDay)
            val fields = WeekFields.ISO
            return "%04d-W%02d".format(
                Locale.ROOT, date.get(fields.weekBasedYear()), date.get(fields.weekOfWeekBasedYear()),
            )
        }
        data class PointRow(
            val key: TrainlogTimestampKey,
            val stableId: String,
            val order: Int,
            val point: StatisticsPoint,
            val dose: String,
        )
        val performanceRows = linkedMapOf<String, MutableList<PointRow>>()
        val names = linkedMapOf<String, String>()
        query("""SELECT s.started_at,e.exercise_id,e.name,eq.equipment_id,eq.display_name,ps.weight_kg,se.entry_id,ps.position,ps.reps,ps.duration_seconds
                 FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id JOIN exercises e ON e.id=se.exercise_row_id
                 JOIN performed_sets ps ON ps.session_exercise_row_id=se.id JOIN equipment eq ON eq.id=se.equipment_row_id
                 WHERE se.load_mode='external' AND eq.load_semantics='external' AND eq.equipment_id <> '' AND ps.weight_kg IS NOT NULL
                   AND (ps.reps>0 OR ps.duration_seconds>0)""".trimIndent()) { c ->
            val time = timestamp(c.getString(0)) ?: return@query
            val value = c.getDouble(5)
            if (value.isFinite() && value >= 0.0) {
                val dose = if (!c.isNull(8)) "reps:${c.getInt(8)}" else "duration:${c.getInt(9)}"
                val key = "work:${c.getString(1)}:${c.getString(3)}:$dose"
                val exerciseName = canonicalExerciseNames[c.getString(1)] ?: c.getString(2)
                names[key] = "$exerciseName · ${c.getString(4)} · ${dose.substringAfter(':')} ${dose.substringBefore(':')} — charges réalisées"
                performanceRows.getOrPut(key) { mutableListOf() } +=
                    PointRow(time, c.getString(6), c.getInt(7), StatisticsPoint(c.getString(0), value), dose)
            }
        }
        query("""SELECT s.started_at,e.exercise_id,e.name,eq.equipment_id,eq.display_name,mr.max_weight_kg,se.entry_id
                 FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id JOIN exercises e ON e.id=se.exercise_row_id
                 JOIN max_results mr ON mr.session_exercise_row_id=se.id JOIN equipment eq ON eq.id=se.equipment_row_id
                 WHERE se.load_mode!='assistance' AND eq.load_semantics='external' AND eq.equipment_id <> ''""".trimIndent()) { c ->
            val time = timestamp(c.getString(0)) ?: return@query
            val value = c.getDouble(5)
            if (value.isFinite() && value > 0.0) {
                val key = "max:${c.getString(1)}:${c.getString(3)}"
                val exerciseName = canonicalExerciseNames[c.getString(1)] ?: c.getString(2)
                names[key] = "$exerciseName · ${c.getString(4)} — MAX explicite"
                performanceRows.getOrPut(key) { mutableListOf() } +=
                    PointRow(time, c.getString(6), 0, StatisticsPoint(c.getString(0), value), "max")
            }
        }
        val performance = mutableListOf<StatisticsSeries>()
        data class EventCount(var working: Int = 0, var maxima: Int = 0)
        val eventCounts = sortedMapOf<String, EventCount>()
        performanceRows.forEach { (context, unsorted) ->
            val rows = unsorted.sortedWith(
                compareBy<PointRow> { it.key }.thenBy { it.stableId }.thenBy { it.order },
            )
            val visible = rows.filter { included(it.key) }
            if (visible.isNotEmpty()) performance += StatisticsSeries(
                context, names.getValue(context), "kg", visible.map { it.point },
            )
            /* CONTRACT: an improvement is a strictly later record above the
             * prior best in one exact context. Stable IDs only order display;
             * equal canonical instants never establish or create an event. */
            rows.groupBy { it.dose }.values.forEach { comparable ->
                comparable.forEachIndexed { index, row ->
                    val priorBest = comparable.asSequence().take(index)
                        .filter { it.key < row.key }.maxOfOrNull { it.point.value }
                    if (priorBest != null && row.point.value > priorBest && included(row.key)) {
                        eventCounts.getOrPut(week(row.key)) { EventCount() }.also {
                            if (context.startsWith("max:")) it.maxima++ else it.working++
                        }
                    }
                }
            }
        }
        val body = mutableListOf<StatisticsSeries>()
        val bodyMetrics = listOf("body_weight_kg" to "Poids" to "kg", "neck_cm" to "Cou" to "cm", "shoulders_cm" to "Épaules" to "cm", "chest_cm" to "Poitrine" to "cm", "waist_cm" to "Tour de taille" to "cm", "hips_cm" to "Hanches" to "cm", "left_arm_cm" to "Bras gauche" to "cm", "right_arm_cm" to "Bras droit" to "cm", "left_forearm_cm" to "Avant-bras gauche" to "cm", "right_forearm_cm" to "Avant-bras droit" to "cm", "left_thigh_cm" to "Cuisse gauche" to "cm", "right_thigh_cm" to "Cuisse droite" to "cm", "left_calf_cm" to "Mollet gauche" to "cm", "right_calf_cm" to "Mollet droit" to "cm")
        bodyMetrics.forEach { triple ->
            val column = triple.first.first; val label = triple.first.second; val unit = triple.second
            val points = mutableListOf<Triple<TrainlogTimestampKey, String, StatisticsPoint>>()
            db.rawQuery("SELECT observed_at,observation_id,$column FROM body_observations WHERE $column IS NOT NULL", null).use { c -> while (c.moveToNext()) {
                val time = timestamp(c.getString(0)) ?: continue
                if (included(time)) points += Triple(time, c.getString(1), StatisticsPoint(c.getString(0), c.getDouble(2)))
            } }
            points.sortWith(compareBy<Triple<TrainlogTimestampKey, String, StatisticsPoint>> { it.first }.thenBy { it.second })
            if (points.isNotEmpty()) body += StatisticsSeries(
                column, label, unit, points.takeLast(64).map { it.third },
            )
        }
        data class FrequencyCount(var all: Int = 0, var maxima: Int = 0)
        val frequency = sortedMapOf<String, FrequencyCount>()
        val sessionTimes = mutableListOf<Pair<TrainlogTimestampKey, String>>()
        query("SELECT started_at,session_type FROM sessions AS s WHERE $ACTUAL_SESSION_PREDICATE") { c ->
            val time = timestamp(c.getString(0)) ?: return@query
            sessionTimes += time to c.getString(1)
            if (included(time)) {
                /* Calendar grouping follows the timestamp's represented local
                 * date. Rolling windows and ordering continue to use its exact
                 * canonical instant key above. */
                frequency.getOrPut(week(time)) { FrequencyCount() }.also { it.all++; if (c.getString(1) == "max_test") it.maxima++ }
            }
        }
        val freq = frequency.map { StatisticsFrequency(it.key, it.value.all, it.value.maxima) }
        fun count(days: Long): Int {
            val boundary = TrainlogTimestampKey(now.utcSecond - days * 86400L, now.fraction)
            return sessionTimes.count { it.first in boundary..now }
        }
        var performedSetCount = 0
        var explicitMaxCount = 0
        val actualExercises = mutableSetOf<String>()
        query("""SELECT s.started_at,e.exercise_id,
                        (SELECT COUNT(*) FROM performed_sets ps WHERE ps.session_exercise_row_id=se.id),
                        (SELECT COUNT(*) FROM max_results mr WHERE mr.session_exercise_row_id=se.id),
                        EXISTS(SELECT 1 FROM continuous_activity ca WHERE ca.session_exercise_row_id=se.id)
                 FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id
                 JOIN exercises e ON e.id=se.exercise_row_id
                 WHERE EXISTS(SELECT 1 FROM performed_sets ps WHERE ps.session_exercise_row_id=se.id)
                    OR EXISTS(SELECT 1 FROM max_results mr WHERE mr.session_exercise_row_id=se.id)
                    OR EXISTS(SELECT 1 FROM continuous_activity ca WHERE ca.session_exercise_row_id=se.id)""".trimIndent()) { c ->
            val time = timestamp(c.getString(0)) ?: return@query
            if (included(time)) {
                actualExercises += c.getString(1)
                performedSetCount += c.getInt(2)
                explicitMaxCount += c.getInt(3)
            }
        }
        val selectedSessions = sessionTimes.count { included(it.first) }
        return StatisticsOverview(
            period = period,
            hasInvalidData = hasInvalidData,
            summary = StatisticsSummary(selectedSessions, performedSetCount, actualExercises.size, explicitMaxCount),
            performanceEvents = eventCounts.map { StatisticsPerformanceWeek(it.key, it.value.working, it.value.maxima) },
            performance = performance,
            body = body.sortedWith(compareByDescending<StatisticsSeries> {
                checkNotNull(TrainlogTimestamp.parse(it.points.last().timestamp))
            }.thenBy { if (it.id == "body_weight_kg") 0 else 1 }.thenBy { it.id }).take(6),
            frequency = freq,
            sessionsLast7Days = count(7),
            sessionsLast30Days = count(30),
        )
    }

    fun getBodyZoneHomeOverview(): BodyZoneHomeOverview = getBodyZoneHomeOverview(
        checkNotNull(TrainlogTimestamp.parse(OffsetDateTime.now().toString())),
    )

    /**
     * WHY: Home guidance must describe recorded exposure, never infer recovery,
     * fatigue, readiness, or a session prescription.
     * CONTRACT: only completed occurrence-owned performed sets, continuous
     * activity, and explicit MAX results qualify. Windows are inclusive rolling
     * instants parsed by [TrainlogTimestamp]; SQL date and lexical comparisons
     * are deliberately forbidden. Parent BODY ZONES and draft/target data never
     * enter this read model.
     */
    internal fun getBodyZoneHomeOverview(now: TrainlogTimestampKey): BodyZoneHomeOverview {
        val db = database.readableDatabase
        val focusZoneIds = setOf("chest", "back", "shoulders", "arms", "core", "glutes", "thighs", "calves")
        val childZones = bodyZones.zones.filter { it.zoneId in focusZoneIds }
        check(childZones.size == focusZoneIds.size && childZones.none { it.kind == BodyZoneKind.GROUP }) {
            "BODY_FOCUS_HOME_V1 doit référencer exactement huit zones anatomiques enfant"
        }
        val childIds = childZones.mapTo(mutableSetOf()) { it.zoneId }
        val canonicalByAlias = mutableMapOf<String, String>()
        db.rawQuery(
            "SELECT source_exercise_id,canonical_exercise_id FROM exercise_aliases", null,
        ).use { cursor ->
            while (cursor.moveToNext()) canonicalByAlias[cursor.getString(0)] = cursor.getString(1)
        }
        fun canonical(id: String): String {
            var current = id
            val seen = mutableSetOf<String>()
            while (seen.add(current)) current = canonicalByAlias[current] ?: return current
            return current
        }

        val relations = mutableMapOf<String, Pair<String?, List<String>>>()
        db.rawQuery(
            """SELECT e.exercise_id,ebz.zone_id,ebz.role
               FROM exercises e JOIN exercise_body_zones ebz ON ebz.exercise_row_id=e.id
               ORDER BY e.exercise_id,ebz.role,ebz.zone_id""".trimIndent(), null,
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val id = canonical(cursor.getString(0))
                val zone = cursor.getString(1).takeIf { it in childIds } ?: continue
                val old = relations[id] ?: (null to emptyList())
                relations[id] = if (cursor.getString(2) == "primary") zone to old.second
                else old.first to (old.second + zone).distinct()
            }
        }

        /* There is no soft-active flag in schema v12: a present exercise row
         * which is not a retired alias source is the active canonical row. A
         * direct child-zone relation is the minimum resolved classification
         * needed by the existing Catalogue filter. */
        val availability = childIds.associateWith { mutableSetOf<String>() }
        db.rawQuery("SELECT exercise_id FROM exercises ORDER BY exercise_id", null).use { cursor ->
            while (cursor.moveToNext()) {
                val stored = cursor.getString(0)
                val id = canonical(stored)
                if (stored != id) continue
                val zones = relations[id] ?: continue
                zones.first?.let { availability.getValue(it).add(id) }
                zones.second.forEach { availability.getValue(it).add(id) }
            }
        }

        data class MutableExposure(
            var lastPrimary: Pair<TrainlogTimestampKey, String>? = null,
            var lastSecondary: Pair<TrainlogTimestampKey, String>? = null,
            var primary7: Int = 0,
            var secondary7: Int = 0,
            var primary30: Int = 0,
            var secondary30: Int = 0,
            val sessions30: MutableSet<String> = mutableSetOf(),
        )
        val exposure = childIds.associateWith { MutableExposure() }
        val cutoff7 = TrainlogTimestampKey(now.utcSecond - 7L * 86400L, now.fraction)
        val cutoff30 = TrainlogTimestampKey(now.utcSecond - 30L * 86400L, now.fraction)
        var hasActualHistory = false
        var invalidTimestamp = false
        db.rawQuery(
            """SELECT s.session_id,s.started_at,e.exercise_id,
                      (SELECT COUNT(*) FROM performed_sets ps WHERE ps.session_exercise_row_id=se.id),
                      EXISTS(SELECT 1 FROM continuous_activity ca WHERE ca.session_exercise_row_id=se.id),
                      EXISTS(SELECT 1 FROM max_results mr WHERE mr.session_exercise_row_id=se.id)
               FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id
               JOIN exercises e ON e.id=se.exercise_row_id
               WHERE EXISTS(SELECT 1 FROM performed_sets ps WHERE ps.session_exercise_row_id=se.id)
                  OR EXISTS(SELECT 1 FROM continuous_activity ca WHERE ca.session_exercise_row_id=se.id)
                  OR EXISTS(SELECT 1 FROM max_results mr WHERE mr.session_exercise_row_id=se.id)
               ORDER BY s.session_id,se.entry_id""".trimIndent(), null,
        ).use { cursor ->
            while (cursor.moveToNext()) {
                hasActualHistory = true
                val timestampText = cursor.getString(1)
                val time = TrainlogTimestamp.parse(timestampText)
                if (time == null) { invalidTimestamp = true; continue }
                if (time > now) continue
                val zones = relations[canonical(cursor.getString(2))] ?: continue
                val work = cursor.getInt(3) + cursor.getInt(4) + cursor.getInt(5)
                fun add(zoneId: String, primary: Boolean) {
                    val item = exposure.getValue(zoneId)
                    if (primary) {
                        if (item.lastPrimary == null || time > item.lastPrimary!!.first) item.lastPrimary = time to timestampText
                        if (time >= cutoff7) item.primary7 += work
                        if (time >= cutoff30) item.primary30 += work
                    } else {
                        if (item.lastSecondary == null || time > item.lastSecondary!!.first) item.lastSecondary = time to timestampText
                        if (time >= cutoff7) item.secondary7 += work
                        if (time >= cutoff30) item.secondary30 += work
                    }
                    if (time >= cutoff30) item.sessions30 += cursor.getString(0)
                }
                zones.first?.let { add(it, true) }
                zones.second.forEach { add(it, false) }
            }
        }

        val supported = childZones.filter { availability.getValue(it.zoneId).isNotEmpty() }
        /* FROZEN V1 rank: absent primary first; then longest elapsed primary
         * exposure; then lower primary work at 7d and 30d; then weaker
         * secondary modifiers (7d, 30d, longest elapsed); finally stable ID. */
        val ranked = supported.sortedWith { left, right ->
            val a = exposure.getValue(left.zoneId)
            val b = exposure.getValue(right.zoneId)
            compareValues(a.lastPrimary != null, b.lastPrimary != null).takeIf { it != 0 }
                ?: compareValues(a.lastPrimary?.first, b.lastPrimary?.first).takeIf { it != 0 }
                ?: compareValues(a.primary7, b.primary7).takeIf { it != 0 }
                ?: compareValues(a.primary30, b.primary30).takeIf { it != 0 }
                ?: compareValues(a.secondary7, b.secondary7).takeIf { it != 0 }
                ?: compareValues(a.secondary30, b.secondary30).takeIf { it != 0 }
                ?: compareValues(a.lastSecondary?.first, b.lastSecondary?.first).takeIf { it != 0 }
                ?: left.zoneId.compareTo(right.zoneId)
        }.take(3).map { it.zoneId }.toSet()

        val statuses = childZones.map { zone ->
            val item = exposure.getValue(zone.zoneId)
            val available = availability.getValue(zone.zoneId).size
            val state = when {
                available == 0 -> BodyZoneHomeState.UNSUPPORTED
                invalidTimestamp -> BodyZoneHomeState.INSUFFICIENT_DATA
                zone.zoneId in ranked -> BodyZoneHomeState.PRIORITIZE
                item.primary7 >= 4 -> BodyZoneHomeState.HIGH_RECENT_EXPOSURE
                item.primary7 > 0 -> BodyZoneHomeState.RECENT_WORK
                else -> BodyZoneHomeState.LITTLE_RECENT_WORK
            }
            val reasons = when (state) {
                BodyZoneHomeState.UNSUPPORTED -> listOf(BodyZoneHomeState.UNSUPPORTED.label)
                BodyZoneHomeState.INSUFFICIENT_DATA -> listOf("Au moins un horodatage historique est invalide.")
                BodyZoneHomeState.PRIORITIZE -> listOf(
                    if (item.lastPrimary == null) "Aucune exposition primaire enregistrée."
                    else "Exposition primaire moins récente ou moins représentée.",
                    "Exposition secondaire comptée comme modificateur plus faible.",
                )
                BodyZoneHomeState.LITTLE_RECENT_WORK -> listOf("Aucun travail primaire enregistré sur les 7 derniers jours.")
                BodyZoneHomeState.RECENT_WORK -> listOf("Travail primaire enregistré sur les 7 derniers jours.")
                BodyZoneHomeState.HIGH_RECENT_EXPOSURE -> listOf("Plusieurs faits de travail primaire récents sont enregistrés.")
            }
            BodyZoneHomeStatus(
                zone.zoneId, zone.displayName,
                item.lastPrimary?.second, item.lastSecondary?.second,
                item.lastPrimary?.first?.let { now.utcSecond - it.utcSecond },
                item.lastSecondary?.first?.let { now.utcSecond - it.utcSecond },
                item.primary7, item.secondary7, item.primary30, item.secondary30,
                item.sessions30.size, available, state, reasons,
            )
        }
        val byId = statuses.associateBy { it.zoneId }
        return BodyZoneHomeOverview(
            zones = statuses,
            recommendations = ranked.mapNotNull(byId::get),
            hasTrainingHistory = hasActualHistory,
            hasInvalidHistoryTimestamp = invalidTimestamp,
        )
    }

    fun getExerciseKnowledge(exerciseId: String): ExerciseKnowledge? =
        trainingKnowledge.getExerciseKnowledge(exerciseId)

    fun getConditionalExerciseKnowledge(exerciseId: String): ExerciseKnowledge? =
        trainingKnowledge.getConditionalExerciseKnowledge(exerciseId)

    fun getMuscleKnowledge(muscleId: String): MuscleKnowledge? = trainingKnowledge.getMuscle(muscleId)
    fun getJointActionKnowledge(actionId: String): JointActionKnowledge? = trainingKnowledge.getJointAction(actionId)
    fun getMovementPatternKnowledge(patternId: String): MovementPatternKnowledge? = trainingKnowledge.getMovementPattern(patternId)
    fun getScienceReference(refId: String): ScienceReference? = trainingKnowledge.getReference(refId)
    fun getEquipmentKnowledge(equipmentId: String): EquipmentKnowledge? = trainingKnowledge.getEquipmentKnowledge(equipmentId)
    fun listEquipmentExerciseOptions(equipmentId: String): List<Pair<EquipmentExerciseRelation, ExerciseKnowledge>> =
        trainingKnowledge.listRelationsForEquipment(equipmentId).mapNotNull { relation ->
            trainingKnowledge.getExerciseKnowledge(relation.exerciseId)?.let { relation to it }
        }
    fun listEquipmentForKnownExercise(exerciseId: String): List<EquipmentKnowledge> =
        resolveExerciseId(database.readableDatabase, exerciseId).let { canonicalExerciseId ->
            // INVARIANT: retired creator IDs canonicalize exactly once before
            // immutable equipment relations are queried; aliases are not extra options.
            trainingKnowledge.getExerciseKnowledge(canonicalExerciseId)?.let {
                trainingKnowledge.listEquipmentForExercise(canonicalExerciseId)
            }.orEmpty()
        }
    fun getScientificBodyZoneMapping(exerciseId: String): ScientificBodyZoneMapping? = trainingKnowledge.getScientificBodyZoneMapping(exerciseId)
    fun listExercisesByMovementPattern(patternId: String): List<ExerciseKnowledge> = trainingKnowledge.listExercisesByMovementPattern(patternId)
    fun listExercisesByMuscle(muscleId: String, role: MuscleRole): List<ExerciseKnowledge> = trainingKnowledge.listExercisesByMuscle(muscleId, role)
    fun listCompatibleKnowledgeExercises(equipmentId: String): List<ExerciseKnowledge> = trainingKnowledge.listCompatibleExercises(equipmentId)
    fun queryExerciseKnowledge(filters: KnowledgeExerciseFilters): List<ExerciseKnowledge> = trainingKnowledge.queryExercises(filters)

    fun listEquipment(): List<EquipmentCatalogEntry> {
        val output = mutableListOf<EquipmentCatalogEntry>()
        database.readableDatabase.query(
            "equipment",
            arrayOf("equipment_id", "label_name", "display_name", "equipment_type", "load_semantics"),
            null, null, null, null, "display_name COLLATE NOCASE, equipment_id",
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val id = cursor.getString(0)
                val aliases = mutableListOf<String>()
                database.readableDatabase.rawQuery(
                    "SELECT alias FROM equipment_aliases ea JOIN equipment e ON e.id = ea.equipment_row_id WHERE e.equipment_id = ? ORDER BY alias COLLATE NOCASE;",
                    arrayOf(id),
                ).use { aliasCursor -> while (aliasCursor.moveToNext()) aliases += aliasCursor.getString(0) }
                output += EquipmentCatalogEntry(
                    equipmentId = id,
                    labelName = cursor.getString(1),
                    displayName = cursor.getString(2),
                    aliases = aliases,
                    type = cursor.getString(3),
                    loadSemantics = EquipmentLoadSemantics.valueOf(cursor.getString(4).uppercase(Locale.ROOT)),
                )
            }
        }
        return output
    }

    fun searchEquipment(query: String): List<EquipmentCatalogEntry> =
        EquipmentCatalog.search(listEquipment(), query)

    /**
     * CONTRACT: resolved machine exercises carry their Phase-1 legacy
     * equipment context themselves. The UI may write this compatibility ID
     * without asking the user for a second identity; NULL remains explicit for
     * custom and unresolved exercises and is never inferred from a name.
     */
    fun machineExerciseLegacyEquipmentId(exerciseId: String): String? =
        database.readableDatabase.rawQuery(
            "SELECT legacy_equipment_id FROM exercises WHERE exercise_id=?;",
            arrayOf(resolveExerciseId(database.readableDatabase, exerciseId)),
        ).use { cursor ->
            if (!cursor.moveToFirst() || cursor.isNull(0)) null else cursor.getString(0)
        }

    /** Custom equipment is local user data, not an edit to the bundled manifest. */
    fun createCustomEquipment(name: String): CreateEquipmentResult {
        val displayName = name.trim().replace(Regex("\\s+"), " ")
        if (displayName.isBlank() || displayName.length > 120) return CreateEquipmentResult.Invalid
        return try {
            val db = database.writableDatabase
            val duplicate = db.rawQuery(
                "SELECT 1 FROM equipment WHERE lower(display_name) = lower(?) LIMIT 1;", arrayOf(displayName),
            ).use { it.moveToFirst() }
            if (duplicate) return CreateEquipmentResult.Conflict
            val entry = EquipmentCatalogEntry(
                equipmentId = "eq_" + UUID.randomUUID(), labelName = "", displayName = displayName,
                aliases = emptyList(), type = "custom_machine", loadSemantics = EquipmentLoadSemantics.EXTERNAL,
            )
            val values = ContentValues().apply {
                put("equipment_id", entry.equipmentId); put("label_name", entry.labelName)
                put("display_name", entry.displayName); put("equipment_type", entry.type)
                put("load_semantics", "external")
            }
            db.insertOrThrow("equipment", null, values)
            CreateEquipmentResult.Created(entry)
        } catch (error: SQLiteConstraintException) { CreateEquipmentResult.Conflict
        } catch (error: Exception) { CreateEquipmentResult.DatabaseError(error.message ?: "erreur SQLite") }
    }

    /** Return manifest sort order; definitions are immutable application assets. */
    fun listBodyZones(): List<BodyZone> = bodyZones.zones

    fun sessionGenerationFormOptions(): SessionGenerationFormOptions = SessionGenerationFormOptions(
        zoneIds = sessionGenerationPolicy.zoneExpansion.keys.toList(),
        goalIds = sessionGenerationPolicy.goals.keys.toList(),
        durationPresets = sessionGenerationPolicy.durationPresets,
        customMinutes = sessionGenerationPolicy.customMinutes,
    )

    /** Stable-ID lookup; null means the ID is not part of taxonomy V1. */
    fun bodyZone(zoneId: String): BodyZone? = bodyZones.lookup(zoneId)

    /** Nearest parent first; the manifest loader has already rejected cycles. */
    fun bodyZoneAncestors(zoneId: String): List<BodyZone> = bodyZones.ancestors(zoneId)

    /**
     * SQL-backed normalized-prefix and body-zone query.
     *
     * CONTRACT: a parent includes manifest descendants only when requested;
     * `primaryOnly` excludes secondary participation and `unclassifiedOnly`
     * selects exercises with no direct relation. An unknown zone ID is a
     * programmer error. Returned profiles own deterministic relation lists.
     */
    fun listExercises(
        query: String = "",
        zoneId: String? = null,
        includeDescendants: Boolean = true,
        primaryOnly: Boolean = false,
        unclassifiedOnly: Boolean = false,
    ): List<ExerciseProfile> {
        require(!(unclassifiedOnly && zoneId != null)) {
            "zoneId et unclassifiedOnly sont mutuellement exclusifs"
        }
        val output =
            mutableListOf<ExerciseProfile>()
        val normalizedPrefix = normalizeName(query)
        val selection = mutableListOf<String>()
        val arguments = mutableListOf<String>()
        if (normalizedPrefix.isNotEmpty()) {
            selection += "e.normalized_name LIKE ? ESCAPE '\\'"
            arguments += normalizedPrefix
                .replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%"
        }
        if (unclassifiedOnly) {
            selection += "NOT EXISTS(SELECT 1 FROM exercise_body_zones missing WHERE missing.exercise_row_id=e.id)"
        } else if (zoneId != null) {
            val accepted = if (includeDescendants) bodyZones.descendantsAndSelf(zoneId) else setOf(zoneId)
            check(bodyZones.lookup(zoneId) != null) { "zone_id inconnu: $zoneId" }
            val placeholders = accepted.joinToString(",") { "?" }
            selection += "EXISTS(SELECT 1 FROM exercise_body_zones selected WHERE " +
                "selected.exercise_row_id=e.id AND selected.zone_id IN($placeholders)" +
                (if (primaryOnly) " AND selected.role='primary'" else "") + ")"
            arguments += accepted
        }
        val sql = "SELECT e.exercise_id,e.name,e.normalized_name,e.recording_mode," +
            "e.tracking_mode,e.data_fields FROM exercises e" +
            (if (selection.isEmpty()) "" else " WHERE " + selection.joinToString(" AND ")) +
            " ORDER BY e.name COLLATE NOCASE,e.exercise_id;"
        val db = database.readableDatabase
        db.rawQuery(sql, arguments.toTypedArray()).use { cursor ->
            val idIndex =
                cursor.getColumnIndexOrThrow(
                    "exercise_id"
                )

            val nameIndex =
                cursor.getColumnIndexOrThrow(
                    "name"
                )

            val normalizedIndex =
                cursor.getColumnIndexOrThrow(
                    "normalized_name"
                )

            val recordingIndex =
                cursor.getColumnIndexOrThrow(
                    "recording_mode"
                )

            val trackingIndex =
                cursor.getColumnIndexOrThrow(
                    "tracking_mode"
                )

            val fieldsIndex =
                cursor.getColumnIndexOrThrow(
                    "data_fields"
                )

            while (cursor.moveToNext()) {
                val exerciseId = cursor.getString(idIndex)
                val selectionForExercise = readExerciseBodyZones(db, exerciseId)
                output +=
                    ExerciseProfile(
                        exerciseId =
                            exerciseId,
                        name =
                            cursor.getString(
                                nameIndex
                            ),
                        normalizedName =
                            cursor.getString(
                                normalizedIndex
                            ),
                        recordingMode =
                            when (
                                cursor.getString(
                                    recordingIndex
                                )
                            ) {
                                "continuous" ->
                                    RecordingMode.CONTINUOUS

                                else ->
                                    RecordingMode.SETS
                            },
                        trackingMode =
                            when (
                                cursor.getString(
                                    trackingIndex
                                )
                            ) {
                                "duration" ->
                                    TrackingMode.DURATION

                                else ->
                                    TrackingMode.REPS
                            },
                        dataFields =
                            cursor.getInt(
                                fieldsIndex
                            ),
                        primaryZoneId = selectionForExercise.first,
                        secondaryZoneIds = selectionForExercise.second,
                    )
            }
        }

        return output
    }

    fun createExercise(
        input: NewExerciseProfile,
    ): CreateExerciseResult {
        if (!input.validate() || !bodyZoneSelectionValid(
                input.primaryZoneId, input.secondaryZoneIds)) {
            return CreateExerciseResult.Invalid
        }

        val name =
            input.name.trim()

        val normalized =
            normalizeName(name)

        if (normalized.isEmpty()) {
            return CreateExerciseResult.Invalid
        }

        val exercise =
            ExerciseProfile(
                exerciseId =
                    "ex_" +
                        UUID.randomUUID()
                            .toString(),
                name = name,
                normalizedName =
                    normalized,
                recordingMode =
                    input.recordingMode,
                trackingMode =
                    input.trackingMode,
                dataFields =
                    input.dataFields,
                primaryZoneId = input.primaryZoneId,
                secondaryZoneIds = input.secondaryZoneIds.sortedBy { id ->
                    bodyZones.lookup(id)?.sortOrder ?: Int.MAX_VALUE
                },
            )

        val sql =
            """
            INSERT INTO exercises(
                exercise_id,
                name,
                normalized_name,
                recording_mode,
                tracking_mode,
                data_fields
            ) VALUES(?, ?, ?, ?, ?, ?);
            """.trimIndent()

        val db = database.writableDatabase
        return try {
            db.beginTransaction()
            db.execSQL(
                sql,
                arrayOf<Any?>(
                    exercise.exerciseId,
                    exercise.name,
                    exercise.normalizedName,
                    exercise.recordingMode
                        .wireValue,
                    exercise.trackingMode
                        .wireValue,
                    exercise.dataFields,
                ),
            )

            replaceExerciseBodyZones(db, exercise.exerciseId,
                exercise.primaryZoneId, exercise.secondaryZoneIds)
            db.setTransactionSuccessful()
            CreateExerciseResult.Created(
                exercise
            )
        } catch (
            error: SQLiteConstraintException
        ) {
            CreateExerciseResult.Conflict
        } catch (error: Exception) {
            CreateExerciseResult.DatabaseError(
                error.message ?: "Enregistrement de l'exercice impossible.",
            )
        } finally {
            if (db.inTransaction()) db.endTransaction()
        }
    }

    /**
     * WHY: completed sessions and active drafts snapshot profile fields, but
     * keeping referenced catalog profiles immutable prevents a later catalog
     * sync/profile edit from appearing to change an established exercise.
     * A rename remains safe because all relationships use the unchanged row.
     */
    fun canEditExerciseProfile(
        exerciseId: String,
    ): Boolean {
        val db = database.readableDatabase
        val rowId = lookupExerciseRowIdOrNull(db, exerciseId) ?: return false
        return !exerciseHasReferences(db, rowId)
    }

    fun editExercise(
        input: ExerciseEditInput,
    ): EditExerciseResult {
        if (!input.validateProfile() || !bodyZoneSelectionValid(
                input.primaryZoneId, input.secondaryZoneIds)) {
            return EditExerciseResult.InvalidNameOrProfile
        }

        val name = input.name.trim()
        val normalized = normalizeName(name)
        if (normalized.isEmpty()) {
            return EditExerciseResult.InvalidNameOrProfile
        }

        val db = database.writableDatabase
        return try {
            db.beginTransaction()
            val current = findExerciseRow(db, "exercise_id = ?", arrayOf(input.exerciseId))
                ?: return EditExerciseResult.DatabaseError
            val profileChanged =
                current.recordingMode != input.recordingMode ||
                    current.trackingMode != input.trackingMode ||
                    current.dataFields != input.dataFields
            if (profileChanged && exerciseHasReferences(db, current.rowId)) {
                return EditExerciseResult.IncompatibleProfileChange
            }

            val nameOwner = findExerciseRow(db, "normalized_name = ?", arrayOf(normalized))
            if (nameOwner != null && nameOwner.rowId != current.rowId) {
                return EditExerciseResult.Conflict
            }

            val values = ContentValues().apply {
                put("name", name)
                put("normalized_name", normalized)
                put("recording_mode", input.recordingMode.wireValue)
                put("tracking_mode", input.trackingMode.wireValue)
                put("data_fields", input.dataFields)
            }
            if (db.update("exercises", values, "id = ?", arrayOf(current.rowId.toString())) != 1) {
                return EditExerciseResult.DatabaseError
            }
            replaceExerciseBodyZones(db, input.exerciseId,
                input.primaryZoneId, input.secondaryZoneIds)
            db.setTransactionSuccessful()
            EditExerciseResult.Saved(
                ExerciseProfile(
                    exerciseId = input.exerciseId,
                    name = name,
                    normalizedName = normalized,
                    recordingMode = input.recordingMode,
                    trackingMode = input.trackingMode,
                    dataFields = input.dataFields,
                    primaryZoneId = input.primaryZoneId,
                    secondaryZoneIds = input.secondaryZoneIds.sortedBy { id ->
                        bodyZones.lookup(id)?.sortOrder ?: Int.MAX_VALUE
                    },
                ),
            )
        } catch (error: SQLiteConstraintException) {
            EditExerciseResult.Conflict
        } catch (error: Exception) {
            EditExerciseResult.DatabaseError
        } finally {
            if (db.inTransaction()) {
                db.endTransaction()
            }
        }
    }

    /**
     * CONTRACT: this is the sole body-zone exchange source in both directions.
     * Session V2 and frozen TRAINLOG_FORMAT_V1 retain their exact shapes.
     */
    fun buildExerciseBodyZonesJson(): String {
        val exercises = JSONArray()
        listExercises().sortedBy { it.exerciseId }.forEach { exercise ->
            exercises.put(JSONObject()
                .put("exercise_id", exercise.exerciseId)
                .put("primary_zone_id", exercise.primaryZoneId ?: JSONObject.NULL)
                .put("secondary_zone_ids", JSONArray(exercise.secondaryZoneIds.sorted())))
        }
        return JSONObject()
            .put("format", "trainlog-exercise-body-zones")
            .put("version", 1)
            .put("generated_at", OffsetDateTime.now().toString())
            .put("exercises", exercises)
            .toString()
    }

    fun applyExerciseBodyZonesJson(json: String): ExerciseBodyZoneImportResult {
        val root = try { JSONObject(json) } catch (_: Exception) {
            return ExerciseBodyZoneImportResult.Invalid("Relations de zones JSON invalides.")
        }
        val rootKeys = setOf("format", "version", "generated_at", "exercises")
        if (!root.hasExactKeys(rootKeys) ||
            root.value("format") != "trainlog-exercise-body-zones" ||
            !root.value("version").isJsonInt(1, 1) ||
            !root.value("generated_at").isNonemptyJsonString() ||
            root.value("exercises") !is JSONArray) {
            return ExerciseBodyZoneImportResult.Invalid("Relations de zones v1 non supportées.")
        }
        try {
            OffsetDateTime.parse(root.getString("generated_at"))
        } catch (_: Exception) {
            return ExerciseBodyZoneImportResult.Invalid("Horodatage des zones invalide.")
        }
        data class Incoming(val exerciseId: String, val primary: String?, val secondary: List<String>)
        val incoming = mutableListOf<Incoming>()
        val seenExercises = mutableSetOf<String>()
        try {
            val array = root.getJSONArray("exercises")
            for (index in 0 until array.length()) {
                val item = array.opt(index) as? JSONObject
                    ?: return ExerciseBodyZoneImportResult.Invalid("Relation de zone invalide.")
                if (!item.hasExactKeys(setOf("exercise_id", "primary_zone_id", "secondary_zone_ids")))
                    return ExerciseBodyZoneImportResult.Invalid("Forme de relation de zone invalide.")
                val exerciseId = item.value("exercise_id")
                val primaryValue = item.value("primary_zone_id")
                val secondaryValue = item.value("secondary_zone_ids")
                if (!exerciseId.isNonemptyJsonString() ||
                    !(exerciseId as String).matches(EXERCISE_ID_V4_PATTERN) ||
                    !seenExercises.add(exerciseId) ||
                    !(primaryValue === JSONObject.NULL || primaryValue.isNonemptyJsonString()) ||
                    secondaryValue !is JSONArray) {
                    return ExerciseBodyZoneImportResult.Invalid("Identité de relation de zone invalide.")
                }
                val secondary = List(secondaryValue.length()) { position ->
                    secondaryValue.opt(position) as? String
                        ?: return ExerciseBodyZoneImportResult.Invalid("zone_id secondaire invalide.")
                }
                val primary = if (primaryValue === JSONObject.NULL) null else primaryValue as String
                if (!bodyZoneSelectionValid(primary, secondary))
                    return ExerciseBodyZoneImportResult.Invalid("Sélection de zones invalide : $exerciseId")
                incoming += Incoming(exerciseId, primary, secondary.sorted())
            }
        } catch (_: Exception) {
            return ExerciseBodyZoneImportResult.Invalid("Relations de zones invalides.")
        }

        val db = database.writableDatabase
        var updated = 0
        var skipped = 0
        var keptLocal = 0
        return try {
            db.beginTransaction()
            incoming.forEach { item ->
                val canonicalExerciseId = resolveExerciseId(db, item.exerciseId)
                val rowId = lookupExerciseRowIdOrNull(db, canonicalExerciseId)
                    ?: return ExerciseBodyZoneImportResult.Invalid(
                        "Exercice de relation inconnu : ${item.exerciseId}",
                    )
                val local = readExerciseBodyZones(db, canonicalExerciseId)
                val localState = bodyZoneSyncState(local.first, local.second)
                val incomingState = bodyZoneSyncState(item.primary, item.secondary)
                val baseline = db.rawQuery(
                    "SELECT synced_state FROM exercise_body_zone_sync WHERE exercise_row_id=?;",
                    arrayOf(rowId.toString()),
                ).use { cursor -> if (cursor.moveToFirst()) cursor.getString(0) else null }
                when {
                    localState == incomingState -> {
                        writeBodyZoneSyncBaseline(db, rowId, incomingState)
                        skipped += 1
                    }
                    baseline != null && localState == baseline -> {
                        replaceExerciseBodyZones(db, canonicalExerciseId, item.primary, item.secondary)
                        writeBodyZoneSyncBaseline(db, rowId, incomingState)
                        updated += 1
                    }
                    baseline != null && incomingState == baseline -> keptLocal += 1
                    baseline == null && localState == "|" -> {
                        replaceExerciseBodyZones(db, canonicalExerciseId, item.primary, item.secondary)
                        writeBodyZoneSyncBaseline(db, rowId, incomingState)
                        updated += 1
                    }
                    else -> return ExerciseBodyZoneImportResult.Conflict(item.exerciseId)
                }
            }
            db.setTransactionSuccessful()
            ExerciseBodyZoneImportResult.Applied(updated, skipped, keptLocal)
        } catch (_: Exception) {
            ExerciseBodyZoneImportResult.DatabaseError
        } finally {
            if (db.inTransaction()) db.endTransaction()
        }
    }

    /**
     * CONTRACT: this companion is the only publication of retired exercise
     * identities. Mappings are sorted, flattened, one hop, and never alter a
     * mobile session exchange version.
     */
    fun buildExerciseAliasesJson(): String {
        val aliases = JSONArray()
        database.readableDatabase.rawQuery(
            "SELECT source_exercise_id,canonical_exercise_id FROM exercise_aliases " +
                "ORDER BY source_exercise_id COLLATE BINARY;",
            null,
        ).use { cursor ->
            while (cursor.moveToNext()) {
                aliases.put(JSONObject()
                    .put("source_exercise_id", cursor.getString(0))
                    .put("canonical_exercise_id", cursor.getString(1)))
            }
        }
        check(aliases.length() <= MAX_EXERCISE_ALIASES) { "Trop d'alias exercice." }
        return JSONObject()
            .put("format", "trainlog-exercise-aliases")
            .put("version", 1)
            .put("aliases", aliases)
            .toString()
    }

    fun applyExerciseAliasesJson(json: String): ExerciseAliasImportResult {
        if (json.toByteArray(Charsets.UTF_8).size > MAX_EXERCISE_ALIAS_BYTES ||
            !jsonHasUniqueObjectKeys(json)) {
            return ExerciseAliasImportResult.Invalid("Artifact alias JSON invalide ou trop volumineux.")
        }
        val root = try { JSONObject(json) } catch (_: Exception) {
            return ExerciseAliasImportResult.Invalid("Artifact alias JSON invalide.")
        }
        if (!root.hasExactKeys(setOf("format", "version", "aliases")) ||
            root.value("format") != "trainlog-exercise-aliases" ||
            !root.value("version").isJsonInt(1, 1) || root.value("aliases") !is JSONArray) {
            return ExerciseAliasImportResult.Invalid("Artifact alias v1 non supporté.")
        }
        val incoming = mutableListOf<Pair<String, String>>()
        val sources = mutableSetOf<String>()
        val array = root.getJSONArray("aliases")
        if (array.length() > MAX_EXERCISE_ALIASES)
            return ExerciseAliasImportResult.Invalid("Trop d'alias exercice.")
        for (index in 0 until array.length()) {
            val item = array.opt(index) as? JSONObject
                ?: return ExerciseAliasImportResult.Invalid("Alias[$index] invalide.")
            if (!item.hasExactKeys(setOf("source_exercise_id", "canonical_exercise_id")))
                return ExerciseAliasImportResult.Invalid("Forme d'alias[$index] invalide.")
            val source = item.optString("source_exercise_id")
            val canonical = item.optString("canonical_exercise_id")
            if (!source.matches(EXERCISE_ID_V4_PATTERN) ||
                !canonical.matches(EXERCISE_ID_V4_PATTERN) || source == canonical ||
                !sources.add(source)) {
                return ExerciseAliasImportResult.Invalid("Identité d'alias[$index] invalide.")
            }
            incoming += source to canonical
        }
        if (incoming != incoming.sortedWith(compareBy<Pair<String, String>> { it.first }.thenBy { it.second }) ||
            incoming.any { it.second in sources }) {
            return ExerciseAliasImportResult.Invalid("Les alias doivent être triés et aplatis.")
        }

        val db = database.writableDatabase
        var added = 0
        var skipped = 0
        return try {
            db.beginTransaction()
            incoming.forEach { (sourceId, canonicalId) ->
                val canonical = findExerciseRow(db, "exercise_id=?", arrayOf(canonicalId))
                    ?: return ExerciseAliasImportResult.Invalid(
                        "Cible canonique absente : $canonicalId",
                    )
                val existing = db.rawQuery(
                    "SELECT canonical_exercise_id FROM exercise_aliases WHERE source_exercise_id=?;",
                    arrayOf(sourceId),
                ).use { cursor -> if (cursor.moveToFirst()) cursor.getString(0) else null }
                if (existing != null) {
                    if (existing != canonicalId) return ExerciseAliasImportResult.Conflict(sourceId)
                    skipped += 1
                    return@forEach
                }
                val retired = findExerciseRow(db, "exercise_id=?", arrayOf(sourceId))
                if (retired != null) {
                    if (retired.recordingMode != canonical.recordingMode ||
                        retired.trackingMode != canonical.trackingMode ||
                        retired.dataFields != canonical.dataFields ||
                        !aliasBodyZonesAreCompatible(db, canonical, retired) ||
                        !catalogEquipmentProfilesAreCompatible(db, sourceId, canonicalId)) {
                        return ExerciseAliasImportResult.Conflict(sourceId)
                    }
                    /* INVARIANT: flatten incoming edges before deleting the
                     * intermediate catalogue row protected by the alias FK. */
                    db.execSQL(
                        "UPDATE exercise_aliases SET canonical_exercise_id=? WHERE canonical_exercise_id=?;",
                        arrayOf(canonicalId, sourceId),
                    )
                    mergeAliasedExerciseRows(db, canonical, retired)
                }
                /* INVARIANT: aliases which previously targeted the retired ID
                 * remain one hop from a live catalogue row. */
                db.execSQL(
                    "UPDATE exercise_aliases SET canonical_exercise_id=? WHERE canonical_exercise_id=?;",
                    arrayOf(canonicalId, sourceId),
                )
                db.execSQL(
                    "INSERT INTO exercise_aliases(source_exercise_id,canonical_exercise_id) VALUES(?,?);",
                    arrayOf(sourceId, canonicalId),
                )
                added += 1
            }
            db.setTransactionSuccessful()
            ExerciseAliasImportResult.Applied(added, skipped)
        } catch (_: Exception) {
            ExerciseAliasImportResult.DatabaseError
        } finally {
            if (db.inTransaction()) db.endTransaction()
        }
    }

    private fun bodyZoneSyncState(primary: String?, secondary: List<String>): String =
        (primary ?: "") + "|" + secondary.sorted().joinToString(",")

    private fun writeBodyZoneSyncBaseline(db: SQLiteDatabase, rowId: Long, state: String) {
        db.execSQL(
            "INSERT OR REPLACE INTO exercise_body_zone_sync(exercise_row_id,synced_state) VALUES(?,?);",
            arrayOf<Any>(rowId, state),
        )
    }

    fun loadActiveSessionDraft():
        ActiveDraftLoadResult =
        try {
            val restored =
                loadActiveSessionDraft(
                    database.readableDatabase
                )

            if (restored == null) {
                ActiveDraftLoadResult.None
            } else {
                ActiveDraftLoadResult.Loaded(
                    draft = restored.draft,
                    warning = restored.warning,
                )
            }
        } catch (error: Exception) {
            ActiveDraftLoadResult.Error(
                error.message
                    ?: "Lecture du brouillon impossible."
            )
        }

    fun startActiveSessionDraft():
        ActiveDraftMutationResult {
        return when (
            loadActiveSessionDraft()
        ) {
            is ActiveDraftLoadResult.Loaded ->
                ActiveDraftMutationResult.Saved

            is ActiveDraftLoadResult.Error ->
                ActiveDraftMutationResult.Error(
                    "Le brouillon existant ne peut pas être lu."
                )

            ActiveDraftLoadResult.None ->
                saveActiveSessionDraft(
                    ActiveSessionDraft()
                )
        }
    }

    fun saveActiveSessionDraft(
        draft: ActiveSessionDraft,
    ): ActiveDraftMutationResult {
        if (
            draft.exercises.any {
                !validateSessionExercise(it, draft.sessionType, allowTargetOnly = true)
            } ||
            (draft.sourceSessionId != null &&
                (draft.sourceSessionId.isBlank() || draft.sessionType != SessionType.MAX_TEST)) ||
            /* exercise_id identifies the catalogue movement. Repeated passages
             * are valid; only their stable occurrence IDs must be unique. */
            draft.exercises
                .map {
                    it.entryId
                }
                .any { it.isBlank() } ||
            draft.exercises
                .map {
                    it.entryId
                }
                .distinct()
                .size !=
            draft.exercises.size ||
            listOf(
                draft.form.setCountText,
                draft.form.repsText,
                draft.form.weightText,
                draft.form.maxWeightText,
                draft.form.durationText,
                draft.form.speedText,
                draft.form.distanceText,
            ).any {
                it.length > MAX_DRAFT_FORM_TEXT_LENGTH
            }
        ) {
            return ActiveDraftMutationResult.Error(
                "Brouillon de séance invalide."
            )
        }

        var db: SQLiteDatabase? = null
        var transactionOpen = false
        return try {
            db = database.writableDatabase
            db.beginTransaction()
            transactionOpen = true
            persistActiveSessionDraft(
                db,
                draft,
            )
            db.setTransactionSuccessful()
            db.endTransaction()
            transactionOpen = false
            ActiveDraftMutationResult.Saved
        } catch (error: Exception) {
            if (transactionOpen && db?.inTransaction() == true) {
                try {
                    db.endTransaction()
                } catch (endError: Exception) {
                    error.addSuppressed(endError)
                }
            }
            ActiveDraftMutationResult.Error(
                error.message
                    ?: "Enregistrement du brouillon impossible."
            )
        }
    }

    fun discardActiveSessionDraft():
        ActiveDraftMutationResult {
        return try {
            database.writableDatabase.delete(
                "active_session_draft",
                "id = ?",
                arrayOf(ACTIVE_DRAFT_ID.toString()),
            )
            ActiveDraftMutationResult.Saved
        } catch (error: Exception) {
            ActiveDraftMutationResult.Error(
                error.message
                    ?: "Suppression du brouillon impossible."
            )
        }
    }

    fun finalizeActiveSessionDraft():
        FinalizeActiveDraftResult {
        var db: SQLiteDatabase? = null
        var transactionOpen = false
        return try {
            db = database.writableDatabase
            db.beginTransaction()
            transactionOpen = true
            val active =
                loadActiveSessionDraft(db)
            if (active == null) {
                db.endTransaction()
                transactionOpen = false
                return FinalizeActiveDraftResult.Invalid(
                    "Aucune séance en cours."
                )
            }

            val completed =
                SessionDraft(
                    exercises =
                        active.draft.exercises,
                    sessionType =
                        active.draft.sessionType,
                )

            if (
                completed.exercises.isEmpty() ||
                completed.exercises.any {
                    !validateSessionExercise(it, completed.sessionType)
                }
            ) {
                db.endTransaction()
                transactionOpen = false
                return FinalizeActiveDraftResult.Invalid(
                    "La séance en cours est invalide."
                )
            }

            val sessionId =
                insertCompletedSession(
                    db,
                    completed,
                    active.draft.sourceSessionId,
                )

            /*
             * INVARIANT: completion and draft deletion share this transaction.
             * A crash or constraint failure therefore leaves the retryable draft
             * and never exposes a completed/draft duplicate pair.
             */
            val deleted =
                db.delete(
                    "active_session_draft",
                    "id = ?",
                    arrayOf(
                        ACTIVE_DRAFT_ID
                            .toString()
                    ),
                )

            check(deleted == 1) {
                "Le brouillon finalisé n'a pas été supprimé."
            }

            db.setTransactionSuccessful()
            db.endTransaction()
            transactionOpen = false
            FinalizeActiveDraftResult.Saved(
                sessionId
            )
        } catch (error: Exception) {
            if (transactionOpen && db?.inTransaction() == true) {
                try {
                    db.endTransaction()
                } catch (endError: Exception) {
                    error.addSuppressed(endError)
                }
            }
            FinalizeActiveDraftResult.DatabaseError(
                error.message
                    ?: "Finalisation de la séance impossible."
            )
        }
    }

    fun saveSession(
        draft: SessionDraft,
    ): SaveSessionResult {
        if (
            draft.exercises.isEmpty() ||
            draft.exercises.any {
                !validateSessionExercise(it, draft.sessionType)
            }
        ) {
            return SaveSessionResult.Invalid
        }

        val db =
            database.writableDatabase

        val sessionId =
            "se_" +
                UUID.randomUUID()
                    .toString()

        val startedAt =
            OffsetDateTime.now()
                .toString()

        db.beginTransaction()

        return try {
            val sessionValues =
                ContentValues().apply {
                    put(
                        "session_id",
                        sessionId
                    )

                    put(
                        "started_at",
                        startedAt
                    )

                    put(
                        "session_type",
                        draft.sessionType
                            .wireValue
                    )
                }

            val sessionRowId =
                db.insertOrThrow(
                    "sessions",
                    null,
                    sessionValues
                )

            draft.exercises.forEachIndexed {
                    exerciseIndex,
                    exerciseDraft ->

                val exerciseRowId =
                    lookupExerciseRowId(
                        db,
                        exerciseDraft.exercise.exerciseId
                    )

                val exerciseValues =
                    ContentValues().apply {
                        put(
                            "session_row_id",
                            sessionRowId
                        )

                        put(
                            "exercise_row_id",
                            exerciseRowId
                        )

                        put(
                            "position",
                            exerciseIndex
                        )

                        put(
                            "recording_mode",
                            exerciseDraft.exercise
                                .recordingMode
                                .wireValue
                        )

                        put(
                            "tracking_mode",
                            exerciseDraft.exercise
                                .trackingMode
                                .wireValue
                        )

                        put(
                            "data_fields",
                            exerciseDraft.exercise
                                .dataFields
                        )
                        put("entry_id", exerciseDraft.entryId)
                        putSessionPlan(exerciseDraft.plan)
                        val equipmentRowId = lookupEquipmentRowIdOrNull(
                            db,
                            exerciseDraft.equipmentId,
                        )
                        check(exerciseDraft.equipmentId == null || equipmentRowId != null) {
                            "Unknown equipment: ${exerciseDraft.equipmentId}"
                        }
                        if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                    }

                val sessionExerciseRowId =
                    db.insertOrThrow(
                        "session_exercises",
                        null,
                        exerciseValues
                    )

                if (exerciseDraft.maxWeightKg != null) {
                    db.insertOrThrow(
                        "max_results",
                        null,
                        ContentValues().apply {
                            put("session_exercise_row_id", sessionExerciseRowId)
                            put("max_weight_kg", exerciseDraft.maxWeightKg)
                        },
                    )
                } else if (
                    exerciseDraft.exercise
                        .recordingMode ==
                    RecordingMode.CONTINUOUS
                ) {
                    val continuousValues =
                        ContentValues().apply {
                            put(
                                "session_exercise_row_id",
                                sessionExerciseRowId
                            )

                            put(
                                "duration_seconds",
                                exerciseDraft
                                    .continuousDurationSeconds
                            )

                            exerciseDraft.speedKmh
                                ?.let {
                                    put(
                                        "speed_kmh",
                                        it
                                    )
                                }

                            exerciseDraft.distanceKm
                                ?.let {
                                    put(
                                        "distance_km",
                                        it
                                    )
                                }
                        }

                    db.insertOrThrow(
                        "continuous_activity",
                        null,
                        continuousValues
                    )
                } else {
                    exerciseDraft.sets
                        .forEachIndexed {
                                setIndex,
                                set ->

                            val setValues =
                                ContentValues().apply {
                                    put(
                                        "session_exercise_row_id",
                                        sessionExerciseRowId
                                    )

                                    put(
                                        "position",
                                        setIndex
                                    )
                                    set.weightKg?.let { put("weight_kg", it) }

                                    if (
                                        exerciseDraft.exercise
                                            .trackingMode ==
                                        TrackingMode.REPS
                                    ) {
                                        put(
                                            "reps",
                                            set.reps
                                        )
                                    } else {
                                        put(
                                            "duration_seconds",
                                            set.durationSeconds
                                        )
                                    }
                                }

                            db.insertOrThrow(
                                "performed_sets",
                                null,
                                setValues
                            )
                        }
                }
            }

            db.setTransactionSuccessful()

            SaveSessionResult.Saved(
                sessionId
            )
        } catch (
            error: Exception
        ) {
            SaveSessionResult.DatabaseError
        } finally {
            db.endTransaction()
        }
    }

    fun listSessions(): List<SessionSummary> {
        val output =
            mutableListOf<SessionSummary>()

        database.readableDatabase.rawQuery(
            """
            SELECT
                s.session_id,
                s.started_at,
                s.session_type,
                COUNT(se.id)
            FROM sessions AS s
            LEFT JOIN session_exercises AS se
                ON se.session_row_id = s.id
            WHERE $ACTUAL_SESSION_PREDICATE
            GROUP BY s.id
            ORDER BY s.started_at DESC, s.id DESC;
            """.trimIndent(),
            null,
        ).use { cursor ->
            while (
                cursor.moveToNext()
            ) {
                output +=
                    SessionSummary(
                        sessionId =
                            cursor.getString(0),
                        startedAt =
                            cursor.getString(1),
                        exerciseCount =
                            cursor.getInt(3),
                        sessionType =
                            SessionType.fromWire(
                                cursor.getString(2)
                            ),
                    )
            }
        }

        return output
    }

    /**
     * Reopen an existing max_test as the one durable draft. The completed row
     * remains the crash-safe baseline until finalization atomically replaces
     * its children; session_id and started_at are never regenerated.
     */
    fun resumeMaxTestSession(sessionId: String): ActiveDraftMutationResult {
        if (sessionId.isBlank()) {
            return ActiveDraftMutationResult.Error("Identité de séance invalide.")
        }
        when (val active = loadActiveSessionDraft()) {
            is ActiveDraftLoadResult.Loaded -> {
                if (active.draft.sourceSessionId == sessionId) {
                    return ActiveDraftMutationResult.Saved
                }
                /*
                 * WHY: the application persists a singleton default draft, so
                 * database-row presence alone does not mean user work exists.
                 * CONTRACT: only the byte-for-byte default empty form may be
                 * replaced implicitly; partial raw input remains protected.
                 */
                if (
                    active.draft.exercises.isNotEmpty() ||
                    active.draft.sourceSessionId != null ||
                    active.draft.form != SessionDraftForm()
                ) {
                    return ActiveDraftMutationResult.Error(
                        "Une autre séance est déjà en cours ; reprenez-la ou supprimez-la explicitement.",
                    )
                }
            }
            is ActiveDraftLoadResult.Error -> return ActiveDraftMutationResult.Error(active.message)
            ActiveDraftLoadResult.None -> Unit
        }

        val detail = getSessionDetail(sessionId)
            ?: return ActiveDraftMutationResult.Error("Séance introuvable.")
        if (detail.summary.sessionType != SessionType.MAX_TEST) {
            return ActiveDraftMutationResult.Error("Seul un Test max peut être repris.")
        }
        val profiles = listExercises().associateBy { it.exerciseId }
        val exercises = detail.exercises.map { item ->
            val profile = profiles[item.exerciseId]
                ?: return ActiveDraftMutationResult.Error(
                    "Profil d'exercice introuvable : ${item.exerciseId}",
                )
            SessionExerciseDraft(
                entryId = item.entryId,
                exercise = profile,
                equipmentId = item.equipmentId,
                maxWeightKg = item.maxWeightKg,
                sets = item.sets,
                continuousDurationSeconds = item.continuousDurationSeconds,
                speedKmh = item.speedKmh,
                distanceKm = item.distanceKm,
            )
        }
        return saveActiveSessionDraft(
            ActiveSessionDraft(
                exercises = exercises,
                sessionType = SessionType.MAX_TEST,
                sourceSessionId = sessionId,
            ),
        )
    }




    fun applyPcCatalogJson(
        json: String,
        exerciseDecisionObserver: ((PcCatalogExerciseDecision) -> Unit)? = null,
    ): PcCatalogImportResult {
        val root =
            try {
                JSONObject(json)
            } catch (
                error: Exception
            ) {
                return PcCatalogImportResult.Invalid(
                    "Catalogue PC JSON invalide."
                )
            }

        if (
            root.optString("format") !=
                "trainlog-pc-catalog" ||
            root.optInt("version", -1) !=
                1
        ) {
            return PcCatalogImportResult.Invalid(
                "Catalogue PC non supporté."
            )
        }

        val exercises =
            root.optJSONArray(
                "exercises"
            )
                ?: return PcCatalogImportResult.Invalid(
                    "Catalogue PC sans exercices."
                )

        val db =
            database.writableDatabase

        var imported = 0
        var reconciled = 0
        var skipped = 0

        db.beginTransaction()

        return try {
            for (
                index in
                0 until exercises.length()
            ) {
                val item =
                    exercises
                        .getJSONObject(
                            index
                        )

                val suppliedExerciseId = item.getString("exercise_id")
                /* CONTRACT: a peer which has not yet consumed the companion
                 * may resend a retired catalogue ID; resolve it before lookup
                 * so the old row can never be resurrected. */
                val exerciseId = resolveExerciseId(db, suppliedExerciseId)
                val suppliedRetiredAlias = suppliedExerciseId != exerciseId

                val suppliedName = item.getString("name")
                val name = canonicalExerciseNames[exerciseId] ?: suppliedName

                val normalized =
                    normalizeName(
                        name
                    )

                val recording =
                    when (
                        item.getString(
                            "recording_mode"
                        )
                    ) {
                        "sets" ->
                            RecordingMode.SETS

                        "continuous" ->
                            RecordingMode.CONTINUOUS

                        else ->
                            return PcCatalogImportResult.Invalid(
                                "recording_mode invalide."
                            )
                    }

                val tracking =
                    when (
                        item.getString(
                            "tracking_mode"
                        )
                    ) {
                        "reps" ->
                            TrackingMode.REPS

                        "duration" ->
                            TrackingMode.DURATION

                        else ->
                            return PcCatalogImportResult.Invalid(
                                "tracking_mode invalide."
                            )
                    }

                val dataFields =
                    item.getInt(
                        "data_fields"
                    )

                fun traceDecision(lookup: String, decision: String) {
                    exerciseDecisionObserver?.invoke(
                        PcCatalogExerciseDecision(
                            exerciseId = exerciseId,
                            name = name,
                            recordingMode = recording.wireValue,
                            trackingMode = tracking.wireValue,
                            dataFields = dataFields,
                            lookup = lookup,
                            decision = decision,
                        )
                    )
                }

                if (
                    !NewExerciseProfile(
                        name = suppliedName,
                        recordingMode = recording,
                        trackingMode = tracking,
                        dataFields = dataFields,
                    ).validate() ||
                    !NewExerciseProfile(
                        name = name,
                        recordingMode = recording,
                        trackingMode = tracking,
                        dataFields = dataFields,
                    ).validate()
                ) {
                    traceDecision("not-run:invalid-profile", "conflict")
                    return PcCatalogImportResult.Invalid(
                        "Profil catalogue PC invalide pour $name."
                    )
                }

                val byId =
                    findExerciseRow(
                        db,
                        "exercise_id = ?",
                        arrayOf(
                            exerciseId
                        )
                    )

                if (byId != null) {
                    if (
                        !exerciseProfilesAreReconcilable(
                            byId,
                            recording,
                            tracking,
                            dataFields,
                        )
                    ) {
                        traceDecision(
                            "exercise_id:${byId.exerciseId}",
                            "conflict",
                        )
                        return PcCatalogImportResult.Invalid(
                            "Conflit de profil catalogue PC pour $name."
                        )
                    }

                    /* Retired identity is routing information only. Its stale
                     * name must not overwrite canonical presentation metadata
                     * or trigger a normalized-name merge. */
                    if (suppliedRetiredAlias) {
                        val richerFields = byId.dataFields or dataFields
                        if (richerFields == byId.dataFields) {
                            skipped += 1
                            traceDecision(
                                "exercise_alias:$suppliedExerciseId;exercise_id:${byId.exerciseId}",
                                "existing-identical",
                            )
                        } else {
                            if (db.update(
                                    "exercises",
                                    ContentValues().apply { put("data_fields", richerFields) },
                                    "id = ?",
                                    arrayOf(byId.rowId.toString()),
                                ) != 1
                            ) return PcCatalogImportResult.DatabaseError
                            reconciled += 1
                            traceDecision(
                                "exercise_alias:$suppliedExerciseId;exercise_id:${byId.exerciseId}",
                                "existing-reconciled",
                            )
                        }
                        continue
                    }

                    /* CONTRACT: a catalog name is mutable metadata.  Identity
                     * reconciliation always prefers exercise_id, so an update
                     * retains the row used by completed sessions and drafts. */
                    val nameOwner =
                        findExerciseRow(
                            db,
                            "normalized_name = ?",
                            arrayOf(normalized),
                        )
                    val mergedNameOwner =
                        nameOwner != null &&
                        nameOwner.rowId != byId.rowId
                    if (mergedNameOwner) {
                        val retired = checkNotNull(nameOwner)
                        if (
                            !exerciseProfilesAreReconcilable(
                                byId,
                                retired.recordingMode,
                                retired.trackingMode,
                                retired.dataFields,
                            ) ||
                            !catalogEquipmentProfilesAreCompatible(
                                db,
                                retired.exerciseId,
                                byId.exerciseId,
                            ) ||
                            !bodyZoneRowsAreCompatibleForMerge(
                                db,
                                byId.exerciseId,
                                retired.exerciseId,
                            )
                        ) {
                            traceDecision(
                                "exercise_id:${byId.exerciseId};" +
                                    "normalized_name:${retired.exerciseId}",
                                "conflict",
                            )
                            return PcCatalogImportResult.Invalid(
                                "Conflit de nom/profil catalogue PC pour $name."
                            )
                        }

                        /* CONTRACT: the incoming PC identity is canonical on
                         * Android.  Every row-ID owner is moved before the
                         * duplicate row disappears; occurrence/session IDs and
                         * all child values remain unchanged. */
                        mergeExerciseRows(
                            db = db,
                            canonical = byId,
                            retired = retired,
                        )
                    }

                    val richerFields =
                        byId.dataFields or dataFields or
                            (nameOwner?.dataFields ?: 0)
                    if (
                        byId.name == name.trim() &&
                        byId.normalizedName == normalized &&
                        byId.dataFields == richerFields &&
                        !mergedNameOwner
                    ) {
                        skipped += 1
                        traceDecision(
                            "exercise_id:${byId.exerciseId}",
                            "existing-identical",
                        )
                        continue
                    }

                    val values = ContentValues().apply {
                        put("name", name.trim())
                        put("normalized_name", normalized)
                        put("data_fields", richerFields)
                    }
                    if (
                        db.update(
                            "exercises",
                            values,
                            "id = ?",
                            arrayOf(byId.rowId.toString()),
                        ) != 1
                    ) {
                        return PcCatalogImportResult.DatabaseError
                    }

                    reconciled += 1
                    traceDecision(
                        if (mergedNameOwner) {
                            "exercise_id:${byId.exerciseId};" +
                                "normalized_name:${nameOwner.exerciseId}"
                        } else {
                            "exercise_id:${byId.exerciseId}"
                        },
                        "existing-reconciled",
                    )
                    continue
                }

                val byName =
                    findExerciseRow(
                        db,
                        "normalized_name = ?",
                        arrayOf(
                            normalized
                        )
                    )

                if (byName != null) {
                    if (
                        !exerciseProfilesAreReconcilable(
                            byName,
                            recording,
                            tracking,
                            dataFields,
                        ) ||
                        !catalogEquipmentProfilesAreCompatible(
                            db,
                            byName.exerciseId,
                            exerciseId,
                        )
                    ) {
                        traceDecision(
                            "normalized_name:${byName.exerciseId}",
                            "conflict",
                        )
                        return PcCatalogImportResult.Invalid(
                            "Conflit de nom/profil entre ${byName.exerciseId} et $exerciseId."
                        )
                    }

                    /* WHY: re-keying the existing Android row preserves every
                     * completed/draft FK directly.  Only the stable catalogue
                     * identity and its string-keyed equipment metadata move. */
                    mergeCatalogEquipmentIdentity(
                        db,
                        byName.exerciseId,
                        exerciseId,
                    )
                    val values = ContentValues().apply {
                        put("exercise_id", exerciseId)
                        put("name", name.trim())
                        put("normalized_name", normalized)
                        put("data_fields", byName.dataFields or dataFields)
                    }
                    if (
                        db.update(
                            "exercises",
                            values,
                            "id = ?",
                            arrayOf(byName.rowId.toString()),
                        ) != 1
                    ) {
                        return PcCatalogImportResult.DatabaseError
                    }

                    reconciled += 1
                    traceDecision(
                        "normalized_name:${byName.exerciseId}",
                        "existing-reconciled",
                    )
                    continue
                }

                val values =
                    ContentValues().apply {
                        put(
                            "exercise_id",
                            exerciseId
                        )

                        put(
                            "name",
                            name
                        )

                        put(
                            "normalized_name",
                            normalized
                        )

                        put(
                            "recording_mode",
                            recording.wireValue
                        )

                        put(
                            "tracking_mode",
                            tracking.wireValue
                        )

                        put(
                            "data_fields",
                            dataFields
                        )
                    }

                db.insertOrThrow(
                    "exercises",
                    null,
                    values
                )

                imported += 1
                traceDecision("none", "inserted")
            }

            db.setTransactionSuccessful()

            PcCatalogImportResult.Applied(
                imported =
                    imported,
                reconciled =
                    reconciled,
                skipped =
                    skipped,
            )
        } catch (
            error: Exception
        ) {
            PcCatalogImportResult.DatabaseError
        } finally {
            db.endTransaction()
        }
    }

    private data class ExerciseRow(
        val rowId: Long,
        val exerciseId: String,
        val name: String,
        val normalizedName: String,
        val recordingMode: RecordingMode,
        val trackingMode: TrackingMode,
        val dataFields: Int,
    )

    private fun exerciseProfilesAreReconcilable(
        row: ExerciseRow,
        recordingMode: RecordingMode,
        trackingMode: TrackingMode,
        dataFields: Int,
    ): Boolean =
        row.recordingMode == recordingMode &&
            row.trackingMode == trackingMode &&
            (
                dataFields and row.dataFields.inv() == 0 ||
                    row.dataFields and dataFields.inv() == 0
            )

    private fun catalogEquipmentProfilesAreCompatible(
        db: SQLiteDatabase,
        retiredExerciseId: String,
        canonicalExerciseId: String,
    ): Boolean =
        db.rawQuery(
            "SELECT 1 FROM catalog_exercise_equipment retired " +
                "JOIN catalog_exercise_equipment canonical " +
                "ON canonical.equipment_row_id=retired.equipment_row_id " +
                "WHERE retired.exercise_id=? AND canonical.exercise_id=? " +
                "AND retired.load_semantics<>canonical.load_semantics LIMIT 1;",
            arrayOf(retiredExerciseId, canonicalExerciseId),
        ).use { !it.moveToFirst() }

    private fun mergeCatalogEquipmentIdentity(
        db: SQLiteDatabase,
        retiredExerciseId: String,
        canonicalExerciseId: String,
    ) {
        /* INVARIANT: equal equipment relationships coalesce; a differing load
         * semantic was rejected before this helper is called. */
        db.execSQL(
            "INSERT OR IGNORE INTO catalog_exercise_equipment(" +
                "exercise_id,equipment_row_id,load_semantics) " +
                "SELECT ?,equipment_row_id,load_semantics " +
                "FROM catalog_exercise_equipment WHERE exercise_id=?;",
            arrayOf(canonicalExerciseId, retiredExerciseId),
        )
        db.execSQL(
            "DELETE FROM catalog_exercise_equipment WHERE exercise_id=?;",
            arrayOf(retiredExerciseId),
        )
    }

    private fun mergeExerciseRows(
        db: SQLiteDatabase,
        canonical: ExerciseRow,
        retired: ExerciseRow,
    ) {
        mergeExerciseBodyZoneRows(db, canonical, retired)
        db.execSQL(
            "UPDATE session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "UPDATE draft_session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "UPDATE active_session_draft SET selected_exercise_row_id=? " +
                "WHERE selected_exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "INSERT OR IGNORE INTO exercise_equipment(exercise_row_id,equipment_row_id) " +
                "SELECT ?,equipment_row_id FROM exercise_equipment WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "DELETE FROM exercise_equipment WHERE exercise_row_id=?;",
            arrayOf(retired.rowId),
        )
        mergeCatalogEquipmentIdentity(
            db,
            retired.exerciseId,
            canonical.exerciseId,
        )
        if (sqliteTableExists(db, "exercise_aliases")) {
            db.execSQL(
                "UPDATE exercise_aliases SET canonical_exercise_id=? WHERE canonical_exercise_id=?;",
                arrayOf(canonical.exerciseId, retired.exerciseId),
            )
            db.execSQL(
                "INSERT INTO exercise_aliases(source_exercise_id,canonical_exercise_id) VALUES(?,?);",
                arrayOf(retired.exerciseId, canonical.exerciseId),
            )
        }
        db.execSQL(
            "DELETE FROM exercises WHERE id=?;",
            arrayOf(retired.rowId),
        )
    }

    private fun aliasBodyZonesAreCompatible(
        db: SQLiteDatabase,
        canonical: ExerciseRow,
        retired: ExerciseRow,
    ): Boolean {
        val canonicalPrimary = readExerciseBodyZones(db, canonical.exerciseId).first
        val retiredPrimary = readExerciseBodyZones(db, retired.exerciseId).first
        return canonicalPrimary == null || retiredPrimary == null ||
            canonicalPrimary == retiredPrimary
    }

    private fun mergeAliasedExerciseRows(
        db: SQLiteDatabase,
        canonical: ExerciseRow,
        retired: ExerciseRow,
    ) {
        val canonicalZones = readExerciseBodyZones(db, canonical.exerciseId)
        val retiredZones = readExerciseBodyZones(db, retired.exerciseId)
        val primary = canonicalZones.first ?: retiredZones.first
        val secondaries = (canonicalZones.second + retiredZones.second)
            .filter { it != primary }.distinct()
        replaceExerciseBodyZones(db, canonical.exerciseId, primary, secondaries)
        db.execSQL("DELETE FROM exercise_body_zones WHERE exercise_row_id=?;", arrayOf(retired.rowId))
        if (sqliteTableExists(db, "exercise_body_zone_sync")) {
            db.execSQL(
                "DELETE FROM exercise_body_zone_sync WHERE exercise_row_id IN(?,?);",
                arrayOf(canonical.rowId, retired.rowId),
            )
        }
        /* CONTRACT: row-owned occurrence and draft children keep their stable
         * IDs and actual/planning values; only the catalogue FK is repointed. */
        db.execSQL("UPDATE session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId))
        db.execSQL("UPDATE draft_session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId))
        db.execSQL("UPDATE active_session_draft SET selected_exercise_row_id=? WHERE selected_exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId))
        db.execSQL(
            "INSERT OR IGNORE INTO exercise_equipment(exercise_row_id,equipment_row_id) " +
                "SELECT ?,equipment_row_id FROM exercise_equipment WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL("DELETE FROM exercise_equipment WHERE exercise_row_id=?;", arrayOf(retired.rowId))
        mergeCatalogEquipmentIdentity(db, retired.exerciseId, canonical.exerciseId)
        db.execSQL("DELETE FROM exercises WHERE id=?;", arrayOf(retired.rowId))
    }

    private fun sqliteTableExists(db: SQLiteDatabase, table: String): Boolean =
        db.rawQuery(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;",
            arrayOf(table),
        ).use { it.moveToFirst() }

    private fun bodyZoneRowsAreCompatibleForMerge(
        db: SQLiteDatabase,
        canonicalExerciseId: String,
        retiredExerciseId: String,
    ): Boolean {
        if (!sqliteTableExists(db, "exercise_body_zones")) return true
        val canonical = readExerciseBodyZones(db, canonicalExerciseId)
        val retired = readExerciseBodyZones(db, retiredExerciseId)
        val canonicalEmpty = canonical.first == null && canonical.second.isEmpty()
        val retiredEmpty = retired.first == null && retired.second.isEmpty()
        return canonical.first == null || retired.first == null || canonical.first == retired.first
    }

    private fun mergeExerciseBodyZoneRows(
        db: SQLiteDatabase,
        canonical: ExerciseRow,
        retired: ExerciseRow,
    ) {
        if (!sqliteTableExists(db, "exercise_body_zones")) return
        val canonicalState = readExerciseBodyZones(db, canonical.exerciseId)
        val retiredState = readExerciseBodyZones(db, retired.exerciseId)
        val canonicalEmpty = canonicalState.first == null && canonicalState.second.isEmpty()
        val retiredEmpty = retiredState.first == null && retiredState.second.isEmpty()
        check(canonicalState.first == null || retiredState.first == null ||
            canonicalState.first == retiredState.first) {
            "Relations de zones incompatibles pendant la réconciliation d'identité."
        }

        /* WHY: row identity reconciliation must not discard the only body-zone
         * decision. CONTRACT: equal states coalesce, one empty side adopts the
         * non-empty state, and differing non-empty states were rejected above.
         * INVARIANT: baselines are cleared because an identity merge is not a
         * synchronization acknowledgement; the next companion must establish
         * a fresh common ancestor before accepting a one-sided change. */
        val primary = canonicalState.first ?: retiredState.first
        if (primary != null) {
            db.execSQL("DELETE FROM exercise_body_zones WHERE exercise_row_id=? AND zone_id=?;",
                arrayOf<Any>(canonical.rowId, primary))
            db.execSQL("INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) VALUES(?,?,'primary');",
                arrayOf<Any>(canonical.rowId, primary))
        }
        db.execSQL(
            "INSERT OR IGNORE INTO exercise_body_zones(exercise_row_id,zone_id,role) " +
                "SELECT ?,zone_id,'secondary' FROM exercise_body_zones " +
                "WHERE exercise_row_id=? AND role='secondary' AND zone_id<>COALESCE(?, '');",
            arrayOf<Any?>(canonical.rowId, retired.rowId, primary),
        )
        db.execSQL("DELETE FROM exercise_body_zones WHERE exercise_row_id=?;",
            arrayOf(retired.rowId))
        if (sqliteTableExists(db, "exercise_body_zone_sync")) {
            db.execSQL(
                "DELETE FROM exercise_body_zone_sync WHERE exercise_row_id IN(?,?);",
                arrayOf(canonical.rowId, retired.rowId),
            )
        }
    }

    private fun findExerciseRow(
        db: SQLiteDatabase,
        selection: String,
        arguments: Array<String>,
    ): ExerciseRow? {
        db.query(
            "exercises",
            arrayOf(
                "id",
                "exercise_id",
                "name",
                "normalized_name",
                "recording_mode",
                "tracking_mode",
                "data_fields",
            ),
            selection,
            arguments,
            null,
            null,
            null,
        ).use {
            cursor ->
                if (
                    !cursor.moveToFirst()
                ) {
                    return null
                }

                return ExerciseRow(
                    rowId =
                        cursor.getLong(0),
                    exerciseId = cursor.getString(1),
                    name = cursor.getString(2),
                    normalizedName = cursor.getString(3),
                    recordingMode =
                        if (
                            cursor.getString(4) ==
                            "continuous"
                        ) {
                            RecordingMode.CONTINUOUS
                        } else {
                            RecordingMode.SETS
                        },
                    trackingMode =
                        if (
                            cursor.getString(5) ==
                            "duration"
                        ) {
                            TrackingMode.DURATION
                        } else {
                            TrackingMode.REPS
                        },
                    dataFields =
                        cursor.getInt(6),
                )
        }
    }

    private fun lookupExerciseRowIdOrNull(
        db: SQLiteDatabase,
        exerciseId: String,
    ): Long? {
        val direct = db.query(
            "exercises",
            arrayOf("id"),
            "exercise_id = ?",
            arrayOf(exerciseId),
            null,
            null,
            null,
        ).use { cursor ->
            if (cursor.moveToFirst()) cursor.getLong(0) else null
        }
        if (direct != null) return direct
        return db.rawQuery(
            "SELECT e.id FROM exercise_aliases a JOIN exercises e " +
                "ON e.exercise_id=a.canonical_exercise_id WHERE a.source_exercise_id=?;",
            arrayOf(exerciseId),
        ).use { cursor -> if (cursor.moveToFirst()) cursor.getLong(0) else null }
    }

    private fun resolveExerciseId(db: SQLiteDatabase, exerciseId: String): String =
        db.rawQuery(
            "SELECT canonical_exercise_id FROM exercise_aliases WHERE source_exercise_id=?;",
            arrayOf(exerciseId),
        ).use { cursor -> if (cursor.moveToFirst()) cursor.getString(0) else exerciseId }

    private fun lookupEquipmentRowIdOrNull(
        db: SQLiteDatabase,
        equipmentId: String?,
    ): Long? {
        if (equipmentId == null) {
            return null
        }
        return db.query(
            "equipment",
            arrayOf("id"),
            "equipment_id = ?",
            arrayOf(equipmentId),
            null,
            null,
            null,
        ).use { cursor ->
            if (cursor.moveToFirst()) cursor.getLong(0) else null
        }
    }

    private fun exerciseHasReferences(
        db: SQLiteDatabase,
        exerciseRowId: Long,
    ): Boolean {
        /* INVARIANT: both completed and active-draft records own a catalog-row
         * reference.  Profile mutation is admitted only while neither exists. */
        return db.rawQuery(
            """
            SELECT EXISTS(
                SELECT 1 FROM session_exercises WHERE exercise_row_id = ?
                UNION ALL
                SELECT 1 FROM draft_session_exercises WHERE exercise_row_id = ?
            );
            """.trimIndent(),
            arrayOf(exerciseRowId.toString(), exerciseRowId.toString()),
        ).use { cursor ->
            cursor.moveToFirst() && cursor.getInt(0) != 0
        }
    }

    fun buildMobileExportJson(): String = buildMobileExport(1)

    private fun buildMobileExport(version: Int): String {
        require(version in 1..3)
        val root = JSONObject()
        root.put("format", "trainlog-mobile-export")
        root.put("version", version)
        root.put("generated_at", OffsetDateTime.now().toString())

        val exerciseArray = JSONArray()
        listExercises().forEach { exercise ->
            exerciseArray.put(
                JSONObject()
                    .put("exercise_id", exercise.exerciseId)
                    .put("name", exercise.name)
                    .put("recording_mode", exercise.recordingMode.wireValue)
                    .put("tracking_mode", exercise.trackingMode.wireValue)
                    .put("data_fields", exercise.dataFields)
            )
        }
        root.put("exercises", exerciseArray)

        val db = database.readableDatabase
        val sessionArray = JSONArray()
        /* CONTRACT: only completed `sessions` are part of mobile export v1;
         * active draft tables are intentionally outside the frozen artifact. */
        db.rawQuery(
            "SELECT id, session_id, started_at, session_type FROM sessions ORDER BY started_at ASC, id ASC;",
            null,
        ).use { sessions ->
            while (sessions.moveToNext()) {
                val sessionRowId = sessions.getLong(0)
                val startedAt = sessions.getString(2)
                /* CONTRACT: current V3 publication admits only the exact
                 * Trainlog timestamp language consumed by later analysis. */
                check(version != 3 || TrainlogTimestamp.parse(startedAt) != null) {
                    "started_at persistant invalide pour session_id=${sessions.getString(1)}"
                }
                val session = JSONObject()
                    .put("session_id", sessions.getString(1))
                    .put("started_at", startedAt)
                    .put("session_type", sessions.getString(3))
                val sessionExercises = JSONArray()
                db.rawQuery(
                    "SELECT se.id, e.exercise_id, e.name, se.recording_mode, se.tracking_mode, se.data_fields, " +
                        "se.entry_id,se.position,eq.equipment_id,mr.max_weight_kg," +
                        "se.load_mode,se.rest_seconds,se.target_sets,se.target_reps," +
                        "se.target_duration_seconds,se.target_weight_kg " +
                        "FROM session_exercises AS se JOIN exercises AS e ON e.id = se.exercise_row_id " +
                        "LEFT JOIN equipment AS eq ON eq.id=se.equipment_row_id " +
                        "LEFT JOIN max_results AS mr ON mr.session_exercise_row_id=se.id " +
                        "WHERE se.session_row_id = ? ORDER BY se.position ASC;",
                    arrayOf(sessionRowId.toString()),
                ).use { exerciseCursor ->
                    while (exerciseCursor.moveToNext()) {
                        val sessionExerciseRowId = exerciseCursor.getLong(0)
                        val recording = exerciseCursor.getString(3)
                        val tracking = exerciseCursor.getString(4)
                        val exportPlan = readSessionPlan(exerciseCursor, 10)
                        val hasPlan = exportPlan != null
                        if (version < 3 && exportPlan != null) {
                            error("Un plan de séance exige l'export mobile V3.")
                        }
                        if (exportPlan != null) {
                            check(recording == "sets" && exerciseCursor.isNull(9) &&
                                exportPlan.sets in 1..MAX_PLAN_SETS &&
                                exportPlan.restSeconds in 0..MAX_PLAN_REST_SECONDS &&
                                if (tracking == "reps") {
                                    exportPlan.reps in 1..MAX_PLAN_REPS && exportPlan.durationSeconds == null
                                } else {
                                    exportPlan.durationSeconds in 1..MAX_PLAN_DURATION_SECONDS && exportPlan.reps == null
                                }) { "Plan de séance SQLite incohérent" }
                            check(if (exportPlan.weightKg == null) exportPlan.loadMode == SessionLoadMode.NONE
                                else exportPlan.weightKg.isFinite() && exportPlan.weightKg > 0.0 &&
                                    exportPlan.loadMode != SessionLoadMode.NONE) {
                                "Mode de charge du plan SQLite incohérent"
                            }
                        }
                        val item = JSONObject()
                            .put("exercise_id", exerciseCursor.getString(1))
                            .put("name", exerciseCursor.getString(2))
                            .put("recording_mode", recording)
                            .put("tracking_mode", tracking)
                            .put("data_fields", exerciseCursor.getInt(5))
                            .put("load_mode", if (version == 3) exerciseCursor.getString(10) else "none")
                            .put("rest_seconds", if (version == 3) exerciseCursor.getInt(11) else 0)
                        if (version == 2) {
                            item.put("entry_id", exerciseCursor.getString(6))
                            item.put("position", exerciseCursor.getInt(7))
                            if (exerciseCursor.isNull(8)) item.put("equipment_id", JSONObject.NULL)
                            else item.put("equipment_id", exerciseCursor.getString(8))
                        }
                        if (version == 3) {
                            item.put("entry_id", exerciseCursor.getString(6))
                            item.put("position", exerciseCursor.getInt(7))
                            if (exerciseCursor.isNull(8)) item.put("equipment_id", JSONObject.NULL)
                            else item.put("equipment_id", exerciseCursor.getString(8))
                            if (!hasPlan) {
                                item.put("target", JSONObject.NULL)
                            } else {
                                val plan = checkNotNull(exportPlan)
                                val target = JSONObject().put("sets", plan.sets)
                                plan.reps?.let { target.put("reps", it) }
                                plan.durationSeconds?.let { target.put("duration_seconds", it) }
                                plan.weightKg?.let { target.put("weight_kg", it) }
                                item.put("target", target)
                            }
                        }
                        if (!exerciseCursor.isNull(9)) {
                            /* TRAINLOG_FORMAT_V1 is frozen and has no max
                             * result shape. Refuse instead of inventing 1x1. */
                            check(version >= 2) {
                                "Un résultat max explicite exige l'export mobile V2."
                            }
                            item.put("max_weight_kg", exerciseCursor.getDouble(9))
                        } else if (recording == "continuous") {
                            db.query(
                                "continuous_activity",
                                arrayOf("duration_seconds", "speed_kmh", "distance_km"),
                                "session_exercise_row_id = ?",
                                arrayOf(sessionExerciseRowId.toString()),
                                null, null, null,
                            ).use { continuous ->
                                if (!continuous.moveToFirst()) {
                                    error("Missing continuous activity")
                                }
                                val payload = JSONObject()
                                    .put("duration_seconds", continuous.getInt(0))
                                if (!continuous.isNull(1)) {
                                    payload.put("speed_kmh", continuous.getDouble(1))
                                }
                                if (!continuous.isNull(2)) {
                                    payload.put("distance_km", continuous.getDouble(2))
                                }
                                item.put("continuous", payload)
                            }
                        } else {
                            val sets = JSONArray()
                            db.query(
                                "performed_sets",
                                arrayOf("reps", "duration_seconds", "weight_kg"),
                                "session_exercise_row_id = ?",
                                arrayOf(sessionExerciseRowId.toString()),
                                null, null, "position ASC",
                            ).use { setCursor ->
                                while (setCursor.moveToNext()) {
                                    val set = JSONObject()
                                    if (tracking == "reps") {
                                        set.put("reps", setCursor.getInt(0))
                                    } else {
                                        set.put("duration_seconds", setCursor.getInt(1))
                                    }
                                    if (version >= 2 && !setCursor.isNull(2)) {
                                        set.put("weight_kg", setCursor.getDouble(2))
                                    }
                                    sets.put(set)
                                }
                            }
                            item.put("sets", sets)
                        }
                        sessionExercises.put(item)
                    }
                }
                session.put("exercises", sessionExercises)
                sessionArray.put(session)
            }
        }
        root.put("sessions", sessionArray)

        val bodyArray = JSONArray()
        db.rawQuery(
            "SELECT observation_id, observed_at, body_weight_kg, neck_cm, shoulders_cm, chest_cm, waist_cm, hips_cm, " +
                "left_arm_cm, right_arm_cm, left_forearm_cm, right_forearm_cm, left_thigh_cm, right_thigh_cm, left_calf_cm, right_calf_cm " +
                "FROM body_observations ORDER BY observed_at ASC, id ASC;",
            null,
        ).use { cursor ->
            val names = arrayOf(
                "body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm", "waist_cm", "hips_cm",
                "left_arm_cm", "right_arm_cm", "left_forearm_cm", "right_forearm_cm",
                "left_thigh_cm", "right_thigh_cm", "left_calf_cm", "right_calf_cm",
            )
            while (cursor.moveToNext()) {
                val observedAt = cursor.getString(1)
                check(version != 3 || TrainlogTimestamp.parse(observedAt) != null) {
                    "observed_at persistant invalide pour observation_id=${cursor.getString(0)}"
                }
                val item = JSONObject()
                    .put("observation_id", cursor.getString(0))
                    .put("observed_at", observedAt)
                for (index in names.indices) {
                    val column = index + 2
                    if (!cursor.isNull(column)) {
                        item.put(names[index], cursor.getDouble(column))
                    }
                }
                bodyArray.put(item)
            }
        }
        root.put("body_observations", bodyArray)
        return root.toString()
    }

    /**
     * V2 is a new artifact, never a reinterpretation of frozen V1.  Each
     * session occurrence carries its durable Android entry_id, ordering,
     * equipment and actual load so repeated catalogue exercises round-trip.
     */
    fun buildMobileExportV2Json(): String = buildMobileExport(2)

    /** Current occurrence exchange; planning and actual rows travel atomically. */
    fun buildMobileExportV3Json(): String = buildMobileExport(3)

    /** Apply the same V2 session artifact emitted by desktop, keyed by entry_id. */
    fun applyPcMobileExportV2Json(json: String): MobileSessionImportResult {
        return applyPcMobileExportJson(json, 2)
    }

    fun applyPcMobileExportV3Json(json: String): MobileSessionImportResult =
        applyPcMobileExportJson(json, 3)

    private fun applyPcMobileExportJson(json: String, version: Int): MobileSessionImportResult {
        if (!hasStrictJsonShape(json)) {
            return MobileSessionImportResult.Invalid("Snapshot séances JSON invalide ou champ dupliqué.")
        }
        val root = try { JSONObject(json) } catch (_: Exception) {
            return MobileSessionImportResult.Invalid("Snapshot séances JSON invalide.")
        }
        validatePcMobileExport(root, version)?.let { return MobileSessionImportResult.Invalid(it) }
        val sessions = root.getJSONArray("sessions")
        val db = database.writableDatabase
        var sessionsAdded = 0
        var sessionsSkipped = 0
        var sessionsUpdated = 0
        var bodyObservationsAdded = 0
        var bodyObservationsSkipped = 0
        return try {
            db.beginTransaction()
            for (i in 0 until sessions.length()) {
                val session = sessions.getJSONObject(i)
                val sessionId = session.optString("session_id")
                val startedAt = session.optString("started_at")
                val type = session.optString("session_type")
                val entries = session.optJSONArray("exercises")
                if (sessionId.isBlank() || startedAt.isBlank() || type !in setOf("training", "max_test") || entries == null) {
                    return MobileSessionImportResult.Invalid("Session V2 invalide.")
                }
                for (index in 0 until entries.length()) {
                    val entry = entries.getJSONObject(index)
                    val exerciseId = entry.optString("exercise_id")
                    val exerciseRow = findExerciseRow(
                        db,
                        "exercise_id = ?",
                        arrayOf(resolveExerciseId(db, exerciseId)),
                    )
                        ?: return MobileSessionImportResult.Invalid("Exercice V2 inconnu : $exerciseId")
                    if (
                        entry.optString("recording_mode") != exerciseRow.recordingMode.wireValue ||
                        entry.optString("tracking_mode") != exerciseRow.trackingMode.wireValue ||
                        entry.optInt("data_fields", -1) and
                            exerciseRow.dataFields.inv() != 0
                    ) {
                        return MobileSessionImportResult.Invalid("Profil V2 incompatible : $exerciseId")
                    }
                }
                val existingRowId = db.rawQuery("SELECT id FROM sessions WHERE session_id=?", arrayOf(sessionId)).use {
                    if (it.moveToFirst()) it.getLong(0) else null
                }
                if (existingRowId != null) {
                    if (pcSessionMatches(db, existingRowId, session, version)) {
                        sessionsSkipped += 1
                        continue
                    }
                    if (!pcResumedMaxUpdateIsSafe(db, existingRowId, session)) {
                        return MobileSessionImportResult.Invalid("Conflit de contenu pour la séance $sessionId")
                    }
                    /* Child replacement is inside this import transaction;
                     * entry identity/order may only be retained and appended. */
                    db.delete(
                        "session_exercises",
                        "session_row_id=?",
                        arrayOf(existingRowId.toString()),
                    )
                    sessionsUpdated += 1
                }
                val rowId = existingRowId ?: run {
                    val values = ContentValues().apply { put("session_id", sessionId); put("started_at", startedAt); put("session_type", type) }
                    db.insertOrThrow("sessions", null, values)
                }
                val seen = mutableSetOf<String>()
                for (index in 0 until entries.length()) {
                    val entry = entries.getJSONObject(index)
                    val entryId = entry.optString("entry_id")
                    val exerciseId = entry.optString("exercise_id")
                    val position = entry.optInt("position", -1)
                    if (entryId.isBlank() || !seen.add(entryId) || exerciseId.isBlank() || position < 0) {
                        return MobileSessionImportResult.Invalid("Identité d'entrée V2 invalide.")
                    }
                    val exerciseRow = findExerciseRow(
                        db,
                        "exercise_id = ?",
                        arrayOf(resolveExerciseId(db, exerciseId)),
                    )
                        ?: return MobileSessionImportResult.Invalid("Exercice V2 inconnu : $exerciseId")
                    val recording = entry.optString("recording_mode")
                    val tracking = entry.optString("tracking_mode")
                    if (recording !in setOf("sets", "continuous") || tracking !in setOf("reps", "duration")) {
                        return MobileSessionImportResult.Invalid("Profil V2 invalide.")
                    }
                    val equipmentId = if (entry.isNull("equipment_id")) null else entry.optString("equipment_id")
                    val equipmentRowId = if (equipmentId == null) null else lookupEquipmentRowIdOrNull(db, equipmentId)
                    if (equipmentId != null && (equipmentId.isBlank() || equipmentRowId == null)) {
                        return MobileSessionImportResult.Invalid("Équipement V2 inconnu : $equipmentId")
                    }
                    val values = ContentValues().apply {
                        put("entry_id", entryId); put("session_row_id", rowId); put("exercise_row_id", exerciseRow.rowId)
                        put("position", position); put("recording_mode", recording); put("tracking_mode", tracking)
                        put("data_fields", entry.optInt("data_fields", 0))
                        if (version == 3) {
                            val target = entry.optJSONObject("target")
                            put("load_mode", entry.getString("load_mode"))
                            put("rest_seconds", entry.getInt("rest_seconds"))
                            if (target == null) {
                                putNull("target_sets"); putNull("target_reps")
                                putNull("target_duration_seconds"); putNull("target_weight_kg")
                            } else {
                                put("target_sets", target.getInt("sets"))
                                if (target.has("reps")) put("target_reps", target.getInt("reps")) else putNull("target_reps")
                                if (target.has("duration_seconds")) put("target_duration_seconds", target.getInt("duration_seconds")) else putNull("target_duration_seconds")
                                if (target.has("weight_kg")) put("target_weight_kg", target.getDouble("weight_kg")) else putNull("target_weight_kg")
                            }
                        }
                        if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                    }
                    val occurrence = db.insertOrThrow("session_exercises", null, values)
                    if (entry.has("max_weight_kg")) {
                        db.insertOrThrow("max_results", null, ContentValues().apply {
                            put("session_exercise_row_id", occurrence)
                            put("max_weight_kg", entry.getDouble("max_weight_kg"))
                        })
                    } else if (recording == "continuous") {
                        val c = entry.optJSONObject("continuous") ?: return MobileSessionImportResult.Invalid("Activité continue manquante.")
                        db.insertOrThrow("continuous_activity", null, ContentValues().apply {
                            put("session_exercise_row_id", occurrence); put("duration_seconds", c.optInt("duration_seconds", 0))
                            if (c.has("speed_kmh")) put("speed_kmh", c.getDouble("speed_kmh")); if (c.has("distance_km")) put("distance_km", c.getDouble("distance_km"))
                        })
                    } else {
                        val sets = entry.optJSONArray("sets") ?: return MobileSessionImportResult.Invalid("Séries manquantes.")
                        for (setIndex in 0 until sets.length()) {
                            val set = sets.getJSONObject(setIndex)
                            db.insertOrThrow("performed_sets", null, ContentValues().apply {
                                put("session_exercise_row_id", occurrence); put("position", setIndex)
                                if (tracking == "reps") put("reps", set.optInt("reps", -1)) else put("duration_seconds", set.optInt("duration_seconds", 0))
                                if (set.has("weight_kg")) put("weight_kg", set.getDouble("weight_kg"))
                            })
                        }
                    }
                }
                if (existingRowId == null) sessionsAdded += 1
            }
            val body = root.optJSONArray("body_observations")
                ?: return MobileSessionImportResult.Invalid("Mesures corporelles manquantes.")
            val metrics = arrayOf("body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm", "waist_cm", "hips_cm",
                "left_arm_cm", "right_arm_cm", "left_forearm_cm", "right_forearm_cm", "left_thigh_cm", "right_thigh_cm",
                "left_calf_cm", "right_calf_cm")
            for (index in 0 until body.length()) {
                val item = body.getJSONObject(index)
                val id = item.optString("observation_id")
                val observedAt = item.optString("observed_at")
                if (id.isBlank() || observedAt.isBlank() || metrics.none { item.has(it) }) {
                    return MobileSessionImportResult.Invalid("Observation corporelle V2 invalide.")
                }
                val existing = db.rawQuery(
                    "SELECT observed_at,${metrics.joinToString(",")} FROM body_observations WHERE observation_id=?",
                    arrayOf(id)).use { cursor ->
                    if (!cursor.moveToFirst()) null else List(1 + metrics.size) { column ->
                        if (cursor.isNull(column)) null else if (column == 0) cursor.getString(column) else cursor.getDouble(column)
                    }
                }
                val expected: List<Any?> = listOf(observedAt) + metrics.map { metric ->
                    if (item.has(metric)) item.getDouble(metric) else null
                }
                if (existing != null) {
                    if (existing != expected) return MobileSessionImportResult.Invalid("Conflit observation corporelle : $id")
                    bodyObservationsSkipped += 1
                    continue
                }
                db.insertOrThrow("body_observations", null, ContentValues().apply {
                    put("observation_id", id); put("observed_at", observedAt)
                    metrics.forEach { metric -> if (item.has(metric)) put(metric, item.getDouble(metric)) }
                })
                bodyObservationsAdded += 1
            }
            db.setTransactionSuccessful()
            MobileSessionImportResult.Applied(
                sessionsAdded = sessionsAdded,
                sessionsSkipped = sessionsSkipped,
                bodyObservationsAdded = bodyObservationsAdded,
                bodyObservationsSkipped = bodyObservationsSkipped,
                sessionsUpdated = sessionsUpdated,
            )
        } catch (_: Exception) { MobileSessionImportResult.DatabaseError
        } finally { db.endTransaction() }
    }

    private fun validatePcMobileExport(root: JSONObject, version: Int): String? {
        val rootKeys = setOf("format", "version", "generated_at", "exercises", "sessions", "body_observations")
        if (!root.hasExactKeys(rootKeys) || root.value("format") != "trainlog-mobile-export" ||
            !root.value("version").isJsonInt(version, version) || !root.value("generated_at").isNonemptyJsonString() ||
            root.value("exercises") !is JSONArray || root.value("sessions") !is JSONArray ||
            root.value("body_observations") !is JSONArray) {
            return "Snapshot séances V2 invalide."
        }
        if (version == 3 && TrainlogTimestamp.parse(root.getString("generated_at")) == null) {
            return "Snapshot séances V3 invalide: generated_at invalide."
        }
        return try {
            val exerciseIds = mutableSetOf<String>()
            val exerciseProfiles = mutableMapOf<String, Triple<String, String, Int>>()
            val exerciseKeys = setOf("exercise_id", "name", "recording_mode", "tracking_mode", "data_fields")
            val exercises = root.getJSONArray("exercises")
            for (index in 0 until exercises.length()) {
                val item = exercises.opt(index) as? JSONObject ?: return "Catalogue exercices V2 invalide."
                val id = item.value("exercise_id")
                if (!item.hasExactKeys(exerciseKeys) || !id.isNonemptyJsonString() || !exerciseIds.add(id as String) ||
                    !item.value("name").isNonemptyJsonString() || !validJsonProfile(item)) {
                    return "Catalogue exercices V2 invalide."
                }
                exerciseProfiles[id] = Triple(
                    item.getString("recording_mode"),
                    item.getString("tracking_mode"),
                    item.getInt("data_fields"),
                )
            }

            val sessionKeys = setOf("session_id", "started_at", "session_type", "exercises")
            val entryBaseKeys = setOf("exercise_id", "name", "recording_mode", "tracking_mode", "data_fields",
                "load_mode", "rest_seconds", "entry_id", "position", "equipment_id")
            val sessionIds = mutableSetOf<String>()
            val sessions = root.getJSONArray("sessions")
            for (sessionIndex in 0 until sessions.length()) {
                val session = sessions.opt(sessionIndex) as? JSONObject ?: return "Session V2 invalide."
                val sessionId = session.value("session_id")
                val entries = session.value("exercises") as? JSONArray
                if (!session.hasExactKeys(sessionKeys) || !sessionId.isNonemptyJsonString() ||
                    !sessionIds.add(sessionId as String) || !session.value("started_at").isNonemptyJsonString() ||
                    session.value("session_type") !in setOf("training", "max_test") || entries == null || entries.length() == 0) {
                    return "Session V2 invalide."
                }
                if (version == 3 && TrainlogTimestamp.parse(session.getString("started_at")) == null) {
                    return "Session V3 invalide: sessions[$sessionIndex].started_at invalide."
                }
                val entryIds = mutableSetOf<String>()
                val positions = mutableSetOf<Int>()
                for (entryIndex in 0 until entries.length()) {
                    val entry = entries.opt(entryIndex) as? JSONObject ?: return "Entrée de séance V2 invalide."
                    val recording = entry.value("recording_mode")
                    val tracking = entry.value("tracking_mode")
                    val hasMax = entry.has("max_weight_kg")
                    val expectedKeys = entryBaseKeys + (if (version == 3) setOf("target") else emptySet()) + when {
                        hasMax -> setOf("max_weight_kg")
                        recording == "continuous" -> setOf("continuous")
                        else -> setOf("sets")
                    }
                    val entryId = entry.value("entry_id")
                    val exerciseId = entry.value("exercise_id")
                    val positionValue = entry.value("position")
                    if (!entry.hasExactKeys(expectedKeys) || !entryId.isNonemptyJsonString() || !entryIds.add(entryId as String) ||
                        !exerciseId.isNonemptyJsonString() || exerciseId !in exerciseIds || !entry.value("name").isNonemptyJsonString() ||
                        !validJsonProfile(entry) || !validEntryPlan(entry, version, recording as String, tracking as String, hasMax) ||
                        !positionValue.isJsonInt(0, 100000) || !positions.add((positionValue as Number).toInt()) ||
                        !(entry.value("equipment_id") === JSONObject.NULL || entry.value("equipment_id").isNonemptyJsonString())) {
                        return "Entrée de séance V2 invalide."
                    }
                    if (hasMax &&
                        (session.getString("session_type") != "max_test" ||
                            !entry.value("max_weight_kg").isPositiveJsonNumber())) {
                        return "Résultat max V2 invalide."
                    }
                    val catalogProfile = exerciseProfiles[exerciseId]
                        ?: return "Exercice de séance V2 absent du catalogue."
                    val entryFields = entry.getInt("data_fields")
                    if (
                        entry.getString("recording_mode") != catalogProfile.first ||
                        entry.getString("tracking_mode") != catalogProfile.second ||
                        entryFields and catalogProfile.third.inv() != 0
                    ) {
                        /* CONTRACT: richer current catalog metadata must not
                         * rewrite an older occurrence snapshot. */
                        return "Profil historique V2 incompatible avec le catalogue."
                    }
                    if (hasMax) {
                        /* The exact-key check above excludes set/continuous
                         * shadows; max identity remains the exercise entry. */
                    } else if (recording == "continuous") {
                        val continuous = entry.value("continuous") as? JSONObject ?: return "Activité continue V2 invalide."
                        val allowed = setOf("duration_seconds", "speed_kmh", "distance_km")
                        val fields = (entry.value("data_fields") as Number).toInt()
                        if (!continuous.hasOnlyKeys(allowed, setOf("duration_seconds")) ||
                            !continuous.value("duration_seconds").isJsonInt(1, 86400) ||
                            continuous.has("speed_kmh") != (fields and 1 != 0) ||
                            continuous.has("distance_km") != (fields and 2 != 0) ||
                            (continuous.has("speed_kmh") && !continuous.value("speed_kmh").isPositiveJsonNumber()) ||
                            (continuous.has("distance_km") && !continuous.value("distance_km").isPositiveJsonNumber())) {
                            return "Activité continue V2 invalide."
                        }
                    } else {
                        val sets = entry.value("sets") as? JSONArray ?: return "Séries V2 invalides."
                        if (sets.length() == 0) return "Séries V2 invalides."
                        for (setIndex in 0 until sets.length()) {
                            val set = sets.opt(setIndex) as? JSONObject ?: return "Série V2 invalide."
                            val valueKey = if (tracking == "reps") "reps" else "duration_seconds"
                            val minimum = if (tracking == "reps") 0 else 1
                            val maximum = if (tracking == "reps") 10000 else 86400
                            if (!set.hasOnlyKeys(setOf(valueKey, "weight_kg"), setOf(valueKey)) ||
                                !set.value(valueKey).isJsonInt(minimum, maximum) ||
                                (set.has("weight_kg") && !set.value("weight_kg").isNonnegativeJsonNumber())) {
                                return "Série V2 invalide."
                            }
                        }
                    }
                }
            }

            val metricKeys = setOf("body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm", "waist_cm", "hips_cm",
                "left_arm_cm", "right_arm_cm", "left_forearm_cm", "right_forearm_cm", "left_thigh_cm", "right_thigh_cm",
                "left_calf_cm", "right_calf_cm")
            val bodyIds = mutableSetOf<String>()
            val body = root.getJSONArray("body_observations")
            for (index in 0 until body.length()) {
                val item = body.opt(index) as? JSONObject ?: return "Observation corporelle V2 invalide."
                val id = item.value("observation_id")
                val presentMetrics = item.keys().asSequence().toSet() intersect metricKeys
                if (!item.hasOnlyKeys(metricKeys + setOf("observation_id", "observed_at"), setOf("observation_id", "observed_at")) ||
                    !id.isNonemptyJsonString() || !bodyIds.add(id as String) || !item.value("observed_at").isNonemptyJsonString() ||
                    presentMetrics.isEmpty() || presentMetrics.any { !item.value(it).isPositiveJsonNumber() }) {
                    return "Observation corporelle V2 invalide."
                }
                if (version == 3 && TrainlogTimestamp.parse(item.getString("observed_at")) == null) {
                    return "Observation corporelle V3 invalide: body_observations[$index].observed_at invalide."
                }
            }
            null
        } catch (_: Exception) {
            "Snapshot séances V2 invalide."
        }
    }

    private fun validJsonProfile(item: JSONObject): Boolean {
        val recording = item.value("recording_mode")
        val tracking = item.value("tracking_mode")
        val dataFields = item.value("data_fields")
        return recording in setOf("sets", "continuous") &&
            tracking in setOf("reps", "duration") &&
            !(recording == "continuous" && tracking != "duration") &&
            dataFields.isJsonInt(0, ExerciseDataFields.KNOWN_MASK) &&
            !(recording == "sets" && (dataFields as Number).toInt() != 0)
    }

    private fun hasStrictJsonShape(json: String): Boolean = try {
        JsonReader(StringReader(json)).use { reader ->
            reader.isLenient = false
            fun readValue() {
                when (reader.peek()) {
                    JsonToken.BEGIN_OBJECT -> {
                        reader.beginObject()
                        val names = mutableSetOf<String>()
                        while (reader.hasNext()) {
                            check(names.add(reader.nextName())) { "duplicate JSON key" }
                            readValue()
                        }
                        reader.endObject()
                    }
                    JsonToken.BEGIN_ARRAY -> {
                        reader.beginArray()
                        while (reader.hasNext()) readValue()
                        reader.endArray()
                    }
                    JsonToken.STRING -> reader.nextString()
                    JsonToken.NUMBER -> {
                        val value = reader.nextString().toDouble()
                        check(value.isFinite()) { "non-finite JSON number" }
                    }
                    JsonToken.BOOLEAN -> reader.nextBoolean()
                    JsonToken.NULL -> reader.nextNull()
                    else -> error("unexpected JSON token")
                }
            }
            readValue()
            check(reader.peek() == JsonToken.END_DOCUMENT) { "trailing JSON" }
        }
        true
    } catch (_: Exception) {
        false
    }

    private fun validEntryPlan(
        entry: JSONObject,
        version: Int,
        recording: String,
        tracking: String,
        hasMax: Boolean,
    ): Boolean {
        if (version == 2) {
            return entry.value("load_mode") == "none" && entry.value("rest_seconds").isJsonInt(0, 0)
        }
        val loadMode = entry.value("load_mode")
        val rest = entry.value("rest_seconds")
        if (loadMode !in setOf("none", "external", "assistance") ||
            !rest.isJsonInt(0, MAX_PLAN_REST_SECONDS)) return false
        val targetValue = entry.value("target")
        if (targetValue === JSONObject.NULL) {
            return loadMode == "none" && rest.isJsonInt(0, 0)
        }
        if (recording != "sets" || hasMax) return false
        val target = targetValue as? JSONObject ?: return false
        val metric = if (tracking == "reps") "reps" else "duration_seconds"
        val otherMetric = if (tracking == "reps") "duration_seconds" else "reps"
        val allowed = setOf("sets", metric, "weight_kg")
        if (!target.hasOnlyKeys(allowed, setOf("sets", metric)) || target.has(otherMetric) ||
            !target.value("sets").isJsonInt(1, MAX_PLAN_SETS) ||
            !target.value(metric).isJsonInt(1, if (tracking == "reps") MAX_PLAN_REPS else MAX_PLAN_DURATION_SECONDS)) return false
        val hasWeight = target.has("weight_kg")
        if (hasWeight && !target.value("weight_kg").isPositiveJsonNumber()) return false
        return if (hasWeight) loadMode == "external" || loadMode == "assistance" else loadMode == "none"
    }

    private fun JSONObject.value(key: String): Any? = if (has(key)) get(key) else null
    private fun JSONObject.hasExactKeys(expected: Set<String>): Boolean = keys().asSequence().toSet() == expected
    private fun JSONObject.hasOnlyKeys(allowed: Set<String>, required: Set<String>): Boolean {
        val actual = keys().asSequence().toSet()
        return actual.all { it in allowed } && required.all { it in actual }
    }
    private fun Any?.isNonemptyJsonString(): Boolean = this is String && isNotEmpty()
    private fun Any?.isJsonInt(minimum: Int, maximum: Int): Boolean {
        if (this !is Number) return false
        val number = toDouble()
        return number.isFinite() && number % 1.0 == 0.0 && number >= minimum && number <= maximum
    }
    private fun Any?.isPositiveJsonNumber(): Boolean = this is Number && toDouble().isFinite() && toDouble() > 0.0
    private fun Any?.isNonnegativeJsonNumber(): Boolean =
        this is Number && toDouble().isFinite() && toDouble() >= 0.0

    private fun pcSessionMatches(db: SQLiteDatabase, rowId: Long, session: JSONObject, version: Int): Boolean {
        val headerMatches = db.rawQuery("SELECT started_at,session_type FROM sessions WHERE id=?", arrayOf(rowId.toString())).use {
            it.moveToFirst() && it.getString(0) == session.optString("started_at") && it.getString(1) == session.optString("session_type")
        }
        if (!headerMatches) return false
        val incoming = session.optJSONArray("exercises") ?: return false
        val rows = mutableListOf<Long>()
        val metadata = mutableListOf<List<Any?>>()
        db.rawQuery(
            "SELECT se.id,se.entry_id,se.position,e.exercise_id,se.recording_mode,se.tracking_mode,se.data_fields,eq.equipment_id," +
                "se.load_mode,se.rest_seconds,se.target_sets,se.target_reps,se.target_duration_seconds,se.target_weight_kg " +
                "FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id " +
                "WHERE se.session_row_id=? ORDER BY se.position", arrayOf(rowId.toString())).use { cursor ->
            while (cursor.moveToNext()) {
                rows += cursor.getLong(0)
                val base = mutableListOf<Any?>(cursor.getString(1), cursor.getInt(2), cursor.getString(3), cursor.getString(4),
                    cursor.getString(5), cursor.getInt(6), if (cursor.isNull(7)) null else cursor.getString(7))
                if (version == 2 && (cursor.getString(8) != "none" || cursor.getInt(9) != 0 ||
                        (10..13).any { !cursor.isNull(it) })) return false
                if (version == 3) base.addAll(listOf(cursor.getString(8), cursor.getInt(9),
                    if (cursor.isNull(10)) null else cursor.getInt(10), if (cursor.isNull(11)) null else cursor.getInt(11),
                    if (cursor.isNull(12)) null else cursor.getInt(12), if (cursor.isNull(13)) null else cursor.getDouble(13)))
                metadata += base
            }
        }
        if (rows.size != incoming.length()) return false
        for (index in rows.indices) {
            val item = incoming.getJSONObject(index)
            val expected = mutableListOf<Any?>(item.optString("entry_id"), item.optInt("position", -1),
                resolveExerciseId(db, item.optString("exercise_id")),
                item.optString("recording_mode"), item.optString("tracking_mode"), item.optInt("data_fields", -1),
                if (item.isNull("equipment_id")) null else item.optString("equipment_id"))
            if (version == 3) {
                val target = item.optJSONObject("target")
                expected.addAll(listOf(item.getString("load_mode"), item.getInt("rest_seconds"),
                    target?.getInt("sets"), target?.optIntOrNull("reps"),
                    target?.optIntOrNull("duration_seconds"), target?.optDoubleOrNull("weight_kg")))
            }
            if (metadata[index] != expected) return false
            if (item.has("max_weight_kg")) {
                val current = db.rawQuery(
                    "SELECT max_weight_kg FROM max_results WHERE session_exercise_row_id=?",
                    arrayOf(rows[index].toString()),
                ).use { cursor -> if (cursor.moveToFirst()) cursor.getDouble(0) else null }
                if (current != item.getDouble("max_weight_kg")) return false
            } else if (item.optString("recording_mode") == "continuous") {
                val value = item.optJSONObject("continuous") ?: return false
                val current = db.rawQuery("SELECT duration_seconds,speed_kmh,distance_km FROM continuous_activity WHERE session_exercise_row_id=?",
                    arrayOf(rows[index].toString())).use { cursor ->
                    if (!cursor.moveToFirst()) null else listOf(cursor.getInt(0), if (cursor.isNull(1)) null else cursor.getDouble(1), if (cursor.isNull(2)) null else cursor.getDouble(2))
                }
                if (current != listOf(value.optInt("duration_seconds", -1), value.optDoubleOrNull("speed_kmh"), value.optDoubleOrNull("distance_km"))) return false
            } else {
                val sets = item.optJSONArray("sets") ?: return false
                val current = mutableListOf<List<Any?>>()
                db.rawQuery("SELECT reps,duration_seconds,weight_kg FROM performed_sets WHERE session_exercise_row_id=? ORDER BY position",
                    arrayOf(rows[index].toString())).use { cursor -> while (cursor.moveToNext()) current += listOf(
                    if (cursor.isNull(0)) null else cursor.getInt(0), if (cursor.isNull(1)) null else cursor.getInt(1),
                    if (cursor.isNull(2)) null else cursor.getDouble(2)) }
                val wanted = (0 until sets.length()).map { setIndex -> sets.getJSONObject(setIndex).let { set ->
                    listOf(if (set.has("reps")) set.getInt("reps") else null,
                        if (set.has("duration_seconds")) set.getInt("duration_seconds") else null,
                        set.optDoubleOrNull("weight_kg")) } }
                if (current != wanted) return false
            }
        }
        return true
    }

    private fun pcResumedMaxUpdateIsSafe(
        db: SQLiteDatabase,
        rowId: Long,
        session: JSONObject,
    ): Boolean {
        if (session.optString("session_type") != "max_test") return false
        val headerMatches = db.rawQuery(
            "SELECT started_at,session_type FROM sessions WHERE id=?",
            arrayOf(rowId.toString()),
        ).use {
            it.moveToFirst() && it.getString(0) == session.optString("started_at") &&
                it.getString(1) == "max_test"
        }
        if (!headerMatches) return false
        val current = mutableListOf<Triple<String, Int, String>>()
        db.rawQuery(
            "SELECT se.entry_id,se.position,e.exercise_id FROM session_exercises se " +
                "JOIN exercises e ON e.id=se.exercise_row_id WHERE se.session_row_id=? " +
                "ORDER BY se.position;",
            arrayOf(rowId.toString()),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                current += Triple(cursor.getString(0), cursor.getInt(1), cursor.getString(2))
            }
        }
        val incoming = session.optJSONArray("exercises") ?: return false
        if (incoming.length() < current.size) return false
        return current.indices.all { index ->
            val item = incoming.optJSONObject(index) ?: return@all false
            current[index] == Triple(
                item.optString("entry_id"),
                item.optInt("position", -1),
                resolveExerciseId(db, item.optString("exercise_id")),
            )
        }
    }

    /**
     * CONTRACT: this companion artifact is deliberately outside frozen mobile
     * export v1. Version 2 keys an occurrence by `(session_id, entry_id)`;
     * `exercise_id` is corroborating catalogue identity, never an occurrence
     * key. `cleared` is intentional state, unlike an absent artifact which
     * conveys no equipment signal.
     */
    fun buildEquipmentAssociationsJson(): String {
        val root = JSONObject()
            .put("format", "trainlog-equipment-associations")
            .put("version", 2)
            .put("generated_at", OffsetDateTime.now().toString())
        val associations = JSONArray()
        database.readableDatabase.rawQuery(
            "SELECT s.session_id, se.entry_id, e.exercise_id, eq.equipment_id FROM session_exercises se " +
                "JOIN sessions s ON s.id = se.session_row_id " +
                "JOIN exercises e ON e.id = se.exercise_row_id " +
                "LEFT JOIN equipment eq ON eq.id = se.equipment_row_id " +
                "ORDER BY s.started_at ASC, s.id ASC, se.position ASC;",
            null,
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val item = JSONObject()
                    .put("session_id", cursor.getString(0))
                    .put("entry_id", cursor.getString(1))
                    .put("exercise_id", cursor.getString(2))
                if (cursor.isNull(3)) item.put("state", "cleared") else {
                    item.put("state", "set")
                    item.put("equipment_id", cursor.getString(3))
                }
                associations.put(item)
            }
        }
        return root.put("associations", associations).toString()
    }

    /** Snapshot only user-created definitions; bundled manifest rows are never
     * exported as mutable data and absence never requests deletion. */
    fun buildEquipmentDefinitionsJson(): String {
        val supplied = EquipmentCatalog.load(applicationContext).map { it.equipmentId }.toSet()
        val items = JSONArray()
        listEquipment().filter { it.equipmentId !in supplied }.forEach { entry ->
            /* A companion is authoritative metadata, never a repair for an
             * arbitrary session reference.  Refuse corrupt persisted custom
             * rows before publication so desktop never receives a partial
             * definition snapshot. */
            check(
                entry.equipmentId.isNotBlank() &&
                    entry.displayName.isNotBlank() &&
                    entry.displayName.length <= 120 &&
                    entry.type.isNotBlank()
            ) {
                "Définition équipement personnalisée invalide : ${entry.equipmentId}"
            }
            items.put(JSONObject()
                .put("equipment_id", entry.equipmentId)
                .put("display_name", entry.displayName)
                .put("label_name", entry.labelName)
                .put("equipment_type", entry.type)
                .put("load_semantics", entry.loadSemantics.name.lowercase(Locale.ROOT)))
        }
        return JSONObject()
            .put("format", "trainlog-equipment-definitions")
            .put("version", 1)
            .put("generated_at", OffsetDateTime.now().toString())
            .put("equipment", items).toString()
    }

    fun applyPcEquipmentDefinitionsJson(json: String): EquipmentDefinitionImportResult {
        val root = try { JSONObject(json) } catch (_: Exception) {
            return EquipmentDefinitionImportResult.Invalid("Définitions équipement JSON invalides.")
        }
        if (root.value("format") != "trainlog-equipment-definitions" || !root.value("version").isJsonInt(1, 1) ||
            root.keys().asSequence().toSet() != setOf("format", "version", "generated_at", "equipment") ||
            !root.value("generated_at").isNonemptyJsonString()) {
            return EquipmentDefinitionImportResult.Invalid("Définitions équipement non supportées.")
        }
        val items = root.optJSONArray("equipment")
            ?: return EquipmentDefinitionImportResult.Invalid("Tableau equipment manquant.")
        val supplied = EquipmentCatalog.load(applicationContext).map { it.equipmentId }.toSet()
        val parsed = mutableListOf<EquipmentCatalogEntry>()
        val seen = mutableSetOf<String>()
        try {
            for (index in 0 until items.length()) {
                val item = items.getJSONObject(index)
                if (item.keys().asSequence().toSet() != setOf("equipment_id", "display_name", "label_name", "equipment_type", "load_semantics")) {
                    return EquipmentDefinitionImportResult.Invalid("Définition équipement invalide.")
                }
                val fields = listOf(
                    "equipment_id",
                    "display_name",
                    "label_name",
                    "equipment_type",
                    "load_semantics",
                )
                if (fields.any { item.value(it) !is String }) {
                    return EquipmentDefinitionImportResult.Invalid("Définition équipement invalide.")
                }
                val id = item.value("equipment_id") as String
                val display = item.value("display_name") as String
                val label = item.value("label_name") as String
                val type = item.value("equipment_type") as String
                val semanticsValue = item.value("load_semantics") as String
                if (semanticsValue !in setOf("none", "external", "assistance")) {
                    return EquipmentDefinitionImportResult.Invalid("Sémantique équipement invalide.")
                }
                val semantics = try { EquipmentLoadSemantics.valueOf(semanticsValue.uppercase(Locale.ROOT)) }
                    catch (_: Exception) { return EquipmentDefinitionImportResult.Invalid("Sémantique équipement invalide.") }
                if (id.isBlank() || !seen.add(id) || id in supplied || display.isBlank() || display.length > 120 || type.isBlank()) {
                    return EquipmentDefinitionImportResult.Invalid("Identité équipement invalide ou réservée : $id")
                }
                parsed += EquipmentCatalogEntry(id, label, display, emptyList(), type, semantics)
            }
        } catch (_: Exception) { return EquipmentDefinitionImportResult.Invalid("Définition équipement invalide.") }

        val db = database.writableDatabase
        return try {
            var imported = 0
            var skipped = 0
            db.beginTransaction()
            for (entry in parsed) {
                val existing = db.rawQuery(
                    "SELECT label_name,display_name,equipment_type,load_semantics FROM equipment WHERE equipment_id=?",
                    arrayOf(entry.equipmentId)).use { cursor ->
                    if (cursor.moveToFirst()) listOf(cursor.getString(0), cursor.getString(1), cursor.getString(2), cursor.getString(3)) else null
                }
                val expected = listOf(entry.labelName, entry.displayName, entry.type, entry.loadSemantics.name.lowercase(Locale.ROOT))
                if (existing != null && existing != expected) {
                    return EquipmentDefinitionImportResult.Invalid("Conflit définition équipement : ${entry.equipmentId}")
                }
                if (existing != null) { skipped++; continue }
                db.insertOrThrow("equipment", null, ContentValues().apply {
                    put("equipment_id", entry.equipmentId); put("label_name", entry.labelName)
                    put("display_name", entry.displayName); put("equipment_type", entry.type)
                    put("load_semantics", entry.loadSemantics.name.lowercase(Locale.ROOT))
                })
                imported++
            }
            db.setTransactionSuccessful()
            EquipmentDefinitionImportResult.Applied(imported, skipped)
        } catch (_: Exception) { EquipmentDefinitionImportResult.DatabaseError
        } finally { db.endTransaction() }
    }

    fun applyPcEquipmentAssociationsJson(json: String): EquipmentAssociationImportResult {
        val root = try { JSONObject(json) } catch (_: Exception) {
            return EquipmentAssociationImportResult.Invalid("Extension équipement JSON invalide.")
        }
        val versionValue = root.value("version")
        if (!root.hasExactKeys(setOf("format", "version", "generated_at", "associations")) ||
            root.value("format") != "trainlog-equipment-associations" ||
            !versionValue.isJsonInt(1, 2) || !root.value("generated_at").isNonemptyJsonString()) {
            return EquipmentAssociationImportResult.Invalid("Extension équipement non supportée.")
        }
        val version = (versionValue as Number).toInt()
        val items = root.optJSONArray("associations")
            ?: return EquipmentAssociationImportResult.Invalid("Associations équipement manquantes.")
        val known = listEquipment().map { it.equipmentId }.toSet()
        val validatedIdentities = mutableSetOf<String>()
        try {
            for (index in 0 until items.length()) {
                val item = items.opt(index) as? JSONObject
                    ?: return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
                val sessionId = item.value("session_id")
                val exerciseId = item.value("exercise_id")
                val entryId = if (version == 2) item.value("entry_id") else null
                val state = item.value("state")
                val required = mutableSetOf("session_id", "exercise_id", "state")
                if (version == 2) required += "entry_id"
                if (state == "set") required += "equipment_id"
                if (!item.hasExactKeys(required) || !sessionId.isNonemptyJsonString() ||
                    !exerciseId.isNonemptyJsonString() || (version == 2 && !entryId.isNonemptyJsonString()) ||
                    state !in setOf("set", "cleared")) {
                    return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
                }
                val identity = "${sessionId as String}\u0000${if (version == 2) entryId as String else exerciseId as String}"
                if (!validatedIdentities.add(identity)) {
                    return EquipmentAssociationImportResult.Invalid("Association équipement dupliquée.")
                }
                if (state == "set") {
                    val equipmentId = item.value("equipment_id")
                    if (!equipmentId.isNonemptyJsonString() || equipmentId !in known) {
                        return EquipmentAssociationImportResult.Invalid("Équipement inconnu : $equipmentId")
                    }
                }
            }
        } catch (_: Exception) {
            return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
        }
        val db = database.writableDatabase
        val seen = mutableSetOf<String>()
        db.beginTransaction()
        try {
            for (index in 0 until items.length()) {
                val item = items.getJSONObject(index)
                val sessionId = item.optString("session_id")
                val exerciseId = item.optString("exercise_id")
                val canonicalExerciseId = resolveExerciseId(db, exerciseId)
                val entryId = if (version == 2) item.optString("entry_id") else null
                val state = item.optString("state")
                val identity = "$sessionId\u0000${entryId ?: exerciseId}"
                if (sessionId.isBlank() || exerciseId.isBlank() || !seen.add(identity) || state !in setOf("set", "cleared") || (version == 2 && entryId.isNullOrBlank())) {
                    return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
                }
                val equipmentId = if (state == "set") item.optString("equipment_id") else null
                if (state == "set" && (equipmentId.isNullOrBlank() || equipmentId !in known)) {
                    return EquipmentAssociationImportResult.Invalid("Équipement inconnu : $equipmentId")
                }
                val row = db.rawQuery(
                    if (version == 2) {
                        "SELECT se.id,e.exercise_id,eq.equipment_id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id WHERE s.session_id=? AND se.entry_id=?;"
                    } else {
                        "SELECT se.id,eq.equipment_id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id WHERE s.session_id=? AND e.exercise_id=?;"
                    },
                    if (version == 2) arrayOf(sessionId, entryId) else arrayOf(sessionId, canonicalExerciseId),
                ).use { cursor ->
                    val first = if (cursor.moveToFirst()) {
                        if (version == 2) {
                            Triple(cursor.getLong(0), cursor.getString(1), if (cursor.isNull(2)) null else cursor.getString(2))
                        } else {
                            Triple(cursor.getLong(0), canonicalExerciseId, if (cursor.isNull(1)) null else cursor.getString(1))
                        }
                    } else null
                    if (version == 1 && first != null && cursor.moveToNext()) null else first
                }
                    ?: return EquipmentAssociationImportResult.Invalid("Entrée de séance inconnue : $sessionId/$exerciseId")
                if (row.second != canonicalExerciseId) {
                    return EquipmentAssociationImportResult.Invalid("Conflit exercice association : $sessionId/${entryId ?: exerciseId}")
                }
                if (row.third != equipmentId) {
                    return EquipmentAssociationImportResult.Invalid("Conflit association équipement : $sessionId/${entryId ?: exerciseId}")
                }
                /* Equal state is an idempotent replay; association snapshots
                 * never overwrite divergent local edits. */
                continue
            }
            db.setTransactionSuccessful()
            return EquipmentAssociationImportResult.Applied(0)
        } catch (_: Exception) {
            return EquipmentAssociationImportResult.DatabaseError
        } finally { db.endTransaction() }
    }

    fun saveBodyObservation(
        draft: BodyObservationDraft,
    ): SaveBodyObservationResult {
        if (
            !draft.hasAnyMetric() ||
            !draft.allValuesPositive()
        ) {
            return SaveBodyObservationResult.Invalid
        }

        val observationId =
            "bo_" +
                UUID.randomUUID()
                    .toString()

        val observedAt =
            OffsetDateTime.now()
                .toString()

        val values =
            ContentValues().apply {
                put(
                    "observation_id",
                    observationId
                )

                put(
                    "observed_at",
                    observedAt
                )

                putOptionalDouble(
                    "body_weight_kg",
                    draft.bodyWeightKg
                )

                putOptionalDouble(
                    "neck_cm",
                    draft.neckCm
                )

                putOptionalDouble(
                    "shoulders_cm",
                    draft.shouldersCm
                )

                putOptionalDouble(
                    "chest_cm",
                    draft.chestCm
                )

                putOptionalDouble(
                    "waist_cm",
                    draft.waistCm
                )

                putOptionalDouble(
                    "hips_cm",
                    draft.hipsCm
                )

                putOptionalDouble(
                    "left_arm_cm",
                    draft.leftArmCm
                )

                putOptionalDouble(
                    "right_arm_cm",
                    draft.rightArmCm
                )

                putOptionalDouble(
                    "left_forearm_cm",
                    draft.leftForearmCm
                )

                putOptionalDouble(
                    "right_forearm_cm",
                    draft.rightForearmCm
                )

                putOptionalDouble(
                    "left_thigh_cm",
                    draft.leftThighCm
                )

                putOptionalDouble(
                    "right_thigh_cm",
                    draft.rightThighCm
                )

                putOptionalDouble(
                    "left_calf_cm",
                    draft.leftCalfCm
                )

                putOptionalDouble(
                    "right_calf_cm",
                    draft.rightCalfCm
                )
            }

        return try {
            database.writableDatabase
                .insertOrThrow(
                    "body_observations",
                    null,
                    values
                )

            SaveBodyObservationResult.Saved(
                observationId
            )
        } catch (
            error: Exception
        ) {
            SaveBodyObservationResult.DatabaseError
        }
    }

    fun listBodyObservations(
        limit: Int = 20,
    ): List<BodyObservationSummary> {
        if (limit <= 0) {
            return emptyList()
        }

        val output =
            mutableListOf<
                BodyObservationSummary
            >()

        database.readableDatabase.rawQuery(
            """
            SELECT
                observation_id,
                observed_at,
                body_weight_kg,
                (
                    (body_weight_kg IS NOT NULL) +
                    (neck_cm IS NOT NULL) +
                    (shoulders_cm IS NOT NULL) +
                    (chest_cm IS NOT NULL) +
                    (waist_cm IS NOT NULL) +
                    (hips_cm IS NOT NULL) +
                    (left_arm_cm IS NOT NULL) +
                    (right_arm_cm IS NOT NULL) +
                    (left_forearm_cm IS NOT NULL) +
                    (right_forearm_cm IS NOT NULL) +
                    (left_thigh_cm IS NOT NULL) +
                    (right_thigh_cm IS NOT NULL) +
                    (left_calf_cm IS NOT NULL) +
                    (right_calf_cm IS NOT NULL)
                ) AS metric_count
            FROM body_observations
            ORDER BY observed_at DESC, id DESC
            LIMIT ?;
            """.trimIndent(),
            arrayOf(
                limit.toString()
            ),
        ).use { cursor ->
            while (
                cursor.moveToNext()
            ) {
                output +=
                    BodyObservationSummary(
                        observationId =
                            cursor.getString(0),
                        observedAt =
                            cursor.getString(1),
                        bodyWeightKg =
                            if (
                                cursor.isNull(2)
                            ) {
                                null
                            } else {
                                cursor.getDouble(2)
                            },
                        metricCount =
                            cursor.getInt(3),
                    )
            }
        }

        return output
    }

    fun getSessionDetail(
        sessionId: String,
    ): SessionDetail? {
        val db =
            database.readableDatabase

        val summary =
            db.rawQuery(
                """
                SELECT
                    s.session_id,
                    s.started_at,
                    s.session_type,
                    COUNT(se.id)
                FROM sessions AS s
                LEFT JOIN session_exercises AS se
                    ON se.session_row_id = s.id
                WHERE s.session_id = ?
                GROUP BY s.id;
                """.trimIndent(),
                arrayOf(sessionId),
            ).use { cursor ->
                if (!cursor.moveToFirst()) {
                    null
                } else {
                    SessionSummary(
                        sessionId =
                            cursor.getString(0),
                        startedAt =
                            cursor.getString(1),
                        exerciseCount =
                            cursor.getInt(3),
                        sessionType =
                            SessionType.fromWire(
                                cursor.getString(2)
                            ),
                    )
                }
            } ?: return null

        val exercises =
            mutableListOf<
                SessionExerciseDetail
            >()

        db.rawQuery(
            """
            SELECT
                se.id,
                se.entry_id,
                e.exercise_id,
                e.name,
                se.recording_mode,
                se.tracking_mode,
                se.data_fields,
                eq.equipment_id,
                eq.display_name,
                se.load_mode,
                se.rest_seconds,
                se.target_sets,
                se.target_reps,
                se.target_duration_seconds,
                se.target_weight_kg,
                mr.max_weight_kg
            FROM session_exercises AS se
            JOIN sessions AS s
                ON s.id = se.session_row_id
            JOIN exercises AS e
                ON e.id = se.exercise_row_id
            LEFT JOIN equipment AS eq
                ON eq.id = se.equipment_row_id
            LEFT JOIN max_results AS mr
                ON mr.session_exercise_row_id = se.id
            WHERE s.session_id = ?
            ORDER BY se.position ASC;
            """.trimIndent(),
            arrayOf(sessionId),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val sessionExerciseRowId =
                    cursor.getLong(0)

                val entryId = cursor.getString(1)
                val exerciseId = cursor.getString(2)
                val name = cursor.getString(3)

                val recording =
                    when (
                        cursor.getString(4)
                    ) {
                        "continuous" ->
                            RecordingMode.CONTINUOUS

                        else ->
                            RecordingMode.SETS
                    }

                val tracking =
                    when (
                        cursor.getString(5)
                    ) {
                        "duration" ->
                            TrackingMode.DURATION

                        else ->
                            TrackingMode.REPS
                    }

                val dataFields =
                    cursor.getInt(6)
                val equipmentId = if (cursor.isNull(7)) null else cursor.getString(7)
                val equipmentDisplayName = if (cursor.isNull(8)) null else cursor.getString(8)
                val plan = readSessionPlan(cursor, 9)
                val maxWeightKg = if (cursor.isNull(15)) null else cursor.getDouble(15)

                if (maxWeightKg != null) {
                    exercises +=
                        SessionExerciseDetail(
                            entryId = entryId,
                            exerciseId = exerciseId,
                            exerciseName = name,
                            equipmentId = equipmentId,
                            equipmentDisplayName = equipmentDisplayName,
                            recordingMode = recording,
                            trackingMode = tracking,
                            dataFields = dataFields,
                            plan = plan,
                            maxWeightKg = maxWeightKg,
                        )
                } else if (
                    recording ==
                    RecordingMode.CONTINUOUS
                ) {
                    db.query(
                        "continuous_activity",
                        arrayOf(
                            "duration_seconds",
                            "speed_kmh",
                            "distance_km",
                        ),
                        "session_exercise_row_id = ?",
                        arrayOf(
                            sessionExerciseRowId
                                .toString()
                        ),
                        null,
                        null,
                        null,
                    ).use {
                            continuous ->

                        if (
                            !continuous
                                .moveToFirst()
                        ) {
                            error(
                                "Missing continuous activity"
                            )
                        }

                        exercises +=
                            SessionExerciseDetail(
                                entryId = entryId,
                                exerciseId = exerciseId,
                                exerciseName =
                                    name,
                                equipmentId = equipmentId,
                                equipmentDisplayName = equipmentDisplayName,
                                recordingMode =
                                    recording,
                                trackingMode =
                                    tracking,
                                dataFields =
                                    dataFields,
                                plan = plan,
                                continuousDurationSeconds =
                                    continuous
                                        .getInt(0),
                                speedKmh =
                                    if (
                                        continuous
                                            .isNull(1)
                                    ) {
                                        null
                                    } else {
                                        continuous
                                            .getDouble(1)
                                    },
                                distanceKm =
                                    if (
                                        continuous
                                            .isNull(2)
                                    ) {
                                        null
                                    } else {
                                        continuous
                                            .getDouble(2)
                                    },
                            )
                    }
                } else {
                    val sets =
                        mutableListOf<
                            SessionSetDraft
                        >()

                    db.query(
                        "performed_sets",
                        arrayOf(
                            "reps",
                            "duration_seconds",
                            "weight_kg",
                        ),
                        "session_exercise_row_id = ?",
                        arrayOf(
                            sessionExerciseRowId
                                .toString()
                        ),
                        null,
                        null,
                        "position ASC",
                    ).use { setCursor ->
                        while (
                            setCursor
                                .moveToNext()
                        ) {
                            sets +=
                                SessionSetDraft(
                                    reps =
                                        if (
                                            setCursor
                                                .isNull(0)
                                        ) {
                                            0
                                        } else {
                                            setCursor
                                                .getInt(0)
                                        },
                                    durationSeconds =
                                        if (
                                            setCursor
                                                .isNull(1)
                                        ) {
                                            0
                                        } else {
                                            setCursor
                                                .getInt(1)
                                        },
                                    weightKg = if (setCursor.isNull(2)) null else setCursor.getDouble(2),
                                )
                        }
                    }

                    exercises +=
                        SessionExerciseDetail(
                            entryId = entryId,
                            exerciseId = exerciseId,
                            exerciseName =
                                name,
                            equipmentId = equipmentId,
                            equipmentDisplayName = equipmentDisplayName,
                            recordingMode =
                                recording,
                            trackingMode =
                                tracking,
                            dataFields =
                                dataFields,
                            plan = plan,
                            sets = sets,
                        )
                }
            }
        }

        return SessionDetail(
            summary = summary,
            exercises = exercises,
        )
    }

    /**
     * Build one transient suggestion from current runtime identities and the
     * complete actual history. No draft/history row is written by this path.
     */
    fun generateSessionPreview(request: SessionGenerationRequest): SessionGenerationResult {
        if (request.referenceTime.isBlank() || request.durationMinutes !in sessionGenerationPolicy.customMinutes ||
            request.zoneId !in sessionGenerationPolicy.zoneExpansion ||
            request.goalId !in sessionGenerationPolicy.goals) {
            return SessionGenerationResult.Invalid("Demande de génération invalide.")
        }
        val db = database.readableDatabase
        val ownsTransaction = !db.inTransaction()
        return try {
            if (ownsTransaction) db.beginTransactionNonExclusive()
            val runtime = mutableMapOf<Pair<String, String>, Pair<String, String>>()
            val candidates = mutableListOf<GenerationCandidate>()
            db.rawQuery(
                "SELECT e.exercise_id,e.name,eq.equipment_id,eq.display_name,eq.load_semantics " +
                    "FROM exercises e CROSS JOIN equipment eq " +
                    "WHERE e.recording_mode='sets' AND e.tracking_mode='reps' ORDER BY e.exercise_id,eq.equipment_id;",
                null,
            ).use { cursor ->
                while (cursor.moveToNext()) {
                    val exerciseId = cursor.requiredText(0, "exercise_id")
                    val equipmentId = cursor.requiredText(2, "equipment_id")
                    if (request.availableEquipmentIds != null && equipmentId !in request.availableEquipmentIds) continue
                    val record = trainingKnowledge.getExerciseKnowledge(exerciseId) ?: continue
                    val interpretation = record.interpretation ?: continue
                    if (record.resolutionStatus != ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED ||
                        record.confidence !in setOf(KnowledgeConfidence.HIGH, KnowledgeConfidence.MODERATE) ||
                        equipmentId !in record.equipmentIds) continue
                    val equipment = trainingKnowledge.getEquipmentKnowledge(equipmentId) ?: continue
                    val runtimeSemantics = parseLoadSemantics(cursor.requiredText(4, "load_semantics"))
                    if (equipment.catalogLoadSemantics != null && equipment.catalogLoadSemantics != runtimeSemantics) continue
                    candidates += GenerationCandidate(
                        exerciseId, equipmentId, interpretation.primaryZoneId,
                        interpretation.secondaryZoneIds, interpretation.patternIds,
                        (record.sourceRefs + interpretation.sourceRefs + equipment.sourceRefs).distinct().sorted(),
                        record.confidence, equipment.catalogLoadSemantics,
                    )
                    runtime[exerciseId to equipmentId] =
                        cursor.requiredText(1, "exercise_name") to cursor.requiredText(3, "equipment_name")
                }
            }
            if (candidates.size > 64) {
                return SessionGenerationResult.Invalid("Trop de contextes compatibles pour une génération bornée.")
            }
            val engineRequest = GenerationRequest(
                request.zoneId, request.goalId, request.durationMinutes, request.referenceTime,
                candidates, request.preferredExerciseIds, request.excludedExerciseIds,
                request.excludedPatternIds,
            )
            val suggestion = sessionGenerationEngine.generate(engineRequest) { visitor ->
                streamGenerationHistory(db, visitor)
            }
            val previewExercises = suggestion.exercises.map { generated ->
                val labels = checkNotNull(runtime[generated.exerciseId to generated.equipmentId]) {
                    "Contexte généré absent de la vue runtime"
                }
                val zoneName = checkNotNull(bodyZones.lookup(generated.primaryZoneId)) {
                    "Zone générée inconnue"
                }.displayName
                SessionGenerationPreviewExercise(
                    generated.exerciseId, labels.first, generated.equipmentId, labels.second,
                    generated.primaryZoneId, zoneName, generated.patternIds,
                    generated.patternIds.map { pattern ->
                        checkNotNull(trainingKnowledge.getMovementPattern(pattern)).displayNameFr
                    },
                    SessionExercisePlan(
                        generated.targetSets, reps = generated.targetRepetitions,
                        weightKg = generated.targetWeightKg,
                        loadMode = generated.plannedLoadMode, restSeconds = generated.restSeconds,
                    ),
                    generated.estimatedSeconds, generated.recency, generated.rationaleCodes,
                    generated.loadSourceSessionId, generated.loadSourceOccurrenceId,
                    generated.loadSourceStartedAt,
                )
            }
            if (ownsTransaction) db.setTransactionSuccessful()
            SessionGenerationResult.Generated(SessionGenerationPreview(
                request, previewExercises, suggestion.estimatedDurationSeconds,
                suggestion.insufficientResolvedCandidates, suggestion.exposure, suggestion.shortageCodes,
            ))
        } catch (error: IllegalArgumentException) {
            SessionGenerationResult.Invalid(error.message ?: "Demande de génération invalide.")
        } catch (error: Exception) {
            SessionGenerationResult.DatabaseError(error.message ?: "Analyse de l'historique impossible.")
        } finally {
            if (ownsTransaction && db.inTransaction()) db.endTransaction()
        }
    }

    /** Atomically install a generated suggestion as the ordinary singleton draft. */
    fun acceptGeneratedSession(preview: SessionGenerationPreview): AcceptGeneratedSessionResult {
        if (preview.exercises.isEmpty()) return AcceptGeneratedSessionResult.Invalid("La proposition est vide.")
        val db = database.writableDatabase
        return try {
            db.beginTransaction()
            val exists = db.rawQuery("SELECT 1 FROM active_session_draft WHERE id=?;",
                arrayOf(ACTIVE_DRAFT_ID.toString())).use { it.moveToFirst() }
            if (exists) return AcceptGeneratedSessionResult.ExistingActiveDraft
            val exercises = preview.exercises.mapIndexed { index, item ->
                check(index <= 100000)
                val row = findExerciseRow(db, "exercise_id=?", arrayOf(item.exerciseId))
                    ?: return AcceptGeneratedSessionResult.Invalid("Exercice généré introuvable.")
                if (row.recordingMode != RecordingMode.SETS || row.trackingMode != TrackingMode.REPS || row.dataFields != 0)
                    return AcceptGeneratedSessionResult.Invalid("Profil généré devenu incompatible.")
                val runtimeEquipment = readRuntimeEquipment(db, item.equipmentId)
                    ?: return AcceptGeneratedSessionResult.Invalid("Équipement généré introuvable.")
                val knowledge = trainingKnowledge.getExerciseKnowledge(item.exerciseId)
                val equipmentKnowledge = trainingKnowledge.getEquipmentKnowledge(item.equipmentId)
                if (knowledge == null || item.equipmentId !in knowledge.equipmentIds ||
                    equipmentKnowledge == null || equipmentKnowledge.catalogLoadSemantics != runtimeEquipment.second)
                    return AcceptGeneratedSessionResult.Invalid("Contexte scientifique généré devenu incompatible.")
                val profile = readExerciseProfileExact(db, item.exerciseId)
                    ?: return AcceptGeneratedSessionResult.Invalid("Exercice généré introuvable.")
                val draft = SessionExerciseDraft(
                    entryId = "sxe_" + UUID.randomUUID(), exercise = profile,
                    equipmentId = item.equipmentId, plan = item.plan, sets = emptyList(),
                )
                if (!validateSessionExercise(draft, SessionType.TRAINING, allowTargetOnly = true))
                    return AcceptGeneratedSessionResult.Invalid("Cible générée invalide.")
                check(runtimeEquipment.first > 0)
                draft
            }
            persistActiveSessionDraft(db, ActiveSessionDraft(exercises = exercises))
            db.setTransactionSuccessful()
            AcceptGeneratedSessionResult.Accepted
        } catch (error: Exception) {
            AcceptGeneratedSessionResult.DatabaseError(error.message ?: "Acceptation de la proposition impossible.")
        } finally {
            if (db.inTransaction()) db.endTransaction()
        }
    }

    fun editGeneratedDose(
        preview: SessionGenerationPreview,
        index: Int,
        targetSets: Int,
        targetRepetitions: Int,
        restSeconds: Int,
        manualWeightKg: Double?,
        loadChoice: GeneratorLoadChoice = GeneratorLoadChoice.AUTOMATIC,
        maxPercent: Int? = null,
    ): SessionGenerationResult {
        val current = preview.exercises.getOrNull(index)
            ?: return SessionGenerationResult.Invalid("Exercice de proposition introuvable.")
        if (targetSets !in 1..MAX_PLAN_SETS || targetRepetitions !in 1..MAX_PLAN_REPS ||
            restSeconds !in 0..MAX_PLAN_REST_SECONDS ||
            (manualWeightKg != null && (!manualWeightKg.isFinite() || manualWeightKg <= 0.0)) ||
            (loadChoice == GeneratorLoadChoice.PERCENT_MAX &&
                (maxPercent == null || maxPercent !in 1..100)))
            return SessionGenerationResult.Invalid("Dose cible invalide.")
        val db = database.readableDatabase
        val ownsTransaction = !db.inTransaction()
        return try {
            if (ownsTransaction) db.beginTransactionNonExclusive()
            val candidate = generationCandidate(db, current.exerciseId, current.equipmentId)
                ?: return SessionGenerationResult.Invalid("Contexte généré devenu incompatible.")
            val qualified = sessionGenerationEngine.requalifyDose(
                candidate, preview.request.referenceTime, targetSets, targetRepetitions, restSeconds,
            ) { visitor -> streamGenerationHistory(db, visitor) }
            val semantics = candidate.equipmentLoadSemantics
            val percentageTarget = if (loadChoice == GeneratorLoadChoice.PERCENT_MAX) {
                readLatestExplicitMax(db, current.exerciseId, current.equipmentId)?.let { maximum ->
                    PercentMaxCalculator.calculate(maximum, current.equipmentId, checkNotNull(maxPercent))
                }
            } else null
            val plan = if (loadChoice == GeneratorLoadChoice.NONE) {
                SessionExercisePlan(targetSets, reps = targetRepetitions,
                    weightKg = null, loadMode = SessionLoadMode.NONE, restSeconds = restSeconds)
            } else if (loadChoice == GeneratorLoadChoice.PERCENT_MAX) {
                SessionExercisePlan(targetSets, reps = targetRepetitions,
                    weightKg = percentageTarget,
                    loadMode = if (percentageTarget == null) SessionLoadMode.NONE else SessionLoadMode.EXTERNAL,
                    restSeconds = restSeconds)
            } else if (manualWeightKg == null) {
                SessionExercisePlan(qualified.targetSets, reps = qualified.targetRepetitions,
                    weightKg = qualified.targetWeightKg, loadMode = qualified.plannedLoadMode,
                    restSeconds = qualified.restSeconds)
            } else {
                val mode = when (semantics) {
                    EquipmentLoadSemantics.ASSISTANCE -> SessionLoadMode.ASSISTANCE
                    EquipmentLoadSemantics.EXTERNAL -> SessionLoadMode.EXTERNAL
                    else -> return SessionGenerationResult.Invalid("Cet équipement ne porte pas de charge cible manuelle.")
                }
                SessionExercisePlan(targetSets, reps = targetRepetitions,
                    weightKg = manualWeightKg, loadMode = mode, restSeconds = restSeconds)
            }
            val changed = current.copy(
                plan = plan,
                estimatedSeconds = sessionGenerationEngine.estimateExerciseSeconds(
                    targetSets, targetRepetitions, restSeconds),
                rationaleCodes = when (loadChoice) {
                    GeneratorLoadChoice.PERCENT_MAX -> listOf(if (percentageTarget != null)
                        "user_selected_max_percentage" else "compatible_max_unavailable")
                    GeneratorLoadChoice.NONE -> listOf("numeric_load_absent")
                    GeneratorLoadChoice.AUTOMATIC -> if (manualWeightKg == null)
                        listOf(qualified.rationaleCode) else listOf("manual_target_load")
                },
                loadSourceSessionId = if (loadChoice == GeneratorLoadChoice.AUTOMATIC && manualWeightKg == null) qualified.sourceSessionId else null,
                loadSourceOccurrenceId = if (loadChoice == GeneratorLoadChoice.AUTOMATIC && manualWeightKg == null) qualified.sourceOccurrenceId else null,
                loadSourceStartedAt = if (loadChoice == GeneratorLoadChoice.AUTOMATIC && manualWeightKg == null) qualified.sourceStartedAt else null,
            )
            val exercises = preview.exercises.toMutableList().also { it[index] = changed }
            val total = Math.addExact(sessionGenerationPolicy.preparationSeconds,
                exercises.fold(0) { sum, exercise -> Math.addExact(sum, exercise.estimatedSeconds) })
            if (ownsTransaction) db.setTransactionSuccessful()
            SessionGenerationResult.Generated(preview.copy(exercises = exercises, estimatedDurationSeconds = total))
        } catch (error: IllegalArgumentException) {
            SessionGenerationResult.Invalid(error.message ?: "Dose cible invalide.")
        } catch (error: Exception) {
            SessionGenerationResult.DatabaseError(error.message ?: "Réévaluation de charge impossible.")
        } finally {
            if (ownsTransaction && db.inTransaction()) db.endTransaction()
        }
    }

    private fun generationCandidate(
        db: SQLiteDatabase,
        exerciseId: String,
        equipmentId: String,
    ): GenerationCandidate? {
        val record = trainingKnowledge.getExerciseKnowledge(exerciseId) ?: return null
        val interpretation = record.interpretation ?: return null
        val equipment = trainingKnowledge.getEquipmentKnowledge(equipmentId) ?: return null
        val runtimeEquipment = readRuntimeEquipment(db, equipmentId) ?: return null
        if (record.resolutionStatus != ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED ||
            record.confidence !in setOf(KnowledgeConfidence.HIGH, KnowledgeConfidence.MODERATE) ||
            equipmentId !in record.equipmentIds ||
            equipment.catalogLoadSemantics != runtimeEquipment.second) return null
        return GenerationCandidate(exerciseId, equipmentId, interpretation.primaryZoneId,
            interpretation.secondaryZoneIds, interpretation.patternIds,
            (record.sourceRefs + interpretation.sourceRefs + equipment.sourceRefs).distinct().sorted(),
            record.confidence, equipment.catalogLoadSemantics)
    }

    private fun readRuntimeEquipment(
        db: SQLiteDatabase,
        equipmentId: String,
    ): Pair<Long, EquipmentLoadSemantics>? = db.rawQuery(
        "SELECT id,load_semantics FROM equipment WHERE equipment_id=?;",
        arrayOf(equipmentId),
    ).use { cursor ->
        if (!cursor.moveToFirst()) null else cursor.getLong(0) to
            parseLoadSemantics(cursor.requiredText(1, "load_semantics"))
    }

    private fun streamGenerationHistory(db: SQLiteDatabase, visitor: (GenerationHistoryRow) -> Unit) {
        db.rawQuery(
            "SELECT s.session_id,se.entry_id,e.exercise_id,s.started_at,eq.equipment_id," +
                "se.recording_mode,se.tracking_mode,se.load_mode,se.rest_seconds," +
                "se.target_sets,se.target_reps,se.target_duration_seconds,se.target_weight_kg," +
                "ps.position,ps.reps,ps.weight_kg,CASE WHEN mr.session_exercise_row_id IS NULL THEN 0 ELSE 1 END " +
                "FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id " +
                "JOIN exercises e ON e.id=se.exercise_row_id " +
                "LEFT JOIN equipment eq ON eq.id=se.equipment_row_id " +
                "LEFT JOIN performed_sets ps ON ps.session_exercise_row_id=se.id " +
                "LEFT JOIN max_results mr ON mr.session_exercise_row_id=se.id " +
                "ORDER BY s.id,se.position,ps.position;", null,
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val recording = parseRecordingMode(cursor.requiredText(5, "recording_mode"))
                val tracking = parseTrackingMode(cursor.requiredText(6, "tracking_mode"))
                val loadMode = SessionLoadMode.fromWire(cursor.requiredText(7, "load_mode"))
                val rest = checkedBoundedInt(cursor.getLong(8), 0, MAX_PLAN_REST_SECONDS, "rest_seconds")
                val targetSets = if (cursor.isNull(9)) null else
                    checkedBoundedInt(cursor.getLong(9), 1, MAX_PLAN_SETS, "target_sets")
                val targetReps = if (cursor.isNull(10)) null else
                    checkedBoundedInt(cursor.getLong(10), 1, MAX_PLAN_REPS, "target_reps")
                val targetDuration = if (cursor.isNull(11)) null else
                    checkedBoundedInt(cursor.getLong(11), 1, MAX_PLAN_DURATION_SECONDS, "target_duration_seconds")
                val targetWeight = if (cursor.isNull(12)) null else cursor.getDouble(12).also {
                    check(it.isFinite() && it > 0.0) { "target_weight_kg corrompu" }
                }
                val hasExplicitMax = cursor.getInt(16) != 0
                val hasTarget = targetSets != null || targetReps != null || targetDuration != null || targetWeight != null
                if (!hasTarget) {
                    check(loadMode == SessionLoadMode.NONE && rest == 0) { "plan absent incohérent" }
                } else {
                    check(recording == RecordingMode.SETS && !hasExplicitMax) { "cible interdite sur ce passage" }
                    check(targetSets != null && ((targetReps != null) xor (targetDuration != null))) { "forme de cible corrompue" }
                    check((tracking == TrackingMode.REPS) == (targetReps != null)) { "métrique de cible corrompue" }
                    check(if (targetWeight == null) loadMode == SessionLoadMode.NONE
                        else loadMode == SessionLoadMode.EXTERNAL || loadMode == SessionLoadMode.ASSISTANCE) {
                        "mode de charge cible incohérent"
                    }
                }
                val setPosition = if (cursor.isNull(13)) null else
                    checkedBoundedInt(cursor.getLong(13), 0, 100000, "set_position")
                val repetitions = if (cursor.isNull(14)) null else
                    checkedBoundedInt(cursor.getLong(14), 0, MAX_PLAN_REPS, "reps")
                visitor(GenerationHistoryRow(
                    cursor.requiredText(0, "session_id"), cursor.requiredText(1, "entry_id"),
                    cursor.requiredText(2, "exercise_id"), cursor.requiredText(3, "started_at"),
                    cursor.optionalText(4), recording, tracking,
                    loadMode, rest, hasTarget, setPosition, repetitions,
                    if (cursor.isNull(15)) null else cursor.finiteNonNegativeDouble(15, "weight_kg"),
                    hasExplicitMax,
                ))
            }
        }
    }

    /**
     * Compose immutable scientific metadata with exact persisted runtime state.
     * WHY: names are editable labels, so every join and lookup stays on stable
     * exercise_id; the transaction gives all components one SQLite read view.
     */
    fun getTrainingExerciseContext(
        exerciseId: String,
        occurrenceLimit: Int = 8,
        setPreviewLimit: Int = 8,
    ): TrainingExerciseContext? {
        require(occurrenceLimit in 1..MAX_OCCURRENCE_PAGE_SIZE) { "occurrenceLimit doit être compris entre 1 et $MAX_OCCURRENCE_PAGE_SIZE" }
        require(setPreviewLimit in 1..MAX_SET_PAGE_SIZE) { "setPreviewLimit doit être compris entre 1 et $MAX_SET_PAGE_SIZE" }
        val db = database.readableDatabase
        val ownsTransaction = !db.inTransaction()
        if (ownsTransaction) db.beginTransactionNonExclusive()
        return try {
            // INVARIANT: retired creator IDs canonicalize once in this read
            // snapshot, so profile, scientific metadata, relations, and actual
            // history all describe the same exercise without alias duplicates.
            val canonicalExerciseId = resolveExerciseId(db, exerciseId)
            val exercise = readExerciseProfileExact(db, canonicalExerciseId) ?: return null
            val direct = buildList {
                exercise.primaryZoneId?.let(::add)
                addAll(exercise.secondaryZoneIds)
            }
            val expanded = linkedSetOf<String>()
            direct.forEach { zoneId ->
                expanded += zoneId
                expanded += bodyZones.ancestors(zoneId).map { it.zoneId }
            }
            val knowledge = trainingKnowledge.getExerciseKnowledge(canonicalExerciseId)
            val compatible = knowledge?.let { trainingKnowledge.listEquipmentForExercise(canonicalExerciseId) }.orEmpty()
            val result = TrainingExerciseContext(
                exercise = exercise,
                persistedDirectZoneIds = direct,
                persistedZoneIdsWithAncestors = bodyZones.zones.map { it.zoneId }.filter { it in expanded },
                knowledge = knowledge,
                compatibleEquipment = compatible,
                latestExplicitMax = readLatestExplicitMax(db, canonicalExerciseId),
                recentPerformance = readExerciseOccurrencePage(db, canonicalExerciseId, occurrenceLimit, null, setPreviewLimit),
            )
            if (ownsTransaction) db.setTransactionSuccessful()
            result
        } finally {
            if (ownsTransaction) db.endTransaction()
        }
    }

    /**
     * Read-only manual target calculation; it never creates or updates a draft.
     * WHY: assistance and similar names cannot establish external resistance.
     * CONTRACT: exact stable exercise/equipment IDs and the latest chronological
     * explicit MAX in that context are required.
     */
    fun calculateManualPercentMaxTarget(
        exerciseId: String,
        equipmentId: String?,
        percent: Int,
    ): ManualPercentMaxResult {
        if (percent !in 1..100)
            return ManualPercentMaxResult.Unavailable("Le pourcentage doit être compris entre 1 et 100.")
        if (equipmentId.isNullOrBlank())
            return ManualPercentMaxResult.Unavailable(
                "MAX compatible indisponible : choisissez un équipement à résistance externe.")
        val db = database.readableDatabase
        val ownsTransaction = !db.inTransaction()
        return try {
            if (ownsTransaction) db.beginTransactionNonExclusive()
            if (readExerciseProfileExact(db, exerciseId) == null)
                return ManualPercentMaxResult.Unavailable("Exercice introuvable.")
            val runtimeEquipment = readRuntimeEquipment(db, equipmentId)
            if (runtimeEquipment?.second == EquipmentLoadSemantics.ASSISTANCE)
                return ManualPercentMaxResult.Unavailable(
                    "Le %MAX est indisponible pour une assistance ; choisissez une résistance externe.")
            if (runtimeEquipment?.second != EquipmentLoadSemantics.EXTERNAL)
                return ManualPercentMaxResult.Unavailable(
                    "Le %MAX est indisponible sans équipement connu à résistance externe.")
            val maximum = readLatestExplicitMax(db, exerciseId, equipmentId)
                ?: return ManualPercentMaxResult.Unavailable(
                    "MAX compatible indisponible pour cet exercice et cet équipement exacts.")
            val target = PercentMaxCalculator.calculate(maximum, equipmentId, percent)
                ?: return ManualPercentMaxResult.Unavailable(
                    "MAX compatible indisponible pour cet exercice et cet équipement exacts.")
            if (ownsTransaction) db.setTransactionSuccessful()
            /* INVARIANT: the repository returns arithmetic context only. The UI
             * persists targetWeightKg solely when the user confirms its form. */
            ManualPercentMaxResult.Available(maximum.maxWeightKg, maximum.startedAt, target)
        } catch (error: Exception) {
            ManualPercentMaxResult.Unavailable(error.message ?: "Lecture du MAX impossible.")
        } finally {
            if (ownsTransaction && db.inTransaction()) db.endTransaction()
        }
    }

    /** Deterministic keyset page over current data; pages do not hold a cross-call snapshot. */
    fun listExerciseOccurrences(
        exerciseId: String,
        limit: Int,
        after: ExerciseOccurrenceCursor? = null,
        setPreviewLimit: Int = 8,
    ): ExerciseOccurrencePage {
        require(limit in 1..MAX_OCCURRENCE_PAGE_SIZE) { "limit doit être compris entre 1 et $MAX_OCCURRENCE_PAGE_SIZE" }
        require(setPreviewLimit in 1..MAX_SET_PAGE_SIZE) { "setPreviewLimit doit être compris entre 1 et $MAX_SET_PAGE_SIZE" }
        validateOccurrenceCursor(after)
        val db = database.readableDatabase
        val ownsTransaction = !db.inTransaction()
        if (ownsTransaction) db.beginTransactionNonExclusive()
        return try {
            require(readExerciseProfileExact(db, exerciseId) != null) { "exercise_id inconnu: $exerciseId" }
            val result = readExerciseOccurrencePage(db, exerciseId, limit, after, setPreviewLimit)
            if (ownsTransaction) db.setTransactionSuccessful()
            result
        } finally {
            if (ownsTransaction) db.endTransaction()
        }
    }

    /** Follow-up bounded set page for one exact occurrence; no global set load exists. */
    fun listExerciseOccurrenceSets(
        exerciseId: String,
        entryId: String,
        limit: Int,
        afterPosition: Int? = null,
    ): ExerciseSetPage {
        require(exerciseId.isNotBlank() && entryId.isNotBlank()) { "identités vides" }
        require(limit in 1..MAX_SET_PAGE_SIZE) { "limit doit être compris entre 1 et $MAX_SET_PAGE_SIZE" }
        require(afterPosition == null || afterPosition >= 0) { "position de curseur invalide" }
        val db = database.readableDatabase
        val ownsTransaction = !db.inTransaction()
        if (ownsTransaction) db.beginTransactionNonExclusive()
        return try {
            val rowId = db.rawQuery(
                "SELECT se.id,se.recording_mode FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id WHERE e.exercise_id=? AND se.entry_id=?;",
                arrayOf(exerciseId, entryId),
            ).use { cursor ->
                require(cursor.moveToFirst()) { "occurrence inconnue pour cet exercice" }
                require(cursor.getString(1) == "sets") { "une activité continue ne possède pas de séries" }
                cursor.getLong(0)
            }
            val result = readSetPage(db, rowId, limit, afterPosition)
            if (ownsTransaction) db.setTransactionSuccessful()
            result
        } finally {
            if (ownsTransaction) db.endTransaction()
        }
    }

    private fun readExerciseProfileExact(db: SQLiteDatabase, exerciseId: String): ExerciseProfile? =
        db.rawQuery(
            "SELECT exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields FROM exercises WHERE exercise_id=?;",
            arrayOf(exerciseId),
        ).use { cursor ->
            if (!cursor.moveToFirst()) null else {
                val zones = readExerciseBodyZones(db, exerciseId)
                ExerciseProfile(
                    exerciseId = cursor.getString(0), name = cursor.getString(1), normalizedName = cursor.getString(2),
                    recordingMode = parseRecordingMode(cursor.getString(3)), trackingMode = parseTrackingMode(cursor.getString(4)),
                    dataFields = checkedNonNegativeInt(cursor.getLong(5), "data_fields"), primaryZoneId = zones.first,
                    secondaryZoneIds = zones.second,
                )
            }
        }

    private data class TemporalCandidate(
        val rowId: Long,
        val sessionId: String,
        val entryId: String,
        val startedAt: String,
        val timestamp: TrainlogTimestampKey,
    )

    private fun compareTemporal(left: TemporalCandidate, right: TemporalCandidate): Int =
        left.timestamp.compareTo(right.timestamp).takeIf { it != 0 }
            ?: TrainlogTimestamp.compareIds(left.sessionId, right.sessionId).takeIf { it != 0 }
            ?: TrainlogTimestamp.compareIds(left.entryId, right.entryId)

    private fun candidate(cursor: android.database.Cursor): TemporalCandidate {
        val startedAt = cursor.requiredText(3, "started_at")
        return TemporalCandidate(
            rowId = cursor.getLong(0),
            sessionId = cursor.requiredText(1, "session_id"),
            entryId = cursor.requiredText(2, "entry_id"),
            startedAt = startedAt,
            timestamp = checkNotNull(TrainlogTimestamp.parse(startedAt)) {
                "started_at persistant invalide pour l'occurrence"
            },
        )
    }

    private fun retainCandidate(rows: MutableList<TemporalCandidate>, value: TemporalCandidate, capacity: Int) {
        val position = rows.indexOfFirst { compareTemporal(value, it) > 0 }.let { if (it < 0) rows.size else it }
        if (position < capacity) {
            rows.add(position, value)
            if (rows.size > capacity) rows.removeAt(rows.lastIndex)
        }
    }

    private fun readLatestExplicitMax(db: SQLiteDatabase, exerciseId: String): ExplicitMaxContext? {
        val selected = mutableListOf<TemporalCandidate>()
        db.rawQuery(
            """SELECT se.id,s.session_id,se.entry_id,s.started_at FROM max_results mr
               JOIN session_exercises se ON se.id=mr.session_exercise_row_id
               JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id
               WHERE e.exercise_id=? AND s.session_type='max_test';""",
            arrayOf(exerciseId),
        ).use { cursor -> while (cursor.moveToNext()) retainCandidate(selected, candidate(cursor), 1) }
        val winner = selected.singleOrNull() ?: return null
        return db.rawQuery(
            """SELECT s.session_id,se.entry_id,s.started_at,mr.max_weight_kg,
                      eq.equipment_id,eq.display_name,eq.load_semantics
               FROM max_results mr JOIN session_exercises se ON se.id=mr.session_exercise_row_id
               JOIN sessions s ON s.id=se.session_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id
               WHERE se.id=?;""",
            arrayOf(winner.rowId.toString()),
        ).use { cursor ->
            check(cursor.moveToFirst()) { "résultat MAX sélectionné absent" }
            ExplicitMaxContext(
                sessionId = cursor.requiredText(0, "session_id"), entryId = cursor.requiredText(1, "entry_id"),
                startedAt = cursor.requiredText(2, "started_at"), maxWeightKg = cursor.finiteNonNegativeDouble(3, "max_weight_kg", strictlyPositive = true),
                equipmentId = cursor.optionalText(4), equipmentDisplayName = cursor.optionalText(5),
                loadSemantics = cursor.optionalText(6)?.let(::parseLoadSemantics),
            )
        }
    }

    /** Latest chronological explicit MAX for one exact external-load context. */
    private fun readLatestExplicitMax(
        db: SQLiteDatabase,
        exerciseId: String,
        equipmentId: String,
    ): ExplicitMaxContext? {
        val selected = mutableListOf<TemporalCandidate>()
        db.rawQuery(
            """SELECT se.id,s.session_id,se.entry_id,s.started_at FROM max_results mr
               JOIN session_exercises se ON se.id=mr.session_exercise_row_id
               JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id
               JOIN equipment eq ON eq.id=se.equipment_row_id
               WHERE e.exercise_id=? AND eq.equipment_id=? AND eq.load_semantics='external';""",
            arrayOf(exerciseId, equipmentId),
        ).use { cursor -> while (cursor.moveToNext()) retainCandidate(selected, candidate(cursor), 1) }
        val winner = selected.singleOrNull() ?: return null
        return db.rawQuery(
            """SELECT s.session_id,se.entry_id,s.started_at,mr.max_weight_kg,
                      eq.equipment_id,eq.display_name,eq.load_semantics
               FROM max_results mr JOIN session_exercises se ON se.id=mr.session_exercise_row_id
               JOIN sessions s ON s.id=se.session_row_id JOIN equipment eq ON eq.id=se.equipment_row_id
               WHERE se.id=?;""",
            arrayOf(winner.rowId.toString()),
        ).use { cursor ->
            check(cursor.moveToFirst()) { "résultat MAX compatible sélectionné absent" }
            ExplicitMaxContext(
                cursor.requiredText(0, "session_id"), cursor.requiredText(1, "entry_id"),
                cursor.requiredText(2, "started_at"),
                cursor.finiteNonNegativeDouble(3, "max_weight_kg", strictlyPositive = true),
                cursor.requiredText(4, "equipment_id"), cursor.requiredText(5, "display_name"),
                parseLoadSemantics(cursor.requiredText(6, "load_semantics")),
            )
        }
    }

    private fun readExerciseOccurrencePage(
        db: SQLiteDatabase, exerciseId: String, limit: Int, after: ExerciseOccurrenceCursor?, setPreviewLimit: Int,
    ): ExerciseOccurrencePage {
        val afterCandidate = after?.let {
            TemporalCandidate(-1L, it.sessionId, it.entryId, it.startedAt,
                requireNotNull(TrainlogTimestamp.parse(it.startedAt)))
        }
        val selected = mutableListOf<TemporalCandidate>()
        db.rawQuery(
            """
            SELECT se.id,s.session_id,se.entry_id,s.started_at
            FROM session_exercises se
            JOIN sessions s ON s.id=se.session_row_id
            JOIN exercises e ON e.id=se.exercise_row_id
            WHERE e.exercise_id=?;
            """.trimIndent(), arrayOf(exerciseId),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val value = candidate(cursor)
                // INVARIANT: selection and the exclusive cursor share compareTemporal.
                if (afterCandidate == null || compareTemporal(value, afterCandidate) < 0)
                    retainCandidate(selected, value, limit + 1)
            }
        }
        val hasMore = selected.size > limit
        val kept = if (hasMore) selected.take(limit) else selected
        val rows = kept.map { selectedRow ->
            db.rawQuery(
                """SELECT se.id,s.session_id,se.entry_id,s.started_at,s.session_type,
                          eq.equipment_id,eq.display_name,eq.load_semantics,
                          se.recording_mode,se.tracking_mode,se.data_fields,
                          ca.duration_seconds,ca.speed_kmh,ca.distance_km
                   FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id
                   LEFT JOIN equipment eq ON eq.id=se.equipment_row_id
                   LEFT JOIN continuous_activity ca ON ca.session_exercise_row_id=se.id
                   WHERE se.id=?;""",
                arrayOf(selectedRow.rowId.toString()),
            ).use { cursor ->
                check(cursor.moveToFirst()) { "occurrence sélectionnée absente" }
                val recording = parseRecordingMode(cursor.requiredText(8, "recording_mode"))
                val rowId = cursor.getLong(0)
                val context = ExerciseOccurrenceContext(
                    sessionId = cursor.requiredText(1, "session_id"), entryId = cursor.requiredText(2, "entry_id"),
                    startedAt = cursor.requiredText(3, "started_at"), sessionType = SessionType.fromWire(cursor.requiredText(4, "session_type")),
                    equipmentId = cursor.optionalText(5), equipmentDisplayName = cursor.optionalText(6),
                    loadSemantics = cursor.optionalText(7)?.let(::parseLoadSemantics), recordingMode = recording,
                    trackingMode = parseTrackingMode(cursor.requiredText(9, "tracking_mode")), dataFields = checkedNonNegativeInt(cursor.getLong(10), "data_fields"),
                    setPreview = if (recording == RecordingMode.SETS) readSetPage(db, rowId, setPreviewLimit, null) else null,
                    continuousDurationSeconds = if (recording == RecordingMode.CONTINUOUS) cursor.requiredPositiveInt(11, "duration_seconds") else null,
                    speedKmh = cursor.optionalFinitePositiveDouble(12, "speed_kmh"), distanceKm = cursor.optionalFinitePositiveDouble(13, "distance_km"),
                )
                if (recording == RecordingMode.SETS) check(cursor.isNull(11) && cursor.isNull(12) && cursor.isNull(13)) { "activité continue attachée à une occurrence SETS" }
                context
            }
        }
        val last = rows.lastOrNull()
        return ExerciseOccurrencePage(
            occurrences = rows,
            nextCursor = if (hasMore && last != null) ExerciseOccurrenceCursor(last.startedAt, last.sessionId, last.entryId) else null,
        )
    }

    private fun readSetPage(db: SQLiteDatabase, occurrenceRowId: Long, limit: Int, afterPosition: Int?): ExerciseSetPage {
        val selection = if (afterPosition == null) "session_exercise_row_id=?" else "session_exercise_row_id=? AND position>?"
        val args = if (afterPosition == null) arrayOf(occurrenceRowId.toString(), (limit + 1).toString()) else arrayOf(occurrenceRowId.toString(), afterPosition.toString(), (limit + 1).toString())
        val rows = mutableListOf<ExerciseSetContext>()
        db.rawQuery("SELECT position,reps,duration_seconds,weight_kg FROM performed_sets WHERE $selection ORDER BY position ASC LIMIT ?;", args).use { cursor ->
            while (cursor.moveToNext()) {
                val reps = if (cursor.isNull(1)) null else checkedNonNegativeInt(cursor.getLong(1), "reps")
                val duration = if (cursor.isNull(2)) null else cursor.requiredPositiveInt(2, "duration_seconds")
                check((reps == null) != (duration == null)) { "forme de série corrompue" }
                rows += ExerciseSetContext(checkedNonNegativeInt(cursor.getLong(0), "position"), reps, duration, if (cursor.isNull(3)) null else cursor.finiteNonNegativeDouble(3, "weight_kg"))
            }
        }
        val hasMore = rows.size > limit
        val kept = if (hasMore) rows.take(limit) else rows
        return ExerciseSetPage(kept, if (hasMore) kept.last().position else null)
    }

    private fun validateOccurrenceCursor(cursor: ExerciseOccurrenceCursor?) {
        if (cursor == null) return
        require(cursor.startedAt.isNotBlank() && cursor.sessionId.isNotBlank() && cursor.entryId.isNotBlank()) { "curseur d'occurrence invalide" }
        require(TrainlogTimestamp.parse(cursor.startedAt) != null) { "startedAt du curseur invalide" }
    }

    private fun parseRecordingMode(value: String): RecordingMode = when (value) {
        "sets" -> RecordingMode.SETS; "continuous" -> RecordingMode.CONTINUOUS; else -> error("recording_mode corrompu: $value")
    }
    private fun parseTrackingMode(value: String): TrackingMode = when (value) {
        "reps" -> TrackingMode.REPS; "duration" -> TrackingMode.DURATION; else -> error("tracking_mode corrompu: $value")
    }
    private fun parseLoadSemantics(value: String): EquipmentLoadSemantics = runCatching { EquipmentLoadSemantics.valueOf(value.uppercase(Locale.ROOT)) }.getOrElse { error("load_semantics corrompu: $value") }

    private fun android.database.Cursor.requiredText(index: Int, name: String): String = getString(index)?.takeIf { it.isNotBlank() } ?: error("$name absent")
    private fun android.database.Cursor.optionalText(index: Int): String? = if (isNull(index)) null else getString(index)
    private fun android.database.Cursor.requiredPositiveInt(index: Int, name: String): Int = checkedNonNegativeInt(getLong(index), name).also { check(it > 0) { "$name doit être positif" } }
    private fun android.database.Cursor.finiteNonNegativeDouble(index: Int, name: String, strictlyPositive: Boolean = false): Double = getDouble(index).also { check(it.isFinite() && if (strictlyPositive) it > 0.0 else it >= 0.0) { "$name invalide" } }
    private fun android.database.Cursor.optionalFinitePositiveDouble(index: Int, name: String): Double? = if (isNull(index)) null else finiteNonNegativeDouble(index, name, true)
    private fun checkedNonNegativeInt(value: Long, name: String): Int { check(value in 0..Int.MAX_VALUE.toLong()) { "$name hors plage" }; return value.toInt() }
    private fun checkedBoundedInt(value: Long, minimum: Int, maximum: Int, name: String): Int {
        check(value in minimum.toLong()..maximum.toLong()) { "$name hors plage" }
        return value.toInt()
    }

    private companion object {
        const val MAX_OCCURRENCE_PAGE_SIZE = 32
        const val MAX_SET_PAGE_SIZE = 64
    }

    /**
     * Return one newest explicit measured max per movement. Equipment is
     * presentation context only and never participates in max identity.
     */
    fun listLatestExerciseMaxima(): List<LatestExerciseMax> {
        data class AggregateCandidate(
            val temporal: TemporalCandidate,
            val result: LatestExerciseMax,
        )

        val selected = linkedMapOf<String, AggregateCandidate>()
        database.readableDatabase.rawQuery(
            """
            SELECT se.id, s.session_id, se.entry_id, s.started_at,
                   e.exercise_id, e.name, mr.max_weight_kg, eq.display_name
            FROM max_results AS mr
            JOIN session_exercises AS se ON se.id = mr.session_exercise_row_id
            JOIN sessions AS s ON s.id = se.session_row_id
            JOIN exercises AS e ON e.id = se.exercise_row_id
            LEFT JOIN equipment AS eq ON eq.id = se.equipment_row_id
            WHERE s.session_type = 'max_test'
            ORDER BY e.name COLLATE NOCASE, e.exercise_id;
            """.trimIndent(),
            null,
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val temporal = candidate(cursor)
                val exerciseId = cursor.requiredText(4, "exercise_id")
                val value = AggregateCandidate(
                    temporal = temporal,
                    result = LatestExerciseMax(
                        exerciseId = exerciseId,
                        exerciseName = cursor.requiredText(5, "exercise_name"),
                        maxWeightKg = cursor.finiteNonNegativeDouble(6, "max_weight_kg", true),
                        startedAt = temporal.startedAt,
                        equipmentDisplayName = cursor.optionalText(7),
                    ),
                )
                val current = selected[exerciseId]
                if (current == null || compareTemporal(temporal, current.temporal) > 0)
                    selected[exerciseId] = value
            }
        }
        return selected.values.map { it.result }
    }

    fun setCompletedSessionEquipment(
        sessionId: String,
        entryId: String,
        equipmentId: String?,
    ): Boolean {
        val db = database.writableDatabase
        val equipmentRowId = lookupEquipmentRowIdOrNull(db, equipmentId)
        if (equipmentId != null && equipmentRowId == null) return false
        return try {
            val values = ContentValues()
            if (equipmentRowId == null) values.putNull("equipment_row_id") else values.put("equipment_row_id", equipmentRowId)
            db.update(
                "session_exercises", values,
                "id = (SELECT se.id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id " +
                    "WHERE s.session_id=? AND se.entry_id=?)",
                arrayOf(sessionId, entryId),
            ) == 1
        } catch (_: Exception) { false }
    }

    private fun loadActiveSessionDraft(
        db: SQLiteDatabase,
    ): ActiveDraftRestore? {
        val header =
            db.rawQuery(
                """
                SELECT
                    d.session_type,
                    d.set_count_text,
                    d.reps_text,
                    d.duration_text,
                    d.speed_text,
                    d.distance_text,
                    d.updated_at,
                    d.selected_exercise_label,
                    d.selected_equipment_id,
                    d.weight_text,
                    d.max_weight_text,
                    d.source_session_id,
                    e.exercise_id,
                    e.name,
                    e.normalized_name,
                    e.recording_mode,
                    e.tracking_mode,
                    e.data_fields
                FROM active_session_draft AS d
                LEFT JOIN exercises AS e
                    ON e.id = d.selected_exercise_row_id
                WHERE d.id = ?;
                """.trimIndent(),
                arrayOf(ACTIVE_DRAFT_ID.toString()),
            ).use { cursor ->
                if (!cursor.moveToFirst()) {
                    null
                } else {
                    val missingSelection =
                        cursor.isNull(12) &&
                            !cursor.isNull(7)
                    val selected =
                        if (cursor.isNull(12)) {
                            null
                        } else {
                            exerciseProfileFromCursor(
                                cursor,
                                12,
                            )
                        }

                    ActiveDraftHeader(
                        sessionType =
                            SessionType.fromWire(
                                cursor.getString(0)
                            ),
                        form = SessionDraftForm(
                            selectedExercise = selected,
                            selectedEquipmentId = if (cursor.isNull(8)) null else cursor.getString(8),
                            weightText = cursor.getString(9),
                            maxWeightText = cursor.getString(10),
                            setCountText = cursor.getString(1),
                            repsText = cursor.getString(2),
                            durationText = cursor.getString(3),
                            speedText = cursor.getString(4),
                            distanceText = cursor.getString(5),
                        ),
                        sourceSessionId = if (cursor.isNull(11)) null else cursor.getString(11),
                        updatedAt =
                            cursor.getString(6),
                        warning =
                            if (missingSelection) {
                                "L'exercice en cours de saisie n'existe plus ; " +
                                    "seule la sélection a été annulée. " +
                                    "La saisie partielle et les exercices ajoutés " +
                                    "sont conservés."
                            } else {
                                null
                            },
                    )
                }
            } ?: return null

        val exercises =
            mutableListOf<SessionExerciseDraft>()

        db.rawQuery(
            """
            SELECT
                de.id,
                e.exercise_id,
                e.name,
                e.normalized_name,
                de.recording_mode,
                de.tracking_mode,
                de.data_fields,
                eq.equipment_id,
                de.entry_id,
                de.load_mode,
                de.rest_seconds,
                de.target_sets,
                de.target_reps,
                de.target_duration_seconds,
                de.target_weight_kg,
                mr.max_weight_kg
            FROM draft_session_exercises AS de
            JOIN exercises AS e
                ON e.id = de.exercise_row_id
            LEFT JOIN equipment AS eq
                ON eq.id = de.equipment_row_id
            LEFT JOIN draft_max_results AS mr
                ON mr.draft_exercise_row_id = de.id
            WHERE de.draft_id = ?
            ORDER BY de.position ASC;
            """.trimIndent(),
            arrayOf(ACTIVE_DRAFT_ID.toString()),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val rowId = cursor.getLong(0)
                val exercise =
                    ExerciseProfile(
                        exerciseId =
                            cursor.getString(1),
                        name = cursor.getString(2),
                        normalizedName =
                            cursor.getString(3),
                        recordingMode =
                            recordingModeFromWire(
                                cursor.getString(4)
                            ),
                        trackingMode =
                            trackingModeFromWire(
                                cursor.getString(5)
                            ),
                            dataFields =
                                cursor.getInt(6),
                    )
                val equipmentId = if (cursor.isNull(7)) null else cursor.getString(7)
                val entryId = cursor.getString(8)
                val plan = readSessionPlan(cursor, 9)
                val maxWeightKg = if (cursor.isNull(15)) null else cursor.getDouble(15)

                if (maxWeightKg != null) {
                    exercises +=
                        SessionExerciseDraft(
                            entryId = entryId,
                            exercise = exercise,
                            equipmentId = equipmentId,
                            maxWeightKg = maxWeightKg,
                            plan = plan,
                        )
                } else if (
                    exercise.recordingMode ==
                    RecordingMode.CONTINUOUS
                ) {
                    val continuous =
                        db.query(
                            "draft_continuous_activity",
                            arrayOf(
                                "duration_seconds",
                                "speed_kmh",
                                "distance_km",
                            ),
                            "draft_exercise_row_id = ?",
                            arrayOf(rowId.toString()),
                            null,
                            null,
                            null,
                        ).use { item ->
                            check(item.moveToFirst()) {
                                "Activité continue du brouillon manquante."
                            }

                            SessionExerciseDraft(
                                entryId = entryId,
                                exercise = exercise,
                                equipmentId = equipmentId,
                                plan = plan,
                                continuousDurationSeconds =
                                    item.getInt(0),
                                speedKmh =
                                    if (item.isNull(1)) {
                                        null
                                    } else {
                                        item.getDouble(1)
                                    },
                                distanceKm =
                                    if (item.isNull(2)) {
                                        null
                                    } else {
                                        item.getDouble(2)
                                    },
                            )
                        }
                    exercises += continuous
                } else {
                    val sets =
                        mutableListOf<SessionSetDraft>()
                    db.query(
                        "draft_performed_sets",
                        arrayOf(
                            "reps",
                            "duration_seconds",
                            "weight_kg",
                        ),
                        "draft_exercise_row_id = ?",
                        arrayOf(rowId.toString()),
                        null,
                        null,
                        "position ASC",
                    ).use { setCursor ->
                        while (setCursor.moveToNext()) {
                            sets +=
                                SessionSetDraft(
                                    reps =
                                        if (setCursor.isNull(0)) {
                                            0
                                        } else {
                                            setCursor.getInt(0)
                                        },
                                    durationSeconds =
                                        if (setCursor.isNull(1)) {
                                            0
                                        } else {
                                            setCursor.getInt(1)
                                        },
                                    weightKg = if (setCursor.isNull(2)) null else setCursor.getDouble(2),
                                )
                        }
                    }
                    exercises +=
                        SessionExerciseDraft(
                            entryId = entryId,
                            exercise = exercise,
                            equipmentId = equipmentId,
                            plan = plan,
                            sets = sets,
                        )
                }
            }
        }

        return ActiveDraftRestore(
            draft = ActiveSessionDraft(
                exercises = exercises,
                sessionType = header.sessionType,
                sourceSessionId = header.sourceSessionId,
                form = header.form,
                updatedAt = header.updatedAt,
            ),
            warning = header.warning,
        )
    }

    private fun persistActiveSessionDraft(
        db: SQLiteDatabase,
        draft: ActiveSessionDraft,
    ) {
        val selectedRowId =
            draft.form.selectedExercise
                ?.let {
                    lookupExerciseRowId(
                        db,
                        it.exerciseId,
                    )
                }
        val now = OffsetDateTime.now().toString()
        val values =
            ContentValues().apply {
                put("session_type", draft.sessionType.wireValue)
                putOptionalString("source_session_id", draft.sourceSessionId)
                putOptionalString("selected_equipment_id", draft.form.selectedEquipmentId)
                put("weight_text", draft.form.weightText)
                put("max_weight_text", draft.form.maxWeightText)
                if (selectedRowId == null) {
                    putNull("selected_exercise_row_id")
                    putNull("selected_exercise_label")
                } else {
                    put("selected_exercise_row_id", selectedRowId)
                    put(
                        "selected_exercise_label",
                        draft.form.selectedExercise.name,
                    )
                }
                put("set_count_text", draft.form.setCountText)
                put("reps_text", draft.form.repsText)
                put("duration_text", draft.form.durationText)
                put("speed_text", draft.form.speedText)
                put("distance_text", draft.form.distanceText)
                put("updated_at", now)
            }

        val updated =
            db.update(
                "active_session_draft",
                values,
                "id = ?",
                arrayOf(ACTIVE_DRAFT_ID.toString()),
            )

        if (updated == 0) {
            values.put("id", ACTIVE_DRAFT_ID)
            db.insertOrThrow(
                "active_session_draft",
                null,
                values,
            )
        }

        db.delete(
            "draft_session_exercises",
            "draft_id = ?",
            arrayOf(ACTIVE_DRAFT_ID.toString()),
        )

        draft.exercises.forEachIndexed {
                index,
                exerciseDraft ->
            val exerciseRowId =
                lookupExerciseRowId(
                    db,
                    exerciseDraft.exercise.exerciseId,
                )
            val exerciseValues =
                ContentValues().apply {
                    put("draft_id", ACTIVE_DRAFT_ID)
                    put("exercise_row_id", exerciseRowId)
                    put("position", index)
                    put(
                        "recording_mode",
                        exerciseDraft.exercise
                            .recordingMode.wireValue,
                    )
                    put(
                        "tracking_mode",
                        exerciseDraft.exercise
                            .trackingMode.wireValue,
                    )
                    put(
                        "data_fields",
                        exerciseDraft.exercise.dataFields,
                    )
                    put("entry_id", exerciseDraft.entryId)
                    putSessionPlan(exerciseDraft.plan)
                    val equipmentRowId = lookupEquipmentRowIdOrNull(
                        db,
                        exerciseDraft.equipmentId,
                    )
                    check(exerciseDraft.equipmentId == null || equipmentRowId != null) {
                        "Unknown equipment: ${exerciseDraft.equipmentId}"
                    }
                    if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                }
            val draftExerciseRowId =
                db.insertOrThrow(
                    "draft_session_exercises",
                    null,
                    exerciseValues,
                )

            if (exerciseDraft.maxWeightKg != null) {
                db.insertOrThrow(
                    "draft_max_results",
                    null,
                    ContentValues().apply {
                        put("draft_exercise_row_id", draftExerciseRowId)
                        put("max_weight_kg", exerciseDraft.maxWeightKg)
                    },
                )
            } else if (
                exerciseDraft.exercise.recordingMode ==
                RecordingMode.CONTINUOUS
            ) {
                val continuousValues =
                    ContentValues().apply {
                        put(
                            "draft_exercise_row_id",
                            draftExerciseRowId,
                        )
                        put(
                            "duration_seconds",
                            exerciseDraft.continuousDurationSeconds,
                        )
                        exerciseDraft.speedKmh?.let {
                            put("speed_kmh", it)
                        }
                        exerciseDraft.distanceKm?.let {
                            put("distance_km", it)
                        }
                    }
                db.insertOrThrow(
                    "draft_continuous_activity",
                    null,
                    continuousValues,
                )
            } else {
                exerciseDraft.sets.forEachIndexed {
                        setIndex,
                        set ->
                    val setValues =
                        ContentValues().apply {
                            put(
                                "draft_exercise_row_id",
                                draftExerciseRowId,
                            )
                            put("position", setIndex)
                            set.weightKg?.let { put("weight_kg", it) }
                            if (
                                exerciseDraft.exercise.trackingMode ==
                                TrackingMode.REPS
                            ) {
                                put("reps", set.reps)
                            } else {
                                put(
                                    "duration_seconds",
                                    set.durationSeconds,
                                )
                            }
                        }
                    db.insertOrThrow(
                        "draft_performed_sets",
                        null,
                        setValues,
                    )
                }
            }
        }
    }

    private fun insertCompletedSession(
        db: SQLiteDatabase,
        draft: SessionDraft,
        sourceSessionId: String? = null,
    ): String {
        val sessionId: String
        val sessionRowId: Long
        if (sourceSessionId == null) {
            sessionId = "se_" + UUID.randomUUID().toString()
            /* Preserve the existing Android meaning: started_at is assigned
             * when a new completed session is saved. */
            val sessionValues = ContentValues().apply {
                put("session_id", sessionId)
                put("started_at", OffsetDateTime.now().toString())
                put("session_type", draft.sessionType.wireValue)
            }
            sessionRowId = db.insertOrThrow("sessions", null, sessionValues)
        } else {
            check(draft.sessionType == SessionType.MAX_TEST) {
                "Seul un Test max peut remplacer une séance reprise."
            }
            sessionId = sourceSessionId
            sessionRowId = db.rawQuery(
                "SELECT id FROM sessions WHERE session_id=? AND session_type='max_test';",
                arrayOf(sourceSessionId),
            ).use { cursor ->
                check(cursor.moveToFirst()) { "Séance Test max source introuvable." }
                cursor.getLong(0)
            }
            check(resumedDraftIdentityIsSafe(db, sessionRowId, draft)) {
                "Une séance reprise ne peut supprimer, réordonner ou réaffecter ses entrées existantes."
            }
            /* INVARIANT: child replacement and draft deletion are in the
             * caller's transaction; failure restores the completed baseline. */
            db.delete(
                "session_exercises",
                "session_row_id=?",
                arrayOf(sessionRowId.toString()),
            )
        }

        draft.exercises.forEachIndexed {
                exerciseIndex,
                exerciseDraft ->
            val exerciseRowId =
                lookupExerciseRowId(
                    db,
                    exerciseDraft.exercise.exerciseId,
                )
            val exerciseValues =
                ContentValues().apply {
                    put("session_row_id", sessionRowId)
                    put("exercise_row_id", exerciseRowId)
                    put("position", exerciseIndex)
                    put(
                        "recording_mode",
                        exerciseDraft.exercise.recordingMode.wireValue,
                    )
                    put(
                        "tracking_mode",
                        exerciseDraft.exercise.trackingMode.wireValue,
                    )
                    put("data_fields", exerciseDraft.exercise.dataFields)
                    put("entry_id", exerciseDraft.entryId)
                    putSessionPlan(exerciseDraft.plan)
                    val equipmentRowId = lookupEquipmentRowIdOrNull(
                        db,
                        exerciseDraft.equipmentId,
                    )
                    check(exerciseDraft.equipmentId == null || equipmentRowId != null) {
                        "Unknown equipment: ${exerciseDraft.equipmentId}"
                    }
                    if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                }
            val sessionExerciseRowId =
                db.insertOrThrow(
                    "session_exercises",
                    null,
                    exerciseValues,
                )

            if (exerciseDraft.maxWeightKg != null) {
                db.insertOrThrow(
                    "max_results",
                    null,
                    ContentValues().apply {
                        put("session_exercise_row_id", sessionExerciseRowId)
                        put("max_weight_kg", exerciseDraft.maxWeightKg)
                    },
                )
            } else if (
                exerciseDraft.exercise.recordingMode ==
                RecordingMode.CONTINUOUS
            ) {
                val continuousValues =
                    ContentValues().apply {
                        put("session_exercise_row_id", sessionExerciseRowId)
                        put(
                            "duration_seconds",
                            exerciseDraft.continuousDurationSeconds,
                        )
                        exerciseDraft.speedKmh?.let { put("speed_kmh", it) }
                        exerciseDraft.distanceKm?.let { put("distance_km", it) }
                    }
                db.insertOrThrow(
                    "continuous_activity",
                    null,
                    continuousValues,
                )
            } else {
                exerciseDraft.sets.forEachIndexed {
                        setIndex,
                        set ->
                    val setValues =
                        ContentValues().apply {
                            put("session_exercise_row_id", sessionExerciseRowId)
                            put("position", setIndex)
                            set.weightKg?.let { put("weight_kg", it) }
                            if (
                                exerciseDraft.exercise.trackingMode ==
                                TrackingMode.REPS
                            ) {
                                put("reps", set.reps)
                            } else {
                                put("duration_seconds", set.durationSeconds)
                            }
                        }
                    db.insertOrThrow(
                        "performed_sets",
                        null,
                        setValues,
                    )
                }
            }
        }

        return sessionId
    }

    private fun resumedDraftIdentityIsSafe(
        db: SQLiteDatabase,
        sessionRowId: Long,
        draft: SessionDraft,
    ): Boolean {
        val current = mutableListOf<Pair<String, String>>()
        db.rawQuery(
            "SELECT se.entry_id,e.exercise_id FROM session_exercises se " +
                "JOIN exercises e ON e.id=se.exercise_row_id " +
                "WHERE se.session_row_id=? ORDER BY se.position;",
            arrayOf(sessionRowId.toString()),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                current += cursor.getString(0) to cursor.getString(1)
            }
        }
        if (draft.exercises.size < current.size) return false
        return current.indices.all { index ->
            draft.exercises[index].let { entry ->
                entry.entryId == current[index].first &&
                    entry.exercise.exerciseId == current[index].second
            }
        }
    }

    private fun exerciseProfileFromCursor(
        cursor: android.database.Cursor,
        offset: Int,
    ): ExerciseProfile =
        ExerciseProfile(
            exerciseId = cursor.getString(offset),
            name = cursor.getString(offset + 1),
            normalizedName = cursor.getString(offset + 2),
            recordingMode =
                recordingModeFromWire(
                    cursor.getString(offset + 3)
                ),
            trackingMode =
                trackingModeFromWire(
                    cursor.getString(offset + 4)
                ),
            dataFields = cursor.getInt(offset + 5),
        )

    private fun recordingModeFromWire(
        value: String,
    ): RecordingMode =
        if (value == "continuous") {
            RecordingMode.CONTINUOUS
        } else {
            RecordingMode.SETS
        }

    private fun trackingModeFromWire(
        value: String,
    ): TrackingMode =
        if (value == "duration") {
            TrackingMode.DURATION
        } else {
            TrackingMode.REPS
        }

    private data class ActiveDraftHeader(
        val sessionType: SessionType,
        val form: SessionDraftForm,
        val sourceSessionId: String?,
        val updatedAt: String,
        val warning: String?,
    )

    private data class ActiveDraftRestore(
        val draft: ActiveSessionDraft,
        val warning: String?,
    )

    private fun validateSessionExercise(
        draft: SessionExerciseDraft,
        sessionType: SessionType,
        allowTargetOnly: Boolean = false,
    ): Boolean {
        if (!validateSessionPlan(draft)) return false
        val maxWeight = draft.maxWeightKg
        if (maxWeight != null) {
            /* INVARIANT: max is a first-class result owned by the movement
             * occurrence. No set or continuous payload shadows it. */
            return sessionType == SessionType.MAX_TEST &&
                maxWeight.isFinite() &&
                maxWeight > 0.0 &&
                draft.sets.isEmpty() &&
                draft.continuousDurationSeconds == 0 &&
                draft.speedKmh == null &&
                draft.distanceKm == null &&
                draft.plan == null
        }

        return when (
            draft.exercise.recordingMode
        ) {
            RecordingMode.CONTINUOUS -> {
                if (
                    draft.continuousDurationSeconds <= 0 ||
                    draft.sets.isNotEmpty() || draft.plan != null
                ) {
                    false
                } else {
                    val wantsSpeed =
                        draft.exercise.dataFields and
                            1 != 0

                    val wantsDistance =
                        draft.exercise.dataFields and
                            2 != 0

                    (
                        (wantsSpeed ==
                            (draft.speedKmh != null)) &&
                            (
                                !wantsSpeed ||
                                    draft.speedKmh!! > 0.0
                            ) &&
                            (wantsDistance ==
                                (draft.distanceKm != null)) &&
                            (
                                !wantsDistance ||
                                    draft.distanceKm!! > 0.0
                            )
                    )
                }
            }

            RecordingMode.SETS -> {
                if (
                    (draft.sets.isEmpty() && !(allowTargetOnly && draft.plan != null)) ||
                    draft.continuousDurationSeconds != 0 ||
                    draft.speedKmh != null ||
                    draft.distanceKm != null
                ) {
                    false
                } else {
                    when (
                        draft.exercise.trackingMode
                    ) {
                        TrackingMode.REPS ->
                            draft.sets.all {
                                it.reps >= 0 &&
                                    it.durationSeconds == 0 &&
                                    (it.weightKg == null ||
                                        (it.weightKg.isFinite() && it.weightKg >= 0.0))
                            }

                        TrackingMode.DURATION ->
                            draft.sets.all {
                                it.durationSeconds > 0 &&
                                    it.reps == 0 &&
                                    (it.weightKg == null ||
                                        (it.weightKg.isFinite() && it.weightKg >= 0.0))
                            }
                    }
                }
            }
        }
    }

    private fun validateSessionPlan(draft: SessionExerciseDraft): Boolean {
        val plan = draft.plan ?: return true
        if (draft.exercise.recordingMode != RecordingMode.SETS ||
            plan.sets !in 1..MAX_PLAN_SETS ||
            plan.restSeconds !in 0..MAX_PLAN_REST_SECONDS ||
            (plan.weightKg != null && (!plan.weightKg.isFinite() || plan.weightKg <= 0.0))) return false
        val metricValid = when (draft.exercise.trackingMode) {
            TrackingMode.REPS -> plan.reps in 1..MAX_PLAN_REPS && plan.durationSeconds == null
            TrackingMode.DURATION -> plan.durationSeconds in 1..MAX_PLAN_DURATION_SECONDS && plan.reps == null
        }
        val modeValid = if (plan.weightKg == null) {
            plan.loadMode == SessionLoadMode.NONE
        } else {
            plan.loadMode == SessionLoadMode.EXTERNAL || plan.loadMode == SessionLoadMode.ASSISTANCE
        }
        return metricValid && modeValid
    }

    private fun lookupExerciseRowId(
        db: SQLiteDatabase,
        exerciseId: String,
    ): Long {
        db.query(
            "exercises",
            arrayOf("id"),
            "exercise_id = ?",
            arrayOf(exerciseId),
            null,
            null,
            null,
        ).use { cursor ->
            if (!cursor.moveToFirst()) {
                error(
                    "Exercise not found: $exerciseId"
                )
            }

            return cursor.getLong(0)
        }
    }

    private fun bodyZoneSelectionValid(
        primaryZoneId: String?,
        secondaryZoneIds: List<String>,
    ): Boolean {
        if (secondaryZoneIds.size != secondaryZoneIds.toSet().size ||
            primaryZoneId in secondaryZoneIds ||
            (primaryZoneId == null && secondaryZoneIds.isNotEmpty())) return false
        val ids = listOfNotNull(primaryZoneId) + secondaryZoneIds
        return ids.all { id ->
            val zone = bodyZones.lookup(id)
            zone != null && zone.kind != BodyZoneKind.GROUP
        }
    }

    /** INVARIANT: the first component is the sole primary relation; the
     * secondary list follows manifest sort order for deterministic UI/export. */
    private fun readExerciseBodyZones(
        db: SQLiteDatabase,
        exerciseId: String,
    ): Pair<String?, List<String>> {
        var primary: String? = null
        val secondary = mutableListOf<String>()
        db.rawQuery(
            "SELECT ebz.zone_id,ebz.role FROM exercise_body_zones ebz " +
                "JOIN exercises e ON e.id=ebz.exercise_row_id WHERE e.exercise_id=?;",
            arrayOf(exerciseId),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val zoneId = cursor.getString(0)
                val zone = checkNotNull(bodyZones.lookup(zoneId)) {
                    "zone_id SQLite inconnu: $zoneId"
                }
                check(zone.kind != BodyZoneKind.GROUP) {
                    "Relation SQLite vers un groupe dérivable: $zoneId"
                }
                when (cursor.getString(1)) {
                    "primary" -> {
                        check(primary == null) { "Plusieurs zones principales" }
                        primary = zoneId
                    }
                    "secondary" -> secondary += zoneId
                    else -> error("Rôle de zone SQLite inconnu")
                }
            }
        }
        check(primary != null || secondary.isEmpty()) {
            "Relations secondaires sans zone principale"
        }
        return primary to secondary.sortedBy { bodyZones.lookup(it)?.sortOrder ?: Int.MAX_VALUE }
    }

    private fun replaceExerciseBodyZones(
        db: SQLiteDatabase,
        exerciseId: String,
        primaryZoneId: String?,
        secondaryZoneIds: List<String>,
    ) {
        check(bodyZoneSelectionValid(primaryZoneId, secondaryZoneIds)) {
            "Sélection de zones corporelles invalide"
        }
        val rowId = lookupExerciseRowId(db, exerciseId)
        db.delete("exercise_body_zones", "exercise_row_id=?", arrayOf(rowId.toString()))
        fun insert(zoneId: String, role: String) {
            db.insertOrThrow("exercise_body_zones", null, ContentValues().apply {
                put("exercise_row_id", rowId)
                put("zone_id", zoneId)
                put("role", role)
            })
        }
        primaryZoneId?.let { insert(it, "primary") }
        secondaryZoneIds.forEach { insert(it, "secondary") }
    }

    private fun normalizeName(
        value: String,
    ): String {
        val decomposed =
            Normalizer.normalize(
                value.trim()
                    .lowercase(
                        Locale.ROOT
                    ),
                Normalizer.Form.NFD,
            )

        return decomposed
            .replace(
                Regex("\\p{Mn}+"),
                "",
            )
            .replace(
                Regex("\\s+"),
                " ",
            )
            .trim()
    }
}


private fun ContentValues.putOptionalDouble(
    key: String,
    value: Double?,
) {
    if (value == null) {
        putNull(key)
    } else {
        put(key, value)
    }
}

private fun ContentValues.putOptionalString(
    key: String,
    value: String?,
) {
    if (value == null) putNull(key) else put(key, value)
}

private fun ContentValues.putSessionPlan(plan: SessionExercisePlan?) {
    put("load_mode", plan?.loadMode?.wireValue ?: "none")
    put("rest_seconds", plan?.restSeconds ?: 0)
    if (plan == null) {
        putNull("target_sets")
        putNull("target_reps")
        putNull("target_duration_seconds")
        putNull("target_weight_kg")
    } else {
        put("target_sets", plan.sets)
        if (plan.reps == null) putNull("target_reps") else put("target_reps", plan.reps)
        if (plan.durationSeconds == null) putNull("target_duration_seconds")
        else put("target_duration_seconds", plan.durationSeconds)
        putOptionalDouble("target_weight_kg", plan.weightKg)
    }
}

private fun readSessionPlan(cursor: Cursor, start: Int): SessionExercisePlan? {
    val loadMode = SessionLoadMode.fromWire(cursor.getString(start))
    val restSeconds = cursor.getInt(start + 1)
    if (cursor.isNull(start + 2)) {
        check(loadMode == SessionLoadMode.NONE && restSeconds == 0 &&
            cursor.isNull(start + 3) && cursor.isNull(start + 4) && cursor.isNull(start + 5)) {
            "Métadonnées de plan cible incohérentes"
        }
        return null
    }
    return SessionExercisePlan(
        sets = cursor.getInt(start + 2),
        reps = if (cursor.isNull(start + 3)) null else cursor.getInt(start + 3),
        durationSeconds = if (cursor.isNull(start + 4)) null else cursor.getInt(start + 4),
        weightKg = if (cursor.isNull(start + 5)) null else cursor.getDouble(start + 5),
        loadMode = loadMode,
        restSeconds = restSeconds,
    )
}

private fun JSONObject.optDoubleOrNull(key: String): Double? =
    if (has(key) && !isNull(key)) getDouble(key) else null

private fun JSONObject.optIntOrNull(key: String): Int? =
    if (has(key) && !isNull(key)) getInt(key) else null

private fun jsonHasUniqueObjectKeys(json: String): Boolean = try {
    JsonReader(StringReader(json)).use { reader ->
        fun consumeValue() {
            when (reader.peek()) {
                JsonToken.BEGIN_OBJECT -> {
                    reader.beginObject()
                    val names = mutableSetOf<String>()
                    while (reader.hasNext()) {
                        check(names.add(reader.nextName())) { "champ JSON dupliqué" }
                        consumeValue()
                    }
                    reader.endObject()
                }
                JsonToken.BEGIN_ARRAY -> {
                    reader.beginArray()
                    while (reader.hasNext()) consumeValue()
                    reader.endArray()
                }
                JsonToken.STRING, JsonToken.NUMBER -> reader.nextString()
                JsonToken.BOOLEAN -> reader.nextBoolean()
                JsonToken.NULL -> reader.nextNull()
                else -> error("JSON incomplet")
            }
        }
        consumeValue()
        check(reader.peek() == JsonToken.END_DOCUMENT) { "JSON supplémentaire" }
    }
    true
} catch (_: Exception) {
    false
}

private fun equipmentAliasNormalize(value: String): String =
    Normalizer.normalize(value, Normalizer.Form.NFD)
        .replace("\\p{M}+".toRegex(), "")
        .lowercase(Locale.ROOT)
        .trim()

private const val ANDROID_DATABASE_NAME =
    "trainlog-android.db"
private const val ACTIVE_DRAFT_ID = 1
private const val MAX_DRAFT_FORM_TEXT_LENGTH = 4096
private const val MAX_PLAN_SETS = 64
private const val MAX_PLAN_REPS = 10000
private const val MAX_PLAN_DURATION_SECONDS = 86400
private const val MAX_PLAN_REST_SECONDS = 86400
private const val MAX_EXERCISE_ALIAS_BYTES = 1024 * 1024
private const val MAX_EXERCISE_ALIASES = 4096
private const val ANDROID_LEG_PRESS_LEGACY_ID =
    "ex_d68a1af1-7247-4fb3-a48b-da8516906a29"
private const val DESKTOP_LEG_PRESS_CANONICAL_ID =
    "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde"
private val EXERCISE_ID_V4_PATTERN = Regex(
    "^ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$",
)

private class TrainlogDatabaseHelper(
    private val appContext: Context,
    databaseName: String,
) : SQLiteOpenHelper(
            appContext,
    databaseName,
    null,
    13,
) {
    override fun onConfigure(
        db: SQLiteDatabase,
    ) {
        super.onConfigure(db)

        db.setForeignKeyConstraintsEnabled(
            true
        )
    }

    override fun onOpen(
        db: SQLiteDatabase,
    ) {
        super.onOpen(db)
        migrateApprovedLegPressIdentity(db)
    }

    override fun onCreate(
        db: SQLiteDatabase,
    ) {
        createExerciseTable(db)
        createSessionTables(db)
        createBodyTable(db)
        createActiveDraftTables(db)
        createEquipmentTables(db)
        createBodyZoneTables(db)
        createExerciseAliasTable(db)
        seedEquipment(db)
    }

    override fun onUpgrade(
        db: SQLiteDatabase,
        oldVersion: Int,
        newVersion: Int,
    ) {
        var version = oldVersion

        if (version < 2 && newVersion >= 2) {
            createSessionTables(db)
            version = 2
        }

        if (version < 3 && newVersion >= 3) {
            createBodyTable(db)
            version = 3
        }

        if (version < 4 && newVersion >= 4) {
            /* CONTRACT: v4 is additive. Existing catalog, completed sessions,
             * performed values, and body observations remain untouched. */
            createActiveDraftTables(db)
            version = 4
        }

        if (version < 5 && newVersion >= 5) {
            /* CONTRACT: v5 adds only canonical equipment metadata. Existing
             * exercises, historical rows, and drafts are never rewritten. */
            createEquipmentTables(db)
            seedEquipment(db)
            addEquipmentReferenceColumns(db)
            version = 5
        }

        if (version < 6 && newVersion >= 6) {
            /* v6 keeps historic sets intact while allowing optional per-set load. */
            db.execSQL("ALTER TABLE performed_sets ADD COLUMN weight_kg REAL CHECK(weight_kg >= 0.0);")
            db.execSQL("ALTER TABLE draft_performed_sets ADD COLUMN weight_kg REAL CHECK(weight_kg >= 0.0);")
            version = 6
        }

        if (version < 7 && newVersion >= 7) {
            /* WHY: exercise_id is a catalogue identity, not an occurrence identity.
             * Historic rows get the deterministic same cross-device legacy key. */
            db.execSQL("ALTER TABLE session_exercises ADD COLUMN entry_id TEXT;")
            db.execSQL("UPDATE session_exercises SET entry_id = 'sxe_legacy_' || (SELECT session_id FROM sessions WHERE sessions.id = session_exercises.session_row_id) || '_' || (SELECT exercise_id FROM exercises WHERE exercises.id = session_exercises.exercise_row_id);")
            db.execSQL("CREATE UNIQUE INDEX session_exercises_entry_id_v7 ON session_exercises(entry_id);")
            db.execSQL("ALTER TABLE draft_performed_sets RENAME TO draft_performed_sets_v6;")
            db.execSQL("ALTER TABLE draft_continuous_activity RENAME TO draft_continuous_activity_v6;")
            db.execSQL("ALTER TABLE draft_session_exercises RENAME TO draft_session_exercises_v6;")
            createActiveDraftTables(db)
            db.execSQL("INSERT INTO draft_session_exercises(id,draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) SELECT id,draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,'sxe_draft_legacy_' || id FROM draft_session_exercises_v6;")
            db.execSQL("INSERT INTO draft_performed_sets(id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg) SELECT id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg FROM draft_performed_sets_v6;")
            db.execSQL("INSERT INTO draft_continuous_activity(id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km) SELECT id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km FROM draft_continuous_activity_v6;")
            db.execSQL("DROP TABLE draft_performed_sets_v6;")
            db.execSQL("DROP TABLE draft_continuous_activity_v6;")
            db.execSQL("DROP TABLE draft_session_exercises_v6;")
            version = 7
        }

        if (version < 8 && newVersion >= 8) {
            /* CONTRACT: desktop custom definitions may explicitly carry
             * load_semantics=none. Rebuild the complete equipment reference
             * graph with stable row IDs so history and the active draft keep
             * pointing at exactly the same equipment records. */
            migrateEquipmentLoadSemanticsToVersionEight(db)
            version = 8
        }

        if (version < 9 && newVersion >= 9) {
            migrateExplicitMaxResultsToVersionNine(db)
            version = 9
        }

        if (version < 10 && newVersion >= 10) {
            /* WHY: the approved legacy Leg press identity must be canonical
             * before identity-keyed manifest mappings are applied. */
            migrateApprovedLegPressIdentity(db)
            createBodyZoneTables(db)
            seedInitialBodyZones(db)
            version = 10
        }

        if (version < 11 && newVersion >= 11) {
            /* CONTRACT: v11 is additive and never reconstructs prescriptions
             * from historical actuals. SQLite defaults make every old row the
             * exact targetless none/zero representation. */
            addOccurrencePlanColumns(db, "session_exercises")
            addOccurrencePlanColumns(db, "draft_session_exercises")
            version = 11
        }

        if (version < 12 && newVersion >= 12) {
            createExerciseAliasTable(db)
            version = 12
        }

        if (version < 13 && newVersion >= 13) {
            migrateMachineExercisesToVersionThirteen(db)
            version = 13
        }

        if (version != newVersion) {
            error(
                "Unsupported Android DB upgrade " +
                    "$oldVersion -> $newVersion"
            )
        }
    }

    private fun createBodyZoneTables(db: SQLiteDatabase) {
        /* CONTRACT: canonical zone definitions remain in the asset; SQLite
         * stores only direct exercise relations and role cardinality. */
        db.execSQL(
            "CREATE TABLE IF NOT EXISTS exercise_body_zones(" +
                "exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE CASCADE," +
                "zone_id TEXT NOT NULL," +
                "role TEXT NOT NULL CHECK(role IN('primary','secondary'))," +
                "PRIMARY KEY(exercise_row_id,zone_id));",
        )
        db.execSQL(
            "CREATE UNIQUE INDEX IF NOT EXISTS exercise_body_zones_one_primary " +
                "ON exercise_body_zones(exercise_row_id) WHERE role='primary';",
        )
        db.execSQL(
            "CREATE TABLE IF NOT EXISTS exercise_body_zone_sync(" +
                "exercise_row_id INTEGER PRIMARY KEY REFERENCES exercises(id) ON DELETE CASCADE," +
                "synced_state TEXT NOT NULL);",
        )
    }

    private fun createExerciseAliasTable(db: SQLiteDatabase) {
        db.execSQL(
            "CREATE TABLE IF NOT EXISTS exercise_aliases(" +
                "source_exercise_id TEXT PRIMARY KEY," +
                "canonical_exercise_id TEXT NOT NULL REFERENCES exercises(exercise_id) ON DELETE RESTRICT," +
                "CHECK(source_exercise_id<>canonical_exercise_id));",
        )
        db.execSQL("CREATE INDEX IF NOT EXISTS exercise_aliases_canonical ON exercise_aliases(canonical_exercise_id);")
    }

    private fun seedInitialBodyZones(db: SQLiteDatabase) {
        val catalog = BodyZoneCatalog.load(appContext)
        catalog.initialMappings.forEach { mapping ->
            val rowId = db.rawQuery(
                "SELECT id FROM exercises WHERE exercise_id=?;",
                arrayOf(mapping.exerciseId),
            ).use { cursor -> if (cursor.moveToFirst()) cursor.getLong(0) else null }
                ?: return@forEach
            fun insert(zoneId: String, role: String) {
                db.insertOrThrow("exercise_body_zones", null, ContentValues().apply {
                    put("exercise_row_id", rowId)
                    put("zone_id", zoneId)
                    put("role", role)
                })
            }
            insert(mapping.primaryZoneId, "primary")
            mapping.secondaryZoneIds.forEach { insert(it, "secondary") }
        }
        db.execSQL(
            "INSERT INTO exercise_body_zone_sync(exercise_row_id,synced_state) " +
                "SELECT e.id,COALESCE((SELECT p.zone_id FROM exercise_body_zones p " +
                "WHERE p.exercise_row_id=e.id AND p.role='primary'),'')||'|'||" +
                "COALESCE((SELECT group_concat(s.zone_id,',') FROM " +
                "(SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=e.id " +
                "AND role='secondary' ORDER BY zone_id) s),'') FROM exercises e;",
        )
    }

    private data class ExerciseIdentityRow(
        val rowId: Long,
        val exerciseId: String,
        val normalizedName: String,
        val recordingMode: String,
        val trackingMode: String,
        val dataFields: Int,
    )

    private fun exerciseIdentityRow(
        db: SQLiteDatabase,
        exerciseId: String,
    ): ExerciseIdentityRow? = db.rawQuery(
        "SELECT id,exercise_id,normalized_name,recording_mode,tracking_mode,data_fields " +
            "FROM exercises WHERE exercise_id=?;",
        arrayOf(exerciseId),
    ).use { cursor ->
        if (!cursor.moveToFirst()) null else ExerciseIdentityRow(
            rowId = cursor.getLong(0),
            exerciseId = cursor.getString(1),
            normalizedName = cursor.getString(2),
            recordingMode = cursor.getString(3),
            trackingMode = cursor.getString(4),
            dataFields = cursor.getInt(5),
        )
    }

    private fun isApprovedLegPressProfile(
        row: ExerciseIdentityRow,
    ): Boolean = row.normalizedName == "leg press" &&
        row.recordingMode == "sets" &&
        row.trackingMode == "reps" &&
        row.dataFields == 0

    /**
     * WHY: the user explicitly designated this one Android-created historic
     * identity as the desktop Leg press identity.  This is intentionally not
     * a generic normalized-name reconciliation: every other different-ID name
     * collision remains an import conflict.
     *
     * INVARIANT: row-ID references are retained whenever the canonical row is
     * absent, preserving completed/draft entries, sets, loads and equipment.
     * If both rows exist, every known reference is moved transactionally before
     * the legacy row is deleted, and incompatible catalog equipment metadata
     * aborts the transaction rather than being silently chosen.
     */
    private fun migrateApprovedLegPressIdentity(
        db: SQLiteDatabase,
    ) {
        val legacy = exerciseIdentityRow(db, ANDROID_LEG_PRESS_LEGACY_ID)
            ?: return /* Already migrated or this database never held it. */
        val canonical = exerciseIdentityRow(db, DESKTOP_LEG_PRESS_CANONICAL_ID)

        check(isApprovedLegPressProfile(legacy)) {
            "Profil Leg press Android inattendu; migration refusée."
        }
        if (canonical != null) {
            check(isApprovedLegPressProfile(canonical) &&
                canonical.normalizedName == legacy.normalizedName &&
                canonical.recordingMode == legacy.recordingMode &&
                canonical.trackingMode == legacy.trackingMode &&
                canonical.dataFields == legacy.dataFields) {
                "Profil Leg press desktop incompatible; migration refusée."
            }
        }

        val ownsTransaction = !db.inTransaction()
        if (ownsTransaction) db.beginTransaction()
        try {
            if (canonical == null) {
                /* Keep the legacy exercise row itself: all foreign-key graph
                 * members therefore retain their row IDs and occurrence IDs. */
                db.execSQL(
                    "UPDATE catalog_exercise_equipment SET exercise_id=? WHERE exercise_id=?;",
                    arrayOf(DESKTOP_LEG_PRESS_CANONICAL_ID, ANDROID_LEG_PRESS_LEGACY_ID),
                )
                db.execSQL(
                    "UPDATE exercises SET exercise_id=? WHERE id=?;",
                    arrayOf<Any>(DESKTOP_LEG_PRESS_CANONICAL_ID, legacy.rowId),
                )
            } else {
                val incompatibleCatalogEquipment = db.rawQuery(
                    "SELECT 1 FROM catalog_exercise_equipment legacy " +
                        "JOIN catalog_exercise_equipment canonical " +
                        "ON canonical.equipment_row_id=legacy.equipment_row_id " +
                        "WHERE legacy.exercise_id=? AND canonical.exercise_id=? " +
                        "AND legacy.load_semantics<>canonical.load_semantics LIMIT 1;",
                    arrayOf(ANDROID_LEG_PRESS_LEGACY_ID, DESKTOP_LEG_PRESS_CANONICAL_ID),
                ).use { it.moveToFirst() }
                check(!incompatibleCatalogEquipment) {
                    "Métadonnées équipement Leg press incompatibles; migration refusée."
                }

                mergeApprovedIdentityBodyZones(db, canonical.rowId, legacy.rowId)

                db.execSQL(
                    "UPDATE session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL(
                    "UPDATE draft_session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL(
                    "UPDATE active_session_draft SET selected_exercise_row_id=? WHERE selected_exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL(
                    "INSERT OR IGNORE INTO exercise_equipment(exercise_row_id,equipment_row_id) " +
                        "SELECT ?,equipment_row_id FROM exercise_equipment WHERE exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL("DELETE FROM exercise_equipment WHERE exercise_row_id=?;", arrayOf(legacy.rowId))
                db.execSQL(
                    "INSERT OR IGNORE INTO catalog_exercise_equipment(exercise_id,equipment_row_id,load_semantics) " +
                        "SELECT ?,equipment_row_id,load_semantics FROM catalog_exercise_equipment WHERE exercise_id=?;",
                    arrayOf(DESKTOP_LEG_PRESS_CANONICAL_ID, ANDROID_LEG_PRESS_LEGACY_ID),
                )
                db.execSQL(
                    "DELETE FROM catalog_exercise_equipment WHERE exercise_id=?;",
                    arrayOf(ANDROID_LEG_PRESS_LEGACY_ID),
                )
                db.execSQL("DELETE FROM exercises WHERE id=?;", arrayOf(legacy.rowId))
            }

            check(exerciseIdentityRow(db, ANDROID_LEG_PRESS_LEGACY_ID) == null) {
                "Référence Leg press Android résiduelle après migration."
            }
            check(exerciseIdentityRow(db, DESKTOP_LEG_PRESS_CANONICAL_ID) != null) {
                "Identité Leg press desktop absente après migration."
            }
            if (ownsTransaction) db.setTransactionSuccessful()
        } finally {
            if (ownsTransaction) db.endTransaction()
        }
    }

    private fun mergeApprovedIdentityBodyZones(
        db: SQLiteDatabase,
        canonicalRowId: Long,
        legacyRowId: Long,
    ) {
        val hasRelations = db.rawQuery(
            "SELECT 1 FROM sqlite_master WHERE type='table' " +
                "AND name='exercise_body_zones';",
            null,
        ).use { it.moveToFirst() }
        if (!hasRelations) return

        fun state(rowId: Long): List<Pair<String, String>> = db.rawQuery(
            "SELECT zone_id,role FROM exercise_body_zones " +
                "WHERE exercise_row_id=? ORDER BY role,zone_id;",
            arrayOf(rowId.toString()),
        ).use { cursor ->
            buildList {
                while (cursor.moveToNext()) add(cursor.getString(0) to cursor.getString(1))
            }
        }
        val canonicalState = state(canonicalRowId)
        val legacyState = state(legacyRowId)
        check(canonicalState.isEmpty() || legacyState.isEmpty() || canonicalState == legacyState) {
            "Relations de zones Leg press incompatibles; migration refusée."
        }
        if (canonicalState.isEmpty() && legacyState.isNotEmpty()) {
            db.execSQL(
                "UPDATE exercise_body_zones SET exercise_row_id=? WHERE exercise_row_id=?;",
                arrayOf(canonicalRowId, legacyRowId),
            )
        } else {
            db.execSQL(
                "DELETE FROM exercise_body_zones WHERE exercise_row_id=?;",
                arrayOf(legacyRowId),
            )
        }

        /* INVARIANT: identity repair is not a sync acknowledgement. A fresh
         * shared baseline must be established by an identical companion. */
        val hasBaseline = db.rawQuery(
            "SELECT 1 FROM sqlite_master WHERE type='table' " +
                "AND name='exercise_body_zone_sync';",
            null,
        ).use { it.moveToFirst() }
        if (hasBaseline) {
            db.execSQL(
                "DELETE FROM exercise_body_zone_sync WHERE exercise_row_id IN(?,?);",
                arrayOf(canonicalRowId, legacyRowId),
            )
        }
    }

    private fun createExerciseTable(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE exercises(
                id INTEGER PRIMARY KEY,
                exercise_id TEXT NOT NULL UNIQUE,
                name TEXT NOT NULL,
                normalized_name TEXT NOT NULL UNIQUE,
                recording_mode TEXT NOT NULL
                    CHECK(
                        recording_mode IN (
                            'sets',
                            'continuous'
                        )
                    ),
                tracking_mode TEXT NOT NULL
                    CHECK(
                        tracking_mode IN (
                            'reps',
                            'duration'
                        )
                    ),
                data_fields INTEGER NOT NULL
                    DEFAULT 0
                    CHECK(
                        data_fields >= 0 AND
                        (data_fields & ~3) = 0
                    ),
                load_semantics TEXT
                    CHECK(load_semantics IN ('none','external','assistance','bodyweight','cardio')),
                machine_variant TEXT,
                machine_provenance TEXT,
                scientific_profile_id TEXT,
                science_state TEXT NOT NULL DEFAULT 'unresolved'
                    CHECK(science_state IN ('resolved','unresolved')),
                legacy_equipment_id TEXT,
                CHECK(
                    recording_mode !=
                        'continuous' OR
                    tracking_mode =
                        'duration'
                ),
                CHECK(
                    recording_mode !=
                        'sets' OR
                    data_fields = 0
                )
            );
            """.trimIndent()
        )
    }

    /**
     * WHY: a fixed gym machine/movement is now the selectable performance
     * identity, while equipment rows remain only compatibility/provenance.
     * CONTRACT: v13 adds exercise metadata and the five frozen creator IDs.
     * INVARIANT: only approved entry/exercise/equipment triples are repointed;
     * sessions, entry IDs, child facts, targets and the durable draft survive.
     */
    private fun migrateMachineExercisesToVersionThirteen(db: SQLiteDatabase) {
        val columns = mutableSetOf<String>()
        db.rawQuery("PRAGMA table_info(exercises);", null).use { cursor ->
            val nameIndex = cursor.getColumnIndexOrThrow("name")
            while (cursor.moveToNext()) columns += cursor.getString(nameIndex)
        }
        fun add(name: String, declaration: String) {
            if (name !in columns) db.execSQL("ALTER TABLE exercises ADD COLUMN $declaration;")
        }
        add("load_semantics", "load_semantics TEXT CHECK(load_semantics IN ('none','external','assistance','bodyweight','cardio'))")
        add("machine_variant", "machine_variant TEXT")
        add("machine_provenance", "machine_provenance TEXT")
        add("scientific_profile_id", "scientific_profile_id TEXT")
        add("science_state", "science_state TEXT NOT NULL DEFAULT 'unresolved' CHECK(science_state IN ('resolved','unresolved'))")
        add("legacy_equipment_id", "legacy_equipment_id TEXT")
        seedMachineExercisesV13(db)
    }

    private data class MachineSeed(
        val exerciseId: String,
        val oldName: String?,
        val name: String,
        val recording: String,
        val tracking: String,
        val fields: Int,
        val load: String?,
        val variant: String?,
        val profile: String?,
        val legacyEquipment: String?,
    )

    private fun seedMachineExercisesV13(db: SQLiteDatabase) {
        fun normalized(value: String): String = Normalizer.normalize(
            value.trim().lowercase(Locale.ROOT), Normalizer.Form.NFD,
        ).replace(Regex("\\p{Mn}+"), "")
        val rows = listOf(
            MachineSeed("ex_7e7cf906-2214-4066-bcb7-c16382d83b3b", "Abduction de hanche assise", "Hip Abduction", "sets", "reps", 0, "external", "selectorized", "sp_hip_abduction_v1", "hip_abduction"),
            MachineSeed("ex_1872246a-39ae-44dc-b58d-f87e90ca49ab", "Extension de genou assise", "Leg Extension", "sets", "reps", 0, "external", "selectorized", "sp_leg_extension_v1", "leg_extension"),
            MachineSeed("ex_474ec393-3efa-4aaa-8e08-1a0245ed7835", "Extension du tronc", "Back Extension", "sets", "reps", 0, "external", "selectorized", "sp_back_extension_v1", "back_extension"),
            MachineSeed("ex_ec619fc2-4685-4044-873c-86764bd4a0fe", "Flexion de coude à la machine", "Arm Curl", "sets", "reps", 0, "external", "selectorized", "sp_arm_curl_v1", "arm_curl"),
            MachineSeed("ex_d7398d9f-d928-4d2e-94e9-74e201da55c5", "Flexion de genou couchée", "Prone Leg Curl", "sets", "reps", 0, "external", "selectorized", "sp_prone_leg_curl_v1", "prone_leg_curl"),
            MachineSeed("ex_1a34814c-2e46-40fc-b1f4-6d60b8e5a3e0", "Rotation du tronc à la machine", "Rotary Torso", "sets", "reps", 0, "external", "selectorized", "sp_rotary_torso_v1", "rotary_torso"),
            MachineSeed("ex_33f79331-871c-4eed-babe-346e53a99070", "Tirage horizontal assis", "Seated Row", "sets", "reps", 0, "external", "selectorized", "sp_seated_row_v1", "seated_row"),
            MachineSeed("ex_1b0c6b8b-b05e-4e6f-8809-5f7d85d668de", "Tirage horizontal divergent assis", "Diverging Seated Row", "sets", "reps", 0, "external", "diverging", "sp_seated_row_v1", "diverging_seated_row"),
            MachineSeed("ex_b4d1daf1-de4a-4016-abdf-487bf6014ce6", "Tirage vertical divergent", "Diverging Lat Pulldown", "sets", "reps", 0, "external", "diverging", "sp_vertical_pull_v1", "diverging_lat_pulldown"),
            MachineSeed("ex_a72fa713-4b0e-431d-95e2-42d95beb77b1", "Tirage vertical à la poulie", "Lat Pull", "sets", "reps", 0, "external", "selectorized", "sp_vertical_pull_v1", "lat_pull"),
            MachineSeed("ex_6dfc7ffd-8891-464e-a995-808baf1b0d7b", "Développé épaules convergent", "Converging Shoulder Press", "sets", "reps", 0, "external", "converging", "sp_shoulder_press_v1", "converging_shoulder_press"),
            MachineSeed("ex_a1ef5047-b44b-4c64-a6ed-c7a3bc13b163", "Flexion de genou assise", "Seated Leg Curl", "sets", "reps", 0, "external", "selectorized", "sp_seated_leg_curl_v1", "seated_leg_curl"),
            MachineSeed("ex_b432623f-bfe9-4daf-a653-60ec7fdffbde", "Presse à cuisses", "Leg Press", "sets", "reps", 0, "external", "selectorized", "sp_leg_press_v1", "leg_press"),
            MachineSeed("ex_4cd2433e-80b1-478a-b8df-73fc6ef80962", "Écarté inversé à la machine", "Rear Delt", "sets", "reps", 0, "external", "rear_delt", "sp_rear_delt_v1", "rear_delt_pec_fly"),
            MachineSeed("ex_0e26c06f-a458-40a4-be20-4ed219ede30d", null, "Plate Loaded Leg Press", "sets", "reps", 0, "external", "plate_loaded", "sp_leg_press_v1", "plate_loaded_leg_press"),
            MachineSeed("ex_f01d2a46-6984-4dec-8934-4d82fca6dfc2", null, "Treadmill", "continuous", "duration", 3, "cardio", "treadmill", null, "treadmill"),
            MachineSeed("ex_54dcdfd2-280d-4c2b-ae6b-c6089a985eee", null, "Pec Fly", "sets", "reps", 0, "external", "pec_fly", "sp_pec_fly_v1", "rear_delt_pec_fly"),
            MachineSeed("ex_58b8dfbc-92b2-4783-a449-9947a42480b8", null, "Chin Assist", "sets", "reps", 0, "assistance", "assisted", "sp_assisted_chin_v1", "assisted_dip_chin_machine"),
            MachineSeed("ex_44358a7b-09c8-4992-8f70-7eee4e99bbdf", null, "Dip Assist", "sets", "reps", 0, "assistance", "assisted", "sp_assisted_dip_v1", "assisted_dip_chin_machine"),
        )
        rows.forEach { item ->
            val existing = exerciseIdentityRow(db, item.exerciseId)
            if (existing == null) {
                db.execSQL(
                    "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields,load_semantics,machine_variant,scientific_profile_id,science_state,legacy_equipment_id) VALUES(?,?,?,?,?,?,?,?,?,?,?);",
                    arrayOf<Any?>(item.exerciseId, item.name, normalized(item.name), item.recording, item.tracking, item.fields, item.load, item.variant, item.profile, if (item.profile == null) "unresolved" else "resolved", item.legacyEquipment),
                )
            } else if (item.oldName != null) {
                db.execSQL(
                    "UPDATE exercises SET name=?,normalized_name=?,load_semantics=?,machine_variant=?,scientific_profile_id=?,science_state='resolved',legacy_equipment_id=? WHERE exercise_id=? AND name=?;",
                    arrayOf<Any?>(item.name, normalized(item.name), item.load, item.variant, item.profile, item.legacyEquipment, item.exerciseId, item.oldName),
                )
            }
        }
        val hasBodyZoneTables = db.rawQuery(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN ('exercise_body_zones','exercise_body_zone_sync');",
            null,
        ).use { cursor -> cursor.moveToFirst() && cursor.getInt(0) == 2 }
        /* CONTRACT: BODY FOCUS consumes the existing exercise-owned relation.
         * New performance identities reuse reviewed profile mappings without
         * grouping their load histories. */
        if (hasBodyZoneTables) {
            db.execSQL("INSERT OR IGNORE INTO exercise_body_zones(exercise_row_id,zone_id,role) SELECT n.id,z.zone_id,z.role FROM exercises n JOIN exercises o ON o.exercise_id='ex_b432623f-bfe9-4daf-a653-60ec7fdffbde' JOIN exercise_body_zones z ON z.exercise_row_id=o.id WHERE n.exercise_id='ex_0e26c06f-a458-40a4-be20-4ed219ede30d';")
            listOf(
            Triple("ex_54dcdfd2-280d-4c2b-ae6b-c6089a985eee", "chest", "primary"),
            Triple("ex_54dcdfd2-280d-4c2b-ae6b-c6089a985eee", "shoulders", "secondary"),
            Triple("ex_58b8dfbc-92b2-4783-a449-9947a42480b8", "back", "primary"),
            Triple("ex_58b8dfbc-92b2-4783-a449-9947a42480b8", "arms", "secondary"),
            Triple("ex_44358a7b-09c8-4992-8f70-7eee4e99bbdf", "arms", "primary"),
            Triple("ex_44358a7b-09c8-4992-8f70-7eee4e99bbdf", "chest", "secondary"),
            Triple("ex_44358a7b-09c8-4992-8f70-7eee4e99bbdf", "shoulders", "secondary"),
            ).forEach { (exerciseId, zoneId, role) -> db.execSQL(
                "INSERT OR IGNORE INTO exercise_body_zones(exercise_row_id,zone_id,role) SELECT id,?,? FROM exercises WHERE exercise_id=?;",
                arrayOf(zoneId, role, exerciseId),
            ) }
            listOf(
            "ex_0e26c06f-a458-40a4-be20-4ed219ede30d" to "thighs|glutes",
            "ex_54dcdfd2-280d-4c2b-ae6b-c6089a985eee" to "chest|shoulders",
            "ex_58b8dfbc-92b2-4783-a449-9947a42480b8" to "back|arms",
            "ex_44358a7b-09c8-4992-8f70-7eee4e99bbdf" to "arms|chest,shoulders",
            ).forEach { (exerciseId, state) -> db.execSQL(
                "INSERT OR REPLACE INTO exercise_body_zone_sync(exercise_row_id,synced_state) SELECT id,? FROM exercises WHERE exercise_id=?;",
                arrayOf(state, exerciseId),
            ) }
        }
        db.execSQL("UPDATE exercises SET load_semantics=NULL,machine_variant=NULL,machine_provenance=NULL,scientific_profile_id=NULL,science_state='unresolved',legacy_equipment_id=NULL WHERE exercise_id='ex_617007f9-7420-4408-91b9-8ffb77900f13';")
        fun equipmentRow(id: String) = "(SELECT id FROM equipment WHERE equipment_id='$id')"
        db.execSQL("UPDATE session_exercises SET exercise_row_id=(SELECT id FROM exercises WHERE exercise_id='ex_0e26c06f-a458-40a4-be20-4ed219ede30d') WHERE entry_id='sxe_draft_legacy_7' AND exercise_row_id=(SELECT id FROM exercises WHERE exercise_id='ex_b432623f-bfe9-4daf-a653-60ec7fdffbde') AND equipment_row_id=${equipmentRow("plate_loaded_leg_press")};")
        val treadmillEntries = listOf("sxe_093c1331-beaa-4b69-91b3-240292709be6", "sxe_f25142c8-455e-4346-9bfc-31d0989e275d", "sxe_f2242691-ba6c-48a0-b938-a1f744375d75", "sxe_9f8882f4-069e-49d5-8216-19d16b467e4a")
        treadmillEntries.forEach { entry -> db.execSQL("UPDATE session_exercises SET exercise_row_id=(SELECT id FROM exercises WHERE exercise_id='ex_f01d2a46-6984-4dec-8934-4d82fca6dfc2') WHERE entry_id=? AND exercise_row_id=(SELECT id FROM exercises WHERE exercise_id='ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d') AND equipment_row_id=${equipmentRow("treadmill")};", arrayOf(entry)) }
    }

    private fun createSessionTables(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS sessions(
                id INTEGER PRIMARY KEY,
                session_id TEXT NOT NULL UNIQUE,
                started_at TEXT NOT NULL,
                session_type TEXT NOT NULL
                    CHECK(
                        session_type IN (
                            'training',
                            'max_test'
                        )
                    )
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS session_exercises(
                id INTEGER PRIMARY KEY,
                session_row_id INTEGER NOT NULL
                    REFERENCES sessions(id)
                    ON DELETE CASCADE,
                exercise_row_id INTEGER NOT NULL
                    REFERENCES exercises(id)
                    ON DELETE RESTRICT,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                recording_mode TEXT NOT NULL
                    CHECK(
                        recording_mode IN (
                            'sets',
                            'continuous'
                        )
                    ),
                tracking_mode TEXT NOT NULL
                    CHECK(
                        tracking_mode IN (
                            'reps',
                            'duration'
                        )
                    ),
                data_fields INTEGER NOT NULL
                    CHECK(
                        data_fields >= 0 AND
                        (data_fields & ~3) = 0
                    ),
                equipment_row_id INTEGER
                    REFERENCES equipment(id)
                    ON DELETE SET NULL,
                entry_id TEXT NOT NULL UNIQUE,
                load_mode TEXT NOT NULL DEFAULT 'none'
                    CHECK(load_mode IN ('none', 'external', 'assistance')),
                rest_seconds INTEGER NOT NULL DEFAULT 0
                    CHECK(rest_seconds BETWEEN 0 AND 86400),
                target_sets INTEGER CHECK(target_sets BETWEEN 1 AND 64),
                target_reps INTEGER CHECK(target_reps BETWEEN 1 AND 10000),
                target_duration_seconds INTEGER
                    CHECK(target_duration_seconds BETWEEN 1 AND 86400),
                target_weight_kg REAL CHECK(target_weight_kg > 0.0),
                UNIQUE(
                    session_row_id,
                    position
                )
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS performed_sets(
                id INTEGER PRIMARY KEY,
                session_exercise_row_id INTEGER NOT NULL
                    REFERENCES session_exercises(id)
                    ON DELETE CASCADE,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                reps INTEGER
                    CHECK(reps >= 0),
                duration_seconds INTEGER
                    CHECK(duration_seconds > 0),
                weight_kg REAL
                    CHECK(weight_kg >= 0.0),
                CHECK(
                    (
                        reps IS NOT NULL AND
                        duration_seconds IS NULL
                    ) OR (
                        reps IS NULL AND
                        duration_seconds IS NOT NULL
                    )
                ),
                UNIQUE(
                    session_exercise_row_id,
                    position
                )
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS continuous_activity(
                id INTEGER PRIMARY KEY,
                session_exercise_row_id INTEGER NOT NULL UNIQUE
                    REFERENCES session_exercises(id)
                    ON DELETE CASCADE,
                duration_seconds INTEGER NOT NULL
                    CHECK(duration_seconds > 0),
                speed_kmh REAL
                    CHECK(speed_kmh > 0.0),
                distance_km REAL
                    CHECK(distance_km > 0.0)
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS max_results(
                session_exercise_row_id INTEGER PRIMARY KEY
                    REFERENCES session_exercises(id)
                    ON DELETE CASCADE,
                max_weight_kg REAL NOT NULL
                    CHECK(max_weight_kg > 0.0)
            );
            """.trimIndent()
        )
    }

    private fun addOccurrencePlanColumns(db: SQLiteDatabase, table: String) {
        check(table == "session_exercises" || table == "draft_session_exercises")
        val present = mutableSetOf<String>()
        db.rawQuery("PRAGMA table_info($table);", null).use { cursor ->
            while (cursor.moveToNext()) present += cursor.getString(1)
        }
        fun add(name: String, declaration: String) {
            if (name !in present) db.execSQL("ALTER TABLE $table ADD COLUMN $name $declaration;")
        }
        add("load_mode", "TEXT NOT NULL DEFAULT 'none' CHECK(load_mode IN ('none','external','assistance'))")
        add("rest_seconds", "INTEGER NOT NULL DEFAULT 0 CHECK(rest_seconds BETWEEN 0 AND 86400)")
        add("target_sets", "INTEGER CHECK(target_sets BETWEEN 1 AND 64)")
        add("target_reps", "INTEGER CHECK(target_reps BETWEEN 1 AND 10000)")
        add("target_duration_seconds", "INTEGER CHECK(target_duration_seconds BETWEEN 1 AND 86400)")
        add("target_weight_kg", "REAL CHECK(target_weight_kg > 0.0)")
    }


    private fun createBodyTable(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS body_observations(
                id INTEGER PRIMARY KEY,
                observation_id TEXT NOT NULL UNIQUE,
                observed_at TEXT NOT NULL,
                body_weight_kg REAL
                    CHECK(body_weight_kg > 0.0),
                neck_cm REAL
                    CHECK(neck_cm > 0.0),
                shoulders_cm REAL
                    CHECK(shoulders_cm > 0.0),
                chest_cm REAL
                    CHECK(chest_cm > 0.0),
                waist_cm REAL
                    CHECK(waist_cm > 0.0),
                hips_cm REAL
                    CHECK(hips_cm > 0.0),
                left_arm_cm REAL
                    CHECK(left_arm_cm > 0.0),
                right_arm_cm REAL
                    CHECK(right_arm_cm > 0.0),
                left_forearm_cm REAL
                    CHECK(left_forearm_cm > 0.0),
                right_forearm_cm REAL
                    CHECK(right_forearm_cm > 0.0),
                left_thigh_cm REAL
                    CHECK(left_thigh_cm > 0.0),
                right_thigh_cm REAL
                    CHECK(right_thigh_cm > 0.0),
                left_calf_cm REAL
                    CHECK(left_calf_cm > 0.0),
                right_calf_cm REAL
                    CHECK(right_calf_cm > 0.0),
                CHECK(
                    body_weight_kg IS NOT NULL OR
                    neck_cm IS NOT NULL OR
                    shoulders_cm IS NOT NULL OR
                    chest_cm IS NOT NULL OR
                    waist_cm IS NOT NULL OR
                    hips_cm IS NOT NULL OR
                    left_arm_cm IS NOT NULL OR
                    right_arm_cm IS NOT NULL OR
                    left_forearm_cm IS NOT NULL OR
                    right_forearm_cm IS NOT NULL OR
                    left_thigh_cm IS NOT NULL OR
                    right_thigh_cm IS NOT NULL OR
                    left_calf_cm IS NOT NULL OR
                    right_calf_cm IS NOT NULL
                )
            );
            """.trimIndent()
        )
    }

    private fun createActiveDraftTables(
        db: SQLiteDatabase,
    ) {
        /* WHY: unfinished capture must be durable without entering completed
         * history. The singleton check enforces the v1 one-active-draft rule. */
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS active_session_draft(
                id INTEGER PRIMARY KEY
                    CHECK(id = 1),
                session_type TEXT NOT NULL
                    CHECK(
                        session_type IN (
                            'training',
                            'max_test'
                        )
                    ),
                selected_exercise_row_id INTEGER
                    REFERENCES exercises(id)
                    ON DELETE SET NULL,
                selected_exercise_label TEXT,
                selected_equipment_id TEXT,
                source_session_id TEXT,
                weight_text TEXT NOT NULL DEFAULT '',
                max_weight_text TEXT NOT NULL DEFAULT '',
                set_count_text TEXT NOT NULL,
                reps_text TEXT NOT NULL,
                duration_text TEXT NOT NULL,
                speed_text TEXT NOT NULL,
                distance_text TEXT NOT NULL,
                updated_at TEXT NOT NULL
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_session_exercises(
                id INTEGER PRIMARY KEY,
                draft_id INTEGER NOT NULL
                    REFERENCES active_session_draft(id)
                    ON DELETE CASCADE,
                exercise_row_id INTEGER NOT NULL
                    REFERENCES exercises(id)
                    ON DELETE RESTRICT,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                recording_mode TEXT NOT NULL
                    CHECK(
                        recording_mode IN (
                            'sets',
                            'continuous'
                        )
                    ),
                tracking_mode TEXT NOT NULL
                    CHECK(
                        tracking_mode IN (
                            'reps',
                            'duration'
                        )
                    ),
                data_fields INTEGER NOT NULL
                    CHECK(
                        data_fields >= 0 AND
                        (data_fields & ~3) = 0
                    ),
                equipment_row_id INTEGER
                    REFERENCES equipment(id)
                    ON DELETE SET NULL,
                entry_id TEXT NOT NULL UNIQUE,
                load_mode TEXT NOT NULL DEFAULT 'none'
                    CHECK(load_mode IN ('none', 'external', 'assistance')),
                rest_seconds INTEGER NOT NULL DEFAULT 0
                    CHECK(rest_seconds BETWEEN 0 AND 86400),
                target_sets INTEGER CHECK(target_sets BETWEEN 1 AND 64),
                target_reps INTEGER CHECK(target_reps BETWEEN 1 AND 10000),
                target_duration_seconds INTEGER
                    CHECK(target_duration_seconds BETWEEN 1 AND 86400),
                target_weight_kg REAL CHECK(target_weight_kg > 0.0),
                UNIQUE(draft_id, position)
            );
            """.trimIndent()
        )

        /* INVARIANT: child cascades terminate at the active-draft singleton;
         * neither discard nor child replacement reaches catalog/history rows. */

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_performed_sets(
                id INTEGER PRIMARY KEY,
                draft_exercise_row_id INTEGER NOT NULL
                    REFERENCES draft_session_exercises(id)
                    ON DELETE CASCADE,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                reps INTEGER
                    CHECK(reps >= 0),
                duration_seconds INTEGER
                    CHECK(duration_seconds > 0),
                weight_kg REAL
                    CHECK(weight_kg >= 0.0),
                CHECK(
                    (
                        reps IS NOT NULL AND
                        duration_seconds IS NULL
                    ) OR (
                        reps IS NULL AND
                        duration_seconds IS NOT NULL
                    )
                ),
                UNIQUE(draft_exercise_row_id, position)
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_continuous_activity(
                id INTEGER PRIMARY KEY,
                draft_exercise_row_id INTEGER NOT NULL UNIQUE
                    REFERENCES draft_session_exercises(id)
                    ON DELETE CASCADE,
                duration_seconds INTEGER NOT NULL
                    CHECK(duration_seconds > 0),
                speed_kmh REAL
                    CHECK(speed_kmh > 0.0),
                distance_km REAL
                    CHECK(distance_km > 0.0)
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_max_results(
                draft_exercise_row_id INTEGER PRIMARY KEY
                    REFERENCES draft_session_exercises(id)
                    ON DELETE CASCADE,
                max_weight_kg REAL NOT NULL
                    CHECK(max_weight_kg > 0.0)
            );
            """.trimIndent()
        )
    }

    private fun createEquipmentTables(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS equipment(
                id INTEGER PRIMARY KEY,
                equipment_id TEXT NOT NULL UNIQUE,
                label_name TEXT NOT NULL,
                display_name TEXT NOT NULL,
                equipment_type TEXT NOT NULL,
                load_semantics TEXT NOT NULL
                    CHECK(load_semantics IN ('none', 'external', 'assistance', 'bodyweight', 'cardio'))
            );
            """.trimIndent(),
        )
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS equipment_aliases(
                equipment_row_id INTEGER NOT NULL REFERENCES equipment(id) ON DELETE CASCADE,
                alias TEXT NOT NULL,
                normalized_alias TEXT NOT NULL,
                UNIQUE(equipment_row_id, normalized_alias)
            );
            """.trimIndent(),
        )
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS exercise_equipment(
                exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE RESTRICT,
                equipment_row_id INTEGER NOT NULL REFERENCES equipment(id) ON DELETE RESTRICT,
                UNIQUE(exercise_row_id, equipment_row_id)
            );
            """.trimIndent(),
        )
        /* WHY: these are logical catalogue capabilities, including exercises
         * which need not yet exist in a user's mutable exercise catalogue. */
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS catalog_exercise_equipment(
                exercise_id TEXT NOT NULL,
                equipment_row_id INTEGER NOT NULL
                    REFERENCES equipment(id)
                    ON DELETE RESTRICT,
                load_semantics TEXT NOT NULL
                    CHECK(load_semantics IN ('none', 'external', 'assistance', 'bodyweight', 'cardio')),
                PRIMARY KEY(exercise_id, equipment_row_id)
            );
            """.trimIndent(),
        )
    }

    private fun migrateEquipmentLoadSemanticsToVersionEight(
        db: SQLiteDatabase,
    ) {
        db.execSQL("PRAGMA defer_foreign_keys = ON;")

        /* Older chained upgrades call the current table creators while still
         * below v9. Those provisional empty result tables must not retain FKs
         * to the v7 names that are rebuilt immediately below. */
        db.execSQL("DROP TABLE IF EXISTS draft_max_results;")
        db.execSQL("DROP TABLE IF EXISTS max_results;")

        db.execSQL("ALTER TABLE equipment_aliases RENAME TO equipment_aliases_v7;")
        db.execSQL("ALTER TABLE exercise_equipment RENAME TO exercise_equipment_v7;")
        db.execSQL("ALTER TABLE catalog_exercise_equipment RENAME TO catalog_exercise_equipment_v7;")
        db.execSQL("ALTER TABLE performed_sets RENAME TO performed_sets_v7;")
        db.execSQL("ALTER TABLE continuous_activity RENAME TO continuous_activity_v7;")
        db.execSQL("ALTER TABLE session_exercises RENAME TO session_exercises_v7;")
        db.execSQL("ALTER TABLE draft_performed_sets RENAME TO draft_performed_sets_v7;")
        db.execSQL("ALTER TABLE draft_continuous_activity RENAME TO draft_continuous_activity_v7;")
        db.execSQL("ALTER TABLE draft_session_exercises RENAME TO draft_session_exercises_v7;")
        db.execSQL("ALTER TABLE equipment RENAME TO equipment_v7;")

        createEquipmentTables(db)
        createSessionTables(db)
        createActiveDraftTables(db)

        db.execSQL(
            "INSERT INTO equipment(id,equipment_id,label_name,display_name,equipment_type,load_semantics) " +
                "SELECT id,equipment_id,label_name,display_name,equipment_type,load_semantics FROM equipment_v7;",
        )
        db.execSQL(
            "INSERT INTO equipment_aliases(equipment_row_id,alias,normalized_alias) " +
                "SELECT equipment_row_id,alias,normalized_alias FROM equipment_aliases_v7;",
        )
        db.execSQL(
            "INSERT INTO exercise_equipment(exercise_row_id,equipment_row_id) " +
                "SELECT exercise_row_id,equipment_row_id FROM exercise_equipment_v7;",
        )
        db.execSQL(
            "INSERT INTO catalog_exercise_equipment(exercise_id,equipment_row_id,load_semantics) " +
                "SELECT exercise_id,equipment_row_id,load_semantics FROM catalog_exercise_equipment_v7;",
        )
        db.execSQL(
            "INSERT INTO session_exercises(id,session_row_id,exercise_row_id,position,recording_mode," +
                "tracking_mode,data_fields,equipment_row_id,entry_id) " +
                "SELECT id,session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields," +
                "equipment_row_id,entry_id FROM session_exercises_v7;",
        )
        db.execSQL(
            "INSERT INTO performed_sets(id,session_exercise_row_id,position,reps,duration_seconds,weight_kg) " +
                "SELECT id,session_exercise_row_id,position,reps,duration_seconds,weight_kg FROM performed_sets_v7;",
        )
        db.execSQL(
            "INSERT INTO continuous_activity(id,session_exercise_row_id,duration_seconds,speed_kmh,distance_km) " +
                "SELECT id,session_exercise_row_id,duration_seconds,speed_kmh,distance_km FROM continuous_activity_v7;",
        )
        db.execSQL(
            "INSERT INTO draft_session_exercises(id,draft_id,exercise_row_id,position,recording_mode," +
                "tracking_mode,data_fields,equipment_row_id,entry_id) " +
                "SELECT id,draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields," +
                "equipment_row_id,entry_id FROM draft_session_exercises_v7;",
        )
        db.execSQL(
            "INSERT INTO draft_performed_sets(id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg) " +
                "SELECT id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg FROM draft_performed_sets_v7;",
        )
        db.execSQL(
            "INSERT INTO draft_continuous_activity(id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km) " +
                "SELECT id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km FROM draft_continuous_activity_v7;",
        )

        db.execSQL("DROP TABLE performed_sets_v7;")
        db.execSQL("DROP TABLE continuous_activity_v7;")
        db.execSQL("DROP TABLE session_exercises_v7;")
        db.execSQL("DROP TABLE draft_performed_sets_v7;")
        db.execSQL("DROP TABLE draft_continuous_activity_v7;")
        db.execSQL("DROP TABLE draft_session_exercises_v7;")
        db.execSQL("DROP TABLE equipment_aliases_v7;")
        db.execSQL("DROP TABLE exercise_equipment_v7;")
        db.execSQL("DROP TABLE catalog_exercise_equipment_v7;")
        db.execSQL("DROP TABLE equipment_v7;")
    }

    private fun migrateExplicitMaxResultsToVersionNine(
        db: SQLiteDatabase,
    ) {
        /*
         * CONTRACT: v9 introduces an occurrence-owned max result without
         * changing exercise profiles or TRAINLOG_FORMAT_V1. A legacy max-test
         * entry is converted only when its sole source row is exactly one
         * successful rep with a positive load. Multiple attempts and every
         * other shape remain byte-for-byte represented by performed_sets.
         */
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS max_results(
                session_exercise_row_id INTEGER PRIMARY KEY
                    REFERENCES session_exercises(id) ON DELETE CASCADE,
                max_weight_kg REAL NOT NULL CHECK(max_weight_kg > 0.0)
            );
            """.trimIndent(),
        )
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_max_results(
                draft_exercise_row_id INTEGER PRIMARY KEY
                    REFERENCES draft_session_exercises(id) ON DELETE CASCADE,
                max_weight_kg REAL NOT NULL CHECK(max_weight_kg > 0.0)
            );
            """.trimIndent(),
        )
        if (!tableHasColumn(db, "active_session_draft", "weight_text")) {
            db.execSQL(
                "ALTER TABLE active_session_draft " +
                    "ADD COLUMN weight_text TEXT NOT NULL DEFAULT '';",
            )
        }
        if (!tableHasColumn(db, "active_session_draft", "max_weight_text")) {
            db.execSQL(
                "ALTER TABLE active_session_draft " +
                    "ADD COLUMN max_weight_text TEXT NOT NULL DEFAULT '';",
            )
        }
        if (!tableHasColumn(db, "active_session_draft", "source_session_id")) {
            db.execSQL(
                "ALTER TABLE active_session_draft ADD COLUMN source_session_id TEXT;",
            )
        }

        db.execSQL(
            """
            INSERT INTO max_results(session_exercise_row_id, max_weight_kg)
            SELECT se.id, ps.weight_kg
            FROM session_exercises AS se
            JOIN sessions AS s ON s.id = se.session_row_id
            JOIN performed_sets AS ps ON ps.session_exercise_row_id = se.id
            WHERE s.session_type = 'max_test'
              AND se.recording_mode = 'sets'
              AND ps.reps = 1
              AND ps.duration_seconds IS NULL
              AND ps.weight_kg > 0.0
              AND (SELECT COUNT(*) FROM performed_sets AS all_ps
                   WHERE all_ps.session_exercise_row_id = se.id) = 1;
            """.trimIndent(),
        )
        db.execSQL(
            "DELETE FROM performed_sets WHERE session_exercise_row_id " +
                "IN (SELECT session_exercise_row_id FROM max_results);",
        )
        db.execSQL(
            """
            INSERT INTO draft_max_results(draft_exercise_row_id, max_weight_kg)
            SELECT de.id, ps.weight_kg
            FROM draft_session_exercises AS de
            JOIN active_session_draft AS d ON d.id = de.draft_id
            JOIN draft_performed_sets AS ps ON ps.draft_exercise_row_id = de.id
            WHERE d.session_type = 'max_test'
              AND de.recording_mode = 'sets'
              AND ps.reps = 1
              AND ps.duration_seconds IS NULL
              AND ps.weight_kg > 0.0
              AND (SELECT COUNT(*) FROM draft_performed_sets AS all_ps
                   WHERE all_ps.draft_exercise_row_id = de.id) = 1;
            """.trimIndent(),
        )
        db.execSQL(
            "DELETE FROM draft_performed_sets WHERE draft_exercise_row_id " +
                "IN (SELECT draft_exercise_row_id FROM draft_max_results);",
        )
    }

    private fun tableHasColumn(
        db: SQLiteDatabase,
        table: String,
        column: String,
    ): Boolean =
        db.rawQuery("PRAGMA table_info($table);", null).use { cursor ->
            var found = false
            while (cursor.moveToNext()) {
                if (cursor.getString(1) == column) {
                    found = true
                    break
                }
            }
            found
        }

    private fun addEquipmentReferenceColumns(
        db: SQLiteDatabase,
    ) {
        /* CONTRACT: v5 retains all historic/draft rows unchanged; equipment
         * is optional because it was not captured before this version. */
        db.execSQL(
            "ALTER TABLE session_exercises ADD COLUMN equipment_row_id INTEGER " +
                "REFERENCES equipment(id) ON DELETE SET NULL;",
        )
        db.execSQL(
            "ALTER TABLE draft_session_exercises ADD COLUMN equipment_row_id INTEGER " +
                "REFERENCES equipment(id) ON DELETE SET NULL;",
        )
        db.execSQL(
            "ALTER TABLE active_session_draft ADD COLUMN selected_equipment_id TEXT;",
        )
    }

    private fun seedEquipment(
        db: SQLiteDatabase,
    ) {
        /* INVARIANT: INSERT OR IGNORE makes application startup idempotent;
         * canonical IDs, rather than display text, are the persistent keys. */
        EquipmentCatalog.load(appContext).forEach { entry ->
            db.execSQL(
                """
                INSERT OR IGNORE INTO equipment(
                    equipment_id, label_name, display_name, equipment_type, load_semantics
                ) VALUES(?, ?, ?, ?, ?);
                """.trimIndent(),
                arrayOf(
                    entry.equipmentId,
                    entry.labelName,
                    entry.displayName,
                    entry.type,
                    entry.loadSemantics.name.lowercase(),
                ),
            )
            db.rawQuery(
                "SELECT id FROM equipment WHERE equipment_id = ?;",
                arrayOf(entry.equipmentId),
            ).use { cursor ->
                check(cursor.moveToFirst())
                val rowId = cursor.getLong(0)
                entry.aliases.forEach { alias ->
                    db.execSQL(
                        """
                        INSERT OR IGNORE INTO equipment_aliases(
                            equipment_row_id, alias, normalized_alias
                        ) VALUES(?, ?, ?);
                        """.trimIndent(),
                        arrayOf<Any>(
                            rowId,
                            alias,
                            equipmentAliasNormalize(alias),
                        ),
                    )
                }
            }
        }
        EquipmentCatalog.exerciseEquipmentRelations(appContext).forEach { relation ->
            db.rawQuery(
                "SELECT id FROM equipment WHERE equipment_id = ?;",
                arrayOf(relation.equipmentId),
            ).use { cursor ->
                check(cursor.moveToFirst()) {
                    "Seeded equipment missing: ${relation.equipmentId}"
                }
                db.execSQL(
                    "INSERT OR IGNORE INTO catalog_exercise_equipment(" +
                        "exercise_id, equipment_row_id, load_semantics) VALUES(?, ?, ?);",
                    arrayOf<Any>(
                        relation.exerciseId,
                        cursor.getLong(0),
                        relation.loadSemantics.name.lowercase(Locale.ROOT),
                    ),
                )
            }
        }
    }
}
