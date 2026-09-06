/**
 * @file test_schema_v5_migration.c
 * @brief Direct v4 -> v5 migration regression test.
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

static bool test_v4_to_v5_preserves_session(void)
{
    char path[] =
        "/tmp/trainlog-schema-v4-v5-XXXXXX";

    static const char *const V4_SQL =
        "CREATE TABLE exercises ("
        "id INTEGER PRIMARY KEY,"
        "exercise_id TEXT NOT NULL UNIQUE,"
        "name TEXT NOT NULL,"
        "normalized_name TEXT NOT NULL UNIQUE,"
        "tracking_mode TEXT NOT NULL,"
        "recording_mode TEXT NOT NULL DEFAULT 'sets',"
        "data_fields INTEGER NOT NULL DEFAULT 0"
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
        "recording_mode TEXT NOT NULL DEFAULT 'sets',"
        "data_fields INTEGER NOT NULL DEFAULT 0,"
        "position INTEGER NOT NULL,"
        "load_mode TEXT NOT NULL,"
        "rest_seconds INTEGER NOT NULL,"
        "target_sets INTEGER CHECK(target_sets > 0),"
        "target_reps INTEGER CHECK(target_reps >= 1),"
        "target_duration_seconds INTEGER CHECK(target_duration_seconds > 0),"
        "target_weight_kg REAL,"
        "notes TEXT,"
        "CHECK("
        "  (recording_mode='sets' AND"
        "   target_sets IS NOT NULL AND"
        "   ((target_reps IS NOT NULL AND"
        "     target_duration_seconds IS NULL) OR"
        "    (target_reps IS NULL AND"
        "     target_duration_seconds IS NOT NULL))) OR"
        "  (recording_mode='continuous' AND"
        "   target_sets IS NULL AND"
        "   target_reps IS NULL AND"
        "   target_duration_seconds IS NULL)"
        ")"
        ");"

        "CREATE TABLE performed_sets ("
        "id INTEGER PRIMARY KEY,"
        "session_exercise_row_id INTEGER NOT NULL"
        " REFERENCES session_exercises(id),"
        "position INTEGER NOT NULL,"
        "reps INTEGER,"
        "duration_seconds INTEGER,"
        "weight_kg REAL"
        ");"

        "CREATE TABLE continuous_activity ("
        "id INTEGER PRIMARY KEY,"
        "session_exercise_row_id INTEGER NOT NULL UNIQUE"
        " REFERENCES session_exercises(id),"
        "duration_seconds INTEGER NOT NULL,"
        "speed_kmh REAL,"
        "distance_km REAL"
        ");"

        "INSERT INTO exercises("
        "exercise_id,name,normalized_name,tracking_mode,"
        "recording_mode,data_fields"
        ") VALUES("
        "'ex_v4','Pompes','pompes','reps','sets',0"
        ");"

        "INSERT INTO sessions("
        "session_id,started_at,ended_at,session_type,notes"
        ") VALUES("
        "'se_v4',"
        "'2026-09-06T10:00:00+02:00',"
        "'2026-09-06T10:30:00+02:00',"
        "'training',NULL"
        ");"

        "INSERT INTO session_exercises("
        "session_row_id,exercise_row_id,recording_mode,data_fields,"
        "position,load_mode,rest_seconds,target_sets,target_reps,"
        "target_duration_seconds,target_weight_kg,notes"
        ") VALUES("
        "1,1,'sets',0,0,'none',60,3,10,NULL,NULL,NULL"
        ");"

        "INSERT INTO performed_sets("
        "session_exercise_row_id,position,reps,duration_seconds,weight_kg"
        ") VALUES"
        "(1,0,10,NULL,NULL),"
        "(1,1,9,NULL,NULL),"
        "(1,2,8,NULL,NULL);"

        "PRAGMA user_version=4;";

    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    TrainlogSessionSummary summary;
    TrainlogPersistedExerciseDetail details[2];
    size_t detail_count = 0U;
    int version = 0;
    int fd;

    fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);

    CHECK(
        sqlite3_open(
            path,
            &raw
        ) == SQLITE_OK
    );

    CHECK(
        sqlite3_exec(
            raw,
            V4_SQL,
            NULL,
            NULL,
            NULL
        ) == SQLITE_OK
    );

    CHECK(
        sqlite3_close(raw) ==
        SQLITE_OK
    );

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

    CHECK(
        version == 5
    );

    CHECK(
        trainlog_database_get_session_details(
            database,
            "se_v4",
            &summary,
            details,
            2U,
            &detail_count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(detail_count == 1U);
    CHECK(details[0].target_sets == 3);
    CHECK(details[0].target_reps == 10);
    CHECK(details[0].actual_set_count == 3U);

    CHECK(
        strcmp(
            details[0].actual_summary,
            "10 / 9 / 8"
        ) == 0
    );

    trainlog_database_close(
        database
    );

    database = NULL;

    CHECK(unlink(path) == 0);

    return true;
}

int main(void)
{
    CHECK(
        test_v4_to_v5_preserves_session()
    );

    (void)printf(
        "PASS schema_v5_migration\n"
    );

    return 0;
}
