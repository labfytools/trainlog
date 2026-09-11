/**
 * @file test_exercise_performance.c
 * @brief Representative exercise-performance history tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/database.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)fprintf(                                                   \
                stderr,                                                      \
                "CHECK failed at %s:%d: %s\n",                               \
                __FILE__,                                                    \
                __LINE__,                                                    \
                #condition                                                   \
            );                                                               \
            return false;                                                    \
        }                                                                    \
    } while (0)

static void bind_exercise(
    TrainlogSessionExerciseInput *exercise,
    const char *exercise_id,
    TrainlogLoadMode load_mode,
    double target_weight,
    TrainlogSetInput *sets,
    size_t set_count
)
{
    (void)memset(exercise, 0, sizeof(*exercise));

    (void)snprintf(
        exercise->exercise_id,
        sizeof(exercise->exercise_id),
        "%s",
        exercise_id
    );

    exercise->load_mode = load_mode;
    exercise->rest_seconds = 60;
    exercise->target_sets = 2;
    exercise->target_reps = 10;
    exercise->target_has_weight =
        load_mode != TRAINLOG_LOAD_NONE;
    exercise->target_weight_kg = target_weight;
    exercise->sets = sets;
    exercise->set_count = set_count;
}

static bool test_performance_semantics(void)
{
    TrainlogDatabase *database = NULL;

    TrainlogSetInput external_sets[2];
    TrainlogSetInput assistance_sets[2];
    TrainlogSetInput bodyweight_sets[2];

    TrainlogSessionExerciseInput exercises[4];
    TrainlogSetInput second_external_set;
    TrainlogCustomEquipment custom_equipment;
    TrainlogSessionInput session;

    TrainlogExercisePerformancePoint points[8];
    size_t count = 0U;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_insert_exercise(
            database,
            "ex_external",
            "External",
            "external",
            TRAINLOG_TRACKING_REPS
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(&custom_equipment, 0, sizeof(custom_equipment));
    (void)snprintf(custom_equipment.equipment_id,
        sizeof(custom_equipment.equipment_id), "%s", "stats_test_cable");
    (void)snprintf(custom_equipment.display_name,
        sizeof(custom_equipment.display_name), "%s", "Stats Test Cable");
    (void)snprintf(custom_equipment.label_name,
        sizeof(custom_equipment.label_name), "%s", "STATS TEST CABLE");
    (void)snprintf(custom_equipment.equipment_type,
        sizeof(custom_equipment.equipment_type), "%s", "cable");
    (void)snprintf(custom_equipment.load_semantics,
        sizeof(custom_equipment.load_semantics), "%s", "external");
    CHECK(trainlog_database_create_custom_equipment(database,
        &custom_equipment) == TRAINLOG_STATUS_OK);

    CHECK(
        trainlog_database_insert_exercise(
            database,
            "ex_assistance",
            "Assistance",
            "assistance",
            TRAINLOG_TRACKING_REPS
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_insert_exercise(
            database,
            "ex_none",
            "Bodyweight",
            "bodyweight",
            TRAINLOG_TRACKING_REPS
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(
        external_sets,
        0,
        sizeof(external_sets)
    );

    external_sets[0].reps = 10;
    external_sets[0].has_weight = true;
    external_sets[0].weight_kg = 80.0;

    external_sets[1].reps = 5;
    external_sets[1].has_weight = true;
    external_sets[1].weight_kg = 90.0;

    (void)memset(
        assistance_sets,
        0,
        sizeof(assistance_sets)
    );

    assistance_sets[0].reps = 10;
    assistance_sets[0].has_weight = true;
    assistance_sets[0].weight_kg = 40.0;

    assistance_sets[1].reps = 5;
    assistance_sets[1].has_weight = true;
    assistance_sets[1].weight_kg = 30.0;

    (void)memset(
        bodyweight_sets,
        0,
        sizeof(bodyweight_sets)
    );

    bodyweight_sets[0].reps = 8;
    bodyweight_sets[1].reps = 12;

    bind_exercise(
        &exercises[0],
        "ex_external",
        TRAINLOG_LOAD_EXTERNAL,
        80.0,
        external_sets,
        2U
    );

    bind_exercise(
        &exercises[1],
        "ex_assistance",
        TRAINLOG_LOAD_ASSISTANCE,
        40.0,
        assistance_sets,
        2U
    );

    bind_exercise(
        &exercises[2],
        "ex_none",
        TRAINLOG_LOAD_NONE,
        0.0,
        bodyweight_sets,
        2U
    );

    (void)memset(&second_external_set, 0, sizeof(second_external_set));
    second_external_set.reps = 12;
    second_external_set.has_weight = true;
    second_external_set.weight_kg = 50.0;
    bind_exercise(&exercises[3], "ex_external", TRAINLOG_LOAD_EXTERNAL,
        50.0, &second_external_set, 1U);
    (void)snprintf(exercises[3].equipment_id,
        sizeof(exercises[3].equipment_id), "%s", "stats_test_cable");

    (void)memset(&session, 0, sizeof(session));

    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        "se_performance"
    );

    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        "2026-09-05T18:00:00+02:00"
    );

    (void)snprintf(
        session.ended_at,
        sizeof(session.ended_at),
        "%s",
        "2026-09-05T19:00:00+02:00"
    );

    session.exercises = exercises;
    session.exercise_count = 4U;

    CHECK(
        trainlog_database_insert_session(
            database,
            &session
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_list_exercise_performance(
            database,
            "ex_external",
            points,
            8U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    /* PUBLIC CONTRACT: the reader retains one representative per session;
     * multiple same-exercise occurrences do not change cardinality. */
    CHECK(count == 1U);
    CHECK(points[0].has_performance != 0);
    CHECK(points[0].weight_kg > 89.99);
    CHECK(points[0].weight_kg < 90.01);
    CHECK(points[0].metric_value == 5);
    CHECK(points[0].actual_set_count == 3U);

    CHECK(
        trainlog_database_list_exercise_performance(
            database,
            "ex_assistance",
            points,
            8U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 1U);
    CHECK(points[0].has_performance != 0);
    CHECK(points[0].weight_kg > 29.99);
    CHECK(points[0].weight_kg < 30.01);
    CHECK(points[0].metric_value == 5);

    CHECK(
        trainlog_database_list_exercise_performance(
            database,
            "ex_none",
            points,
            8U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 1U);
    CHECK(points[0].has_performance != 0);
    CHECK(points[0].metric_value == 12);
    CHECK(points[0].has_weight == 0);

    trainlog_database_close(database);
    return true;
}

int main(void)
{
    CHECK(test_performance_semantics());

    (void)printf(
        "PASS exercise_performance\n"
    );

    return 0;
}
