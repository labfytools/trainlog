#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

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
    char path[] =
        "/tmp/trainlog-continuous-XXXXXX";

    TrainlogDatabase *database = NULL;
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;
    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;
    int fd;

    fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);

    CHECK(
        trainlog_database_open(
            path,
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
        2700;

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
        "se_walk"
    );

    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        "2026-09-06T12:00:00+02:00"
    );

    session.exercises = &exercise;
    session.exercise_count = 1U;

    CHECK(
        trainlog_database_insert_session(
            database,
            &session
        ) == TRAINLOG_STATUS_OK
    );

    trainlog_database_close(database);
    database = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);

    CHECK(
        sqlite3_prepare_v2(
            raw,
            "SELECT recording_mode, data_fields, "
            "target_sets, target_reps, "
            "target_duration_seconds "
            "FROM session_exercises "
            "WHERE id = 1;",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);

    CHECK(
        strcmp(
            (const char *)
                sqlite3_column_text(
                    statement,
                    0
                ),
            "continuous"
        ) == 0
    );

    CHECK(
        sqlite3_column_int(
            statement,
            1
        ) ==
        (int)TRAINLOG_EXERCISE_DATA_SPEED_KMH
    );

    CHECK(
        sqlite3_column_type(
            statement,
            2
        ) == SQLITE_NULL
    );

    CHECK(
        sqlite3_column_type(
            statement,
            3
        ) == SQLITE_NULL
    );

    CHECK(
        sqlite3_column_type(
            statement,
            4
        ) == SQLITE_NULL
    );

    CHECK(
        sqlite3_finalize(
            statement
        ) == SQLITE_OK
    );

    statement = NULL;

    CHECK(
        sqlite3_prepare_v2(
            raw,
            "SELECT duration_seconds, "
            "speed_kmh, distance_km "
            "FROM continuous_activity "
            "WHERE session_exercise_row_id = 1;",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);

    CHECK(
        sqlite3_column_int(
            statement,
            0
        ) == 2700
    );

    CHECK(
        sqlite3_column_double(
            statement,
            1
        ) > 5.79
    );

    CHECK(
        sqlite3_column_double(
            statement,
            1
        ) < 5.81
    );

    CHECK(
        sqlite3_column_type(
            statement,
            2
        ) == SQLITE_NULL
    );

    CHECK(
        sqlite3_finalize(
            statement
        ) == SQLITE_OK
    );

    statement = NULL;

    CHECK(
        sqlite3_prepare_v2(
            raw,
            "SELECT COUNT(*) "
            "FROM performed_sets;",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);

    CHECK(
        sqlite3_column_int(
            statement,
            0
        ) == 0
    );

    CHECK(
        sqlite3_finalize(
            statement
        ) == SQLITE_OK
    );

    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(unlink(path) == 0);

    (void)printf(
        "PASS continuous_session\n"
    );

    return 0;
}
