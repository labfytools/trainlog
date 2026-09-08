/**
 * @file test_schema_v7_migration.c
 * @brief Synthetic v7 -> v8 migration and reopen regression coverage.
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
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",            \
                __FILE__, __LINE__, #condition);                             \
            return false;                                                    \
        }                                                                    \
    } while (0)

static const char *const V7_FIXTURE_SQL =
    "PRAGMA foreign_keys=ON;"
    "CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT NOT NULL UNIQUE,name TEXT NOT NULL,normalized_name TEXT NOT NULL UNIQUE,tracking_mode TEXT NOT NULL,recording_mode TEXT NOT NULL,data_fields INTEGER NOT NULL);"
    "CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT NOT NULL UNIQUE,started_at TEXT NOT NULL,ended_at TEXT,session_type TEXT NOT NULL,notes TEXT);"
    "CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT NOT NULL UNIQUE,session_row_id INTEGER NOT NULL REFERENCES sessions(id),exercise_row_id INTEGER NOT NULL REFERENCES exercises(id),recording_mode TEXT NOT NULL,data_fields INTEGER NOT NULL,position INTEGER NOT NULL,load_mode TEXT NOT NULL,rest_seconds INTEGER NOT NULL,target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,equipment_id TEXT,notes TEXT,UNIQUE(session_row_id,position));"
    "CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id),position INTEGER NOT NULL,reps INTEGER,duration_seconds INTEGER,weight_kg REAL,UNIQUE(session_exercise_row_id,position));"
    "CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER NOT NULL UNIQUE REFERENCES session_exercises(id),duration_seconds INTEGER NOT NULL,speed_kmh REAL,distance_km REAL);"
    "CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT NOT NULL UNIQUE,observed_at TEXT NOT NULL,session_row_id INTEGER UNIQUE REFERENCES sessions(id),body_weight_kg REAL,neck_cm REAL,shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL,notes TEXT);"
    "INSERT INTO exercises VALUES(11,'ex_fixture_sets','Squat','squat','reps','sets',0);"
    "INSERT INTO exercises VALUES(12,'ex_fixture_run','Run','run','duration','continuous',3);"
    "INSERT INTO sessions VALUES(21,'se_fixture','2026-09-08T06:00:00Z','2026-09-08T07:00:00Z','training','fixture session');"
    "INSERT INTO session_exercises VALUES(31,'sxe_fixture_sets',21,11,'sets',0,0,'external',90,2,5,NULL,82.5,'eq_fixture_rack','sets note');"
    "INSERT INTO session_exercises VALUES(32,'sxe_fixture_run',21,12,'continuous',3,1,'none',0,NULL,NULL,NULL,NULL,NULL,'run note');"
    "INSERT INTO performed_sets VALUES(41,31,0,5,NULL,80.25);"
    "INSERT INTO performed_sets VALUES(42,31,1,4,NULL,82.5);"
    "INSERT INTO continuous_activity VALUES(51,32,1234,10.75,3.67);"
    "INSERT INTO body_observations(id,observation_id,observed_at,session_row_id,body_weight_kg,waist_cm,notes) VALUES(61,'bo_fixture','2026-09-08T07:05:00Z',21,74.25,81.5,'body note');"
    "PRAGMA user_version=7;";

static bool verify_preserved_values(const char *path)
{
    sqlite3 *connection = NULL;
    sqlite3_stmt *statement = NULL;

    CHECK(sqlite3_open_v2(path, &connection, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(sqlite3_prepare_v2(connection,
        "SELECT s.session_id,se.entry_id,se.position,se.equipment_id,ps.position,ps.reps,ps.weight_kg "
        "FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id "
        "JOIN performed_sets ps ON ps.session_exercise_row_id=se.id ORDER BY ps.position;",
        -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(strcmp((const char *)sqlite3_column_text(statement, 0), "se_fixture") == 0);
    CHECK(strcmp((const char *)sqlite3_column_text(statement, 1), "sxe_fixture_sets") == 0);
    CHECK(sqlite3_column_int(statement, 2) == 0);
    CHECK(strcmp((const char *)sqlite3_column_text(statement, 3), "eq_fixture_rack") == 0);
    CHECK(sqlite3_column_int(statement, 4) == 0);
    CHECK(sqlite3_column_int(statement, 5) == 5);
    CHECK(sqlite3_column_double(statement, 6) == 80.25);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(sqlite3_column_int(statement, 4) == 1);
    CHECK(sqlite3_column_int(statement, 5) == 4);
    CHECK(sqlite3_column_double(statement, 6) == 82.5);
    CHECK(sqlite3_step(statement) == SQLITE_DONE);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);

    CHECK(sqlite3_prepare_v2(connection,
        "SELECT se.entry_id,se.position,ca.duration_seconds,ca.speed_kmh,ca.distance_km,bo.observation_id,bo.body_weight_kg,bo.waist_cm,bo.notes "
        "FROM session_exercises se JOIN continuous_activity ca ON ca.session_exercise_row_id=se.id "
        "JOIN sessions s ON s.id=se.session_row_id JOIN body_observations bo ON bo.session_row_id=s.id;",
        -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(strcmp((const char *)sqlite3_column_text(statement, 0), "sxe_fixture_run") == 0);
    CHECK(sqlite3_column_int(statement, 1) == 1);
    CHECK(sqlite3_column_int(statement, 2) == 1234);
    CHECK(sqlite3_column_double(statement, 3) == 10.75);
    CHECK(sqlite3_column_double(statement, 4) == 3.67);
    CHECK(strcmp((const char *)sqlite3_column_text(statement, 5), "bo_fixture") == 0);
    CHECK(sqlite3_column_double(statement, 6) == 74.25);
    CHECK(sqlite3_column_double(statement, 7) == 81.5);
    CHECK(strcmp((const char *)sqlite3_column_text(statement, 8), "body note") == 0);
    CHECK(sqlite3_step(statement) == SQLITE_DONE);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_close(connection) == SQLITE_OK);
    return true;
}

static bool test_v7_migrates_and_v8_reopens(void)
{
    char path[] = "/tmp/trainlog-schema-v7-XXXXXX";
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    int version = 0;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, V7_FIXTURE_SQL, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(database, &version) == TRAINLOG_STATUS_OK);
    CHECK(version == 8);
    trainlog_database_close(database);
    CHECK(verify_preserved_values(path));

    database = NULL;
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    trainlog_database_close(database);
    CHECK(verify_preserved_values(path));
    CHECK(unlink(path) == 0);
    return true;
}

static bool test_v7_migration_sqlite_failure_has_diagnostic(void)
{
    char path[] = "/tmp/trainlog-schema-v7-failure-XXXXXX";
    char diagnostic[256];
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, V7_FIXTURE_SQL, NULL, NULL, NULL) == SQLITE_OK);
    /* Synthetic collision: only the v8 migration target is pre-created. */
    CHECK(sqlite3_exec(raw, "CREATE TABLE custom_equipment (equipment_id TEXT PRIMARY KEY);",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(trainlog_database_open_with_diagnostic(path, &database, diagnostic,
        sizeof(diagnostic)) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(database == NULL);
    CHECK(strncmp(diagnostic, "migrate database to schema v8: SQLite rc=",
        strlen("migrate database to schema v8: SQLite rc=")) == 0);
    CHECK(strstr(diagnostic, "extended_rc=") != NULL);
    CHECK(strstr(diagnostic, "custom_equipment") != NULL);
    CHECK(strstr(diagnostic, "already exists") != NULL);
    CHECK(unlink(path) == 0);
    return true;
}

static bool test_newer_schema_has_application_diagnostic(void)
{
    char path[] = "/tmp/trainlog-schema-v9-XXXXXX";
    char diagnostic[256];
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "PRAGMA user_version=9;", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(trainlog_database_open_with_diagnostic(path, &database, diagnostic,
        sizeof(diagnostic)) == TRAINLOG_STATUS_SCHEMA_UNSUPPORTED);
    CHECK(database == NULL);
    CHECK(strstr(diagnostic, "schema version 9 is newer") != NULL);
    CHECK(strstr(diagnostic, "SQLite") == NULL);
    CHECK(unlink(path) == 0);
    return true;
}

static bool test_unrecognized_historic_schema_has_application_diagnostic(void)
{
    char path[] = "/tmp/trainlog-schema-invalid-XXXXXX";
    char diagnostic[256];
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "PRAGMA user_version=-1;", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(trainlog_database_open_with_diagnostic(path, &database, diagnostic,
        sizeof(diagnostic)) == TRAINLOG_STATUS_SCHEMA_UNSUPPORTED);
    CHECK(database == NULL);
    CHECK(strstr(diagnostic, "schema version -1 is unsupported") != NULL);
    CHECK(strstr(diagnostic, "SQLite") == NULL);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    CHECK(test_v7_migrates_and_v8_reopens());
    CHECK(test_v7_migration_sqlite_failure_has_diagnostic());
    CHECK(test_newer_schema_has_application_diagnostic());
    CHECK(test_unrecognized_historic_schema_has_application_diagnostic());
    (void)printf("PASS schema_v7_migration\n");
    return 0;
}
