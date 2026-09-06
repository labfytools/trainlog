#include <stdbool.h>
#include <stdint.h>
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

static bool test_profile_roundtrip(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogExercise exercises[4];
    size_t count = 0U;
    size_t index;
    bool found_walk = false;
    bool found_legacy = false;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_insert_exercise(
            database,
            "ex_legacy",
            "Presse",
            "presse",
            TRAINLOG_TRACKING_REPS
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

    CHECK(
        trainlog_database_insert_exercise_profiled(
            database,
            "ex_invalid",
            "Invalid",
            "invalid",
            TRAINLOG_TRACKING_REPS,
            TRAINLOG_RECORDING_CONTINUOUS,
            0U
        ) == TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_database_insert_exercise_profiled(
            database,
            "ex_unknown",
            "Unknown",
            "unknown",
            TRAINLOG_TRACKING_DURATION,
            TRAINLOG_RECORDING_CONTINUOUS,
            UINT32_C(8)
        ) == TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_database_list_exercises(
            database,
            exercises,
            4U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 2U);

    for (index = 0U; index < count; ++index) {
        if (strcmp(exercises[index].exercise_id, "ex_walk") == 0) {
            CHECK(
                exercises[index].recording_mode ==
                TRAINLOG_RECORDING_CONTINUOUS
            );
            CHECK(
                exercises[index].tracking_mode ==
                TRAINLOG_TRACKING_DURATION
            );
            CHECK(
                exercises[index].data_fields ==
                TRAINLOG_EXERCISE_DATA_SPEED_KMH
            );
            found_walk = true;
        }

        if (strcmp(exercises[index].exercise_id, "ex_legacy") == 0) {
            CHECK(
                exercises[index].recording_mode ==
                TRAINLOG_RECORDING_SETS
            );
            CHECK(exercises[index].data_fields == 0U);
            found_legacy = true;
        }
    }

    CHECK(found_walk);
    CHECK(found_legacy);

    trainlog_database_close(database);
    return true;
}

static bool test_v2_to_current_migration(void)
{
    char path[] =
        "/tmp/trainlog-schema-v2-profile-XXXXXX";

    static const char *const V2_SQL =
        "CREATE TABLE exercises ("
        "id INTEGER PRIMARY KEY,"
        "exercise_id TEXT NOT NULL UNIQUE,"
        "name TEXT NOT NULL,"
        "normalized_name TEXT NOT NULL UNIQUE,"
        "tracking_mode TEXT NOT NULL"
        ");"
        "CREATE TABLE sessions ("
        "id INTEGER PRIMARY KEY,"
        "session_id TEXT NOT NULL UNIQUE,"
        "started_at TEXT NOT NULL,"
        "ended_at TEXT,"
        "session_type TEXT NOT NULL DEFAULT 'training',"
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
        "INSERT INTO exercises("
        "exercise_id, name, normalized_name, tracking_mode"
        ") VALUES('ex_old', 'Ancien', 'ancien', 'duration');"
        "INSERT INTO sessions("
        "session_id, started_at, session_type"
        ") VALUES('se_old', '2026-09-01T10:00:00+02:00', 'training');"
        "INSERT INTO session_exercises("
        "session_row_id, exercise_row_id, position, load_mode,"
        "rest_seconds, target_sets, target_duration_seconds"
        ") VALUES(1, 1, 0, 'none', 0, 1, 60);"
        "PRAGMA user_version = 2;";

    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;
    TrainlogDatabase *database = NULL;
    int fd;
    int version = 0;

    fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(
        sqlite3_exec(raw, V2_SQL, NULL, NULL, NULL) ==
        SQLITE_OK
    );
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(
        trainlog_database_open(path, &database) ==
        TRAINLOG_STATUS_OK
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
            "SELECT recording_mode, data_fields "
            "FROM exercises WHERE exercise_id='ex_old';",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(
        strcmp(
            (const char *)sqlite3_column_text(statement, 0),
            "sets"
        ) == 0
    );
    CHECK(sqlite3_column_int64(statement, 1) == 0);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    statement = NULL;

    CHECK(
        sqlite3_prepare_v2(
            raw,
            "SELECT recording_mode, data_fields "
            "FROM session_exercises WHERE id=1;",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(
        strcmp(
            (const char *)sqlite3_column_text(statement, 0),
            "sets"
        ) == 0
    );
    CHECK(sqlite3_column_int64(statement, 1) == 0);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    statement = NULL;

    CHECK(
        sqlite3_prepare_v2(
            raw,
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name='continuous_activity';",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);

    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    CHECK(test_profile_roundtrip());
    CHECK(test_v2_to_current_migration());

    (void)printf("PASS exercise_profile_schema\n");
    return 0;
}
