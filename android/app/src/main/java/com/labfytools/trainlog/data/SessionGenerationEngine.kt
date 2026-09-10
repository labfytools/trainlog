package com.labfytools.trainlog.data

import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionLoadMode
import com.labfytools.trainlog.model.TrackingMode

enum class GenerationWarningLevel { NONE, NOTICE, WARNING }

data class GenerationHistoryRow(
    val sessionId: String, val occurrenceId: String, val exerciseId: String, val startedAt: String,
    val equipmentId: String?, val recordingMode: RecordingMode, val trackingMode: TrackingMode,
    val loadMode: SessionLoadMode, val restSeconds: Int, val hasAnyTarget: Boolean,
    val setPosition: Int?, val repetitions: Int?, val weightKg: Double?, val hasExplicitMax: Boolean = false,
)

fun interface GenerationHistoryReader {
    /** The repository calls this once inside one read transaction and streams all occurrences. */
    fun read(visitor: (GenerationHistoryRow) -> Unit)
}

data class GenerationCandidate(
    val exerciseId: String, val equipmentId: String, val primaryZoneId: String,
    val secondaryZoneIds: List<String>, val patternIds: List<String>, val sourceRefIds: List<String>,
    val confidence: KnowledgeConfidence, val equipmentLoadSemantics: EquipmentLoadSemantics?,
)

data class GenerationRequest(
    val zoneId: String, val goalId: String, val durationMinutes: Int, val referenceTime: String,
    val candidates: List<GenerationCandidate>, val preferredExerciseIds: Set<String> = emptySet(),
    val excludedExerciseIds: Set<String> = emptySet(), val excludedPatternIds: Set<String> = emptySet(),
)

data class ExposureWindowSummary(
    val primarySetCount: Int, val secondarySetCount: Int, val sessionCount: Int,
    val patternIds: List<String>,
)

data class BodyZoneRecentExposure(
    val within24h: ExposureWindowSummary, val within72h: ExposureWindowSummary,
    val recentExposure: Boolean, val repeatedExposure: Boolean, val warningLevel: GenerationWarningLevel,
    val latestStartedAt: String?, val latestSessionId: String?, val latestOccurrenceId: String?,
    val latestPatternIds: List<String>, val unclassifiedActualSetCount: Int,
)

data class TrainingRecencyWarning(val recentSameExercise: Boolean, val recentSamePattern: Boolean)

data class GeneratedSessionExercise(
    val exerciseId: String, val equipmentId: String, val equipmentLoadSemantics: EquipmentLoadSemantics?,
    val primaryZoneId: String, val secondaryZoneIds: List<String>, val patternIds: List<String>,
    val targetSets: Int, val targetRepetitions: Int, val restSeconds: Int,
    val targetWeightKg: Double?, val plannedLoadMode: SessionLoadMode, val estimatedSeconds: Int,
    val confidence: KnowledgeConfidence, val recency: TrainingRecencyWarning,
    val exposureWarningLevel: GenerationWarningLevel, val rationaleCodes: List<String>,
    val sourceRefIds: List<String>, val loadSourceSessionId: String? = null,
    val loadSourceOccurrenceId: String? = null, val loadSourceStartedAt: String? = null,
)

data class GeneratedSessionSuggestion(
    val exercises: List<GeneratedSessionExercise>, val estimatedDurationSeconds: Int,
    val insufficientResolvedCandidates: Boolean, val exposure: BodyZoneRecentExposure,
    val shortageCodes: List<String> = emptyList(),
)

data class DoseQualification(
    val targetSets: Int, val targetRepetitions: Int, val restSeconds: Int,
    val targetWeightKg: Double?, val plannedLoadMode: SessionLoadMode, val estimatedSeconds: Int,
    val rationaleCode: String, val sourceSessionId: String?, val sourceOccurrenceId: String?,
    val sourceStartedAt: String?,
)

