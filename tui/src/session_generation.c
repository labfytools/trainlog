#include "trainlog/session_generation.h"

#include "trainlog/body_zone_catalog.h"
#include "trainlog/database.h"
#include "trainlog/training_knowledge.h"
#include "trainlog/session_generation_policy_internal.h"
#include "timestamp.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct CandidateState {
    TrainlogGenerationCandidate candidate;
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    char primary_zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
    char confidence[16];
    char equipment_load_semantics[32];
    char secondary_zone_ids[TRAINLOG_GENERATOR_MAX_ZONES][TRAINLOG_ZONE_ID_MAX + 1U];
    const char *secondary_zone_ptrs[TRAINLOG_GENERATOR_MAX_ZONES];
    char pattern_ids[TRAINLOG_GENERATOR_MAX_PATTERNS][TRAINLOG_ID_MAX + 1U];
    const char *pattern_ptrs[TRAINLOG_GENERATOR_MAX_PATTERNS];
    char source_ref_ids[TRAINLOG_GENERATOR_MAX_SOURCE_REFS][TRAINLOG_ID_MAX + 1U];
    const char *source_ref_ptrs[TRAINLOG_GENERATOR_MAX_SOURCE_REFS];
    bool preferred, excluded, recent_exercise, recent_pattern;
    size_t primary_24, secondary_24, primary_72, secondary_72;
    size_t secondary_zone_primary_24, secondary_zone_secondary_24;
    size_t secondary_zone_primary_72, secondary_zone_secondary_72;
    bool has_anchor;
    bool has_compatible_explicit_max;
    double anchor_weight;
    TrainlogTimestampKey anchor_time;
    char anchor_time_text[TRAINLOG_TIMESTAMP_MAX + 1U];
    char anchor_session[TRAINLOG_ID_MAX + 1U];
    char anchor_occurrence[TRAINLOG_ID_MAX + 1U];
} CandidateState;

struct TrainlogSessionGenerationAnalyzer {
    TrainlogGenerationRequest request;
    char request_zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
    char *reference_owned;
    TrainlogTimestampKey reference;
    const TrainlogSessionGenerationGoalPolicy *goal;
    TrainlogSessionGenerationGoalPolicy edited_goal;
    CandidateState candidates[TRAINLOG_GENERATOR_MAX_CANDIDATES];
    TrainlogBodyZoneRecentExposure exposure;
    bool failed;
    bool have_latest_key;
    TrainlogTimestampKey latest_key;
    char current_session[TRAINLOG_ID_MAX + 1U];
    bool current_session_matches_24, current_session_matches_72;
    char current_occurrence[TRAINLOG_ID_MAX + 1U];
    char current_occurrence_session[TRAINLOG_ID_MAX + 1U];
    char *current_occurrence_time;
    TrainlogTimestampKey current_occurrence_key;
    char current_exercise[TRAINLOG_ID_MAX + 1U];
    char current_equipment[TRAINLOG_ID_MAX + 1U];
    bool current_has_equipment;
    TrainlogLoadMode current_load_mode;
    int current_rest;
    bool current_has_targets;
    size_t qualifying_sets[TRAINLOG_GENERATOR_MAX_CANDIDATES];
    double qualifying_min[TRAINLOG_GENERATOR_MAX_CANDIDATES];
    bool occurrence_open;
    bool have_previous_set;
    size_t previous_set_position;
};

static bool copy_text(char *output, size_t capacity, const char *value)
{
    size_t length;
    if (output == NULL || value == NULL || capacity == 0U) return false;
    length = strlen(value);
    if (length == 0U || length >= capacity) return false;
    (void)memcpy(output, value, length + 1U);
    return true;
}

static char *duplicate_text(const char *value)
{
    size_t length;
    char *copy;
    if (value == NULL) return NULL;
    length = strlen(value);
    if (length == SIZE_MAX) return NULL;
    copy = malloc(length + 1U);
    if (copy != NULL) (void)memcpy(copy, value, length + 1U);
    return copy;
}

static bool candidate_copy(CandidateState *output, const TrainlogGenerationCandidate *input)
{
    size_t index;
    if (!copy_text(output->exercise_id, sizeof(output->exercise_id), input->exercise_id) ||
        !copy_text(output->equipment_id, sizeof(output->equipment_id), input->equipment_id) ||
        !copy_text(output->primary_zone_id, sizeof(output->primary_zone_id), input->primary_zone_id) ||
        !copy_text(output->confidence, sizeof(output->confidence), input->confidence) ||
        !copy_text(output->equipment_load_semantics, sizeof(output->equipment_load_semantics),
            input->equipment_load_semantics)) return false;
    output->candidate = *input;
    output->candidate.exercise_id = output->exercise_id;
    output->candidate.equipment_id = output->equipment_id;
    output->candidate.primary_zone_id = output->primary_zone_id;
    output->candidate.confidence = output->confidence;
    output->candidate.equipment_load_semantics = output->equipment_load_semantics;
    for (index = 0U; index < input->secondary_zone_count; ++index) {
        if (!copy_text(output->secondary_zone_ids[index], sizeof(output->secondary_zone_ids[index]),
                input->secondary_zone_ids[index])) return false;
        output->secondary_zone_ptrs[index] = output->secondary_zone_ids[index];
    }
    for (index = 0U; index < input->pattern_count; ++index) {
        if (!copy_text(output->pattern_ids[index], sizeof(output->pattern_ids[index]),
                input->pattern_ids[index])) return false;
        output->pattern_ptrs[index] = output->pattern_ids[index];
    }
    for (index = 0U; index < input->source_ref_count; ++index) {
        if (!copy_text(output->source_ref_ids[index], sizeof(output->source_ref_ids[index]),
                input->source_ref_ids[index])) return false;
        output->source_ref_ptrs[index] = output->source_ref_ids[index];
    }
    output->candidate.secondary_zone_ids = output->secondary_zone_ptrs;
    output->candidate.pattern_ids = output->pattern_ptrs;
    output->candidate.source_ref_ids = output->source_ref_ptrs;
    return true;
}

static bool id_in(const char *id, const char *const *values, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index)
        if (values[index] != NULL && strcmp(id, values[index]) == 0) return true;
    return false;
}

static bool newline_list_has(const char *list, const char *id)
{
    const char *at = list;
    size_t id_length = strlen(id);
    while (at != NULL && at[0] != '\0') {
        const char *end = strchr(at, '\n');
        size_t length = end == NULL ? strlen(at) : (size_t)(end - at);
        if (length == id_length && memcmp(at, id, length) == 0) return true;
        at = end == NULL ? NULL : end + 1;
    }
    return false;
}

static int fraction_compare(const TrainlogTimestampKey *left, const TrainlogTimestampKey *right)
{
    TrainlogTimestampKey a = *left, b = *right;
    a.utc_second = 0;
    b.utc_second = 0;
    return trainlog_timestamp_compare(&a, &b);
}

static bool nonfuture_within(const TrainlogTimestampKey *instant,
    const TrainlogTimestampKey *reference, int seconds, bool inclusive)
{
    int64_t delta;
    if (trainlog_timestamp_compare(instant, reference) > 0) return false;
    delta = reference->utc_second - instant->utc_second;
    if (delta < (int64_t)seconds) return true;
    if (delta > (int64_t)seconds) return false;
    return inclusive ? fraction_compare(reference, instant) <= 0
                     : fraction_compare(reference, instant) < 0;
}

