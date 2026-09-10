package com.labfytools.trainlog.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

data class GenerationGoalPolicy(
    val sets: Int, val repetitions: Int, val restSeconds: Int,
    val setsRange: IntRange, val repetitionsRange: IntRange, val restSecondsRange: IntRange,
)

data class GenerationScorePolicy(
    val requestedPrimaryZone: Int, val requestedSecondaryZoneOnly: Int,
    val newPrimaryZone: Int, val newPattern: Int, val qualifyingWorkingLoadHistory: Int,
    val preferredExercise: Int, val recentSameExercise: Int, val recentSamePattern: Int,
    val recentPrimaryThreshold: Int, val recentSecondaryThreshold: Int,
    val repeatedPrimaryThreshold: Int, val repeatedSecondaryThreshold: Int,
    val anySecondaryZoneExposure: Int,
)

data class SessionGenerationPolicy(
    val goals: Map<String, GenerationGoalPolicy>, val zoneExpansion: Map<String, List<String>>,
    val loadLookbackSeconds: Long, val shortWindowSeconds: Long, val longWindowSeconds: Long,
    val sameExerciseWindowSeconds: Long, val samePatternWindowSeconds: Long,
    val recentPrimaryThreshold: Int, val recentSecondaryThreshold: Int,
    val repeatedPrimaryThreshold: Int, val repeatedSecondaryThreshold: Int,
    val maxExercises: Int, val scores: GenerationScorePolicy,
    val preparationSeconds: Int, val setupSeconds: Int, val repetitionSeconds: Int,
    val customMinutes: IntRange, val durationPresets: List<Int>,
    val upperPushPatterns: Set<String>, val upperPullPatterns: Set<String>,
    val lowerExtensionPatterns: Set<String>, val lowerFlexionPatterns: Set<String>,
)

/** Strict loader for the one authored policy asset. No policy number is repeated in Kotlin. */
object SessionGenerationPolicyLoader {
    private const val ASSET = "session-generation-policy-v1.json"

    fun load(context: Context, knowledge: TrainingKnowledgeCatalog, bodyZones: BodyZoneCatalog): SessionGenerationPolicy =
        context.assets.open(ASSET).bufferedReader().use { load(it.readText(), knowledge, bodyZones) }

