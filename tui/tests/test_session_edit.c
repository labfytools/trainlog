/**
 * @file test_session_edit.c
 * @brief Transaction-safe persisted session editing tests.
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

static bool add_exercises(
    TrainlogDatabase *database
)
{
    CHECK(
        trainlog_database_insert_exercise(
            database,
            "ex_press",
            "Presse",
            "presse",
            TRAINLOG_TRACKING_REPS
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_insert_exercise(
            database,
            "ex_plank",
            "Gainage",
            "gainage",
            TRAINLOG_TRACKING_DURATION
        ) == TRAINLOG_STATUS_OK
    );

    return true;
}

static void bind_reps_exercise(
    TrainlogSessionExerciseInput *exercise,
    TrainlogSetInput *sets,
    size_t set_count,
    double weight
)
{
    size_t index;

    (void)memset(
        exercise,
        0,
        sizeof(*exercise)
    );

    (void)snprintf(
        exercise->exercise_id,
        sizeof(exercise->exercise_id),
        "%s",
        "ex_press"
    );

    exercise->load_mode =
        TRAINLOG_LOAD_EXTERNAL;

    exercise->rest_seconds = 90;
    exercise->target_sets = 3;
    exercise->target_reps = 10;
    exercise->target_has_weight = true;
    exercise->target_weight_kg = weight;
    exercise->notes = "note presse";
    exercise->sets = sets;
    exercise->set_count = set_count;

    for (index = 0U;
         index < set_count;
         ++index) {
        (void)memset(
            &sets[index],
            0,
            sizeof(sets[index])
        );

        sets[index].reps =
            10 - (int)index;

        sets[index].has_weight = true;
        sets[index].weight_kg = weight;
    }
}

static bool test_load_replace_and_rollback(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogSetInput original_sets[3];
    TrainlogSetInput replacement_sets[2];
    TrainlogSetInput rollback_sets[1];
    TrainlogSessionExerciseInput original;
    TrainlogSessionExerciseInput replacement;
    TrainlogSessionExerciseInput bad[2];
    TrainlogSessionInput session;
    TrainlogBodyObservationInput body;

    TrainlogSessionSummary loaded_session;
    TrainlogEditableExerciseRecord loaded_exercises[4];
    TrainlogSetInput loaded_sets[16];
    size_t exercise_count = 0U;
    size_t set_count = 0U;

    TrainlogBodyMetricPoint weight_points[4];
    size_t weight_count = 0U;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(add_exercises(database));

    bind_reps_exercise(
        &original,
        original_sets,
        3U,
        80.0
    );

    (void)memset(
        &session,
        0,
        sizeof(session)
    );

    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        "se_edit"
    );

    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        "2026-09-06T08:00:00+02:00"
    );

    (void)snprintf(
        session.ended_at,
        sizeof(session.ended_at),
        "%s",
        "2026-09-06T09:00:00+02:00"
    );

    session.session_type =
        TRAINLOG_SESSION_TRAINING;

    session.exercises = &original;
    session.exercise_count = 1U;

    CHECK(
        trainlog_database_insert_session(
            database,
            &session
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(&body, 0, sizeof(body));

    (void)snprintf(
        body.observation_id,
        sizeof(body.observation_id),
        "%s",
        "bo_edit"
    );

    (void)snprintf(
        body.observed_at,
        sizeof(body.observed_at),
        "%s",
        "2026-09-06T09:05:00+02:00"
    );

    body.session_id = "se_edit";
    body.has_body_weight = true;
    body.body_weight_kg = 84.1;

    CHECK(
        trainlog_database_insert_body_observation(
            database,
            &body
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_load_session_editable(
            database,
            "se_edit",
            &loaded_session,
            loaded_exercises,
            4U,
            &exercise_count,
            loaded_sets,
            16U,
            &set_count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(exercise_count == 1U);
    CHECK(set_count == 3U);
    CHECK(
        strcmp(
            loaded_exercises[0].exercise_id,
            "ex_press"
        ) == 0
    );
    CHECK(
        strcmp(
            loaded_exercises[0].notes,
            "note presse"
        ) == 0
    );
    CHECK(loaded_exercises[0].set_offset == 0U);
    CHECK(loaded_exercises[0].set_count == 3U);
    CHECK(loaded_sets[0].reps == 10);
    CHECK(loaded_sets[2].reps == 8);
    CHECK(loaded_sets[0].weight_kg > 79.99);
    CHECK(loaded_sets[0].weight_kg < 80.01);

    bind_reps_exercise(
        &replacement,
        replacement_sets,
        2U,
        85.0
    );

    replacement_sets[0].reps = 12;
    replacement_sets[1].reps = 11;

    CHECK(
        trainlog_database_replace_session_exercises(
            database,
            "se_edit",
            &replacement,
            1U
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_load_session_editable(
            database,
            "se_edit",
            &loaded_session,
            loaded_exercises,
            4U,
            &exercise_count,
            loaded_sets,
            16U,
            &set_count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(exercise_count == 1U);
    CHECK(set_count == 2U);
    CHECK(loaded_sets[0].reps == 12);
    CHECK(loaded_sets[1].reps == 11);
    CHECK(loaded_sets[0].weight_kg > 84.99);
    CHECK(loaded_sets[0].weight_kg < 85.01);

    /*
     * The body observation is linked to the stable sessions row. Replacing
     * child exercise/set rows must not delete or detach it.
     */
    CHECK(
        trainlog_database_list_body_metric_points(
            database,
            TRAINLOG_BODY_METRIC_WEIGHT,
            weight_points,
            4U,
            &weight_count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(weight_count == 1U);
    CHECK(weight_points[0].value > 84.09);
    CHECK(weight_points[0].value < 84.11);

    bind_reps_exercise(
        &bad[0],
        rollback_sets,
        1U,
        95.0
    );

    bad[1] = bad[0];

    (void)snprintf(
        bad[1].exercise_id,
        sizeof(bad[1].exercise_id),
        "%s",
        "ex_missing"
    );

    CHECK(
        trainlog_database_replace_session_exercises(
            database,
            "se_edit",
            bad,
            2U
        ) == TRAINLOG_STATUS_NOT_FOUND
    );

    /*
     * The failed replacement deleted and reinserted rows inside one
     * transaction. Rollback must restore the previous 85 kg / 12,11 data.
     */
    CHECK(
        trainlog_database_load_session_editable(
            database,
            "se_edit",
            &loaded_session,
            loaded_exercises,
            4U,
            &exercise_count,
            loaded_sets,
            16U,
            &set_count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(exercise_count == 1U);
    CHECK(set_count == 2U);
    CHECK(loaded_sets[0].reps == 12);
    CHECK(loaded_sets[1].reps == 11);
    CHECK(loaded_sets[0].weight_kg > 84.99);
    CHECK(loaded_sets[0].weight_kg < 85.01);

    trainlog_database_close(database);
    return true;
}

int main(void)
{
    CHECK(test_load_replace_and_rollback());

    (void)printf(
        "PASS session_edit\n"
    );

    return 0;
}