static bool zone_matches(const char *actual, const char *requested)
{
    size_t index;
    for (index = 0U; index < trainlog_session_generation_policy_v1.zone_expansion_count; ++index) {
        const TrainlogSessionGenerationZoneExpansion *expansion =
            &trainlog_session_generation_policy_v1.zone_expansions[index];
        if (strcmp(expansion->zone_id, requested) == 0)
            return newline_list_has(expansion->expanded_zone_ids, actual);
    }
    return false;
}

static bool interpretation_secondary_matches(
    const TrainlogKnowledgeInterpretation *interpretation, const char *zone)
{
    const char *cursor = interpretation->secondary_zone_ids;
    while (cursor != NULL && cursor[0] != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        char id[TRAINLOG_ZONE_ID_MAX + 1U];
        if (length == 0U || length > TRAINLOG_ZONE_ID_MAX) return false;
        (void)memcpy(id, cursor, length); id[length] = '\0';
        if (zone_matches(id, zone)) return true;
        cursor = end == NULL ? NULL : end + 1;
    }
    return false;
}

static bool candidate_has_pattern(const TrainlogGenerationCandidate *candidate, const char *pattern)
{
    return id_in(pattern, candidate->pattern_ids, candidate->pattern_count);
}

static bool candidate_matches_zone(const TrainlogGenerationCandidate *candidate, const char *zone)
{
    size_t index;
    if (zone_matches(candidate->primary_zone_id, zone)) return true;
    for (index = 0U; index < candidate->secondary_zone_count; ++index)
        if (zone_matches(candidate->secondary_zone_ids[index], zone)) return true;
    return false;
}

static bool interpretation_matches_any_zone(const TrainlogKnowledgeInterpretation *interpretation,
    const char *const *zones, size_t zone_count, bool *primary)
{
    size_t index;
    *primary = false;
    for (index = 0U; index < zone_count; ++index)
        if (zone_matches(interpretation->primary_zone_id, zones[index])) { *primary = true; return true; }
    for (index = 0U; index < zone_count; ++index)
        if (interpretation_secondary_matches(interpretation, zones[index])) return true;
    return false;
}

static bool interpretation_pattern_intersects(
    const TrainlogKnowledgeInterpretation *interpretation,
    const TrainlogGenerationCandidate *candidate)
{
    size_t index;
    for (index = 0U; index < candidate->pattern_count; ++index)
        if (newline_list_has(interpretation->pattern_ids, candidate->pattern_ids[index])) return true;
    return false;
}

static bool add_pattern(char output[][TRAINLOG_ID_MAX + 1U], size_t *count, const char *id)
{
    size_t index, insert;
    if (id == NULL || id[0] == '\0' || strlen(id) > TRAINLOG_ID_MAX) return false;
    for (index = 0U; index < *count; ++index) {
        int order = strcmp(id, output[index]);
        if (order == 0) return true;
        if (order < 0) break;
    }
    if (*count == TRAINLOG_GENERATOR_MAX_PATTERNS) return false;
    insert = index;
    for (index = *count; index > insert; --index)
        (void)memcpy(output[index], output[index - 1U], sizeof(output[index]));
    (void)strcpy(output[insert], id);
    ++*count;
    return true;
}

static bool add_interpretation_patterns(char output[][TRAINLOG_ID_MAX + 1U],
    size_t *count, const char *list)
{
    const char *cursor = list;
    while (cursor != NULL && cursor[0] != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        char id[TRAINLOG_ID_MAX + 1U];
        if (length == 0U || length > TRAINLOG_ID_MAX) return false;
        (void)memcpy(id, cursor, length); id[length] = '\0';
        if (!add_pattern(output, count, id)) return false;
        cursor = end == NULL ? NULL : end + 1;
    }
    return true;
}

static bool candidate_valid(const TrainlogGenerationCandidate *value)
{
    size_t index;
    const TrainlogExerciseKnowledge *knowledge;
    const TrainlogEquipmentKnowledge *equipment;
    if (value == NULL || value->exercise_id == NULL || value->equipment_id == NULL ||
        value->primary_zone_id == NULL || value->confidence == NULL ||
        value->equipment_load_semantics == NULL || value->pattern_count == 0U ||
        value->pattern_count > TRAINLOG_GENERATOR_MAX_PATTERNS ||
        value->secondary_zone_count > TRAINLOG_GENERATOR_MAX_ZONES ||
        value->source_ref_count > TRAINLOG_GENERATOR_MAX_SOURCE_REFS ||
        (strcmp(value->confidence, "high") != 0 && strcmp(value->confidence, "moderate") != 0) ||
        trainlog_body_zone_catalog_lookup(value->primary_zone_id) == NULL) return false;
    knowledge = trainlog_exercise_knowledge_lookup(value->exercise_id);
    equipment = trainlog_equipment_knowledge_lookup(value->equipment_id);
    if (knowledge == NULL || knowledge->interpretation == NULL || equipment == NULL ||
        strcmp(knowledge->resolution_status, "resolved_family_variant_limited") != 0 ||
        !newline_list_has(knowledge->equipment_ids, value->equipment_id) ||
        equipment->catalog_load_semantics == NULL ||
        strcmp(equipment->catalog_load_semantics, value->equipment_load_semantics) != 0 ||
        strcmp(knowledge->interpretation->primary_zone_id, value->primary_zone_id) != 0 ||
        strcmp(knowledge->confidence, value->confidence) != 0)
        return false;
    for (index = 0U; index < value->pattern_count; ++index)
        if (value->pattern_ids == NULL || value->pattern_ids[index] == NULL ||
            trainlog_knowledge_movement_pattern_lookup(value->pattern_ids[index]) == NULL ||
            !newline_list_has(knowledge->interpretation->pattern_ids, value->pattern_ids[index])) return false;
    for (index = 0U; index < value->secondary_zone_count; ++index)
        if (value->secondary_zone_ids == NULL || value->secondary_zone_ids[index] == NULL ||
            trainlog_body_zone_catalog_lookup(value->secondary_zone_ids[index]) == NULL ||
            !newline_list_has(knowledge->interpretation->secondary_zone_ids,
                value->secondary_zone_ids[index])) return false;
    for (index = 0U; index < value->source_ref_count; ++index)
        if (value->source_ref_ids == NULL || value->source_ref_ids[index] == NULL) return false;
    return true;
}

static void finish_session(TrainlogSessionGenerationAnalyzer *analyzer)
{
    if (analyzer->current_session[0] == '\0') return;
    if (analyzer->current_session_matches_24) ++analyzer->exposure.within_24h.session_count;
    if (analyzer->current_session_matches_72) ++analyzer->exposure.within_72h.session_count;
    analyzer->current_session_matches_24 = false;
    analyzer->current_session_matches_72 = false;
}