    internal fun load(text: String, knowledge: TrainingKnowledgeCatalog, bodyZones: BodyZoneCatalog): SessionGenerationPolicy {
        SessionPolicyDuplicateKeys.validate(text)
        val root = JSONObject(text)
        root.keysExact(setOf("format", "version", "policy_id", "status", "scientific_review_date", "confidence",
            "scope", "numeric_rule_status", "source_refs", "additional_references", "goals",
            "goal_range_interpretation", "zone_expansion", "eligibility", "load", "exposure",
            "selection", "duration", "guidance"), "policy")
        check(root.text("format") == "trainlog-session-generation-policy-v1" && root.int("version", 1, 1) == 1 &&
            root.text("policy_id") == "session_generator_v1") { "unsupported session-generation policy" }
        val knownRefs = knowledge.references.mapTo(mutableSetOf()) { it.refId }
        root.array("additional_references").objects().forEach { row ->
            row.keysExact(setOf("ref_id", "title", "authors_or_organization", "year", "pmid", "doi", "url",
                "type", "notes", "limitations", "accessed_on"), "additional reference")
            check(knownRefs.add(row.text("ref_id"))) { "duplicate policy reference" }
            row.int("year", 1, 9999)
        }
        root.array("source_refs").strings().also { check(it.isNotEmpty() && it.all(knownRefs::contains)) }

        val goalsObject = root.obj("goals")
        goalsObject.keysExact(setOf("general", "strength", "hypertrophy", "endurance"), "goals")
        val goals = goalsObject.keys().asSequence().associateWith { id ->
            val row = goalsObject.obj(id)
            row.keysExact(setOf("sets", "repetitions", "rest_seconds", "sets_range", "repetitions_range",
                "rest_seconds_range"), "goal $id")
            fun range(key: String, max: Int): IntRange {
                val values = row.array(key).ints(0, max)
                check(values.size == 2 && values[0] <= values[1]) { "$key invalid" }
                return values[0]..values[1]
            }
            GenerationGoalPolicy(row.int("sets", 1, 64), row.int("repetitions", 1, 10_000),
                row.int("rest_seconds", 0, 86_400), range("sets_range", 64),
                range("repetitions_range", 10_000), range("rest_seconds_range", 86_400)).also {
                check(it.sets in it.setsRange && it.repetitions in it.repetitionsRange && it.restSeconds in it.restSecondsRange)
            }
        }
        val knownZones = bodyZones.zones.mapTo(mutableSetOf()) { it.zoneId }
        val expansions = root.obj("zone_expansion").let { value ->
            value.keysExact(setOf("full_body", "upper_body", "chest", "back", "shoulders", "arms", "core",
                "lower_body", "glutes", "thighs", "calves"), "zone expansion")
            value.keys().asSequence().associateWith { key -> value.array(key).strings().also {
                check(it.isNotEmpty() && it.size == it.toSet().size && it.all(knownZones::contains))
            } }
        }
        val knownPatterns = knowledge.movementPatterns.mapTo(mutableSetOf()) { it.patternId }
        val load = root.obj("load")
        val exposure = root.obj("exposure")
        val selection = root.obj("selection")
        root.obj("eligibility").keysExact(setOf("recording_mode", "tracking_mode", "knowledge_resolution",
            "allowed_confidence", "require_runtime_exercise_id", "require_explicit_equipment_compatibility",
            "unknown_conditional_and_unlinked_capabilities", "scientific_zone_role", "persisted_zone_disagreement",
            "equipment_choice"), "eligibility")
        load.keysExact(setOf("lookback_seconds", "window", "required_context", "priority", "working_rule",
            "actual_load_mode_compatibility", "planned_load_mode", "working_confidence", "working_meaning",
            "max_rule", "max_confidence", "max_only_does_not_create_performed_sets", "assistance",
            "bodyweight_or_unknown_semantics", "machine_increment", "progression"), "load")
        exposure.keysExact(setOf("short_window_seconds", "long_window_seconds", "window", "timestamp_policy",
            "invalid_timestamp", "counted_rows", "excluded", "zone_source", "primary_secondary",
            "requested_zone_aggregation", "summary_consistency", "recent_primary_sets_24h_threshold",
            "recent_secondary_sets_24h_threshold", "repeated_primary_sets_72h_threshold",
            "repeated_secondary_sets_72h_threshold", "status", "warning_levels", "unknown_history",
            "user_continuation"), "exposure")
        selection.keysExact(setOf("max_exercises", "max_per_exact_pattern", "duplicate_exercise", "pattern_overlap",
            "optional_preferences", "recency", "score", "algorithm", "group_coverage", "upper_push_patterns",
            "upper_pull_patterns", "lower_extension_patterns", "lower_flexion_patterns", "coverage_shortage"), "selection")
        val recency = selection.obj("recency")
        val score = selection.obj("score")
        score.keysExact(setOf("requested_primary_zone", "requested_secondary_zone_only", "new_primary_zone", "new_pattern",
            "qualifying_working_load_history", "preferred_exercise", "recent_same_exercise", "recent_same_pattern",
            "recent_primary_threshold_on_candidate_primary", "recent_secondary_threshold_on_candidate_primary",
            "repeated_primary_threshold_on_candidate_primary", "repeated_secondary_threshold_on_candidate_primary",
            "any_exposure_flag_on_candidate_secondary_zones"), "score")
        fun score(key: String) = score.int(key, -10_000, 10_000)
        fun patterns(key: String) = selection.array(key).strings().also {
            check(it.isNotEmpty() && it.size == it.toSet().size && it.all(knownPatterns::contains))
        }.toSet()
        val duration = root.obj("duration")
        duration.keysExact(setOf("presets_minutes", "custom_min_minutes", "custom_max_minutes", "preparation_seconds",
            "setup_and_transition_seconds_per_exercise", "estimated_seconds_per_repetition", "exercise_seconds_formula",
            "session_seconds_formula", "budget_rule", "precision"), "duration")
        root.obj("guidance").keysExact(setOf("effort", "load", "rest", "recent_exposure"), "guidance")
        return SessionGenerationPolicy(goals, expansions, load.long("lookback_seconds", 1, Int.MAX_VALUE.toLong()),
            exposure.long("short_window_seconds", 1, Int.MAX_VALUE.toLong()),
            exposure.long("long_window_seconds", 1, Int.MAX_VALUE.toLong()),
            recency.long("same_exercise_window_seconds", 1, Int.MAX_VALUE.toLong()),
            recency.long("same_pattern_window_seconds", 1, Int.MAX_VALUE.toLong()),
            exposure.int("recent_primary_sets_24h_threshold", 1, Int.MAX_VALUE),
            exposure.int("recent_secondary_sets_24h_threshold", 1, Int.MAX_VALUE),
            exposure.int("repeated_primary_sets_72h_threshold", 1, Int.MAX_VALUE),
            exposure.int("repeated_secondary_sets_72h_threshold", 1, Int.MAX_VALUE),
            selection.int("max_exercises", 1, 64),
            GenerationScorePolicy(score("requested_primary_zone"), score("requested_secondary_zone_only"),
                score("new_primary_zone"), score("new_pattern"), score("qualifying_working_load_history"),
                score("preferred_exercise"), score("recent_same_exercise"), score("recent_same_pattern"),
                score("recent_primary_threshold_on_candidate_primary"), score("recent_secondary_threshold_on_candidate_primary"),
                score("repeated_primary_threshold_on_candidate_primary"), score("repeated_secondary_threshold_on_candidate_primary"),
                score("any_exposure_flag_on_candidate_secondary_zones")),
            duration.int("preparation_seconds", 1, 86_400),
            duration.int("setup_and_transition_seconds_per_exercise", 1, 86_400),
            duration.int("estimated_seconds_per_repetition", 1, 86_400),
            duration.int("custom_min_minutes", 1, 1440)..duration.int("custom_max_minutes", 1, 1440),
            duration.array("presets_minutes").ints(1, 1440).also {
                check(it.isNotEmpty() && it == it.distinct().sorted()) { "duration presets invalid" }
            },
            patterns("upper_push_patterns"), patterns("upper_pull_patterns"),
            patterns("lower_extension_patterns"), patterns("lower_flexion_patterns"))
    }

