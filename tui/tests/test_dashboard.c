/** @file test_dashboard.c @brief Core Dashboard characterization parity. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trainlog/dashboard.h"

#define CHECK(condition) do { if (!(condition)) {                           \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
        __FILE__, __LINE__, #condition); return false; } } while (0)

static bool dashboard_load(TrainlogDatabase *database,
    TrainlogDashboardPeriod period, TrainlogDashboardSnapshot *snapshot)
{
    const TrainlogDashboardQuery query = {period, INT64_C(1789560000)};
    return trainlog_dashboard_load(database, &query, snapshot) ==
        TRAINLOG_STATUS_OK;
}

static bool dashboard_insert_completed_set_dose(TrainlogDatabase *database,
    const char *session_id, const char *started_at, const char *ended_at,
    TrainlogLoadMode load_mode, const char *equipment_id, double weight, int dose)
{
    TrainlogSetInput set = {dose, 0, true, weight};
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        "ex_33333333-3333-4333-8333-333333333333");
    (void)snprintf(occurrence.equipment_id, sizeof(occurrence.equipment_id), "%s",
        equipment_id);
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.load_mode = load_mode;
    occurrence.rest_seconds = 60;
    occurrence.target_sets = 2;
    occurrence.target_reps = 10;
    occurrence.target_has_weight = true;
    occurrence.target_weight_kg = 999.0;
    occurrence.sets = &set;
    occurrence.set_count = 1U;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s", ended_at);
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool dashboard_insert_completed_set(TrainlogDatabase *database,
    const char *session_id, const char *started_at, const char *ended_at,
    TrainlogLoadMode load_mode, const char *equipment_id, double weight)
{
    return dashboard_insert_completed_set_dose(database, session_id, started_at,
        ended_at, load_mode, equipment_id, weight, 8);
}
static bool dashboard_insert_max_for_exercise(TrainlogDatabase *database,
    const char *session_id, const char *entry_id, const char *exercise_id,
    const char *started_at, double weight)
{
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.entry_id, sizeof(occurrence.entry_id), "%s", entry_id);
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        exercise_id);
    (void)snprintf(occurrence.equipment_id, sizeof(occurrence.equipment_id), "%s",
        "leg_press");
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.tracking_mode = TRAINLOG_TRACKING_REPS;
    occurrence.load_mode = TRAINLOG_LOAD_NONE;
    occurrence.has_max_weight = true;
    occurrence.max_weight_kg = weight;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    session.session_type = TRAINLOG_SESSION_MAX_TEST;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool dashboard_insert_plan_for_exercise(TrainlogDatabase *database,
    const char *session_id, const char *exercise_id, const char *started_at)
{
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        exercise_id);
    (void)snprintf(occurrence.equipment_id, sizeof(occurrence.equipment_id), "%s",
        "leg_press");
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.tracking_mode = TRAINLOG_TRACKING_REPS;
    occurrence.load_mode = TRAINLOG_LOAD_EXTERNAL;
    occurrence.target_sets = 3;
    occurrence.target_reps = 10;
    occurrence.target_has_weight = true;
    occurrence.target_weight_kg = 50.0;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool dashboard_insert_optional_weight_set(TrainlogDatabase *database,
    const char *session_id, const char *exercise_id, const char *started_at,
    bool has_weight, double weight)
{
    TrainlogSetInput set = {10, 0, has_weight, weight};
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        exercise_id);
    (void)snprintf(occurrence.equipment_id, sizeof(occurrence.equipment_id), "%s",
        "leg_press");
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.tracking_mode = TRAINLOG_TRACKING_REPS;
    occurrence.load_mode = TRAINLOG_LOAD_EXTERNAL;
    occurrence.target_sets = 1;
    occurrence.target_reps = 10;
    occurrence.target_has_weight = true;
    occurrence.target_weight_kg = 25.0;
    occurrence.sets = &set;
    occurrence.set_count = 1U;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool test_dashboard_characterization_empty_and_window_boundaries(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogDashboardSnapshot dashboard;
    size_t index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&dashboard, 0, sizeof(dashboard));
    dashboard.period = TRAINLOG_DASHBOARD_30_DAYS;
    CHECK(dashboard_load(database, dashboard.period, &dashboard));
    CHECK(!dashboard.error && !dashboard.partial &&
        !dashboard.invalid_data);
    CHECK(!dashboard.has_performance && !dashboard.has_explicit_max &&
        !dashboard.has_body);
    CHECK(dashboard.performance_count == 0U && dashboard.body_count == 0U);
    CHECK(dashboard.completed_session_count == 0U &&
        dashboard.performed_set_count == 0U &&
        dashboard.distinct_exercise_count == 0U &&
        dashboard.explicit_max_count == 0U);
    /* The database is empty of training history, not of the seeded canonical
     * catalogue: zone distribution is therefore deliberately non-empty. */
    CHECK(dashboard.zone_count == 4U);
    CHECK(dashboard.zones[0].zone_id[0] == '\0' &&
        dashboard.zones[0].count == 2U);
    CHECK(dashboard.week_count == 6U);
    for (index = 0U; index < dashboard.week_count; ++index)
        CHECK(dashboard.weeks[index].sessions == 0U &&
            dashboard.weeks[index].maxima == 0U &&
            dashboard.weeks[index].working_improvements == 0U &&
            dashboard.weeks[index].max_improvements == 0U);

    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Frontière",
        "frontiere", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(dashboard_insert_completed_set(database,
        "se_31000000-0000-4000-8000-000000000001",
        "2026-08-17T11:59:59Z", "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 10.0));
    CHECK(dashboard_insert_completed_set(database,
        "se_31000000-0000-4000-8000-000000000002",
        "2026-08-17T12:00:00Z", "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 20.0));
    CHECK(dashboard_insert_completed_set(database,
        "se_31000000-0000-4000-8000-000000000003",
        "2026-08-17T12:00:01Z", "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 30.0));
    CHECK(dashboard_load(database, dashboard.period, &dashboard));
    CHECK(!dashboard.error && !dashboard.invalid_data);
    CHECK(dashboard.completed_session_count == 2U);
    CHECK(dashboard.performed_set_count == 2U);
    CHECK(dashboard.distinct_exercise_count == 1U);
    CHECK(dashboard.has_performance && dashboard.performance_count == 2U);
    CHECK(strcmp(dashboard.performance[0].started_at,
        "2026-08-17T12:00:00Z") == 0);
    CHECK(strcmp(dashboard.performance[1].started_at,
        "2026-08-17T12:00:01Z") == 0);
    trainlog_database_close(database);
    return true;
}

static bool test_dashboard_characterization_modalities_ties_and_zones(void)
{
    static const char *const primary_secondary[] = {"back"};
    TrainlogDatabase *database = NULL;
    TrainlogDashboardSnapshot dashboard;
    TrainlogSetInput rep_sets[] = {
        {10, 0, true, 40.0}, {8, 0, true, 1000.0}
    };
    TrainlogSetInput repeated_set = {12, 0, true, 50.0};
    TrainlogSetInput duration_set = {0, 60, true, 30.0};
    TrainlogSetInput assistance_set = {10, 0, true, 5.0};
    TrainlogSetInput unweighted_set = {10, 0, false, 0.0};
    TrainlogSessionExerciseInput occurrences[4];
    TrainlogSessionInput session;
    bool chest = false;
    bool back = false;
    bool unclassified = false;
    size_t working = 0U;
    size_t index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_10000000-0000-4000-8000-000000000000", "MAX égalité", "max egalite",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Répétitions",
        "repetitions", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_10000000-0000-4000-8000-000000000001", "MAX secondaire",
        "max secondaire",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_10000000-0000-4000-8000-000000000002", "Durée séries", "duree series",
        TRAINLOG_TRACKING_DURATION, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_10000000-0000-4000-8000-000000000003", "Continu", "continu",
        TRAINLOG_TRACKING_DURATION, TRAINLOG_RECORDING_CONTINUOUS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_10000000-0000-4000-8000-000000000004", "Assistance", "assistance",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_10000000-0000-4000-8000-000000000005", "Poids absent antérieur",
        "poids absent anterieur", TRAINLOG_TRACKING_REPS,
        TRAINLOG_RECORDING_SETS, (TrainlogExerciseDataFields)0) ==
        TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database,
        "ex_33333333-3333-4333-8333-333333333333", "chest",
        primary_secondary, 1U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database,
        "ex_10000000-0000-4000-8000-000000000002", "back", NULL, 0U) ==
        TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database,
        "ex_10000000-0000-4000-8000-000000000004", "chest", NULL, 0U) ==
        TRAINLOG_STATUS_OK);

    (void)memset(occurrences, 0, sizeof(occurrences));
    (void)memset(&session, 0, sizeof(session));
    for (index = 0U; index < 2U; ++index) {
        (void)snprintf(occurrences[index].exercise_id,
            sizeof(occurrences[index].exercise_id), "%s",
            "ex_33333333-3333-4333-8333-333333333333");
        (void)snprintf(occurrences[index].equipment_id,
            sizeof(occurrences[index].equipment_id), "%s", "leg_press");
        occurrences[index].recording_mode = TRAINLOG_RECORDING_SETS;
        occurrences[index].tracking_mode = TRAINLOG_TRACKING_REPS;
        occurrences[index].load_mode = TRAINLOG_LOAD_EXTERNAL;
        occurrences[index].target_sets = 1;
        occurrences[index].target_reps = 10;
        occurrences[index].target_has_weight = true;
        occurrences[index].target_weight_kg = 40.0;
    }
    (void)snprintf(occurrences[0].entry_id, sizeof(occurrences[0].entry_id), "%s",
        "sxe_10000000-0000-4000-8000-000000000001");
    occurrences[0].sets = rep_sets;
    occurrences[0].set_count = 2U;
    (void)snprintf(occurrences[1].entry_id, sizeof(occurrences[1].entry_id), "%s",
        "sxe_10000000-0000-4000-8000-000000000002");
    occurrences[1].sets = &repeated_set;
    occurrences[1].set_count = 1U;
    (void)snprintf(occurrences[2].exercise_id, sizeof(occurrences[2].exercise_id),
        "%s", "ex_10000000-0000-4000-8000-000000000002");
    (void)snprintf(occurrences[2].equipment_id, sizeof(occurrences[2].equipment_id),
        "%s", "leg_press");
    occurrences[2].recording_mode = TRAINLOG_RECORDING_SETS;
    occurrences[2].tracking_mode = TRAINLOG_TRACKING_DURATION;
    occurrences[2].load_mode = TRAINLOG_LOAD_EXTERNAL;
    occurrences[2].target_sets = 1;
    occurrences[2].target_duration_seconds = 60;
    occurrences[2].target_has_weight = true;
    occurrences[2].target_weight_kg = 30.0;
    occurrences[2].sets = &duration_set;
    occurrences[2].set_count = 1U;
    (void)snprintf(occurrences[3].exercise_id, sizeof(occurrences[3].exercise_id),
        "%s", "ex_10000000-0000-4000-8000-000000000003");
    occurrences[3].recording_mode = TRAINLOG_RECORDING_CONTINUOUS;
    occurrences[3].tracking_mode = TRAINLOG_TRACKING_DURATION;
    occurrences[3].load_mode = TRAINLOG_LOAD_NONE;
    occurrences[3].continuous_duration_seconds = 600;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_32000000-0000-4000-8000-000000000001");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2026-09-10T08:00:00Z");
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = occurrences;
    session.exercise_count = 4U;
    CHECK(trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK);
    CHECK(dashboard_insert_optional_weight_set(database,
        "se_32000000-0000-4000-8000-000000000009",
        "ex_10000000-0000-4000-8000-000000000005", "2026-09-08T08:00:00Z",
        false, 0.0));
    CHECK(dashboard_insert_optional_weight_set(database,
        "se_32000000-0000-4000-8000-000000000010",
        "ex_10000000-0000-4000-8000-000000000005", "2026-09-09T08:00:00Z",
        true, 25.0));

    (void)memset(&occurrences[0], 0, sizeof(occurrences[0]));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrences[0].exercise_id, sizeof(occurrences[0].exercise_id),
        "%s", "ex_10000000-0000-4000-8000-000000000004");
    (void)snprintf(occurrences[0].equipment_id, sizeof(occurrences[0].equipment_id),
        "%s", "assisted_dip_chin_machine");
    occurrences[0].recording_mode = TRAINLOG_RECORDING_SETS;
    occurrences[0].tracking_mode = TRAINLOG_TRACKING_REPS;
    occurrences[0].load_mode = TRAINLOG_LOAD_ASSISTANCE;
    occurrences[0].target_sets = 1;
    occurrences[0].target_reps = 10;
    occurrences[0].target_has_weight = true;
    occurrences[0].target_weight_kg = 5.0;
    occurrences[0].sets = &assistance_set;
    occurrences[0].set_count = 1U;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_32000000-0000-4000-8000-000000000002");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2026-09-11T08:00:00Z");
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s",
        "2026-09-11T09:00:00Z");
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = occurrences;
    session.exercise_count = 1U;
    CHECK(trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK);
    CHECK(dashboard_insert_plan_for_exercise(database,
        "se_32000000-0000-4000-8000-000000000003",
        "ex_10000000-0000-4000-8000-000000000001", "2026-09-12T08:00:00Z"));
    (void)memset(&occurrences[0], 0, sizeof(occurrences[0]));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrences[0].exercise_id, sizeof(occurrences[0].exercise_id),
        "%s", "ex_33333333-3333-4333-8333-333333333333");
    (void)snprintf(occurrences[0].equipment_id, sizeof(occurrences[0].equipment_id),
        "%s", "leg_press");
    occurrences[0].recording_mode = TRAINLOG_RECORDING_SETS;
    occurrences[0].tracking_mode = TRAINLOG_TRACKING_REPS;
    occurrences[0].load_mode = TRAINLOG_LOAD_EXTERNAL;
    occurrences[0].target_sets = 1;
    occurrences[0].target_reps = 10;
    occurrences[0].target_has_weight = true;
    occurrences[0].target_weight_kg = 40.0;
    occurrences[0].sets = &unweighted_set;
    occurrences[0].set_count = 1U;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_32000000-0000-4000-8000-000000000008");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2026-09-12T12:00:00Z");
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = occurrences;
    session.exercise_count = 1U;
    CHECK(trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK);
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_32000000-0000-4000-8000-000000000004", "2026-09-13T08:00:00Z",
        "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 45.0, 10));
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_32000000-0000-4000-8000-000000000005", "2026-09-13T08:00:00Z",
        "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 50.0, 10));
    CHECK(dashboard_insert_max_for_exercise(database,
        "se_32000000-0000-4000-8000-000000000006",
        "sxe_32000000-0000-4000-8000-000000000006",
        "ex_10000000-0000-4000-8000-000000000001", "2026-09-14T08:00:00Z", 100.0));
    CHECK(dashboard_insert_max_for_exercise(database,
        "se_32000000-0000-4000-8000-000000000007",
        "sxe_32000000-0000-4000-8000-000000000007",
        "ex_10000000-0000-4000-8000-000000000000", "2026-09-14T08:00:00Z", 200.0));

    (void)memset(&dashboard, 0, sizeof(dashboard));
    dashboard.period = TRAINLOG_DASHBOARD_30_DAYS;
    CHECK(dashboard_load(database, dashboard.period, &dashboard));
    CHECK(!dashboard.error && !dashboard.partial &&
        !dashboard.invalid_data);
    CHECK(dashboard.completed_session_count == 9U);
    CHECK(dashboard.performed_set_count == 10U);
    CHECK(dashboard.explicit_max_count == 2U);
    CHECK(dashboard.distinct_exercise_count == 7U);
    CHECK(dashboard.has_performance);
    CHECK(strcmp(dashboard.exercise.exercise_id,
        "ex_33333333-3333-4333-8333-333333333333") == 0);
    CHECK(dashboard.performance_tracking_mode == TRAINLOG_TRACKING_REPS &&
        dashboard.performance_dose == 10);
    CHECK(dashboard.performance_count == 4U);
    CHECK(dashboard.performance[0].weight_kg == 40.0 &&
        dashboard.performance[1].has_weight == 1 &&
        dashboard.performance[1].weight_kg == 0.0 &&
        dashboard.performance[2].weight_kg == 45.0 &&
        dashboard.performance[3].weight_kg == 50.0);
    CHECK(dashboard.has_explicit_max && dashboard.max_weight_kg == 200.0);
    CHECK(strcmp(dashboard.max_exercise.exercise_id,
        "ex_10000000-0000-4000-8000-000000000000") == 0);
    for (index = 0U; index < dashboard.week_count; ++index)
        working += dashboard.weeks[index].working_improvements;
    /* Existing behavior: a matching earlier unweighted set supplies a 0.0
     * baseline and therefore makes the later weighted set an improvement. */
    CHECK(working == 3U);
    CHECK(dashboard.zone_count == 4U);
    for (index = 0U; index < dashboard.zone_count; ++index) {
        chest = chest || (strcmp(dashboard.zones[index].zone_id, "chest") == 0 &&
            dashboard.zones[index].count == 3U);
        back = back || (strcmp(dashboard.zones[index].zone_id, "back") == 0 &&
            dashboard.zones[index].count == 2U);
        unclassified = unclassified ||
            (dashboard.zones[index].zone_id[0] == '\0' &&
             dashboard.zones[index].count == 6U);
    }
    CHECK(chest && back && unclassified);
    trainlog_database_close(database);
    return true;
}

