package com.labfytools.trainlog.data

import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionLoadMode
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SessionGenerationEngineTest {
    private fun JSONArray.strings() = List(length(), ::getString)

    @Test
    fun sharedGoldenFixturesMatchEveryPublicOutputField() {
        val context = ApplicationProvider.getApplicationContext<android.content.Context>()
        val zones = BodyZoneCatalog.load(context)
        val knowledge = TrainingKnowledgeCatalog.load(context)
        val engine = SessionGenerationEngine(SessionGenerationPolicyLoader.load(context, knowledge, zones), knowledge, zones)
        val root = context.assets.open("session-generation-v1.json").bufferedReader().use { JSONObject(it.readText()) }
        assertEquals("trainlog-session-generation-fixtures-v1", root.getString("format"))
        assertEquals(1, root.getInt("version"))

        // CONTRACT: named coverage lives in the authored fixture so neither runner can
        // silently drop a frozen scoring or group-priority rule from the parity corpus.
        val required = root.getJSONArray("required_rule_coverage").strings().toSet()
        val cases = root.getJSONArray("cases")
        val covered = buildSet {
            repeat(cases.length()) { index -> addAll(cases.getJSONObject(index).getJSONArray("covers").strings()) }
        }
        assertEquals(required, covered)
        val templates = root.getJSONObject("expected_exercise_templates")

        repeat(cases.length()) { caseIndex ->
            val fixture = cases.getJSONObject(caseIndex)
            val id = fixture.getString("id")
            val candidatesJson = fixture.getJSONArray("candidates")
            val candidates = List(candidatesJson.length()) { index -> candidatesJson.getJSONObject(index).let { row ->
                GenerationCandidate(
                    row.getString("exercise_id"), row.getString("equipment_id"), row.getString("primary_zone_id"),
                    row.getJSONArray("secondary_zone_ids").strings(), row.getJSONArray("pattern_ids").strings(),
                    row.getJSONArray("source_ref_ids").strings(),
                    KnowledgeConfidence.valueOf(row.getString("confidence").uppercase()),
                    EquipmentLoadSemantics.valueOf(row.getString("equipment_load_semantics").uppercase()),
                )
            } }
            val history = buildList {
                val occurrences = fixture.getJSONArray("history")
                repeat(occurrences.length()) { occurrenceIndex ->
                    val occurrence = occurrences.getJSONObject(occurrenceIndex)
                    val sets = occurrence.optJSONArray("sets")
                    if (sets == null) add(historyRow(occurrence, null))
                    else repeat(sets.length()) { setIndex -> add(historyRow(occurrence, sets.getJSONObject(setIndex))) }
                }
            }
            val preferred = fixture.optJSONArray("preferred_ids")?.strings()?.toSet().orEmpty()
            val result = engine.generate(
                GenerationRequest(fixture.getString("zone_id"), fixture.getString("goal_id"),
                    fixture.getInt("duration_minutes"), fixture.getString("reference_time"), candidates, preferred),
                GenerationHistoryReader { visitor -> history.forEach(visitor) },
            )
            assertCompleteResult(id, fixture.getJSONObject("expected"), templates, result)
        }
    }

    private fun historyRow(occurrence: JSONObject, set: JSONObject?) = GenerationHistoryRow(
        occurrence.getString("session_id"), occurrence.getString("occurrence_id"), occurrence.getString("exercise_id"),
        occurrence.getString("started_at"), if (occurrence.isNull("equipment_id")) null else occurrence.getString("equipment_id"),
        RecordingMode.SETS, TrackingMode.REPS,
        SessionLoadMode.valueOf(occurrence.optString("load_mode", "none").uppercase()),
        occurrence.optInt("rest_seconds", 0), occurrence.optBoolean("has_any_target", false),
        set?.getInt("position"), set?.getInt("repetitions"), set?.optDouble("weight_kg")?.takeUnless { set.isNull("weight_kg") },
        occurrence.optBoolean("explicit_max", false),
    )

    private fun assertCompleteResult(id: String, expected: JSONObject, templates: JSONObject, actual: GeneratedSessionSuggestion) {
        assertEquals(id, expected.getInt("estimated_duration_seconds"), actual.estimatedDurationSeconds)
        assertEquals(id, expected.getBoolean("insufficient_resolved_candidates"), actual.insufficientResolvedCandidates)
        assertEquals(id, expected.getJSONArray("shortage_codes").strings(), actual.shortageCodes)
        assertExposure(id, expected.getJSONObject("exposure"), actual.exposure)
        val exercises = expected.getJSONArray("exercises")
        assertEquals(id, exercises.length(), actual.exercises.size)
        repeat(exercises.length()) { index ->
            val raw = exercises.getJSONObject(index)
            val row = if (raw.has("template")) templates.getJSONObject(raw.getString("template")) else raw
            assertExercise("$id exercise $index", row, actual.exercises[index])
        }
    }

    private fun assertExposure(id: String, expected: JSONObject, actual: BodyZoneRecentExposure) {
        fun window(name: String, value: ExposureWindowSummary) {
            val row = expected.getJSONObject(name)
            assertEquals(id, row.getInt("primary_set_count"), value.primarySetCount)
            assertEquals(id, row.getInt("secondary_set_count"), value.secondarySetCount)
            assertEquals(id, row.getInt("session_count"), value.sessionCount)
            assertEquals(id, row.getJSONArray("pattern_ids").strings(), value.patternIds)
        }
        window("within_24h", actual.within24h)
        window("within_72h", actual.within72h)
        assertEquals(id, expected.getBoolean("recent_exposure"), actual.recentExposure)
        assertEquals(id, expected.getBoolean("repeated_exposure"), actual.repeatedExposure)
        assertEquals(id, expected.getString("warning_level").uppercase(), actual.warningLevel.name)
        assertEquals(id, expected.getInt("unclassified_actual_set_count"), actual.unclassifiedActualSetCount)
        val latest = expected.optJSONObject("latest")
        assertEquals(id, latest?.getString("started_at"), actual.latestStartedAt)
        assertEquals(id, latest?.getString("session_id"), actual.latestSessionId)
        assertEquals(id, latest?.getString("occurrence_id"), actual.latestOccurrenceId)
        assertEquals(id, latest?.getJSONArray("pattern_ids")?.strings().orEmpty(), actual.latestPatternIds)
    }

    private fun assertExercise(id: String, expected: JSONObject, actual: GeneratedSessionExercise) {
        assertEquals(id, expected.getString("exercise_id"), actual.exerciseId)
        assertEquals(id, expected.getString("equipment_id"), actual.equipmentId)
        assertEquals(id, expected.getString("equipment_load_semantics").uppercase(), actual.equipmentLoadSemantics?.name)
        assertEquals(id, expected.getString("primary_zone_id"), actual.primaryZoneId)
        assertEquals(id, expected.getJSONArray("secondary_zone_ids").strings(), actual.secondaryZoneIds)
        assertEquals(id, expected.getJSONArray("pattern_ids").strings(), actual.patternIds)
        assertEquals(id, expected.getInt("target_sets"), actual.targetSets)
        assertEquals(id, expected.getInt("target_repetitions"), actual.targetRepetitions)
        assertEquals(id, expected.getInt("rest_seconds"), actual.restSeconds)
        assertEquals(id, if (expected.isNull("target_weight_kg")) null else expected.getDouble("target_weight_kg"), actual.targetWeightKg)
        assertEquals(id, expected.getString("planned_load_mode").uppercase(), actual.plannedLoadMode.name)
        assertEquals(id, expected.getInt("estimated_seconds"), actual.estimatedSeconds)
        assertEquals(id, expected.getString("confidence").uppercase(), actual.confidence.name)
        val recency = expected.getJSONObject("recency")
        assertEquals(id, recency.getBoolean("recent_same_exercise"), actual.recency.recentSameExercise)
        assertEquals(id, recency.getBoolean("recent_same_pattern"), actual.recency.recentSamePattern)
        assertEquals(id, expected.getString("exposure_warning_level").uppercase(), actual.exposureWarningLevel.name)
        assertEquals(id, expected.getJSONArray("rationale_codes").strings(), actual.rationaleCodes)
        assertEquals(id, expected.getJSONArray("source_ref_ids").strings(), actual.sourceRefIds)
        val source = expected.optJSONObject("load_source")
        assertEquals(id, source?.getString("session_id"), actual.loadSourceSessionId)
        assertEquals(id, source?.getString("occurrence_id"), actual.loadSourceOccurrenceId)
        assertEquals(id, source?.getString("started_at"), actual.loadSourceStartedAt)
    }
}
