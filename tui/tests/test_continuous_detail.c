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
            return 1;                                                        \
        }                                                                    \
    } while (0)

int main(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;
    TrainlogSessionSummary summary;
    TrainlogPersistedExerciseDetail details[2];
    size_t count = 0U;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_insert_exercise_profiled(
            database,
            "ex_walk",
            "Marche",
            "marche",
            TRAINLOG_TRACKING_DURATION,
            TRAINLOG_RECORDING_CONTINUOUS,
            TRAINLOG_EXERCISE_DATA_SPEED_KMH
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(
        &exercise,
        0,
        sizeof(exercise)
    );

    (void)snprintf(
        exercise.exercise_id,
        sizeof(exercise.exercise_id),
        "%s",
        "ex_walk"
    );

    exercise.recording_mode =
        TRAINLOG_RECORDING_CONTINUOUS;

    exercise.data_fields =
        TRAINLOG_EXERCISE_DATA_SPEED_KMH;

    exercise.load_mode =
        TRAINLOG_LOAD_NONE;

    exercise.continuous_duration_seconds =
        1800;

    exercise.continuous_has_speed =
        true;

    exercise.continuous_speed_kmh =
        5.8;

    (void)memset(
        &session,
        0,
        sizeof(session)
    );

    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        "se_walk_detail"
    );

    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        "2026-09-06T13:00:00+02:00"
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
            session.session_id,
            &summary,
            details,
            2U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 1U);

    CHECK(
        details[0].recording_mode ==
        TRAINLOG_RECORDING_CONTINUOUS
    );

    CHECK(
        details[0].continuous_duration_seconds ==
        1800
    );

    CHECK(
        details[0].has_continuous_speed != 0
    );

    CHECK(
        details[0].continuous_speed_kmh >
        5.79
    );

    CHECK(
        details[0].continuous_speed_kmh <
        5.81
    );

    CHECK(
        details[0].actual_set_count ==
        0U
    );

    CHECK(
        strcmp(
            details[0].actual_summary,
            "activité continue"
        ) == 0
    );

    trainlog_database_close(
        database
    );

    (void)printf(
        "PASS continuous_detail\n"
    );

    return 0;
}
