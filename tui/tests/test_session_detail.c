/**
 * @file test_session_detail.c
 * @brief Tests for persisted workout detail queries.
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

static bool test_session_details(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogSetInput sets[3];
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;
    TrainlogSessionSummary summary;
    TrainlogPersistedExerciseDetail details[4];
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
            "ex_detail",
            "Presse à cuisses",
            "presse à cuisses",
            TRAINLOG_TRACKING_REPS
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(sets, 0, sizeof(sets));

    sets[0].reps = 5;
    sets[0].has_weight = true;
    sets[0].weight_kg = 80.0;

    sets[1] = sets[0];

    sets[2].reps = 3;
    sets[2].has_weight = true;
    sets[2].weight_kg = 80.0;

    (void)memset(
        &exercise,
        0,
        sizeof(exercise)
    );

    (void)snprintf(
        exercise.exercise_id,
        sizeof(exercise.exercise_id),
        "%s",
        "ex_detail"
    );

    exercise.load_mode =
        TRAINLOG_LOAD_EXTERNAL;
    exercise.rest_seconds = 60;
    exercise.target_sets = 3;
    exercise.target_reps = 5;
    exercise.target_has_weight = true;
    exercise.target_weight_kg = 80.0;
    exercise.sets = sets;
    exercise.set_count = 3U;

    (void)memset(
        &session,
        0,
        sizeof(session)
    );

    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        "se_detail"
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

    session.exercises = &exercise;
    session.exercise_count = 1U;

    CHECK(
        trainlog_database_insert_session(
            database,
            &session
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_get_session_details(
            database,
            "se_detail",
            &summary,
            details,
            4U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 1U);
    CHECK(summary.exercise_count == 1U);
    CHECK(
        strcmp(
            details[0].name,
            "Presse à cuisses"
        ) == 0
    );
    CHECK(details[0].target_sets == 3);
    CHECK(details[0].target_reps == 5);
    CHECK(details[0].rest_seconds == 60);
    CHECK(details[0].actual_set_count == 3U);
    CHECK(
        strcmp(
            details[0].actual_summary,
            "5@80.0 / 5@80.0 / 3@80.0"
        ) == 0
    );

    trainlog_database_close(database);
    return true;
}

int main(void)
{
    CHECK(test_session_details());
    (void)printf("PASS session_details\n");
    return 0;
}
