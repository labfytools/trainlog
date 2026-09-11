/**
 * @file test_schema_v9_migration.c
 * @brief Lossless performed-set weight migration regression.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "trainlog/database.h"

#define CHECK(condition) do {                                                \
    if (!(condition)) {                                                      \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",               \
            __FILE__, __LINE__, #condition);                                 \
        return false;                                                        \
    }                                                                        \
} while (0)

static const char *const V9_FIXTURE_SQL =
    "PRAGMA foreign_keys=ON;"
    "CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT NOT NULL UNIQUE,"
    "name TEXT NOT NULL,normalized_name TEXT NOT NULL UNIQUE,tracking_mode TEXT NOT NULL,"
    "recording_mode TEXT NOT NULL,data_fields INTEGER NOT NULL);"
    "CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT NOT NULL UNIQUE,"
    "started_at TEXT NOT NULL,ended_at TEXT,session_type TEXT NOT NULL,notes TEXT);"
    "CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT NOT NULL UNIQUE,"
    "session_row_id INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,"
    "exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE RESTRICT,"
    "recording_mode TEXT NOT NULL,data_fields INTEGER NOT NULL,position INTEGER NOT NULL,"
    "load_mode TEXT NOT NULL,rest_seconds INTEGER NOT NULL,target_sets INTEGER,"
    "target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,"
    "equipment_id TEXT,notes TEXT,UNIQUE(session_row_id,position));"
    "CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER NOT NULL "
    "REFERENCES session_exercises(id) ON DELETE CASCADE,position INTEGER NOT NULL CHECK(position>=0),"
    "reps INTEGER CHECK(reps>=0),duration_seconds INTEGER CHECK(duration_seconds>0),"
    "weight_kg REAL CHECK(weight_kg>0.0),UNIQUE(session_exercise_row_id,position),"
    "CHECK((reps IS NOT NULL AND duration_seconds IS NULL) OR "
    "(reps IS NULL AND duration_seconds IS NOT NULL)));"
    "CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY "
    "REFERENCES session_exercises(id) ON DELETE CASCADE,max_weight_kg REAL NOT NULL "
    "CHECK(max_weight_kg>0.0));"
    "INSERT INTO exercises VALUES(7,'ex_fixture','Fixture','fixture','reps','sets',0);"
    "INSERT INTO sessions VALUES(11,'se_fixture','2026-09-09T10:00:00+02:00',NULL,'training',NULL);"
    "INSERT INTO session_exercises VALUES(13,'sxe_fixture',11,7,'sets',0,4,'none',0,NULL,NULL,NULL,NULL,NULL,NULL);"
    "INSERT INTO performed_sets VALUES(17,13,2,8,NULL,32.5);"
    "INSERT INTO performed_sets VALUES(19,13,5,7,NULL,NULL);"
    "PRAGMA user_version=9;";

static bool scalar_text_is(sqlite3 *db, const char *sql, const char *expected)
{
    sqlite3_stmt *statement = NULL;
    bool matches = false;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char *value = sqlite3_column_text(statement, 0);
        matches = value != NULL && strcmp((const char *)value, expected) == 0;
    }
    (void)sqlite3_finalize(statement);
    return matches;
}

static bool test_v9_to_current_is_lossless(void)
{
    char path[] = "/tmp/trainlog-schema-v9-current-XXXXXX";
    sqlite3 *raw = NULL;
    sqlite3_stmt *rows = NULL;
    TrainlogDatabase *database = NULL;
    int version = 0;
    int foreign_keys = 0;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, V9_FIXTURE_SQL, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(database, &version) == TRAINLOG_STATUS_OK);
    CHECK(version == TRAINLOG_DATABASE_SCHEMA_VERSION);
    CHECK(trainlog_database_foreign_keys_enabled(database, &foreign_keys) == TRAINLOG_STATUS_OK);
    CHECK(foreign_keys == 1);
    trainlog_database_close(database);
    database = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(scalar_text_is(raw, "PRAGMA integrity_check;", "ok"));
    CHECK(sqlite3_prepare_v2(raw, "PRAGMA foreign_key_check;", -1, &rows, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(rows) == SQLITE_DONE);
    CHECK(sqlite3_finalize(rows) == SQLITE_OK);
    rows = NULL;
    CHECK(sqlite3_prepare_v2(raw,
        "SELECT id,session_exercise_row_id,position,reps,duration_seconds,weight_kg "
        "FROM performed_sets ORDER BY position;", -1, &rows, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(rows) == SQLITE_ROW);
    CHECK(sqlite3_column_int64(rows, 0) == 17);
    CHECK(sqlite3_column_int64(rows, 1) == 13);
    CHECK(sqlite3_column_int(rows, 2) == 2);
    CHECK(sqlite3_column_int(rows, 3) == 8);
    CHECK(sqlite3_column_type(rows, 4) == SQLITE_NULL);
    CHECK(sqlite3_column_double(rows, 5) == 32.5);
    CHECK(sqlite3_step(rows) == SQLITE_ROW);
    CHECK(sqlite3_column_int64(rows, 0) == 19);
    CHECK(sqlite3_column_type(rows, 5) == SQLITE_NULL);
    CHECK(sqlite3_step(rows) == SQLITE_DONE);
    CHECK(sqlite3_finalize(rows) == SQLITE_OK);
    rows = NULL;

    CHECK(sqlite3_exec(raw,
        "INSERT INTO performed_sets VALUES(23,13,6,6,NULL,0.0);",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
        "INSERT INTO performed_sets VALUES(29,13,7,5,NULL,-0.5);",
        NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(raw,
        "INSERT INTO performed_sets VALUES(31,13,8,4,3,NULL);",
        NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(raw,
        "INSERT INTO performed_sets VALUES(37,999,9,4,NULL,NULL);",
        NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(unlink(path) == 0);
    return true;
}

static bool test_v9_to_v10_failure_rolls_back(void)
{
    char path[] = "/tmp/trainlog-schema-v9-v10-failure-XXXXXX";
    char diagnostic[256];
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, V9_FIXTURE_SQL, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
        "CREATE TABLE performed_sets_v9(collision INTEGER);",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(trainlog_database_open_with_diagnostic(path, &database, diagnostic,
        sizeof(diagnostic)) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(database == NULL);
    CHECK(strstr(diagnostic, "migrate database to schema v12") != NULL);
    CHECK(strstr(diagnostic, "performed_sets_v9") != NULL);

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(scalar_text_is(raw, "PRAGMA integrity_check;", "ok"));
    {
        sqlite3_stmt *statement = NULL;
        CHECK(sqlite3_prepare_v2(raw, "PRAGMA user_version;", -1,
            &statement, NULL) == SQLITE_OK);
        CHECK(sqlite3_step(statement) == SQLITE_ROW);
        CHECK(sqlite3_column_int(statement, 0) == 9);
        CHECK(sqlite3_finalize(statement) == SQLITE_OK);
        CHECK(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM performed_sets;",
            -1, &statement, NULL) == SQLITE_OK);
        CHECK(sqlite3_step(statement) == SQLITE_ROW);
        CHECK(sqlite3_column_int(statement, 0) == 2);
        CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    }
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    if (!test_v9_to_current_is_lossless() ||
        !test_v9_to_v10_failure_rolls_back()) return 1;
    (void)printf("PASS schema_v9_migration\n");
    return 0;
}
