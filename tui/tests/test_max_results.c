/**
 * @file test_max_results.c
 * @brief Explicit measured-max persistence and bounded migration regressions.
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
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",           \
                __FILE__, __LINE__, #condition);                             \
            return false;                                                    \
        }                                                                    \
    } while (0)

static bool add_exercise(TrainlogDatabase *database, const char *id,
                         const char *name)
{
    return trainlog_database_insert_exercise(database, id, name, name,
        TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK;
}

static void max_input(TrainlogSessionExerciseInput *input,
                      const char *entry_id, const char *exercise_id,
                      double weight)
{
    (void)memset(input, 0, sizeof(*input));
    (void)snprintf(input->entry_id, sizeof(input->entry_id), "%s", entry_id);
    (void)snprintf(input->exercise_id, sizeof(input->exercise_id), "%s",
        exercise_id);
    (void)snprintf(input->equipment_id, sizeof(input->equipment_id), "%s",
        "rear_delt_pec_fly");
    input->recording_mode = TRAINLOG_RECORDING_SETS;
    input->load_mode = TRAINLOG_LOAD_NONE;
    input->has_max_weight = true;
    input->max_weight_kg = weight;
}

static bool test_explicit_max_round_trip_and_identity(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogSessionExerciseInput entries[3];
    TrainlogSessionInput session;
    TrainlogSessionSummary summary;
    TrainlogPersistedExerciseDetail details[3];
    TrainlogEditableExerciseRecord editable[3];
    TrainlogExercisePerformancePoint points[2];
    size_t count = 0U;
    size_t set_count = 0U;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(add_exercise(database, "ex_pec", "Pec Fly"));
    CHECK(add_exercise(database, "ex_rear", "Rear Delt Fly"));
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_walk", "Marche", "marche", TRAINLOG_TRACKING_DURATION,
        TRAINLOG_RECORDING_CONTINUOUS, TRAINLOG_EXERCISE_DATA_SPEED_KMH) ==
        TRAINLOG_STATUS_OK);
    max_input(&entries[0], "sxe_pec", "ex_pec", 100.0);
    max_input(&entries[1], "sxe_rear", "ex_rear", 86.0);
    (void)memset(&entries[2], 0, sizeof(entries[2]));
    (void)snprintf(entries[2].entry_id, sizeof(entries[2].entry_id), "%s",
        "sxe_walk");
    (void)snprintf(entries[2].exercise_id, sizeof(entries[2].exercise_id), "%s",
        "ex_walk");
    entries[2].recording_mode = TRAINLOG_RECORDING_CONTINUOUS;
    entries[2].data_fields = TRAINLOG_EXERCISE_DATA_SPEED_KMH;
    entries[2].load_mode = TRAINLOG_LOAD_NONE;
    entries[2].continuous_duration_seconds = 600;
    entries[2].continuous_has_speed = true;
    entries[2].continuous_speed_kmh = 5.5;

    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_explicit_max");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2031-02-03T08:15:00+01:00");
    session.session_type = TRAINLOG_SESSION_MAX_TEST;
    session.exercises = entries;
    session.exercise_count = 3U;
    CHECK(trainlog_database_insert_session(database, &session) ==
        TRAINLOG_STATUS_OK);

    CHECK(trainlog_database_get_session_details(database, "se_explicit_max",
        &summary, details, 3U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 3U);
    CHECK(details[0].has_max_weight != 0 && details[0].max_weight_kg == 100.0);
    CHECK(details[1].has_max_weight != 0 && details[1].max_weight_kg == 86.0);
    CHECK(details[0].actual_set_count == 0U && details[1].actual_set_count == 0U);
    CHECK(strcmp(details[0].entry_id, "sxe_pec") == 0);
    CHECK(strcmp(details[0].equipment_id, details[1].equipment_id) == 0);
    CHECK(details[2].continuous_duration_seconds == 600);
    CHECK(details[2].has_continuous_speed != 0 &&
        details[2].continuous_speed_kmh == 5.5);

    trainlog_database_free_session_details(details, count);

    CHECK(trainlog_database_load_session_editable(database, "se_explicit_max",
        &summary, editable, 3U, &count, NULL, 0U, &set_count) ==
        TRAINLOG_STATUS_OK);
    CHECK(count == 3U && set_count == 0U);
    CHECK(editable[2].continuous_duration_seconds == 600);
    CHECK(editable[2].has_continuous_speed != 0 &&
        editable[2].continuous_speed_kmh == 5.5);

    CHECK(trainlog_database_list_exercise_performance(database, "ex_pec",
        points, 2U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && points[0].has_performance != 0);
    CHECK(points[0].has_explicit_max != 0);
    CHECK(points[0].actual_set_count == 0U && points[0].weight_kg == 100.0);
    CHECK(strcmp(points[0].equipment_id, "rear_delt_pec_fly") == 0);

    entries[0].max_weight_kg = 101.5;
    CHECK(trainlog_database_replace_session_exercises(database,
        "se_explicit_max", entries, 3U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_get_session_details(database, "se_explicit_max",
        &summary, details, 3U, &count) == TRAINLOG_STATUS_OK);
    CHECK(details[0].max_weight_kg == 101.5);
    CHECK(strcmp(details[0].entry_id, "sxe_pec") == 0);

    trainlog_database_free_session_details(details, count);

    session.session_type = TRAINLOG_SESSION_TRAINING;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_invalid_training_max");
    CHECK(trainlog_database_insert_session(database, &session) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT);

    trainlog_database_close(database);
    return true;
}

static bool test_v8_migration_refuses_to_guess_multiple_attempts(void)
{
    static const char *const SQL =
        "CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_type TEXT);"
        "CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,"
        "session_row_id INTEGER,recording_mode TEXT);"
        "CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,"
        "session_exercise_row_id INTEGER,position INTEGER,reps INTEGER,"
        "duration_seconds INTEGER,weight_kg REAL);"
        "INSERT INTO sessions VALUES(1,'max_test');"
        "INSERT INTO session_exercises VALUES(10,1,'sets');"
        "INSERT INTO session_exercises VALUES(11,1,'sets');"
        "INSERT INTO session_exercises VALUES(12,1,'sets');"
        "INSERT INTO performed_sets VALUES(20,10,0,1,NULL,100.0);"
        "INSERT INTO performed_sets VALUES(21,11,0,1,NULL,80.0);"
        "INSERT INTO performed_sets VALUES(22,11,1,1,NULL,86.0);"
        "INSERT INTO performed_sets VALUES(23,12,0,1,NULL,0.0);"
        "PRAGMA user_version=8;";
    char path[] = "/tmp/trainlog-max-v8-XXXXXX";
    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;
    TrainlogDatabase *database = NULL;
    int fd = mkstemp(path);

    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, SQL, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    trainlog_database_close(database);

    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(sqlite3_prepare_v2(raw,
        "SELECT session_exercise_row_id,max_weight_kg FROM max_results;",
        -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(sqlite3_column_int(statement, 0) == 10);
    CHECK(sqlite3_column_double(statement, 1) == 100.0);
    CHECK(sqlite3_step(statement) == SQLITE_DONE);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_prepare_v2(raw,
        "SELECT reps,weight_kg FROM performed_sets "
        "WHERE session_exercise_row_id=12;",
        -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(sqlite3_column_int(statement, 0) == 1);
    CHECK(sqlite3_column_double(statement, 1) == 0.0);
    CHECK(sqlite3_step(statement) == SQLITE_DONE);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_prepare_v2(raw,
        "SELECT position,reps,weight_kg FROM performed_sets "
        "WHERE session_exercise_row_id=11 ORDER BY position;",
        -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_double(statement, 2) == 80.0);
    CHECK(sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_double(statement, 2) == 86.0);
    CHECK(sqlite3_step(statement) == SQLITE_DONE);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    if (!test_explicit_max_round_trip_and_identity() ||
        !test_v8_migration_refuses_to_guess_multiple_attempts()) {
        return 1;
    }
    (void)printf("max results: PASS\n");
    return 0;
}
