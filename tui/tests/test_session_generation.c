#include "trainlog/session_generation.h"
#include "trainlog/database.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void trainlog_run_generated_session_generation_fixtures(void);

static TrainlogStatus accept_row(void *context, const TrainlogGenerationHistoryRow *row)
{
    return trainlog_session_generation_analyzer_accept(context, row);
}

static TrainlogGenerationCandidate leg_press_candidate(void)
{
    static const char *const patterns[] = {"knee_dominant"};
    static const char *const refs[] = {"acsm_2009"};
    TrainlogGenerationCandidate result = {
        .exercise_id = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde",
        .equipment_id = "leg_press",
        .primary_zone_id = "thighs",
        .pattern_ids = patterns,
        .pattern_count = 1U,
        .source_ref_ids = refs,
        .source_ref_count = 1U,
        .confidence = "moderate",
        .equipment_load_semantics = "external",
    };
    return result;
}

static TrainlogGenerationHistoryRow base_row(void)
{
    TrainlogGenerationHistoryRow row = {
        .session_id = "se_10000000-0000-4000-8000-000000000000",
        .occurrence_id = "sxe_10000000-0000-4000-8000-000000000000",
        .exercise_id = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde",
        .started_at = "2026-09-08T12:00:00.500+00:00",
        .equipment_id = "leg_press",
        .recording_mode = TRAINLOG_RECORDING_SETS,
        .tracking_mode = TRAINLOG_TRACKING_REPS,
        .load_mode = TRAINLOG_LOAD_NONE,
        .rest_seconds = 0,
        .has_actual_set = true,
        .repetitions = 10,
        .has_weight = true,
        .weight_kg = 80.0,
    };
    return row;
}

static void test_anchor_exposure_and_boundaries(void)
{
    TrainlogGenerationCandidate candidate = leg_press_candidate();
    TrainlogGenerationRequest request = {
        .zone_id = "thighs", .goal_id = "hypertrophy", .duration_minutes = 30,
        .reference_time = "2026-09-09T12:00:00.500Z",
        .candidates = &candidate, .candidate_count = 1U,
    };
    TrainlogSessionGenerationAnalyzer *analyzer = NULL;
    TrainlogGeneratedSession result;
    TrainlogGenerationHistoryRow row = base_row();
    size_t index;
    assert(trainlog_session_generation_analyzer_create(&request, &analyzer) == TRAINLOG_STATUS_OK);
    for (index = 0U; index < 3U; ++index) {
        row.set_position = index;
        row.weight_kg = index == 1U ? 85.0 : 80.0;
        assert(accept_row(analyzer, &row) == TRAINLOG_STATUS_OK);
    }
    assert(trainlog_session_generation_analyzer_finish(analyzer, &result) == TRAINLOG_STATUS_OK);
    assert(result.exercise_count == 1U);
    assert(result.exercises[0].has_target_weight && result.exercises[0].target_weight_kg == 80.0);
    /* Exactly 24h is excluded; it remains the latest full-history exposure. */
    assert(result.exposure.within_24h.primary_set_count == 0U);
    assert(result.exposure.within_72h.primary_set_count == 3U);
    assert(result.exposure.warning_level == TRAINLOG_GENERATION_WARNING_NONE);
    assert(result.exposure.has_latest && strcmp(result.exposure.latest_occurrence_id, row.occurrence_id) == 0);
    trainlog_session_generation_analyzer_destroy(analyzer);
}