static bool test_dashboard_characterization_session_and_fact_bounds(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogDashboardSnapshot dashboard;
    TrainlogSetInput *sets = NULL;
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    size_t index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Bornes", "bornes",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    for (index = 0U; index < TRAINLOG_DASHBOARD_SESSION_CAPACITY; ++index) {
        char session_id[TRAINLOG_ID_MAX + 1U];
        (void)snprintf(session_id, sizeof(session_id),
            "se_33000000-0000-4000-8000-%012zu", index);
        CHECK(dashboard_insert_completed_set(database, session_id,
            "2026-09-10T08:00:00Z", "", TRAINLOG_LOAD_NONE, "", 1.0));
    }
    (void)memset(&dashboard, 0, sizeof(dashboard));
    dashboard.period = TRAINLOG_DASHBOARD_ALL;
    CHECK(dashboard_load(database, dashboard.period, &dashboard));
    CHECK(!dashboard.error && dashboard.partial);
    CHECK(dashboard.completed_session_count == TRAINLOG_DASHBOARD_SESSION_CAPACITY);
    CHECK(dashboard.performed_set_count == TRAINLOG_DASHBOARD_SESSION_CAPACITY);
    CHECK(dashboard_insert_completed_set(database,
        "se_33000000-0000-4000-8000-000000000128",
        "2026-09-10T08:00:00Z", "", TRAINLOG_LOAD_NONE, "", 1.0));
    CHECK(dashboard_load(database, dashboard.period, &dashboard));
    CHECK(!dashboard.error && dashboard.partial);
    CHECK(dashboard.completed_session_count == TRAINLOG_DASHBOARD_SESSION_CAPACITY);
    CHECK(dashboard.performed_set_count == TRAINLOG_DASHBOARD_SESSION_CAPACITY + 1U);
    trainlog_database_close(database);

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Faits bornés",
        "faits bornes", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    sets = calloc(TRAINLOG_DASHBOARD_FACT_CAPACITY, sizeof(*sets));
    CHECK(sets != NULL);
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        "ex_33333333-3333-4333-8333-333333333333");
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.tracking_mode = TRAINLOG_TRACKING_REPS;
    occurrence.load_mode = TRAINLOG_LOAD_NONE;
    occurrence.target_sets = 1;
    occurrence.target_reps = 1;
    occurrence.sets = sets;
    occurrence.set_count = TRAINLOG_DASHBOARD_FACT_CAPACITY;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_34000000-0000-4000-8000-000000000001");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2026-09-10T08:00:00Z");
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    CHECK(trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK);
    free(sets);
    sets = NULL;
    (void)memset(&dashboard, 0, sizeof(dashboard));
    dashboard.period = TRAINLOG_DASHBOARD_ALL;
    CHECK(dashboard_load(database, dashboard.period, &dashboard));
    CHECK(!dashboard.error && !dashboard.partial);
    CHECK(dashboard.performed_set_count == TRAINLOG_DASHBOARD_FACT_CAPACITY);
    /* A second honest persisted occurrence crosses the fact capacity without
     * bypassing the public persistence contract. */
    CHECK(dashboard_insert_completed_set(database,
        "se_34000000-0000-4000-8000-000000000002",
        "2026-09-10T08:00:01Z", "", TRAINLOG_LOAD_NONE, "", 1.0));
    CHECK(dashboard_load(database, dashboard.period, &dashboard));
    CHECK(!dashboard.error && dashboard.partial);
    CHECK(dashboard.performed_set_count == TRAINLOG_DASHBOARD_FACT_CAPACITY);
    trainlog_database_close(database);
    return true;
}

int main(void)
{
    if (!test_dashboard_characterization_empty_and_window_boundaries() ||
        !test_dashboard_characterization_modalities_ties_and_zones() ||
        !test_dashboard_characterization_session_and_fact_bounds()) return 1;
    return 0;
}