static void finish_occurrence(TrainlogSessionGenerationAnalyzer *analyzer)
{
    size_t index;
    if (!analyzer->occurrence_open) return;
    for (index = 0U; index < analyzer->request.candidate_count; ++index) {
        CandidateState *state = &analyzer->candidates[index];
        bool actual_only = analyzer->current_load_mode == TRAINLOG_LOAD_NONE &&
            analyzer->current_rest == 0 && !analyzer->current_has_targets;
        bool load_compatible = analyzer->current_load_mode == TRAINLOG_LOAD_EXTERNAL || actual_only;
        if (strcmp(state->candidate.exercise_id, analyzer->current_exercise) == 0 &&
            analyzer->current_has_equipment &&
            strcmp(state->candidate.equipment_id, analyzer->current_equipment) == 0 &&
            strcmp(state->candidate.equipment_load_semantics, "external") == 0 && load_compatible &&
            analyzer->qualifying_sets[index] >= (size_t)analyzer->goal->sets &&
            nonfuture_within(&analyzer->current_occurrence_key, &analyzer->reference,
                trainlog_session_generation_policy_v1.load_lookback_seconds, true) &&
            (!state->has_anchor || trainlog_timestamp_compare(&analyzer->current_occurrence_key,
                &state->anchor_time) > 0)) {
            if (!copy_text(state->anchor_time_text, sizeof(state->anchor_time_text),
                    analyzer->current_occurrence_time)) {
                analyzer->failed = true;
                break;
            }
            state->has_anchor = true;
            state->anchor_weight = analyzer->qualifying_min[index];
            if (!trainlog_timestamp_parse(state->anchor_time_text, strlen(state->anchor_time_text),
                    &state->anchor_time)) {
                analyzer->failed = true;
                break;
            }
            (void)strcpy(state->anchor_session, analyzer->current_occurrence_session);
            (void)strcpy(state->anchor_occurrence, analyzer->current_occurrence);
        }
    }
    free(analyzer->current_occurrence_time);
    analyzer->current_occurrence_time = NULL;
    analyzer->occurrence_open = false;
    analyzer->have_previous_set = false;
}

