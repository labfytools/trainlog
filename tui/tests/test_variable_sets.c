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

static bool test_targetless_variable_sets(void)
{
    static const int reps[] = {
        4, 5, 6, 7, 8, 9, 10,
        9, 8, 7, 6, 5, 4
    };

    TrainlogDatabase *database = NULL;
    TrainlogSetInput sets[
        sizeof(reps) /
        sizeof(reps[0])
    ];

    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;
    TrainlogSessionSummary summary;
    TrainlogPersistedExerciseDetail detail[2];
    size_t detail_count = 0U;
    size_t index;
    int schema_version = 0;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_schema_version(
            database,
            &schema_version
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        schema_version ==
        TRAINLOG_DATABASE_SCHEMA_VERSION
    );

    CHECK(
        trainlog_database_insert_exercise(
            database,
            "ex_variable_reps",
            "Pompes pyramide",
            "pompes pyramide",
            TRAINLOG_TRACKING_REPS
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(
        sets,
        0,
        sizeof(sets)
    );

    for (
        index = 0U;
        index <
            sizeof(reps) /
            sizeof(reps[0]);
        ++index
    ) {
        sets[index].reps =
            reps[index];
    }

    (void)memset(
        &exercise,
        0,
        sizeof(exercise)
    );

    (void)snprintf(
        exercise.exercise_id,
        sizeof(exercise.exercise_id),
        "%s",
        "ex_variable_reps"
    );

    exercise.recording_mode =
        TRAINLOG_RECORDING_SETS;

    exercise.load_mode =
        TRAINLOG_LOAD_NONE;

    exercise.rest_seconds = 0;

    /*
     * No target is invented. These are actual observations imported from
     * a mobile session.
     */
    exercise.target_sets = 0;
    exercise.target_reps = 0;
    exercise.target_duration_seconds = 0;

    exercise.sets = sets;

    exercise.set_count =
        sizeof(reps) /
        sizeof(reps[0]);

    (void)memset(
        &session,
        0,
        sizeof(session)
    );

    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        "se_variable_reps"
    );

    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        "2026-09-06T16:30:00+02:00"
    );

    (void)snprintf(
        session.ended_at,
        sizeof(session.ended_at),
        "%s",
        "2026-09-06T16:45:00+02:00"
    );

    session.exercises =
        &exercise;

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
            "se_variable_reps",
            &summary,
            detail,
            2U,
            &detail_count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(detail_count == 1U);
    CHECK(summary.exercise_count == 1U);
    CHECK(detail[0].target_sets == 0);
    CHECK(detail[0].target_reps == 0);

    CHECK(
        detail[0].actual_set_count ==
        sizeof(reps) /
            sizeof(reps[0])
    );

    CHECK(
        strcmp(
            detail[0].actual_summary,
            "4 / 5 / 6 / 7 / 8 / 9 / 10 / 9 / 8 / 7 / 6 / 5 / 4"
        ) == 0
    );

    trainlog_database_close(
        database
    );

    return true;
}

int main(void)
{
    CHECK(
        test_targetless_variable_sets()
    );

    (void)printf(
        "PASS variable_sets\n"
    );

    return 0;
}
