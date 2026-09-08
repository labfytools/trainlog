/**
 * @file test_custom_equipment.c
 * @brief Schema-v8 custom equipment persistence and occurrence tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "trainlog/database.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return false; } } while (0)

static const char *const V7_SQL =
    "CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT NOT NULL UNIQUE,name TEXT NOT NULL,normalized_name TEXT NOT NULL UNIQUE,tracking_mode TEXT NOT NULL,recording_mode TEXT NOT NULL DEFAULT 'sets',data_fields INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT NOT NULL UNIQUE,started_at TEXT NOT NULL,ended_at TEXT,session_type TEXT NOT NULL DEFAULT 'training',notes TEXT);"
    "CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT NOT NULL UNIQUE,session_row_id INTEGER NOT NULL REFERENCES sessions(id),exercise_row_id INTEGER NOT NULL REFERENCES exercises(id),recording_mode TEXT NOT NULL DEFAULT 'sets',data_fields INTEGER NOT NULL DEFAULT 0,position INTEGER NOT NULL,load_mode TEXT NOT NULL,rest_seconds INTEGER NOT NULL,target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,equipment_id TEXT,notes TEXT,UNIQUE(session_row_id,position));"
    "CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id),position INTEGER NOT NULL,reps INTEGER,duration_seconds INTEGER,weight_kg REAL);"
    "CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER NOT NULL UNIQUE REFERENCES session_exercises(id),duration_seconds INTEGER NOT NULL,speed_kmh REAL,distance_km REAL);"
    "CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT NOT NULL UNIQUE,observed_at TEXT NOT NULL,session_row_id INTEGER REFERENCES sessions(id),body_weight_kg REAL,neck_cm REAL,shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL,notes TEXT);"
    "PRAGMA user_version=7;";

static bool test_custom_equipment_round_trip(void)
{
    char path[] = "/tmp/trainlog-custom-equipment-XXXXXX";
    TrainlogDatabase *database = NULL;
    TrainlogCustomEquipment custom;
    TrainlogCustomEquipment listed[2];
    TrainlogResolvedEquipment resolved;
    TrainlogResolvedEquipment used[2];
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;
    TrainlogSetInput set;
    TrainlogSessionSummary summary;
    TrainlogPersistedExerciseDetail detail[2];
    sqlite3 *raw = NULL;
    size_t count = 0U;
    int version = 0;
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, V7_SQL, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);

    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(database, &version) == TRAINLOG_STATUS_OK);
    CHECK(version == 9);
    (void)memset(&custom, 0, sizeof(custom));
    (void)snprintf(custom.equipment_id, sizeof(custom.equipment_id),
        "%s", "eq_123e4567-e89b-42d3-a456-426614174000");
    (void)snprintf(custom.display_name, sizeof(custom.display_name), "%s", "Poulie maison");
    (void)snprintf(custom.label_name, sizeof(custom.label_name), "%s", "POULIE A");
    (void)snprintf(custom.equipment_type, sizeof(custom.equipment_type), "%s", "cable_machine");
    (void)snprintf(custom.load_semantics, sizeof(custom.load_semantics), "%s", "external");
    CHECK(trainlog_database_create_custom_equipment(database, &custom) == TRAINLOG_STATUS_OK);
    trainlog_database_close(database);
    database = NULL;

    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_custom_equipment(database, listed, 2U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U);
    CHECK(strcmp(listed[0].equipment_id, custom.equipment_id) == 0);
    CHECK(trainlog_database_insert_exercise(database, "ex_custom_equipment",
        "Tirage custom", "tirage custom", TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    (void)memset(&set, 0, sizeof(set));
    set.reps = 8;
    set.has_weight = true;
    set.weight_kg = 20.0;
    (void)memset(&exercise, 0, sizeof(exercise));
    (void)snprintf(exercise.exercise_id, sizeof(exercise.exercise_id), "%s", "ex_custom_equipment");
    (void)snprintf(exercise.entry_id, sizeof(exercise.entry_id), "%s", "sxe_custom_equipment");
    (void)snprintf(exercise.equipment_id, sizeof(exercise.equipment_id), "%s", custom.equipment_id);
    exercise.load_mode = TRAINLOG_LOAD_EXTERNAL;
    exercise.rest_seconds = 60;
    exercise.target_sets = 1;
    exercise.target_reps = 8;
    exercise.target_has_weight = true;
    exercise.target_weight_kg = 20.0;
    exercise.sets = &set;
    exercise.set_count = 1U;
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", "se_custom_equipment");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", "2026-09-08T10:00:00+02:00");
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s", "2026-09-08T10:30:00+02:00");
    session.exercises = &exercise;
    session.exercise_count = 1U;
    CHECK(trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK);
    trainlog_database_close(database);
    database = NULL;
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_get_session_details(database, session.session_id,
        &summary, detail, 2U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U);
    CHECK(strcmp(detail[0].entry_id, exercise.entry_id) == 0);
    CHECK(strcmp(detail[0].equipment_id, custom.equipment_id) == 0);
    CHECK(trainlog_database_list_exercise_equipment(database, exercise.exercise_id,
        used, 2U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U);
    CHECK(used[0].origin == TRAINLOG_EQUIPMENT_CUSTOM);
    CHECK(strcmp(used[0].display_name, custom.display_name) == 0);
    CHECK(trainlog_database_resolve_equipment(database, "retired_machine", &resolved) == TRAINLOG_STATUS_OK);
    CHECK(resolved.origin == TRAINLOG_EQUIPMENT_UNKNOWN);
    CHECK(strstr(resolved.display_name, "retired_machine") != NULL);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    CHECK(test_custom_equipment_round_trip());
    (void)printf("PASS custom_equipment\n");
    return 0;
}
