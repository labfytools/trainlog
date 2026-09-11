/**
 * @file test_custom_equipment.c
 * @brief Schema-v8 custom equipment persistence and occurrence tests.
 */

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
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
    CHECK(version == TRAINLOG_DATABASE_SCHEMA_VERSION);
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
    trainlog_database_free_session_details(detail, count);
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

static bool insert_paged_equipment(
    TrainlogDatabase *database, const char *equipment_id, const char *display_name)
{
    TrainlogCustomEquipment equipment;
    (void)memset(&equipment, 0, sizeof(equipment));
    (void)snprintf(equipment.equipment_id, sizeof(equipment.equipment_id), "%s", equipment_id);
    (void)snprintf(equipment.display_name, sizeof(equipment.display_name), "%s", display_name);
    (void)snprintf(equipment.label_name, sizeof(equipment.label_name), "%s", display_name);
    (void)snprintf(equipment.equipment_type, sizeof(equipment.equipment_type), "%s", "machine");
    (void)snprintf(equipment.load_semantics, sizeof(equipment.load_semantics), "%s", "external");
    return trainlog_database_create_custom_equipment(database, &equipment) == TRAINLOG_STATUS_OK;
}

static bool test_custom_equipment_page_corruption(void)
{
    char path[] = "/tmp/trainlog-custom-equipment-page-XXXXXX";
    TrainlogDatabase *database = NULL;
    TrainlogCustomEquipment page[2];
    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;
    const unsigned char blob[] = { 'x', 'y' };
    const char embedded_nul[] = { 'x', '\0', 'y' };
    char oversized[TRAINLOG_NAME_MAX + 2U];
    const void *invalid_values[] = { oversized, blob, embedded_nul };
    const int invalid_lengths[] = { (int)sizeof(oversized), (int)sizeof(blob), (int)sizeof(embedded_nul) };
    size_t count;
    bool more;
    size_t index;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(insert_paged_equipment(database, "eq_page_valid", "Alpha"));
    CHECK(insert_paged_equipment(database, "eq_page_invalid", "Bravo"));
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_prepare_v2(raw, "UPDATE custom_equipment SET label_name=?1 WHERE equipment_id='eq_page_invalid';",
        -1, &statement, NULL) == SQLITE_OK);
    (void)memset(oversized, 'x', sizeof(oversized));
    /* label_name is NOT NULL, so NULL cannot reach the reader's defensive branch. */
    for (index = 0U; index < 3U; ++index) {
        if (index == 1U) {
            CHECK(sqlite3_bind_blob(statement, 1, invalid_values[index], invalid_lengths[index], SQLITE_TRANSIENT) == SQLITE_OK);
        } else {
            CHECK(sqlite3_bind_text(statement, 1, (const char *)invalid_values[index],
                invalid_lengths[index], SQLITE_TRANSIENT) == SQLITE_OK);
        }
        CHECK(sqlite3_step(statement) == SQLITE_DONE);
        CHECK(sqlite3_reset(statement) == SQLITE_OK);
        count = 9U;
        more = true;
        CHECK(trainlog_database_list_custom_equipment_page(database, 0U, page, 1U, &count, &more) == TRAINLOG_STATUS_DATABASE_ERROR);
        CHECK(count == 0U && !more);
        count = 9U;
        more = true;
        CHECK(trainlog_database_list_custom_equipment_page(database, 0U, page, 2U, &count, &more) == TRAINLOG_STATUS_DATABASE_ERROR);
        CHECK(count == 0U && !more);
        CHECK(sqlite3_bind_text(statement, 1, "Bravo", -1, SQLITE_STATIC) == SQLITE_OK);
        CHECK(sqlite3_step(statement) == SQLITE_DONE);
        CHECK(sqlite3_reset(statement) == SQLITE_OK);
        CHECK(trainlog_database_list_custom_equipment_page(database, 0U, page, 2U, &count, &more) == TRAINLOG_STATUS_OK);
        CHECK(count == 2U && !more && strcmp(page[0].equipment_id, "eq_page_valid") == 0);
    }
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

static bool test_custom_equipment_page_reader(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogCustomEquipment page[TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX];
    char equipment_id[32];
    char display_name[32];
    size_t count = 99U;
    bool more = true;
    size_t index;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(insert_paged_equipment(database, "eq_tie_b", "alpha"));
    CHECK(insert_paged_equipment(database, "eq_tie_a", "ALPHA"));
    for (index = 0U; index < 130U; ++index) {
        (void)snprintf(equipment_id, sizeof(equipment_id), "eq_page_%03zu", index);
        (void)snprintf(display_name, sizeof(display_name), "Page %03zu", index);
        CHECK(insert_paged_equipment(database, equipment_id, display_name));
    }

    CHECK(trainlog_database_list_custom_equipment_page(database, 0U, page,
        TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX, &count, &more) == TRAINLOG_STATUS_OK);
    CHECK(count == TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX);
    CHECK(more);
    CHECK(strcmp(page[0].equipment_id, "eq_tie_a") == 0);
    CHECK(strcmp(page[1].equipment_id, "eq_tie_b") == 0);
    CHECK(strcmp(page[2].equipment_id, "eq_page_000") == 0);
    CHECK(strcmp(page[127].equipment_id, "eq_page_125") == 0);

    CHECK(trainlog_database_list_custom_equipment_page(database, 128U, page,
        TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX, &count, &more) == TRAINLOG_STATUS_OK);
    CHECK(count == 4U);
    CHECK(!more);
    CHECK(strcmp(page[0].equipment_id, "eq_page_126") == 0);
    CHECK(strcmp(page[3].equipment_id, "eq_page_129") == 0);

    CHECK(trainlog_database_list_custom_equipment_page(database, 131U, page, 1U,
        &count, &more) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U);
    CHECK(!more);
    CHECK(strcmp(page[0].equipment_id, "eq_page_129") == 0);
    CHECK(trainlog_database_list_custom_equipment_page(database, 132U, page, 1U,
        &count, &more) == TRAINLOG_STATUS_OK);
    CHECK(count == 0U);
    CHECK(!more);

    count = 7U;
    more = true;
    CHECK(trainlog_database_list_custom_equipment_page(database, 0U, page, 0U,
        &count, &more) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(count == 7U);
    CHECK(more);
    CHECK(trainlog_database_list_custom_equipment_page(database, 0U, NULL, 1U,
        &count, &more) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_database_list_custom_equipment_page(database, 0U, page,
        TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX + 1U, &count, &more) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    if ((uintmax_t)SIZE_MAX > (uintmax_t)INT64_MAX) {
        count = 7U;
        more = true;
        CHECK(trainlog_database_list_custom_equipment_page(database, SIZE_MAX, page, 1U,
            &count, &more) == TRAINLOG_STATUS_INVALID_ARGUMENT);
        CHECK(count == 0U);
        CHECK(!more);
    }
    trainlog_database_close(database);
    return true;
}

int main(void)
{
    if (!test_custom_equipment_round_trip()) return 1;
    if (!test_custom_equipment_page_corruption()) return 1;
    if (!test_custom_equipment_page_reader()) return 1;
    (void)printf("PASS custom_equipment\n");
    return 0;
}