/** Pure deterministic engine. It owns bounded accumulator state and retains no history rows. */
class SessionGenerationEngine(
    private val policy: SessionGenerationPolicy,
    private val knowledge: TrainingKnowledgeCatalog,
    private val bodyZones: BodyZoneCatalog,
) {
    private data class Anchor(val weight: Double, val time: ExactInstant, val text: String, val session: String, val occurrence: String)
    private data class CandidateState(
        val candidate: GenerationCandidate, var recentExercise: Boolean = false, var recentPattern: Boolean = false,
        var primary24: Int = 0, var secondary24: Int = 0, var primary72: Int = 0, var secondary72: Int = 0,
        var secondaryZonePrimary24: Int = 0, var secondaryZoneSecondary24: Int = 0,
        var secondaryZonePrimary72: Int = 0, var secondaryZoneSecondary72: Int = 0,
        var anchor: Anchor? = null, var hasCompatibleExplicitMax: Boolean = false,
    )
    private data class OccurrenceDose(
        val session: String, val occurrence: String, val exercise: String, val equipment: String?,
        val time: ExactInstant, val timeText: String, val loadMode: SessionLoadMode, val rest: Int,
        val hasTargets: Boolean, val qualifying: IntArray, val minimum: DoubleArray,
    )

    fun generate(request: GenerationRequest, history: GenerationHistoryReader): GeneratedSessionSuggestion {
        require(request.durationMinutes in policy.customMinutes)
        val goal = requireNotNull(policy.goals[request.goalId]) { "unknown goal" }
        require(policy.zoneExpansion.containsKey(request.zoneId)) { "unknown generation zone" }
        val reference = ExactInstant.parse(request.referenceTime)
        require(request.candidates.size <= 64)
        require(request.excludedPatternIds.all { knowledge.getMovementPattern(it) != null })
        val duplicateContexts = request.candidates.groupBy { it.exerciseId to it.equipmentId }.values.any { it.size > 1 }
        require(!duplicateContexts)
        val states = request.candidates.map { candidate ->
            validateCandidate(candidate)
            CandidateState(candidate)
        }
        var occurrence: OccurrenceDose? = null
        var previousSetPosition: Int? = null
        var currentSession: String? = null
        var session24 = false
        var session72 = false
        var sessions24 = 0
        var sessions72 = 0
        var primary24 = 0
        var secondary24 = 0
        var primary72 = 0
        var secondary72 = 0
        var unclassified = 0
        val patterns24 = sortedSetOf<String>()
        val patterns72 = sortedSetOf<String>()
        var latest: Triple<ExactInstant, GenerationHistoryRow, List<String>>? = null

        fun finishOccurrence() {
            val dose = occurrence ?: return
            states.forEachIndexed { index, state ->
                val actualOnly = dose.loadMode == SessionLoadMode.NONE && dose.rest == 0 && !dose.hasTargets
                if (dose.exercise == state.candidate.exerciseId && dose.equipment == state.candidate.equipmentId &&
                    state.candidate.equipmentLoadSemantics == EquipmentLoadSemantics.EXTERNAL &&
                    (dose.loadMode == SessionLoadMode.EXTERNAL || actualOnly) && dose.qualifying[index] >= goal.sets &&
                    dose.time.within(reference, policy.loadLookbackSeconds, inclusive = true)) {
                    val candidate = Anchor(dose.minimum[index], dose.time, dose.timeText, dose.session, dose.occurrence)
                    if (state.anchor == null || candidate.time > state.anchor!!.time) state.anchor = candidate
                }
            }
            occurrence = null
            previousSetPosition = null
        }
        fun finishSession() { if (session24) sessions24++; if (session72) sessions72++; session24 = false; session72 = false }

        history.read { row ->
            val instant = try { ExactInstant.parse(row.startedAt) } catch (error: IllegalArgumentException) {
                throw IllegalStateException("invalid stored session timestamp", error)
            }
            if (currentSession != row.sessionId) { finishOccurrence(); finishSession(); currentSession = row.sessionId }
            if (occurrence?.occurrence != row.occurrenceId) {
                finishOccurrence()
                occurrence = OccurrenceDose(row.sessionId, row.occurrenceId, row.exerciseId, row.equipmentId,
                    instant, row.startedAt, row.loadMode, row.restSeconds, row.hasAnyTarget,
                    IntArray(states.size), DoubleArray(states.size) { Double.POSITIVE_INFINITY })
            } else require(occurrence?.session == row.sessionId && occurrence?.exercise == row.exerciseId && occurrence?.time == instant) {
                "inconsistent occurrence rows"
            }
            if (row.hasExplicitMax && row.equipmentId != null && instant <= reference) states.forEach { state ->
                if (row.exerciseId == state.candidate.exerciseId && row.equipmentId == state.candidate.equipmentId)
                    state.hasCompatibleExplicitMax = true
            }
            val position = row.setPosition ?: return@read
            require(position >= 0 && previousSetPosition != position) { "duplicate/invalid performed-set identity" }
            previousSetPosition = position
            val repetitions = row.repetitions ?: return@read
            if (row.recordingMode != RecordingMode.SETS || row.trackingMode != TrackingMode.REPS || repetitions <= 0 || instant > reference) return@read
            val interpretation = knowledge.getExerciseKnowledge(row.exerciseId)?.interpretation
            val isPrimary = interpretation?.let { zoneMatches(it.primaryZoneId, request.zoneId) } == true
            val isSecondary = !isPrimary && interpretation?.secondaryZoneIds?.any { zoneMatches(it, request.zoneId) } == true
            val in24 = instant.within(reference, policy.shortWindowSeconds, inclusive = false)
            val in72 = instant.within(reference, policy.longWindowSeconds, inclusive = false)
            if (interpretation == null) unclassified++
            if (isPrimary || isSecondary) {
                if (in24) { if (isPrimary) primary24++ else secondary24++; session24 = true; patterns24 += interpretation.patternIds }
                if (in72) { if (isPrimary) primary72++ else secondary72++; session72 = true; patterns72 += interpretation.patternIds }
                val prior = latest
                if (prior == null || instant > prior.first || (instant == prior.first &&
                    (row.sessionId > prior.second.sessionId || row.sessionId == prior.second.sessionId && row.occurrenceId > prior.second.occurrenceId)))
                    latest = Triple(instant, row, interpretation.patternIds.sorted())
            }
            states.forEachIndexed { index, state ->
                val candidateInterpretation = interpretation
                val cp = candidateInterpretation?.primaryZoneId == state.candidate.primaryZoneId
                val cs = !cp && candidateInterpretation?.secondaryZoneIds?.any { zoneMatches(it, state.candidate.primaryZoneId) } == true
                if (in24) { if (cp) state.primary24++ else if (cs) state.secondary24++ }
                if (in72) { if (cp) state.primary72++ else if (cs) state.secondary72++ }
                val secondaryPrimary = candidateInterpretation?.let { interpretation ->
                    state.candidate.secondaryZoneIds.any { zoneMatches(interpretation.primaryZoneId, it) }
                } == true
                val secondaryMatch = secondaryPrimary || candidateInterpretation?.let { interpretation ->
                    state.candidate.secondaryZoneIds.any { zone -> interpretation.secondaryZoneIds.any { zoneMatches(it, zone) } }
                } == true
                if (secondaryMatch && in24) { if (secondaryPrimary) state.secondaryZonePrimary24++ else state.secondaryZoneSecondary24++ }
                if (secondaryMatch && in72) { if (secondaryPrimary) state.secondaryZonePrimary72++ else state.secondaryZoneSecondary72++ }
                if (row.exerciseId == state.candidate.exerciseId && instant.within(reference, policy.sameExerciseWindowSeconds, false)) state.recentExercise = true
                if (candidateInterpretation != null && candidateInterpretation.patternIds.any(state.candidate.patternIds::contains) &&
                    instant.within(reference, policy.samePatternWindowSeconds, false)) state.recentPattern = true
                val weight = row.weightKg
                if (row.exerciseId == state.candidate.exerciseId && row.equipmentId == state.candidate.equipmentId &&
                    repetitions >= goal.repetitions && weight != null && weight.isFinite() && weight > 0.0) {
                    occurrence!!.qualifying[index]++
                    occurrence!!.minimum[index] = minOf(occurrence!!.minimum[index], weight)
                }
            }
        }
        finishOccurrence(); finishSession()
        val recent = primary24 >= policy.recentPrimaryThreshold || secondary24 >= policy.recentSecondaryThreshold
        val repeated = primary72 >= policy.repeatedPrimaryThreshold || secondary72 >= policy.repeatedSecondaryThreshold
        val warning = when {
            primary24 >= policy.recentPrimaryThreshold || primary72 >= policy.repeatedPrimaryThreshold -> GenerationWarningLevel.WARNING
            secondary24 >= policy.recentSecondaryThreshold || secondary72 >= policy.repeatedSecondaryThreshold -> GenerationWarningLevel.NOTICE
            else -> GenerationWarningLevel.NONE
        }
        val exposure = BodyZoneRecentExposure(ExposureWindowSummary(primary24, secondary24, sessions24, patterns24.toList()),
            ExposureWindowSummary(primary72, secondary72, sessions72, patterns72.toList()), recent, repeated, warning,
            latest?.second?.startedAt, latest?.second?.sessionId, latest?.second?.occurrenceId, latest?.third.orEmpty(), unclassified)
        return select(request, goal, states, exposure)
    }

    private fun select(request: GenerationRequest, goal: GenerationGoalPolicy, states: List<CandidateState>,
        exposure: BodyZoneRecentExposure): GeneratedSessionSuggestion {
        val selected = mutableListOf<GeneratedSessionExercise>()
        val used = mutableSetOf<Int>()
        val usedPatterns = mutableSetOf<String>()
        val exerciseSeconds = Math.addExact(policy.setupSeconds, Math.addExact(
            Math.multiplyExact(Math.multiplyExact(goal.sets, goal.repetitions), policy.repetitionSeconds),
            Math.multiplyExact(goal.sets - 1, goal.restSeconds)))
        var total = policy.preparationSeconds
        while (selected.size < policy.maxExercises) {
            val eligible = states.indices.filter { index ->
                val state = states[index]
                index !in used && state.candidate.exerciseId !in request.excludedExerciseIds &&
                    state.candidate.patternIds.none(request.excludedPatternIds::contains) &&
                    state.candidate.patternIds.none(usedPatterns::contains) &&
                    candidateMatchesZone(state.candidate, request.zoneId) && total <= request.durationMinutes * 60 - exerciseSeconds
            }
            val best = eligible.maxWithOrNull(Comparator { left, right ->
                val priorityOrder = coveragePriority(states[left].candidate, selected, request.zoneId).compareTo(
                    coveragePriority(states[right].candidate, selected, request.zoneId))
                val scoreOrder = score(states[left], request, selected).compareTo(score(states[right], request, selected))
                if (priorityOrder != 0) priorityOrder else if (scoreOrder != 0) scoreOrder else -tieCompare(states[left], states[right])
            }) ?: break
            val state = states[best]
            val anchor = state.anchor
            val rationale = mutableListOf(if (anchor != null) "observed_repeated_dose_anchor" else if (state.hasCompatibleExplicitMax)
                "explicit_max_present_no_numeric_prescription" else when (state.candidate.equipmentLoadSemantics) {
                EquipmentLoadSemantics.ASSISTANCE -> "assistance_numeric_load_omitted"
                else -> "numeric_load_absent"
            })
            if (state.candidate.exerciseId in request.preferredExerciseIds) rationale += "preferred_exercise"
            rationale += if (zoneMatches(state.candidate.primaryZoneId, request.zoneId)) "requested_primary_zone" else "requested_secondary_zone"
            rationale += if (state.candidate.equipmentLoadSemantics == EquipmentLoadSemantics.EXTERNAL)
                "external_equipment_context" else "non_external_equipment_context"
            rationale += "new_exact_pattern"
            if (state.recentExercise) rationale += "recent_same_exercise_penalty"
            if (state.recentPattern) rationale += "recent_same_pattern_penalty"
            selected += GeneratedSessionExercise(state.candidate.exerciseId, state.candidate.equipmentId,
                state.candidate.equipmentLoadSemantics, state.candidate.primaryZoneId, state.candidate.secondaryZoneIds.sorted(),
                state.candidate.patternIds.sorted(), goal.sets, goal.repetitions, goal.restSeconds, anchor?.weight,
                if (anchor == null) SessionLoadMode.NONE else SessionLoadMode.EXTERNAL, exerciseSeconds, state.candidate.confidence,
                TrainingRecencyWarning(state.recentExercise, state.recentPattern), exposure.warningLevel, rationale,
                state.candidate.sourceRefIds.sorted(), anchor?.session, anchor?.occurrence, anchor?.text)
            used += best; usedPatterns += state.candidate.patternIds; total = Math.addExact(total, exerciseSeconds)
        }
        val insufficient = selected.size < policy.maxExercises
        val shortages = if (!insufficient) emptyList() else buildList {
            add("insufficient_resolved_candidates")
            if (request.zoneId == "full_body") {
                fun region(primary: String) = when (primary) { "core" -> "core"; "chest", "back", "shoulders", "arms" -> "upper"; else -> "lower" }
                val regions = selected.map { region(it.primaryZoneId) }.toSet()
                if ("upper" !in regions) add("missing_upper_region")
                if ("lower" !in regions) add("missing_lower_region")
                if ("core" !in regions) add("missing_core_region")
            } else if (request.zoneId == "upper_body") {
                if (selected.none { it.patternIds.any(policy.upperPushPatterns::contains) }) add("missing_upper_push")
                if (selected.none { it.patternIds.any(policy.upperPullPatterns::contains) }) add("missing_upper_pull")
            } else if (request.zoneId == "lower_body") {
                if (selected.none { it.patternIds.any(policy.lowerExtensionPatterns::contains) }) add("missing_lower_extension")
                if (selected.none { it.patternIds.any(policy.lowerFlexionPatterns::contains) }) add("missing_lower_flexion")
            }
        }
        return GeneratedSessionSuggestion(selected, total, insufficient, exposure, shortages)
    }

    private fun score(state: CandidateState, request: GenerationRequest,
        selected: List<GeneratedSessionExercise>): Int {
        val s = policy.scores
        var value = if (zoneMatches(state.candidate.primaryZoneId, request.zoneId)) s.requestedPrimaryZone else s.requestedSecondaryZoneOnly
        if (selected.none { it.primaryZoneId == state.candidate.primaryZoneId }) value += s.newPrimaryZone
        if (state.candidate.patternIds.none { pattern -> selected.any { pattern in it.patternIds } }) value += s.newPattern
        if (state.anchor != null) value += s.qualifyingWorkingLoadHistory
        if (state.candidate.exerciseId in request.preferredExerciseIds) value += s.preferredExercise
        if (state.recentExercise) value += s.recentSameExercise
        if (state.recentPattern) value += s.recentSamePattern
        if (state.primary24 >= policy.recentPrimaryThreshold) value += s.recentPrimaryThreshold
        if (state.secondary24 >= policy.recentSecondaryThreshold) value += s.recentSecondaryThreshold
        if (state.primary72 >= policy.repeatedPrimaryThreshold) value += s.repeatedPrimaryThreshold
        if (state.secondary72 >= policy.repeatedSecondaryThreshold) value += s.repeatedSecondaryThreshold
        if (state.secondaryZonePrimary24 >= policy.recentPrimaryThreshold ||
            state.secondaryZoneSecondary24 >= policy.recentSecondaryThreshold ||
            state.secondaryZonePrimary72 >= policy.repeatedPrimaryThreshold ||
            state.secondaryZoneSecondary72 >= policy.repeatedSecondaryThreshold) value += s.anySecondaryZoneExposure
        return value
    }

    private fun coveragePriority(candidate: GenerationCandidate, selected: List<GeneratedSessionExercise>, zone: String): Int {
        if (zone == "full_body") {
            fun region(primary: String) = when (primary) {
                "core" -> "core"
                "chest", "back", "shoulders", "arms" -> "upper"
                else -> "lower"
            }
            return if (selected.none { region(it.primaryZoneId) == region(candidate.primaryZoneId) }) 1 else 0
        }
        if (zone == "upper_body") {
            val pushSeen = selected.any { it.patternIds.any(policy.upperPushPatterns::contains) }
            val pullSeen = selected.any { it.patternIds.any(policy.upperPullPatterns::contains) }
            return if ((!pushSeen && candidate.patternIds.any(policy.upperPushPatterns::contains)) ||
                (!pullSeen && candidate.patternIds.any(policy.upperPullPatterns::contains))) 1 else 0
        }
        if (zone == "lower_body") {
            val extensionSeen = selected.any { it.patternIds.any(policy.lowerExtensionPatterns::contains) }
            val flexionSeen = selected.any { it.patternIds.any(policy.lowerFlexionPatterns::contains) }
            return if ((!extensionSeen && candidate.patternIds.any(policy.lowerExtensionPatterns::contains)) ||
                (!flexionSeen && candidate.patternIds.any(policy.lowerFlexionPatterns::contains))) 1 else 0
        }
        return 0
    }

    private fun tieCompare(left: CandidateState, right: CandidateState): Int {
        val exercise = left.candidate.exerciseId.compareTo(right.candidate.exerciseId)
        if (exercise != 0) return exercise
        if ((left.anchor != null) != (right.anchor != null)) return if (left.anchor != null) -1 else 1
        if (left.anchor != null && right.anchor != null && left.anchor!!.time != right.anchor!!.time)
            return -left.anchor!!.time.compareTo(right.anchor!!.time)
        return left.candidate.equipmentId.compareTo(right.candidate.equipmentId)
    }

    private fun validateCandidate(candidate: GenerationCandidate) {
        val record = requireNotNull(knowledge.getExerciseKnowledge(candidate.exerciseId))
        require(record.resolutionStatus == ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED &&
            record.confidence in setOf(KnowledgeConfidence.HIGH, KnowledgeConfidence.MODERATE) &&
            candidate.equipmentId in record.equipmentIds &&
            knowledge.getEquipmentKnowledge(candidate.equipmentId)?.catalogLoadSemantics == candidate.equipmentLoadSemantics)
        val interpretation = requireNotNull(record.interpretation)
        require(candidate.confidence == record.confidence && candidate.primaryZoneId == interpretation.primaryZoneId &&
            candidate.patternIds.isNotEmpty() && candidate.patternIds.size <= 16 &&
            candidate.patternIds.all { it in interpretation.patternIds })
    }
    private fun candidateMatchesZone(candidate: GenerationCandidate, zone: String) =
        zoneMatches(candidate.primaryZoneId, zone) || candidate.secondaryZoneIds.any { zoneMatches(it, zone) }
    private fun zoneMatches(actual: String, requested: String) = actual in requireNotNull(policy.zoneExpansion[requested])

    fun estimateExerciseSeconds(targetSets: Int, targetRepetitions: Int, restSeconds: Int): Int {
        require(targetSets in 1..64 && targetRepetitions in 1..10_000 && restSeconds in 0..86_400)
        return Math.addExact(policy.setupSeconds, Math.addExact(
            Math.multiplyExact(Math.multiplyExact(targetSets, targetRepetitions), policy.repetitionSeconds),
            Math.multiplyExact(targetSets - 1, restSeconds)))
    }

    /** Requalifies one edited exercise only; selection order and other preview rows are untouched. */
    fun requalifyDose(candidate: GenerationCandidate, referenceTime: String, targetSets: Int,
        targetRepetitions: Int, restSeconds: Int, history: GenerationHistoryReader): DoseQualification {
        validateCandidate(candidate)
        val reference = ExactInstant.parse(referenceTime)
        var currentOccurrence: String? = null
        var current: MutableList<GenerationHistoryRow> = mutableListOf()
        var best: Anchor? = null
        var hasMax = false
        fun finish() {
            if (current.isEmpty()) return
            val first = current.first()
            val instant = ExactInstant.parse(first.startedAt)
            val positions = current.mapNotNull { it.setPosition }
            require(positions.size == positions.toSet().size) { "duplicate performed-set identity" }
            val actualOnly = first.loadMode == SessionLoadMode.NONE && first.restSeconds == 0 && !first.hasAnyTarget
            val qualifying = current.filter { it.repetitions != null && it.repetitions >= targetRepetitions &&
                it.weightKg != null && it.weightKg.isFinite() && it.weightKg > 0.0 }
            if (first.exerciseId == candidate.exerciseId && first.equipmentId == candidate.equipmentId &&
                candidate.equipmentLoadSemantics == EquipmentLoadSemantics.EXTERNAL &&
                (first.loadMode == SessionLoadMode.EXTERNAL || actualOnly) && qualifying.size >= targetSets &&
                instant.within(reference, policy.loadLookbackSeconds, true)) {
                val anchor = Anchor(qualifying.minOf { it.weightKg!! }, instant, first.startedAt, first.sessionId, first.occurrenceId)
                if (best == null || anchor.time > best!!.time) best = anchor
            }
            current = mutableListOf()
        }
        history.read { row ->
            try { ExactInstant.parse(row.startedAt) } catch (error: IllegalArgumentException) {
                throw IllegalStateException("invalid stored session timestamp", error)
            }
            val instant = ExactInstant.parse(row.startedAt)
            if (row.hasExplicitMax && row.exerciseId == candidate.exerciseId && row.equipmentId == candidate.equipmentId &&
                instant <= reference) hasMax = true
            if (currentOccurrence != row.occurrenceId) { finish(); currentOccurrence = row.occurrenceId }
            current += row
        }
        finish()
        val anchor = best
        return DoseQualification(targetSets, targetRepetitions, restSeconds, anchor?.weight,
            if (anchor == null) SessionLoadMode.NONE else SessionLoadMode.EXTERNAL,
            estimateExerciseSeconds(targetSets, targetRepetitions, restSeconds),
            if (anchor != null) "observed_repeated_dose_anchor" else if (hasMax)
                "explicit_max_present_no_numeric_prescription" else if (candidate.equipmentLoadSemantics == EquipmentLoadSemantics.ASSISTANCE)
                "assistance_numeric_load_omitted" else "numeric_load_absent",
            anchor?.session, anchor?.occurrence, anchor?.text)
    }
}