TrainlogStatus trainlog_session_generation_analyzer_create(
    const TrainlogGenerationRequest *request, TrainlogSessionGenerationAnalyzer **output)
{
    TrainlogSessionGenerationAnalyzer *result;
    size_t index, other;
    if (output == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output = NULL;
    if (request == NULL || request->zone_id == NULL || request->goal_id == NULL ||
        request->reference_time == NULL || request->candidate_count > TRAINLOG_GENERATOR_MAX_CANDIDATES ||
        (request->candidate_count > 0U && request->candidates == NULL) ||
        request->duration_minutes < trainlog_session_generation_policy_v1.min_minutes ||
        request->duration_minutes > trainlog_session_generation_policy_v1.max_minutes ||
        trainlog_body_zone_catalog_lookup(request->zone_id) == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    result = calloc(1U, sizeof(*result));
    if (result == NULL) return TRAINLOG_STATUS_SYSTEM_ERROR;
    result->request = *request;
    result->request.candidates = NULL;
    result->request.goal_id = NULL;
    result->request.reference_time = NULL;
    result->request.preferred_exercise_ids = NULL;
    result->request.preferred_count = 0U;
    result->request.excluded_exercise_ids = NULL;
    result->request.excluded_exercise_count = 0U;
    result->request.excluded_pattern_ids = NULL;
    result->request.excluded_pattern_count = 0U;
    if (!copy_text(result->request_zone_id, sizeof(result->request_zone_id), request->zone_id)) {
        free(result); return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    result->request.zone_id = result->request_zone_id;
    result->reference_owned = duplicate_text(request->reference_time);
    if (result->reference_owned == NULL) { free(result); return TRAINLOG_STATUS_SYSTEM_ERROR; }
    if (!trainlog_timestamp_parse(result->reference_owned, strlen(result->reference_owned), &result->reference)) {
        free(result->reference_owned);
        free(result); return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < trainlog_session_generation_policy_v1.goal_count; ++index)
        if (strcmp(request->goal_id, trainlog_session_generation_policy_v1.goals[index].id) == 0)
            result->goal = &trainlog_session_generation_policy_v1.goals[index];
    if (result->goal == NULL) {
        free(result->reference_owned); free(result); return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < request->excluded_pattern_count; ++index)
        if (request->excluded_pattern_ids == NULL || request->excluded_pattern_ids[index] == NULL ||
            trainlog_knowledge_movement_pattern_lookup(request->excluded_pattern_ids[index]) == NULL) {
            free(result->reference_owned); free(result); return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
    for (index = 0U; index < request->candidate_count; ++index) {
        if (!candidate_valid(&request->candidates[index])) {
            free(result->reference_owned); free(result); return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        for (other = 0U; other < index; ++other)
            if (strcmp(request->candidates[index].exercise_id, request->candidates[other].exercise_id) == 0 &&
                strcmp(request->candidates[index].equipment_id, request->candidates[other].equipment_id) == 0) {
                free(result->reference_owned); free(result); return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
        if (!candidate_copy(&result->candidates[index], &request->candidates[index])) {
            free(result->reference_owned); free(result); return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        result->candidates[index].preferred = id_in(request->candidates[index].exercise_id,
            request->preferred_exercise_ids, request->preferred_count);
        result->candidates[index].excluded = id_in(request->candidates[index].exercise_id,
            request->excluded_exercise_ids, request->excluded_exercise_count);
        for (other = 0U; other < request->excluded_pattern_count; ++other)
            if (candidate_has_pattern(&request->candidates[index], request->excluded_pattern_ids[other]))
                result->candidates[index].excluded = true;
        result->qualifying_min[index] = HUGE_VAL;
    }
    *output = result;
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus begin_occurrence(TrainlogSessionGenerationAnalyzer *analyzer,
    const TrainlogGenerationHistoryRow *row)
{
    size_t index;
    finish_occurrence(analyzer);
    if (!copy_text(analyzer->current_occurrence, sizeof(analyzer->current_occurrence), row->occurrence_id) ||
        !copy_text(analyzer->current_occurrence_session, sizeof(analyzer->current_occurrence_session), row->session_id) ||
        row->exercise_id == NULL ||
        !copy_text(analyzer->current_exercise, sizeof(analyzer->current_exercise), row->exercise_id))
        return TRAINLOG_STATUS_DATABASE_ERROR;
    analyzer->current_occurrence_time = duplicate_text(row->started_at);
    if (analyzer->current_occurrence_time == NULL) return TRAINLOG_STATUS_SYSTEM_ERROR;
    if (!trainlog_timestamp_parse(analyzer->current_occurrence_time,
            strlen(analyzer->current_occurrence_time), &analyzer->current_occurrence_key))
        return TRAINLOG_STATUS_DATABASE_ERROR;
    analyzer->current_has_equipment = row->equipment_id != NULL;
    if (analyzer->current_has_equipment &&
        !copy_text(analyzer->current_equipment, sizeof(analyzer->current_equipment), row->equipment_id))
        return TRAINLOG_STATUS_DATABASE_ERROR;
    analyzer->current_load_mode = row->load_mode;
    analyzer->current_rest = row->rest_seconds;
    analyzer->current_has_targets = row->has_target_sets || row->has_target_reps ||
        row->has_target_duration || row->has_target_weight;
    analyzer->occurrence_open = true;
    for (index = 0U; index < analyzer->request.candidate_count; ++index) {
        analyzer->qualifying_sets[index] = 0U;
        analyzer->qualifying_min[index] = HUGE_VAL;
    }
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_session_generation_analyzer_accept(
    TrainlogSessionGenerationAnalyzer *analyzer, const TrainlogGenerationHistoryRow *row)
{
    TrainlogTimestampKey key;
    const TrainlogExerciseKnowledge *knowledge;
    const TrainlogKnowledgeInterpretation *interpretation;
    bool primary = false, secondary = false, in24, in72;
    size_t index;
    if (analyzer == NULL || row == NULL || analyzer->failed || row->session_id == NULL ||
        row->occurrence_id == NULL || row->exercise_id == NULL || row->started_at == NULL ||
        !trainlog_timestamp_parse(row->started_at, strlen(row->started_at), &key)) {
        if (analyzer != NULL) analyzer->failed = true;
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (analyzer->current_session[0] == '\0' || strcmp(analyzer->current_session, row->session_id) != 0) {
        finish_session(analyzer);
        if (!copy_text(analyzer->current_session, sizeof(analyzer->current_session), row->session_id)) {
            analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    if (!analyzer->occurrence_open || strcmp(analyzer->current_occurrence, row->occurrence_id) != 0) {
        TrainlogStatus status = begin_occurrence(analyzer, row);
        if (status != TRAINLOG_STATUS_OK) { analyzer->failed = true; return status; }
    } else if (strcmp(analyzer->current_occurrence_session, row->session_id) != 0 ||
        strcmp(analyzer->current_exercise, row->exercise_id) != 0 ||
        trainlog_timestamp_compare(&analyzer->current_occurrence_key, &key) != 0) {
        analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (row->has_explicit_max && row->equipment_id != NULL &&
        trainlog_timestamp_compare(&key, &analyzer->reference) <= 0) {
        for (index = 0U; index < analyzer->request.candidate_count; ++index) {
            CandidateState *state = &analyzer->candidates[index];
            if (strcmp(row->exercise_id, state->candidate.exercise_id) == 0 &&
                strcmp(row->equipment_id, state->candidate.equipment_id) == 0)
                state->has_compatible_explicit_max = true;
        }
    }
    if (!row->has_actual_set) return TRAINLOG_STATUS_OK;
    if (analyzer->have_previous_set && analyzer->previous_set_position == row->set_position) {
        analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    analyzer->have_previous_set = true;
    analyzer->previous_set_position = row->set_position;
    if (row->recording_mode != TRAINLOG_RECORDING_SETS || row->tracking_mode != TRAINLOG_TRACKING_REPS ||
        row->repetitions <= 0 || trainlog_timestamp_compare(&key, &analyzer->reference) > 0) return TRAINLOG_STATUS_OK;
    knowledge = trainlog_exercise_knowledge_lookup(row->exercise_id);
    interpretation = knowledge == NULL ? NULL : knowledge->interpretation;
    if (interpretation == NULL) {
        ++analyzer->exposure.unclassified_actual_set_count;
    } else {
        primary = zone_matches(interpretation->primary_zone_id, analyzer->request.zone_id);
        secondary = !primary && interpretation_secondary_matches(interpretation, analyzer->request.zone_id);
    }
    in24 = nonfuture_within(&key, &analyzer->reference,
        trainlog_session_generation_policy_v1.short_window_seconds, false);
    in72 = nonfuture_within(&key, &analyzer->reference,
        trainlog_session_generation_policy_v1.long_window_seconds, false);
    if (primary || secondary) {
        if (in24) {
            if (primary) ++analyzer->exposure.within_24h.primary_set_count; else ++analyzer->exposure.within_24h.secondary_set_count;
            analyzer->current_session_matches_24 = true;
            if (!add_interpretation_patterns(analyzer->exposure.within_24h.pattern_ids,
                &analyzer->exposure.within_24h.pattern_count, interpretation->pattern_ids)) {
                analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
            }
        }
        if (in72) {
            if (primary) ++analyzer->exposure.within_72h.primary_set_count; else ++analyzer->exposure.within_72h.secondary_set_count;
            analyzer->current_session_matches_72 = true;
            if (!add_interpretation_patterns(analyzer->exposure.within_72h.pattern_ids,
                &analyzer->exposure.within_72h.pattern_count, interpretation->pattern_ids)) {
                analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
            }
        }
        if (!analyzer->have_latest_key || trainlog_timestamp_compare(&key,
                &analyzer->latest_key) > 0 ||
            (trainlog_timestamp_compare(&key, &analyzer->latest_key) == 0 &&
             (strcmp(row->session_id, analyzer->exposure.latest_session_id) > 0 ||
              (strcmp(row->session_id, analyzer->exposure.latest_session_id) == 0 &&
               strcmp(row->occurrence_id, analyzer->exposure.latest_occurrence_id) > 0)))) {
            analyzer->exposure.has_latest = true;
            analyzer->have_latest_key = true;
            analyzer->exposure.latest_pattern_count = 0U;
            if (!copy_text(analyzer->exposure.latest_started_at, sizeof(analyzer->exposure.latest_started_at), row->started_at) ||
                !copy_text(analyzer->exposure.latest_session_id, sizeof(analyzer->exposure.latest_session_id), row->session_id) ||
                !copy_text(analyzer->exposure.latest_occurrence_id, sizeof(analyzer->exposure.latest_occurrence_id), row->occurrence_id)) {
                analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            if (!trainlog_timestamp_parse(analyzer->exposure.latest_started_at,
                    strlen(analyzer->exposure.latest_started_at), &analyzer->latest_key)) {
                analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
            }
        }
        if (analyzer->have_latest_key && trainlog_timestamp_compare(&key, &analyzer->latest_key) == 0 &&
            strcmp(row->session_id, analyzer->exposure.latest_session_id) == 0 &&
            strcmp(row->occurrence_id, analyzer->exposure.latest_occurrence_id) == 0 &&
            !add_interpretation_patterns(analyzer->exposure.latest_pattern_ids,
                &analyzer->exposure.latest_pattern_count, interpretation->pattern_ids)) {
            analyzer->failed = true; return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    for (index = 0U; index < analyzer->request.candidate_count; ++index) {
        CandidateState *state = &analyzer->candidates[index];
        bool cp = interpretation != NULL && strcmp(interpretation->primary_zone_id,
            state->candidate.primary_zone_id) == 0;
        bool cs = !cp && interpretation != NULL && interpretation_secondary_matches(
            interpretation, state->candidate.primary_zone_id);
        bool secondary_primary = false;
        bool secondary_match = interpretation != NULL && interpretation_matches_any_zone(
            interpretation, state->candidate.secondary_zone_ids,
            state->candidate.secondary_zone_count, &secondary_primary);
        if (in24) { if (cp) ++state->primary_24; else if (cs) ++state->secondary_24; }
        if (in72) { if (cp) ++state->primary_72; else if (cs) ++state->secondary_72; }
        if (secondary_match && in24) {
            if (secondary_primary) ++state->secondary_zone_primary_24;
            else ++state->secondary_zone_secondary_24;
        }
        if (secondary_match && in72) {
            if (secondary_primary) ++state->secondary_zone_primary_72;
            else ++state->secondary_zone_secondary_72;
        }
        if (strcmp(row->exercise_id, state->candidate.exercise_id) == 0 &&
            nonfuture_within(&key, &analyzer->reference,
                trainlog_session_generation_policy_v1.exercise_recency_seconds, false)) state->recent_exercise = true;
        if (interpretation != NULL && interpretation_pattern_intersects(interpretation, &state->candidate) &&
            nonfuture_within(&key, &analyzer->reference,
                trainlog_session_generation_policy_v1.pattern_recency_seconds, false)) state->recent_pattern = true;
        if (row->has_weight && isfinite(row->weight_kg) && row->weight_kg > 0.0 &&
            row->repetitions >= analyzer->goal->repetitions &&
            strcmp(row->exercise_id, state->candidate.exercise_id) == 0 && row->equipment_id != NULL &&
            strcmp(row->equipment_id, state->candidate.equipment_id) == 0) {
            ++analyzer->qualifying_sets[index];
            if (row->weight_kg < analyzer->qualifying_min[index]) analyzer->qualifying_min[index] = row->weight_kg;
        }
    }
    return TRAINLOG_STATUS_OK;
}

static bool pattern_overlap(const TrainlogGeneratedSession *output,
    const TrainlogGenerationCandidate *candidate)
{
    size_t selected, left, right;
    for (selected = 0U; selected < output->exercise_count; ++selected)
        for (left = 0U; left < output->exercises[selected].pattern_count; ++left)
            for (right = 0U; right < candidate->pattern_count; ++right)
                if (strcmp(output->exercises[selected].pattern_ids[left], candidate->pattern_ids[right]) == 0) return true;
    return false;
}

static bool output_has_primary(const TrainlogGeneratedSession *output, const char *zone)
{
    size_t index;
    for (index = 0U; index < output->exercise_count; ++index)
        if (strcmp(output->exercises[index].primary_zone_id, zone) == 0) return true;
    return false;
}

static int candidate_score(const TrainlogSessionGenerationAnalyzer *analyzer, size_t index,
    const TrainlogGeneratedSession *output)
{
    const CandidateState *state = &analyzer->candidates[index];
    const TrainlogSessionGenerationScores *s = &trainlog_session_generation_policy_v1.scores;
    int score = zone_matches(state->candidate.primary_zone_id, analyzer->request.zone_id)
        ? s->requested_primary : s->requested_secondary;
    if (!output_has_primary(output, state->candidate.primary_zone_id)) score += s->new_primary;
    if (!pattern_overlap(output, &state->candidate)) score += s->new_pattern;
    if (state->has_anchor) score += s->load_history;
    if (state->preferred) score += s->preferred;
    if (state->recent_exercise) score += s->recent_exercise;
    if (state->recent_pattern) score += s->recent_pattern;
    if (state->primary_24 >= (size_t)trainlog_session_generation_policy_v1.primary_24h)
        score += s->exposure_primary_24h;
    if (state->secondary_24 >= (size_t)trainlog_session_generation_policy_v1.secondary_24h)
        score += s->exposure_secondary_24h;
    if (state->primary_72 >= (size_t)trainlog_session_generation_policy_v1.primary_72h)
        score += s->exposure_primary_72h;
    if (state->secondary_72 >= (size_t)trainlog_session_generation_policy_v1.secondary_72h)
        score += s->exposure_secondary_72h;
    if (state->secondary_zone_primary_24 >= (size_t)trainlog_session_generation_policy_v1.primary_24h ||
        state->secondary_zone_secondary_24 >= (size_t)trainlog_session_generation_policy_v1.secondary_24h ||
        state->secondary_zone_primary_72 >= (size_t)trainlog_session_generation_policy_v1.primary_72h ||
        state->secondary_zone_secondary_72 >= (size_t)trainlog_session_generation_policy_v1.secondary_72h)
        score += s->secondary_zone_exposure;
    return score;
}

static bool candidate_has_any_pattern(const TrainlogGenerationCandidate *candidate,
    const char *patterns)
{
    size_t index;
    for (index = 0U; index < candidate->pattern_count; ++index)
        if (newline_list_has(patterns, candidate->pattern_ids[index])) return true;
    return false;
}

static int candidate_coverage_priority(const TrainlogSessionGenerationAnalyzer *analyzer,
    const TrainlogGeneratedSession *output, const TrainlogGenerationCandidate *candidate)
{
    const TrainlogSessionGenerationPolicy *policy = &trainlog_session_generation_policy_v1;
    bool represented_a = false, represented_b = false;
    size_t index, pattern;
    if (strcmp(analyzer->request.zone_id, "full_body") == 0) {
        const char *region = strcmp(candidate->primary_zone_id, "core") == 0 ? "core" :
            (strcmp(candidate->primary_zone_id, "chest") == 0 || strcmp(candidate->primary_zone_id, "back") == 0 ||
             strcmp(candidate->primary_zone_id, "shoulders") == 0 || strcmp(candidate->primary_zone_id, "arms") == 0) ? "upper" : "lower";
        for (index = 0U; index < output->exercise_count; ++index) {
            const char *selected = output->exercises[index].primary_zone_id;
            const char *selected_region = strcmp(selected, "core") == 0 ? "core" :
                (strcmp(selected, "chest") == 0 || strcmp(selected, "back") == 0 ||
                 strcmp(selected, "shoulders") == 0 || strcmp(selected, "arms") == 0) ? "upper" : "lower";
            if (strcmp(region, selected_region) == 0) return 0;
        }
        return 1;
    }
    for (index = 0U; index < output->exercise_count; ++index) {
        bool has_a = false, has_b = false;
        for (pattern = 0U; pattern < output->exercises[index].pattern_count; ++pattern) {
            const char *id = output->exercises[index].pattern_ids[pattern];
            if (strcmp(analyzer->request.zone_id, "upper_body") == 0) {
                has_a = has_a || newline_list_has(policy->upper_push_patterns, id);
                has_b = has_b || newline_list_has(policy->upper_pull_patterns, id);
            } else if (strcmp(analyzer->request.zone_id, "lower_body") == 0) {
                has_a = has_a || newline_list_has(policy->lower_extension_patterns, id);
                has_b = has_b || newline_list_has(policy->lower_flexion_patterns, id);
            }
        }
        represented_a = represented_a || has_a;
        represented_b = represented_b || has_b;
    }
    if (strcmp(analyzer->request.zone_id, "upper_body") == 0)
        return ((!represented_a && candidate_has_any_pattern(candidate, policy->upper_push_patterns)) ||
                (!represented_b && candidate_has_any_pattern(candidate, policy->upper_pull_patterns))) ? 1 : 0;
    if (strcmp(analyzer->request.zone_id, "lower_body") == 0)
        return ((!represented_a && candidate_has_any_pattern(candidate, policy->lower_extension_patterns)) ||
                (!represented_b && candidate_has_any_pattern(candidate, policy->lower_flexion_patterns))) ? 1 : 0;
    return 0;
}

static bool candidate_precedes_on_tie(const CandidateState *left, const CandidateState *right)
{
    int exercise_order = strcmp(left->candidate.exercise_id, right->candidate.exercise_id);
    if (exercise_order != 0) return exercise_order < 0;
    /* CONTRACT: equipment choice for one exercise prefers the newest qualifying
     * actual anchor. MAX never reaches CandidateState and cannot break this tie. */
    if (left->has_anchor != right->has_anchor) return left->has_anchor;
    if (left->has_anchor) {
        int time_order = trainlog_timestamp_compare(&left->anchor_time, &right->anchor_time);
        if (time_order != 0) return time_order > 0;
    }
    return strcmp(left->candidate.equipment_id, right->candidate.equipment_id) < 0;
}

static int exercise_seconds(const TrainlogSessionGenerationAnalyzer *analyzer)
{
    int64_t result = trainlog_session_generation_policy_v1.setup_seconds +
        (int64_t)analyzer->goal->sets * analyzer->goal->repetitions *
        trainlog_session_generation_policy_v1.repetition_seconds +
        (int64_t)(analyzer->goal->sets - 1) * analyzer->goal->rest_seconds;
    return result > INT_MAX ? INT_MAX : (int)result;
}

TrainlogStatus trainlog_session_generation_analyzer_finish(
    TrainlogSessionGenerationAnalyzer *analyzer, TrainlogGeneratedSession *output)
{
    bool used[TRAINLOG_GENERATOR_MAX_CANDIDATES] = {false};
    int budget, per_exercise;
    size_t index;
    if (analyzer == NULL || output == NULL || analyzer->failed) return TRAINLOG_STATUS_DATABASE_ERROR;
    finish_occurrence(analyzer); finish_session(analyzer);
    if (analyzer->failed) return TRAINLOG_STATUS_DATABASE_ERROR;
    (void)memset(output, 0, sizeof(*output));
    output->exposure = analyzer->exposure;
    output->exposure.recent_exposure = output->exposure.within_24h.primary_set_count >= (size_t)trainlog_session_generation_policy_v1.primary_24h ||
        output->exposure.within_24h.secondary_set_count >= (size_t)trainlog_session_generation_policy_v1.secondary_24h;
    output->exposure.repeated_exposure = output->exposure.within_72h.primary_set_count >= (size_t)trainlog_session_generation_policy_v1.primary_72h ||
        output->exposure.within_72h.secondary_set_count >= (size_t)trainlog_session_generation_policy_v1.secondary_72h;
    if (output->exposure.within_24h.primary_set_count >= (size_t)trainlog_session_generation_policy_v1.primary_24h ||
        output->exposure.within_72h.primary_set_count >= (size_t)trainlog_session_generation_policy_v1.primary_72h)
        output->exposure.warning_level = TRAINLOG_GENERATION_WARNING_WARNING;
    else if (output->exposure.within_24h.secondary_set_count >= (size_t)trainlog_session_generation_policy_v1.secondary_24h ||
        output->exposure.within_72h.secondary_set_count >= (size_t)trainlog_session_generation_policy_v1.secondary_72h)
        output->exposure.warning_level = TRAINLOG_GENERATION_WARNING_NOTICE;
    budget = analyzer->request.duration_minutes * 60;
    output->estimated_duration_seconds = trainlog_session_generation_policy_v1.preparation_seconds;
    per_exercise = exercise_seconds(analyzer);
    while (output->exercise_count < (size_t)trainlog_session_generation_policy_v1.max_exercises &&
           output->exercise_count < TRAINLOG_GENERATOR_MAX_SELECTED) {
        size_t best = SIZE_MAX;
        int best_score = INT_MIN;
        int best_priority = INT_MIN;
        for (index = 0U; index < analyzer->request.candidate_count; ++index) {
            CandidateState *state = &analyzer->candidates[index];
            int score;
            if (used[index] || state->excluded || pattern_overlap(output, &state->candidate) ||
                output->estimated_duration_seconds > budget - per_exercise ||
                !candidate_matches_zone(&state->candidate, analyzer->request.zone_id)) continue;
            score = candidate_score(analyzer, index, output);
            {
                int priority = candidate_coverage_priority(analyzer, output, &state->candidate);
                if (best == SIZE_MAX || priority > best_priority ||
                    (priority == best_priority && (score > best_score ||
                     (score == best_score && candidate_precedes_on_tie(
                        state, &analyzer->candidates[best]))))) {
                    best = index; best_score = score; best_priority = priority;
                }
            }
        }
        if (best == SIZE_MAX) break;
        {
            CandidateState *state = &analyzer->candidates[best];
            TrainlogGeneratedExercise *generated = &output->exercises[output->exercise_count];
            if (!copy_text(generated->exercise_id, sizeof(generated->exercise_id), state->candidate.exercise_id) ||
                !copy_text(generated->equipment_id, sizeof(generated->equipment_id), state->candidate.equipment_id) ||
                !copy_text(generated->primary_zone_id, sizeof(generated->primary_zone_id), state->candidate.primary_zone_id))
                return TRAINLOG_STATUS_DATABASE_ERROR;
            generated->target_sets = analyzer->goal->sets;
            generated->target_repetitions = analyzer->goal->repetitions;
            generated->rest_seconds = analyzer->goal->rest_seconds;
            generated->planned_load_mode = state->has_anchor ? TRAINLOG_LOAD_EXTERNAL : TRAINLOG_LOAD_NONE;
            generated->recency.recent_same_exercise = state->recent_exercise;
            generated->recency.recent_same_pattern = state->recent_pattern;
            generated->exposure_warning_level = output->exposure.warning_level;
            generated->estimated_seconds = per_exercise;
            if (!copy_text(generated->equipment_load_semantics,
                    sizeof(generated->equipment_load_semantics), state->candidate.equipment_load_semantics) ||
                !copy_text(generated->confidence, sizeof(generated->confidence), state->candidate.confidence))
                return TRAINLOG_STATUS_DATABASE_ERROR;
            for (index = 0U; index < state->candidate.secondary_zone_count; ++index)
                if (!copy_text(generated->secondary_zone_ids[index],
                    sizeof(generated->secondary_zone_ids[index]), state->candidate.secondary_zone_ids[index]))
                    return TRAINLOG_STATUS_DATABASE_ERROR;
            generated->secondary_zone_count = state->candidate.secondary_zone_count;
            for (index = 0U; index < state->candidate.pattern_count; ++index) {
                if (!copy_text(generated->pattern_ids[index], sizeof(generated->pattern_ids[index]),
                    state->candidate.pattern_ids[index])) return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            generated->pattern_count = state->candidate.pattern_count;
            for (index = 0U; index < state->candidate.source_ref_count; ++index) {
                if (!copy_text(generated->source_ref_ids[index], sizeof(generated->source_ref_ids[index]),
                    state->candidate.source_ref_ids[index])) return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            generated->source_ref_count = state->candidate.source_ref_count;
            if (state->has_anchor) {
                generated->has_target_weight = true;
                generated->target_weight_kg = state->anchor_weight;
                (void)strcpy(generated->load_source_session_id, state->anchor_session);
                (void)strcpy(generated->load_source_occurrence_id, state->anchor_occurrence);
                (void)strcpy(generated->load_source_started_at, state->anchor_time_text);
                (void)strcpy(generated->rationale_codes[generated->rationale_count++], "observed_repeated_dose_anchor");
            } else if (state->has_compatible_explicit_max) {
                (void)strcpy(generated->rationale_codes[generated->rationale_count++],
                    "explicit_max_present_no_numeric_prescription");
            } else if (strcmp(state->candidate.equipment_load_semantics, "assistance") == 0) {
                (void)strcpy(generated->rationale_codes[generated->rationale_count++], "assistance_numeric_load_omitted");
            } else {
                (void)strcpy(generated->rationale_codes[generated->rationale_count++], "numeric_load_absent");
            }
            if (state->preferred)
                (void)strcpy(generated->rationale_codes[generated->rationale_count++], "preferred_exercise");
            (void)strcpy(generated->rationale_codes[generated->rationale_count++],
                zone_matches(state->candidate.primary_zone_id, analyzer->request.zone_id) ?
                "requested_primary_zone" : "requested_secondary_zone");
            (void)strcpy(generated->rationale_codes[generated->rationale_count++],
                strcmp(state->candidate.equipment_load_semantics, "external") == 0 ?
                "external_equipment_context" : "non_external_equipment_context");
            (void)strcpy(generated->rationale_codes[generated->rationale_count++], "new_exact_pattern");
            if (state->recent_exercise)
                (void)strcpy(generated->rationale_codes[generated->rationale_count++], "recent_same_exercise_penalty");
            if (state->recent_pattern)
                (void)strcpy(generated->rationale_codes[generated->rationale_count++], "recent_same_pattern_penalty");
            used[best] = true;
            ++output->exercise_count;
            output->estimated_duration_seconds += per_exercise;
        }
    }
    output->insufficient_resolved_candidates = output->exercise_count == 0U ||
        output->exercise_count < (size_t)trainlog_session_generation_policy_v1.max_exercises;
    if (output->insufficient_resolved_candidates) {
        bool upper = false, lower = false, core = false;
        bool push = false, pull = false, extension = false, flexion = false;
        size_t selected, pattern;
        (void)strcpy(output->shortage_codes[output->shortage_count++],
            "insufficient_resolved_candidates");
        for (selected = 0U; selected < output->exercise_count; ++selected) {
            const char *zone = output->exercises[selected].primary_zone_id;
            core = core || strcmp(zone, "core") == 0;
            upper = upper || strcmp(zone, "chest") == 0 || strcmp(zone, "back") == 0 ||
                strcmp(zone, "shoulders") == 0 || strcmp(zone, "arms") == 0;
            lower = lower || strcmp(zone, "glutes") == 0 || strcmp(zone, "thighs") == 0 ||
                strcmp(zone, "calves") == 0;
            for (pattern = 0U; pattern < output->exercises[selected].pattern_count; ++pattern) {
                const char *id = output->exercises[selected].pattern_ids[pattern];
                push = push || newline_list_has(trainlog_session_generation_policy_v1.upper_push_patterns, id);
                pull = pull || newline_list_has(trainlog_session_generation_policy_v1.upper_pull_patterns, id);
                extension = extension || newline_list_has(trainlog_session_generation_policy_v1.lower_extension_patterns, id);
                flexion = flexion || newline_list_has(trainlog_session_generation_policy_v1.lower_flexion_patterns, id);
            }
        }
        if (strcmp(analyzer->request.zone_id, "full_body") == 0) {
            if (!upper) (void)strcpy(output->shortage_codes[output->shortage_count++], "missing_upper_region");
            if (!lower) (void)strcpy(output->shortage_codes[output->shortage_count++], "missing_lower_region");
            if (!core) (void)strcpy(output->shortage_codes[output->shortage_count++], "missing_core_region");
        } else if (strcmp(analyzer->request.zone_id, "upper_body") == 0) {
            if (!push) (void)strcpy(output->shortage_codes[output->shortage_count++], "missing_upper_push");
            if (!pull) (void)strcpy(output->shortage_codes[output->shortage_count++], "missing_upper_pull");
        } else if (strcmp(analyzer->request.zone_id, "lower_body") == 0) {
            if (!extension) (void)strcpy(output->shortage_codes[output->shortage_count++], "missing_lower_extension");
            if (!flexion) (void)strcpy(output->shortage_codes[output->shortage_count++], "missing_lower_flexion");
        }
    }
    return TRAINLOG_STATUS_OK;
}

void trainlog_session_generation_analyzer_destroy(TrainlogSessionGenerationAnalyzer *analyzer)
{
    if (analyzer == NULL) return;
    free(analyzer->current_occurrence_time);
    free(analyzer->reference_owned);
    free(analyzer);
}

TrainlogStatus trainlog_session_generation_requalify_dose(
    const TrainlogGenerationRequest *request, size_t selected_index, int target_sets,
    int target_repetitions, TrainlogSessionGenerationAnalyzer **output_analyzer)
{
    TrainlogStatus status;
    TrainlogSessionGenerationAnalyzer *result;
    TrainlogGenerationRequest selected_request;
    if (request == NULL || selected_index >= request->candidate_count || target_sets < 1 ||
        target_sets > 64 || target_repetitions < 1 || target_repetitions > 10000)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    selected_request = *request;
    selected_request.candidates = &request->candidates[selected_index];
    selected_request.candidate_count = 1U;
    status = trainlog_session_generation_analyzer_create(&selected_request, &result);
    if (status != TRAINLOG_STATUS_OK) return status;
    /* A private per-call goal copy avoids mutating generated immutable policy. */
    result->edited_goal = *result->goal;
    result->edited_goal.sets = target_sets;
    result->edited_goal.repetitions = target_repetitions;
    result->goal = &result->edited_goal;
    *output_analyzer = result;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_session_generation_exercise_duration(
    int target_sets, int target_repetitions, int rest_seconds, int *output_seconds)
{
    int64_t value;
    if (output_seconds == NULL || target_sets < 1 || target_sets > 64 ||
        target_repetitions < 1 || target_repetitions > 10000 ||
        rest_seconds < 0 || rest_seconds > 86400) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    value = (int64_t)trainlog_session_generation_policy_v1.setup_seconds +
        (int64_t)target_sets * target_repetitions * trainlog_session_generation_policy_v1.repetition_seconds +
        (int64_t)(target_sets - 1) * rest_seconds;
    if (value > INT_MAX) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_seconds = (int)value;
    return TRAINLOG_STATUS_OK;
}

typedef struct OwnedCandidate {
    TrainlogGenerationCandidate value;
    char secondary[TRAINLOG_GENERATOR_MAX_ZONES][TRAINLOG_ID_MAX + 1U];
    const char *secondary_ptrs[TRAINLOG_GENERATOR_MAX_ZONES];
    char patterns[TRAINLOG_GENERATOR_MAX_PATTERNS][TRAINLOG_ID_MAX + 1U];
    const char *pattern_ptrs[TRAINLOG_GENERATOR_MAX_PATTERNS];
    char refs[TRAINLOG_GENERATOR_MAX_SOURCE_REFS][TRAINLOG_ID_MAX + 1U];
    const char *ref_ptrs[TRAINLOG_GENERATOR_MAX_SOURCE_REFS];
} OwnedCandidate;

static bool split_ids(const char *list, char storage[][TRAINLOG_ID_MAX + 1U],
    const char **pointers, size_t capacity, size_t *output_count)
{
    const char *cursor = list;
    size_t count = 0U;
    while (cursor != NULL && cursor[0] != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        if (count == capacity || length == 0U || length > TRAINLOG_ID_MAX) return false;
        (void)memcpy(storage[count], cursor, length); storage[count][length] = '\0';
        pointers[count] = storage[count];
        ++count;
        cursor = end == NULL ? NULL : end + 1;
    }
    *output_count = count;
    return true;
}

static TrainlogStatus build_database_candidates(TrainlogDatabase *database,
    const TrainlogGenerationDatabaseRequest *request, TrainlogExercise **exercises_owner,
    OwnedCandidate **owned_output, TrainlogGenerationCandidate **values_output, size_t *count_output)
{
    TrainlogExercise *exercises = NULL;
    OwnedCandidate *owned = NULL;
    TrainlogGenerationCandidate *values = NULL;
    size_t exercise_count = 0U, count = 0U, index;
    TrainlogStatus status = trainlog_database_list_exercises(database, NULL, 0U, &exercise_count);
    if (status != TRAINLOG_STATUS_OK) return status;
    if (exercise_count > 0U) {
        exercises = calloc(exercise_count, sizeof(*exercises));
        if (exercises == NULL) return TRAINLOG_STATUS_SYSTEM_ERROR;
        status = trainlog_database_list_exercises(database, exercises, exercise_count, &exercise_count);
        if (status != TRAINLOG_STATUS_OK) { free(exercises); return status; }
    }
    owned = calloc(TRAINLOG_GENERATOR_MAX_CANDIDATES, sizeof(*owned));
    values = calloc(TRAINLOG_GENERATOR_MAX_CANDIDATES, sizeof(*values));
    if (owned == NULL || values == NULL) { free(exercises); free(owned); free(values); return TRAINLOG_STATUS_SYSTEM_ERROR; }
    for (index = 0U; index < exercise_count; ++index) {
        const TrainlogExerciseKnowledge *knowledge;
        const char *equipment_cursor;
        if (exercises[index].recording_mode != TRAINLOG_RECORDING_SETS ||
            exercises[index].tracking_mode != TRAINLOG_TRACKING_REPS) continue;
        knowledge = trainlog_exercise_knowledge_lookup(exercises[index].exercise_id);
        if (knowledge == NULL || knowledge->interpretation == NULL ||
            strcmp(knowledge->resolution_status, "resolved_family_variant_limited") != 0 ||
            (strcmp(knowledge->confidence, "high") != 0 && strcmp(knowledge->confidence, "moderate") != 0)) continue;
        equipment_cursor = knowledge->equipment_ids;
        while (equipment_cursor != NULL && equipment_cursor[0] != '\0') {
            const char *end = strchr(equipment_cursor, '\n');
            size_t length = end == NULL ? strlen(equipment_cursor) : (size_t)(end - equipment_cursor);
            char equipment_id[TRAINLOG_ID_MAX + 1U];
            const TrainlogEquipmentKnowledge *equipment;
            OwnedCandidate *item;
            if (length == 0U || length > TRAINLOG_ID_MAX) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto fail; }
            (void)memcpy(equipment_id, equipment_cursor, length); equipment_id[length] = '\0';
            if (request->available_equipment_ids != NULL && !id_in(equipment_id,
                request->available_equipment_ids, request->available_equipment_count)) {
                equipment_cursor = end == NULL ? NULL : end + 1; continue;
            }
            equipment = trainlog_equipment_knowledge_lookup(equipment_id);
            if (equipment == NULL || equipment->catalog_load_semantics == NULL) {
                equipment_cursor = end == NULL ? NULL : end + 1; continue;
            }
            if (count == TRAINLOG_GENERATOR_MAX_CANDIDATES) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto fail; }
            item = &owned[count];
            item->value.exercise_id = knowledge->exercise_id;
            item->value.equipment_id = equipment->equipment_id;
            item->value.primary_zone_id = knowledge->interpretation->primary_zone_id;
            item->value.confidence = knowledge->confidence;
            item->value.equipment_load_semantics = equipment->catalog_load_semantics;
            if (!split_ids(knowledge->interpretation->secondary_zone_ids, item->secondary,
                    item->secondary_ptrs, TRAINLOG_GENERATOR_MAX_ZONES, &item->value.secondary_zone_count) ||
                !split_ids(knowledge->interpretation->pattern_ids, item->patterns,
                    item->pattern_ptrs, TRAINLOG_GENERATOR_MAX_PATTERNS, &item->value.pattern_count) ||
                !split_ids(knowledge->interpretation->source_refs, item->refs,
                    item->ref_ptrs, TRAINLOG_GENERATOR_MAX_SOURCE_REFS, &item->value.source_ref_count)) {
                status = TRAINLOG_STATUS_DATABASE_ERROR; goto fail;
            }
            item->value.secondary_zone_ids = item->secondary_ptrs;
            item->value.pattern_ids = item->pattern_ptrs;
            item->value.source_ref_ids = item->ref_ptrs;
            values[count] = item->value;
            ++count;
            equipment_cursor = end == NULL ? NULL : end + 1;
        }
    }
    *exercises_owner = exercises; *owned_output = owned; *values_output = values; *count_output = count;
    return TRAINLOG_STATUS_OK;
fail:
    free(exercises); free(owned); free(values); return status;
}

static TrainlogStatus database_accept_visitor(void *context, const TrainlogGenerationHistoryRow *row)
{
    return trainlog_session_generation_analyzer_accept(context, row);
}

TrainlogStatus trainlog_session_generate_from_database(TrainlogDatabase *database,
    const TrainlogGenerationDatabaseRequest *request, TrainlogGeneratedSession *output)
{
    TrainlogExercise *exercises = NULL;
    OwnedCandidate *owned = NULL;
    TrainlogGenerationCandidate *candidates = NULL;
    TrainlogSessionGenerationAnalyzer *analyzer = NULL;
    TrainlogGenerationRequest pure;
    size_t count = 0U;
    TrainlogStatus status, end_status;
    if (database == NULL || request == NULL || output == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) return status;
    status = build_database_candidates(database, request, &exercises, &owned, &candidates, &count);
    if (status == TRAINLOG_STATUS_OK) {
        (void)memset(&pure, 0, sizeof(pure));
        pure.zone_id = request->zone_id; pure.goal_id = request->goal_id;
        pure.duration_minutes = request->duration_minutes; pure.reference_time = request->reference_time;
        pure.candidates = candidates; pure.candidate_count = count;
        pure.preferred_exercise_ids = request->preferred_exercise_ids; pure.preferred_count = request->preferred_count;
        pure.excluded_exercise_ids = request->excluded_exercise_ids; pure.excluded_exercise_count = request->excluded_exercise_count;
        pure.excluded_pattern_ids = request->excluded_pattern_ids; pure.excluded_pattern_count = request->excluded_pattern_count;
        status = trainlog_session_generation_analyzer_create(&pure, &analyzer);
    }
    if (status == TRAINLOG_STATUS_OK)
        status = trainlog_database_scan_generation_history(database, database_accept_visitor, analyzer);
    if (status == TRAINLOG_STATUS_OK)
        status = trainlog_session_generation_analyzer_finish(analyzer, output);
    trainlog_session_generation_analyzer_destroy(analyzer);
    free(candidates); free(owned); free(exercises);
    end_status = trainlog_database_read_snapshot_end(database, status == TRAINLOG_STATUS_OK);
    return status == TRAINLOG_STATUS_OK ? end_status : status;
}