    private fun JSONObject.keysExact(expected: Set<String>, where: String) =
        check(keys().asSequence().toSet() == expected) { "$where: invalid keys" }
    private fun JSONObject.text(key: String) = get(key).let { check(it is String && it.isNotBlank()); it }
    private fun JSONObject.obj(key: String) = get(key).let { check(it is JSONObject); it }
    private fun JSONObject.array(key: String) = get(key).let { check(it is JSONArray); it }
    private fun JSONObject.int(key: String, min: Int, max: Int): Int = get(key).let {
        check(it is Int && it in min..max) { "$key: integer outside bounds" }; it
    }
    private fun JSONObject.long(key: String, min: Long, max: Long): Long = int(key, min.toInt(), max.toInt()).toLong()
    private fun JSONArray.objects() = List(length()) { get(it).let { row -> check(row is JSONObject); row } }
    private fun JSONArray.strings() = List(length()) { get(it).let { value -> check(value is String && value.isNotBlank()); value } }
    private fun JSONArray.ints(min: Int, max: Int) = List(length()) { get(it).let { value -> check(value is Int && value in min..max); value } }
}

/** org.json accepts duplicate names; this lexical pass makes deployment failure explicit. */
private object SessionPolicyDuplicateKeys {
    fun validate(source: String) = Parser(source).parse()
    private class Parser(private val source: String) {
        private var at = 0
        fun parse() { value(); ws(); check(at == source.length) }
        private fun value() { ws(); check(at < source.length); when (source[at]) {
            '{' -> obj(); '[' -> array(); '"' -> string(); 't' -> literal("true"); 'f' -> literal("false");
            'n' -> literal("null"); else -> number()
        } }
        private fun obj() { at++; ws(); val keys = mutableSetOf<String>(); if (take('}')) return
            while (true) { val key = string(); check(keys.add(key)) { "duplicate JSON key: $key" }; ws(); expect(':'); value(); ws(); if (take('}')) return; expect(','); ws() } }
        private fun array() { at++; ws(); if (take(']')) return; while (true) { value(); ws(); if (take(']')) return; expect(',') } }
        private fun string(): String { expect('"'); val result = StringBuilder(); while (at < source.length) { val c = source[at++]; when (c) {
            '"' -> return result.toString(); '\\' -> { check(at < source.length); val escaped = source[at++]; if (escaped == 'u') { check(at + 4 <= source.length); val hex = source.substring(at, at + 4); check(hex.all { it.isDigit() || it.lowercaseChar() in 'a'..'f' }); result.append(hex.toInt(16).toChar()); at += 4 } else { check(escaped in "\"\\/bfnrt"); result.append(escaped) } }
            else -> { check(c.code >= 0x20); result.append(c) }
        } }; error("truncated JSON string") }
        private fun literal(value: String) { check(source.startsWith(value, at)); at += value.length }
        private fun number() { if (take('-')) Unit; check(at < source.length && source[at].isDigit()); while (at < source.length && (source[at].isDigit() || source[at] in ".eE+-")) at++ }
        private fun ws() { while (at < source.length && source[at].isWhitespace()) at++ }
        private fun take(c: Char) = if (at < source.length && source[at] == c) { at++; true } else false
        private fun expect(c: Char) = check(take(c)) { "expected $c at $at" }
    }
}