/** Exact frozen timestamp key, including arbitrary fractions and offsets through 23:59. */
private data class ExactInstant(val second: Long, val fraction: String) : Comparable<ExactInstant> {
    override fun compareTo(other: ExactInstant): Int {
        val seconds = second.compareTo(other.second); if (seconds != 0) return seconds
        val count = maxOf(fraction.length, other.fraction.length)
        return fraction.padEnd(count, '0').compareTo(other.fraction.padEnd(count, '0'))
    }
    fun within(reference: ExactInstant, seconds: Long, inclusive: Boolean): Boolean {
        if (this > reference) return false
        val delta = Math.subtractExact(reference.second, second)
        return delta < seconds || delta == seconds && if (inclusive) reference.fraction <= fraction else reference.fraction < fraction
    }
    companion object {
        private val regex = Regex("""^(\d{4})-(\d{2})-(\d{2})[Tt](\d{2}):(\d{2})(?::(\d{2})(?:\.(\d+))?)?([Zz]|[+-]\d{2}:\d{2})$""")
        fun parse(text: String): ExactInstant {
            val match = requireNotNull(regex.matchEntire(text)) { "invalid timestamp" }
            val (year, month, day, hour, minute, secondsText, fraction, zone) = match.destructured
            val y = year.toInt(); val m = month.toInt(); val d = day.toInt(); val h = hour.toInt(); val min = minute.toInt(); val sec = secondsText.ifEmpty { "0" }.toInt()
            require(y >= 1 && m in 1..12 && d in 1..daysInMonth(y, m) && h in 0..23 && min in 0..59 && sec in 0..59)
            val sign = when (zone.first()) { '+' -> 1; '-' -> -1; else -> 0 }
            val offset = if (sign == 0) 0 else { val oh = zone.substring(1, 3).toInt(); val om = zone.substring(4, 6).toInt(); require(oh <= 23 && om <= 59); sign * (oh * 3600 + om * 60) }
            var days = (y - 1L) * 365 + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400
            for (prior in 1 until m) days += daysInMonth(y, prior)
            days += d - 1
            return ExactInstant(days * 86_400 + h * 3600 + min * 60 + sec - offset, fraction)
        }
        private fun daysInMonth(year: Int, month: Int): Int = intArrayOf(31, if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) 29 else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)[month - 1]
    }
}