static void test_fraction_inside_boundary_and_invalid_time(void)
{
    TrainlogGenerationCandidate candidate = leg_press_candidate();
    TrainlogGenerationRequest request = {
        .zone_id = "thighs", .goal_id = "general", .duration_minutes = 30,
        .reference_time = "2026-09-09T12:00:00.500Z",
        .candidates = &candidate, .candidate_count = 1U,
    };
    TrainlogSessionGenerationAnalyzer *analyzer = NULL;
    TrainlogGenerationHistoryRow row = base_row();
    TrainlogGeneratedSession result;
    row.started_at = "2026-09-08T12:00:00.501Z";
    row.has_weight = false;
    assert(trainlog_session_generation_analyzer_create(&request, &analyzer) == TRAINLOG_STATUS_OK);
    assert(trainlog_session_generation_analyzer_accept(analyzer, &row) == TRAINLOG_STATUS_OK);
    assert(trainlog_session_generation_analyzer_finish(analyzer, &result) == TRAINLOG_STATUS_OK);
    assert(result.exposure.within_24h.primary_set_count == 1U);
    assert(result.exposure.warning_level == TRAINLOG_GENERATION_WARNING_WARNING);
    trainlog_session_generation_analyzer_destroy(analyzer);

    assert(trainlog_session_generation_analyzer_create(&request, &analyzer) == TRAINLOG_STATUS_OK);
    row.started_at = "not-a-time";
    assert(trainlog_session_generation_analyzer_accept(analyzer, &row) == TRAINLOG_STATUS_DATABASE_ERROR);
    assert(trainlog_session_generation_analyzer_finish(analyzer, &result) == TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_session_generation_analyzer_destroy(analyzer);
}

static void test_sparse_and_max_only_shape(void)
{
    TrainlogGenerationRequest request = {
        .zone_id = "chest", .goal_id = "strength", .duration_minutes = 30,
        .reference_time = "2026-09-09T12:00Z",
    };
    TrainlogSessionGenerationAnalyzer *analyzer = NULL;
    TrainlogGeneratedSession result;
    assert(trainlog_session_generation_analyzer_create(&request, &analyzer) == TRAINLOG_STATUS_OK);
    assert(trainlog_session_generation_analyzer_finish(analyzer, &result) == TRAINLOG_STATUS_OK);
    assert(result.exercise_count == 0U && result.insufficient_resolved_candidates);
    trainlog_session_generation_analyzer_destroy(analyzer);
}

static void test_database_convenience_and_explicit_empty_equipment(void)
{
    TrainlogDatabase *database = NULL;
    const TrainlogSetInput sets[] = {
        {.reps = 10, .has_weight = true, .weight_kg = 70.0},
        {.reps = 10, .has_weight = true, .weight_kg = 75.0},
    };
    const TrainlogSessionExerciseInput exercise = {
        .entry_id = "sxe_30000000-0000-4000-8000-000000000000",
        .exercise_id = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde",
        .equipment_id = "leg_press",
        .recording_mode = TRAINLOG_RECORDING_SETS,
        .load_mode = TRAINLOG_LOAD_NONE,
        .sets = sets,
        .set_count = 2U,
    };
    const TrainlogSessionInput session = {
        .session_id = "se_30000000-0000-4000-8000-000000000000",
        .started_at = "2026-09-08T12:00:00Z",
        .session_type = TRAINLOG_SESSION_TRAINING,
        .exercises = &exercise,
        .exercise_count = 1U,
    };
    TrainlogGenerationDatabaseRequest request = {
        .zone_id = "thighs", .goal_id = "general", .duration_minutes = 30,
        .reference_time = "2026-09-09T12:00:00Z",
    };
    TrainlogGeneratedSession result;
    const char *const no_equipment_storage[] = {NULL};
    assert(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    assert(trainlog_database_insert_exercise_profiled(database,
        "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde", "Leg press", "leg press",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U) == TRAINLOG_STATUS_OK);
    assert(trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK);
    assert(trainlog_session_generate_from_database(database, &request, &result) == TRAINLOG_STATUS_OK);
    assert(result.exercise_count == 1U && result.exercises[0].has_target_weight);
    request.available_equipment_ids = no_equipment_storage;
    request.available_equipment_count = 0U;
    assert(trainlog_session_generate_from_database(database, &request, &result) == TRAINLOG_STATUS_OK);
    assert(result.exercise_count == 0U && result.insufficient_resolved_candidates);
    trainlog_database_close(database);
}

static void test_analyzer_owns_request_bytes_after_create(void)
{
    char zone[] = "thighs";
    char goal[] = "general";
    char reference[] = "2026-09-09T12:00:00.500Z";
    char exercise[] = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde";
    char equipment[] = "leg_press";
    char primary[] = "thighs";
    char secondary[] = "glutes";
    char pattern[] = "knee_dominant";
    char source[] = "acsm_2009";
    char confidence[] = "moderate";
    char semantics[] = "external";
    const char *secondary_values[] = {secondary};
    const char *pattern_values[] = {pattern};
    const char *source_values[] = {source};
    TrainlogGenerationCandidate candidate = {
        .exercise_id=exercise,.equipment_id=equipment,.primary_zone_id=primary,
        .secondary_zone_ids=secondary_values,.secondary_zone_count=1U,
        .pattern_ids=pattern_values,.pattern_count=1U,
        .source_ref_ids=source_values,.source_ref_count=1U,
        .confidence=confidence,.equipment_load_semantics=semantics,
    };
    TrainlogGenerationRequest request = {
        .zone_id=zone,.goal_id=goal,.duration_minutes=30,.reference_time=reference,
        .candidates=&candidate,.candidate_count=1U,
    };
    TrainlogSessionGenerationAnalyzer *analyzer = NULL;
    TrainlogGeneratedSession output;
    assert(trainlog_session_generation_analyzer_create(&request, &analyzer) == TRAINLOG_STATUS_OK);
    (void)memset(zone, 'x', sizeof(zone)-1U);
    (void)memset(goal, 'x', sizeof(goal)-1U);
    (void)memset(reference, 'x', sizeof(reference)-1U);
    (void)memset(exercise, 'x', sizeof(exercise)-1U);
    (void)memset(equipment, 'x', sizeof(equipment)-1U);
    (void)memset(primary, 'x', sizeof(primary)-1U);
    (void)memset(secondary, 'x', sizeof(secondary)-1U);
    (void)memset(pattern, 'x', sizeof(pattern)-1U);
    (void)memset(source, 'x', sizeof(source)-1U);
    (void)memset(confidence, 'x', sizeof(confidence)-1U);
    (void)memset(semantics, 'x', sizeof(semantics)-1U);
    assert(trainlog_session_generation_analyzer_finish(analyzer, &output) == TRAINLOG_STATUS_OK);
    assert(output.exercise_count == 1U);
    assert(strcmp(output.exercises[0].exercise_id,
        "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde") == 0);
    assert(strcmp(output.exercises[0].secondary_zone_ids[0], "glutes") == 0);
    assert(strcmp(output.exercises[0].pattern_ids[0], "knee_dominant") == 0);
    assert(strcmp(output.exercises[0].source_ref_ids[0], "acsm_2009") == 0);
    trainlog_session_generation_analyzer_destroy(analyzer);
}

int main(void)
{
    trainlog_run_generated_session_generation_fixtures();
    test_anchor_exposure_and_boundaries();
    test_fraction_inside_boundary_and_invalid_time();
    test_sparse_and_max_only_shape();
    test_database_convenience_and_explicit_empty_equipment();
    test_analyzer_owns_request_bytes_after_create();
    (void)puts("session generation tests passed");
    return 0;
}
