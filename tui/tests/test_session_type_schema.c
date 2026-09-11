/**
 * @file test_session_type_schema.c
 * @brief Historical schema migration and session type persistence tests.
 */

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
            return false;                                                    \
        }                                                                    \
    } while (0)

static bool test_session_type_roundtrip(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogSessionInput training;
    TrainlogSessionInput max_test;
    TrainlogSessionExerciseInput training_occurrence;
    TrainlogSessionExerciseInput max_occurrence;
    TrainlogSetInput set = {8, 0, false, 0.0};
    TrainlogSessionSummary sessions[4];
    size_t count = 0U;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Presse test",
        "presse test", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);

    (void)memset(&training, 0, sizeof(training));
    (void)memset(&training_occurrence, 0, sizeof(training_occurrence));
    (void)snprintf(training_occurrence.exercise_id,
        sizeof(training_occurrence.exercise_id), "%s",
        "ex_33333333-3333-4333-8333-333333333333");
    training_occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    training_occurrence.sets = &set;
    training_occurrence.set_count = 1U;

    (void)snprintf(
        training.session_id,
        sizeof(training.session_id),
        "%s",
        "se_training"
    );

    (void)snprintf(
        training.started_at,
        sizeof(training.started_at),
        "%s",
        "2026-09-05T18:00:00+02:00"
    );

    CHECK(
        training.session_type ==
        TRAINLOG_SESSION_TRAINING
    );
    training.exercises = &training_occurrence;
    training.exercise_count = 1U;

    CHECK(
        trainlog_database_insert_session(
            database,
            &training
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(&max_test, 0, sizeof(max_test));
    (void)memset(&max_occurrence, 0, sizeof(max_occurrence));
    (void)snprintf(max_occurrence.exercise_id,
        sizeof(max_occurrence.exercise_id), "%s",
        "ex_33333333-3333-4333-8333-333333333333");
    max_occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    max_occurrence.has_max_weight = true;
    max_occurrence.max_weight_kg = 100.0;

    (void)snprintf(
        max_test.session_id,
        sizeof(max_test.session_id),
        "%s",
        "se_max_test"
    );

    (void)snprintf(
        max_test.started_at,
        sizeof(max_test.started_at),
        "%s",
        "2026-09-05T19:00:00+02:00"
    );

    max_test.session_type =
        TRAINLOG_SESSION_MAX_TEST;
    max_test.exercises = &max_occurrence;
    max_test.exercise_count = 1U;

    CHECK(
        trainlog_database_insert_session(
            database,
            &max_test
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_list_sessions(
            database,
            sessions,
            4U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 2U);

    CHECK(
        strcmp(
            sessions[0].session_id,
            "se_max_test"
        ) == 0
    );

    CHECK(
        sessions[0].session_type ==
        TRAINLOG_SESSION_MAX_TEST
    );

    CHECK(
        strcmp(
            sessions[1].session_id,
            "se_training"
        ) == 0
    );

    CHECK(
        sessions[1].session_type ==
        TRAINLOG_SESSION_TRAINING
    );

    trainlog_database_close(database);
    return true;
}

static bool test_v1_to_v2_migration(void)
{
    char path[] =
        "/tmp/trainlog-schema-v1-XXXXXX";

    static const char *const V1_SQL =
        "CREATE TABLE exercises ("
        "id INTEGER PRIMARY KEY,"
        "exercise_id TEXT NOT NULL UNIQUE,"
        "name TEXT NOT NULL,"
        "normalized_name TEXT NOT NULL UNIQUE,"
        "tracking_mode TEXT NOT NULL"
        " CHECK (tracking_mode IN ('reps', 'duration'))"
        ");"
        "CREATE TABLE sessions ("
        "id INTEGER PRIMARY KEY,"
        "session_id TEXT NOT NULL UNIQUE,"
        "started_at TEXT NOT NULL,"
        "ended_at TEXT,"
        "notes TEXT"
        ");"
        "CREATE TABLE session_exercises ("
        "id INTEGER PRIMARY KEY,"
        "session_row_id INTEGER NOT NULL REFERENCES sessions(id),"
        "exercise_row_id INTEGER NOT NULL REFERENCES exercises(id),"
        "position INTEGER NOT NULL,"
        "load_mode TEXT NOT NULL,"
        "rest_seconds INTEGER NOT NULL,"
        "target_sets INTEGER NOT NULL,"
        "target_reps INTEGER,"
        "target_duration_seconds INTEGER,"
        "target_weight_kg REAL,"
        "notes TEXT"
        ");"
        "CREATE TABLE performed_sets ("
        "id INTEGER PRIMARY KEY,"
        "session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id),"
        "position INTEGER NOT NULL,"
        "reps INTEGER,"
        "duration_seconds INTEGER,"
        "weight_kg REAL"
        ");"
        "CREATE TABLE body_observations ("
        "id INTEGER PRIMARY KEY,"
        "observation_id TEXT NOT NULL UNIQUE,"
        "observed_at TEXT NOT NULL,"
        "session_row_id INTEGER UNIQUE REFERENCES sessions(id),"
        "body_weight_kg REAL,"
        "neck_cm REAL,"
        "shoulders_cm REAL,"
        "chest_cm REAL,"
        "waist_cm REAL,"
        "hips_cm REAL,"
        "left_arm_cm REAL,"
        "right_arm_cm REAL,"
        "left_forearm_cm REAL,"
        "right_forearm_cm REAL,"
        "left_thigh_cm REAL,"
        "right_thigh_cm REAL,"
        "left_calf_cm REAL,"
        "right_calf_cm REAL,"
        "notes TEXT"
        ");"
        "INSERT INTO sessions("
        "session_id, started_at, ended_at, notes"
        ") VALUES("
        "'se_old',"
        "'2026-08-01T10:00:00+02:00',"
        "NULL,"
        "NULL"
        ");"
        "PRAGMA user_version = 1;";

    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;
    TrainlogDatabase *database = NULL;
    int fd;
    int version = 0;
    int rc;

    fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);

    rc = sqlite3_open(path, &raw);
    CHECK(rc == SQLITE_OK);

    CHECK(
        sqlite3_exec(
            raw,
            V1_SQL,
            NULL,
            NULL,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(
        trainlog_database_open(
            path,
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_schema_version(
            database,
            &version
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        version ==
        TRAINLOG_DATABASE_SCHEMA_VERSION
    );

    trainlog_database_close(database);
    database = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);

    CHECK(
        sqlite3_prepare_v2(
            raw,
            "SELECT session_type "
            "FROM sessions "
            "WHERE session_id = 'se_old';",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);

    {
        const unsigned char *type =
            sqlite3_column_text(statement, 0);

        CHECK(type != NULL);

        CHECK(
            strcmp(
                (const char *)type,
                "training"
            ) == 0
        );
    }

    CHECK(
        sqlite3_finalize(statement) ==
        SQLITE_OK
    );

    statement = NULL;

    rc = sqlite3_exec(
        raw,
        "INSERT INTO sessions("
        "session_id, started_at, session_type"
        ") VALUES("
        "'se_invalid',"
        "'2026-08-02T10:00:00+02:00',"
        "'invalid'"
        ");",
        NULL,
        NULL,
        NULL
    );

    CHECK(rc == SQLITE_CONSTRAINT);

    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    CHECK(test_session_type_roundtrip());
    CHECK(test_v1_to_v2_migration());

    (void)printf("PASS session_type_schema\n");
    return 0;
}
