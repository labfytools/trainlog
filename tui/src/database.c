/**
 * @file database.c
 * @brief SQLite persistence implementation for Trainlog.
 */

#include "trainlog/database.h"
#include "trainlog/body_zone_catalog.h"
#include "trainlog/duration.h"
#include "trainlog/equipment_catalog.h"
#include "trainlog/id.h"
#include "trainlog/session_generation.h"
#include "timestamp.h"

#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

struct TrainlogDatabase {
    sqlite3 *connection;
    unsigned int read_snapshot_depth;
};

TrainlogStatus trainlog_database_read_snapshot_begin(TrainlogDatabase *database)
{
    char sql[64];
    if (database == NULL || database->connection == NULL || database->read_snapshot_depth == UINT_MAX)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)snprintf(sql, sizeof(sql), "SAVEPOINT trainlog_read_%u;", database->read_snapshot_depth);
    if (sqlite3_exec(database->connection, sql, NULL, NULL, NULL) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    ++database->read_snapshot_depth;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_read_snapshot_end(TrainlogDatabase *database, bool commit_snapshot)
{
    char sql[160];
    unsigned int depth;
    if (database == NULL || database->connection == NULL || database->read_snapshot_depth == 0U)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    depth = database->read_snapshot_depth - 1U;
    if (commit_snapshot)
        (void)snprintf(sql, sizeof(sql), "RELEASE trainlog_read_%u;", depth);
    else
        (void)snprintf(sql, sizeof(sql), "ROLLBACK TO trainlog_read_%u; RELEASE trainlog_read_%u;", depth, depth);
    if (sqlite3_exec(database->connection, sql, NULL, NULL, NULL) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    database->read_snapshot_depth = depth;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_scan_generation_history(
    TrainlogDatabase *database, TrainlogGenerationHistoryVisitor visitor, void *context)
{
    static const char *const SQL =
        "SELECT s.session_id,s.started_at,se.entry_id,e.exercise_id,e.recording_mode,"
        "e.tracking_mode,se.equipment_id,se.load_mode,se.rest_seconds,"
        "se.target_sets,se.target_reps,se.target_duration_seconds,se.target_weight_kg,"
        "ps.position,ps.reps,ps.weight_kg,mr.max_weight_kg "
        "FROM sessions s JOIN session_exercises se ON se.session_row_id=s.id "
        "JOIN exercises e ON e.id=se.exercise_row_id "
        "LEFT JOIN performed_sets ps ON ps.session_exercise_row_id=se.id "
        "LEFT JOIN max_results mr ON mr.session_exercise_row_id=se.id "
        "ORDER BY s.session_id COLLATE BINARY,se.entry_id COLLATE BINARY,ps.position;";
    sqlite3_stmt *statement = NULL;
    TrainlogStatus status;
    int rc;
    if (database == NULL || visitor == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) return status;
    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK) {
        (void)trainlog_database_read_snapshot_end(database, false);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogGenerationHistoryRow row;
        const unsigned char *recording, *tracking, *load;
        int index;
        (void)memset(&row, 0, sizeof(row));
        /* CONTRACT: SQLite affinity cannot turn corrupt dynamic types into a
         * complete scientific result; validate every projected field before
         * the borrowed callback row is exposed. */
        for (index = 0; index <= 3; ++index)
            if (sqlite3_column_type(statement, index) != SQLITE_TEXT) {
                status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
            }
        for (index = 4; index <= 5; ++index)
            if (sqlite3_column_type(statement, index) != SQLITE_TEXT) {
                status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
            }
        if (sqlite3_column_type(statement, 6) != SQLITE_NULL &&
            sqlite3_column_type(statement, 6) != SQLITE_TEXT) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        if (sqlite3_column_type(statement, 7) != SQLITE_TEXT ||
            sqlite3_column_type(statement, 8) != SQLITE_INTEGER) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        row.session_id = (const char *)sqlite3_column_text(statement, 0);
        row.started_at = (const char *)sqlite3_column_text(statement, 1);
        row.occurrence_id = (const char *)sqlite3_column_text(statement, 2);
        row.exercise_id = (const char *)sqlite3_column_text(statement, 3);
        recording = sqlite3_column_text(statement, 4);
        tracking = sqlite3_column_text(statement, 5);
        row.equipment_id = sqlite3_column_type(statement, 6) == SQLITE_NULL ? NULL :
            (const char *)sqlite3_column_text(statement, 6);
        load = sqlite3_column_text(statement, 7);
        if (strcmp((const char *)recording, "sets") == 0) row.recording_mode = TRAINLOG_RECORDING_SETS;
        else if (strcmp((const char *)recording, "continuous") == 0) row.recording_mode = TRAINLOG_RECORDING_CONTINUOUS;
        else { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
        if (strcmp((const char *)tracking, "reps") == 0) row.tracking_mode = TRAINLOG_TRACKING_REPS;
        else if (strcmp((const char *)tracking, "duration") == 0) row.tracking_mode = TRAINLOG_TRACKING_DURATION;
        else { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
        if (strcmp((const char *)load, "none") == 0) row.load_mode = TRAINLOG_LOAD_NONE;
        else if (strcmp((const char *)load, "external") == 0) row.load_mode = TRAINLOG_LOAD_EXTERNAL;
        else if (strcmp((const char *)load, "assistance") == 0) row.load_mode = TRAINLOG_LOAD_ASSISTANCE;
        else { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
        row.rest_seconds = sqlite3_column_int(statement, 8);
        row.has_target_sets = sqlite3_column_type(statement, 9) != SQLITE_NULL;
        row.has_target_reps = sqlite3_column_type(statement, 10) != SQLITE_NULL;
        row.has_target_duration = sqlite3_column_type(statement, 11) != SQLITE_NULL;
        row.has_target_weight = sqlite3_column_type(statement, 12) != SQLITE_NULL;
        row.has_actual_set = sqlite3_column_type(statement, 13) != SQLITE_NULL;
        row.has_explicit_max = sqlite3_column_type(statement, 16) != SQLITE_NULL;
        if (row.has_explicit_max &&
            ((sqlite3_column_type(statement, 16) != SQLITE_FLOAT &&
              sqlite3_column_type(statement, 16) != SQLITE_INTEGER) ||
             !isfinite(sqlite3_column_double(statement, 16)) ||
             sqlite3_column_double(statement, 16) <= 0.0)) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        if (row.has_actual_set) {
            sqlite3_int64 position = sqlite3_column_int64(statement, 13);
            if (sqlite3_column_type(statement, 13) != SQLITE_INTEGER || position < 0 ||
                (uint64_t)position > (uint64_t)SIZE_MAX ||
                (sqlite3_column_type(statement, 14) != SQLITE_INTEGER &&
                 sqlite3_column_type(statement, 14) != SQLITE_NULL)) {
                status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
            }
            row.set_position = (size_t)position;
            row.repetitions = sqlite3_column_type(statement, 14) == SQLITE_NULL ? 0 :
                sqlite3_column_int(statement, 14);
            row.has_weight = sqlite3_column_type(statement, 15) != SQLITE_NULL;
            if (row.has_weight) {
                if (sqlite3_column_type(statement, 15) != SQLITE_FLOAT &&
                    sqlite3_column_type(statement, 15) != SQLITE_INTEGER) {
                    status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
                }
                row.weight_kg = sqlite3_column_double(statement, 15);
            }
        }
        status = visitor(context, &row);
        if (status != TRAINLOG_STATUS_OK) goto done;
    }
    status = rc == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
done:
    if (sqlite3_finalize(statement) != SQLITE_OK && status == TRAINLOG_STATUS_OK)
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    {
        TrainlogStatus end_status = trainlog_database_read_snapshot_end(database,
            status == TRAINLOG_STATUS_OK);
        if (status == TRAINLOG_STATUS_OK) status = end_status;
    }
    return status;
}

static TrainlogStatus lookup_exercise_row_id(
    TrainlogDatabase *database,
    const char *exercise_id,
    sqlite3_int64 *output_row_id
);

static bool custom_equipment_exists(TrainlogDatabase *database, const char *equipment_id)
{
    sqlite3_stmt *statement = NULL;
    int rc;
    bool found = false;
    if (database == NULL || equipment_id == NULL) return false;
    rc = sqlite3_prepare_v2(database->connection,
        "SELECT 1 FROM custom_equipment WHERE equipment_id=?1;", -1, &statement, NULL);
    if (rc != SQLITE_OK) return false;
    (void)sqlite3_bind_text(statement, 1, equipment_id, -1, SQLITE_TRANSIENT);
    found = sqlite3_step(statement) == SQLITE_ROW;
    (void)sqlite3_finalize(statement);
    return found;
}

static void set_open_diagnostic(
    char *output,
    size_t capacity,
    const char *operation,
    sqlite3 *connection,
    int sqlite_status
)
{
    const char *message;
    int extended_status = sqlite_status;

    if (output == NULL || capacity == 0U) {
        return;
    }

    if (connection != NULL) {
        message = sqlite3_errmsg(connection);
        sqlite_status = sqlite3_errcode(connection);
        extended_status = sqlite3_extended_errcode(connection);
    } else {
        message = sqlite3_errstr(sqlite_status);
    }
    /* WHY: preserve the actionable SQLite result while the owning connection
     * is still open; rollback/close may replace its diagnostic state. */
    (void)snprintf(
        output,
        capacity,
        "%s: SQLite rc=%d extended_rc=%d: %s",
        operation,
        sqlite_status,
        extended_status,
        message
    );
}

static void set_application_diagnostic(
    char *output,
    size_t capacity,
    const char *message
)
{
    if (output == NULL || capacity == 0U) {
        return;
    }
    (void)snprintf(output, capacity, "%s", message);
}

static const char *const SCHEMA_V7_SQL_A =
    "BEGIN IMMEDIATE;"

    "CREATE TABLE IF NOT EXISTS exercises ("
    "  id INTEGER PRIMARY KEY,"
    "  exercise_id TEXT NOT NULL UNIQUE,"
    "  name TEXT NOT NULL,"
    "  normalized_name TEXT NOT NULL UNIQUE,"
    "  tracking_mode TEXT NOT NULL"
    "    CHECK (tracking_mode IN ('reps', 'duration')),"
    "  recording_mode TEXT NOT NULL DEFAULT 'sets'"
    "    CHECK (recording_mode IN ('sets', 'continuous')),"
    "  data_fields INTEGER NOT NULL DEFAULT 0"
    "    CHECK (data_fields >= 0 AND (data_fields & ~3) = 0),"
    "  CHECK (recording_mode != 'continuous' OR"
    "         tracking_mode = 'duration')"
    ");"

    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id INTEGER PRIMARY KEY,"
    "  session_id TEXT NOT NULL UNIQUE,"
    "  started_at TEXT NOT NULL,"
    "  ended_at TEXT,"
    "  session_type TEXT NOT NULL DEFAULT 'training'"
    "    CHECK (session_type IN ('training', 'max_test')),"
    "  notes TEXT"
    ");"

    "CREATE TABLE IF NOT EXISTS session_exercises ("
    "  id INTEGER PRIMARY KEY,"
    "  entry_id TEXT NOT NULL UNIQUE,"
    "  session_row_id INTEGER NOT NULL"
    "    REFERENCES sessions(id) ON DELETE CASCADE,"
    "  exercise_row_id INTEGER NOT NULL"
    "    REFERENCES exercises(id) ON DELETE RESTRICT,"
    "  recording_mode TEXT NOT NULL DEFAULT 'sets'"
    "    CHECK (recording_mode IN ('sets', 'continuous')),"
    "  data_fields INTEGER NOT NULL DEFAULT 0"
    "    CHECK (data_fields >= 0 AND (data_fields & ~3) = 0),"
    "  position INTEGER NOT NULL CHECK (position >= 0),"
    "  load_mode TEXT NOT NULL"
    "    CHECK (load_mode IN ('none', 'external', 'assistance')),"
    "  rest_seconds INTEGER NOT NULL CHECK (rest_seconds >= 0),"
    "  target_sets INTEGER CHECK (target_sets > 0),"
    "  target_reps INTEGER CHECK (target_reps >= 1),"
    "  target_duration_seconds INTEGER"
    "    CHECK (target_duration_seconds > 0),"
    "  target_weight_kg REAL CHECK (target_weight_kg > 0.0),"
    "  equipment_id TEXT,"
    "  notes TEXT,"
    "  UNIQUE (session_row_id, position),"
    "  CHECK ("
    "    (recording_mode = 'sets' AND"
    "     ("
    "       (target_sets IS NULL AND"
    "        target_reps IS NULL AND"
    "        target_duration_seconds IS NULL) OR"
    "       (target_sets IS NOT NULL AND"
    "        ((target_reps IS NOT NULL AND"
    "          target_duration_seconds IS NULL) OR"
    "         (target_reps IS NULL AND"
    "          target_duration_seconds IS NOT NULL)))"
    "     )) OR"
    "    (recording_mode = 'continuous' AND"
    "     target_sets IS NULL AND"
    "     target_reps IS NULL AND"
    "     target_duration_seconds IS NULL AND"
    "     load_mode = 'none' AND"
    "     rest_seconds = 0 AND"
    "     target_weight_kg IS NULL)"
    "  ),"
    "  CHECK ("
    "    (load_mode = 'none' AND target_weight_kg IS NULL) OR"
    "    (load_mode IN ('external', 'assistance') AND"
    "     target_weight_kg IS NOT NULL)"
    "  )"
    ");"

    "CREATE TABLE IF NOT EXISTS performed_sets ("
    "  id INTEGER PRIMARY KEY,"
    "  session_exercise_row_id INTEGER NOT NULL"
    "    REFERENCES session_exercises(id) ON DELETE CASCADE,"
    "  position INTEGER NOT NULL CHECK (position >= 0),"
    "  reps INTEGER CHECK (reps >= 0),"
    "  duration_seconds INTEGER CHECK (duration_seconds > 0),"
    "  weight_kg REAL CHECK (weight_kg > 0.0),"
    "  UNIQUE (session_exercise_row_id, position),"
    "  CHECK ("
    "    (reps IS NOT NULL AND duration_seconds IS NULL) OR"
    "    (reps IS NULL AND duration_seconds IS NOT NULL)"
    "  )"
    ");";

static const char *const SCHEMA_V7_SQL_B =
    "CREATE TABLE IF NOT EXISTS continuous_activity ("
    "  id INTEGER PRIMARY KEY,"
    "  session_exercise_row_id INTEGER NOT NULL UNIQUE"
    "    REFERENCES session_exercises(id) ON DELETE CASCADE,"
    "  duration_seconds INTEGER NOT NULL CHECK (duration_seconds > 0),"
    "  speed_kmh REAL CHECK (speed_kmh > 0.0),"
    "  distance_km REAL CHECK (distance_km > 0.0)"
    ");"

    "CREATE TABLE IF NOT EXISTS body_observations ("
    "  id INTEGER PRIMARY KEY,"
    "  observation_id TEXT NOT NULL UNIQUE,"
    "  observed_at TEXT NOT NULL,"
    "  session_row_id INTEGER UNIQUE"
    "    REFERENCES sessions(id) ON DELETE CASCADE,"
    "  body_weight_kg REAL CHECK (body_weight_kg > 0.0),"
    "  neck_cm REAL CHECK (neck_cm > 0.0),"
    "  shoulders_cm REAL CHECK (shoulders_cm > 0.0),"
    "  chest_cm REAL CHECK (chest_cm > 0.0),"
    "  waist_cm REAL CHECK (waist_cm > 0.0),"
    "  hips_cm REAL CHECK (hips_cm > 0.0),"
    "  left_arm_cm REAL CHECK (left_arm_cm > 0.0),"
    "  right_arm_cm REAL CHECK (right_arm_cm > 0.0),"
    "  left_forearm_cm REAL CHECK (left_forearm_cm > 0.0),"
    "  right_forearm_cm REAL CHECK (right_forearm_cm > 0.0),"
    "  left_thigh_cm REAL CHECK (left_thigh_cm > 0.0),"
    "  right_thigh_cm REAL CHECK (right_thigh_cm > 0.0),"
    "  left_calf_cm REAL CHECK (left_calf_cm > 0.0),"
    "  right_calf_cm REAL CHECK (right_calf_cm > 0.0),"
    "  notes TEXT,"
    "  CHECK ("
    "    body_weight_kg IS NOT NULL OR"
    "    neck_cm IS NOT NULL OR shoulders_cm IS NOT NULL OR"
    "    chest_cm IS NOT NULL OR waist_cm IS NOT NULL OR"
    "    hips_cm IS NOT NULL OR left_arm_cm IS NOT NULL OR"
    "    right_arm_cm IS NOT NULL OR left_forearm_cm IS NOT NULL OR"
    "    right_forearm_cm IS NOT NULL OR left_thigh_cm IS NOT NULL OR"
    "    right_thigh_cm IS NOT NULL OR left_calf_cm IS NOT NULL OR"
    "    right_calf_cm IS NOT NULL"
    "  )"
    ");"

    "PRAGMA user_version = 7;"
    "COMMIT;";

static const char *const MIGRATE_V5_TO_V6_SQL =
    "BEGIN IMMEDIATE;"
    "ALTER TABLE session_exercises ADD COLUMN equipment_id TEXT;"
    "PRAGMA user_version = 6;"
    "COMMIT;";

/* WHY: a catalogue exercise can occur twice in one completed session. v7
 * introduces a persistent occurrence ID and removes the invalid uniqueness
 * constraint while retaining primary keys and all dependent measurements. */
static const char *const MIGRATE_V6_TO_V7_SQL =
    "PRAGMA foreign_keys = OFF;"
    "BEGIN IMMEDIATE;"
    "ALTER TABLE session_exercises RENAME TO session_exercises_v6;"
    "CREATE TABLE session_exercises ("
    "id INTEGER PRIMARY KEY, entry_id TEXT NOT NULL UNIQUE,"
    "session_row_id INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,"
    "exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE RESTRICT,"
    "recording_mode TEXT NOT NULL, data_fields INTEGER NOT NULL,"
    "position INTEGER NOT NULL, load_mode TEXT NOT NULL, rest_seconds INTEGER NOT NULL,"
    "target_sets INTEGER, target_reps INTEGER, target_duration_seconds INTEGER,"
    "target_weight_kg REAL, equipment_id TEXT, notes TEXT, UNIQUE(session_row_id,position));"
    "INSERT INTO session_exercises(id,entry_id,session_row_id,exercise_row_id,recording_mode,data_fields,position,load_mode,rest_seconds,target_sets,target_reps,target_duration_seconds,target_weight_kg,equipment_id,notes) "
    "SELECT id,'sxe_legacy_' || id,session_row_id,exercise_row_id,recording_mode,data_fields,position,load_mode,rest_seconds,target_sets,target_reps,target_duration_seconds,target_weight_kg,equipment_id,notes FROM session_exercises_v6;"
    "ALTER TABLE performed_sets RENAME TO performed_sets_v6;"
    "CREATE TABLE performed_sets (id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id) ON DELETE CASCADE,position INTEGER NOT NULL,reps INTEGER,duration_seconds INTEGER,weight_kg REAL,UNIQUE(session_exercise_row_id,position));"
    "INSERT INTO performed_sets SELECT * FROM performed_sets_v6;"
    "ALTER TABLE continuous_activity RENAME TO continuous_activity_v6;"
    "CREATE TABLE continuous_activity (id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER NOT NULL UNIQUE REFERENCES session_exercises(id) ON DELETE CASCADE,duration_seconds INTEGER NOT NULL,speed_kmh REAL,distance_km REAL);"
    "INSERT INTO continuous_activity SELECT * FROM continuous_activity_v6;"
    "DROP TABLE performed_sets_v6;DROP TABLE continuous_activity_v6;DROP TABLE session_exercises_v6;"
    "PRAGMA user_version = 7;COMMIT;PRAGMA foreign_keys = ON;";

/* CONTRACT: v8 stores only user-created equipment definitions. Canonical
 * equipment remains generated from the frozen manifest; historic IDs are not
 * backfilled because an occurrence reference has no trustworthy metadata. */
static const char *const MIGRATE_V7_TO_V8_SQL =
    "BEGIN IMMEDIATE;"
    "CREATE TABLE custom_equipment ("
    "equipment_id TEXT PRIMARY KEY,display_name TEXT NOT NULL,"
    "label_name TEXT NOT NULL,equipment_type TEXT NOT NULL,"
    "load_semantics TEXT NOT NULL CHECK(load_semantics IN ('none','external','assistance'))"
    ");PRAGMA user_version = 8;COMMIT;";

/*
 * WHY: a measured maximum is not a performed set with invented cardinality.
 * CONTRACT: conversion is limited to the only legacy shape whose successful
 * result is unambiguous: one max_test set, one rep, positive weight. Multiple
 * attempts and every other shape remain in performed_sets without data loss.
 */
static const char *const MIGRATE_V8_TO_V9_SQL =
    "BEGIN IMMEDIATE;"
    "CREATE TABLE max_results ("
    "session_exercise_row_id INTEGER PRIMARY KEY "
    "REFERENCES session_exercises(id) ON DELETE CASCADE,"
    "max_weight_kg REAL NOT NULL CHECK(max_weight_kg > 0.0)"
    ");"
    "INSERT INTO max_results(session_exercise_row_id,max_weight_kg) "
    "SELECT se.id,ps.weight_kg FROM session_exercises se "
    "JOIN sessions s ON s.id=se.session_row_id "
    "JOIN performed_sets ps ON ps.session_exercise_row_id=se.id "
    "WHERE s.session_type='max_test' AND se.recording_mode='sets' "
    "AND ps.reps=1 AND ps.duration_seconds IS NULL "
    "AND ps.weight_kg>0.0 AND "
    "(SELECT COUNT(*) FROM performed_sets all_ps "
    " WHERE all_ps.session_exercise_row_id=se.id)=1;"
    "DELETE FROM performed_sets WHERE session_exercise_row_id IN "
    "(SELECT session_exercise_row_id FROM max_results);"
    "PRAGMA user_version = 9;COMMIT;";

/*
 * WHY: actual load absence and an explicit zero are distinct observations.
 * CONTRACT: v10 changes only performed_sets.weight_kg from strictly positive
 * to nonnegative. The explicit projection preserves every row ID, owning
 * occurrence, position, metric value and NULL/positive load byte-for-byte.
 */
static const char *const MIGRATE_V9_TO_V10_SQL =
    "PRAGMA foreign_keys = OFF;"
    "BEGIN IMMEDIATE;"
    "ALTER TABLE performed_sets RENAME TO performed_sets_v9;"
    "CREATE TABLE performed_sets ("
    "id INTEGER PRIMARY KEY,"
    "session_exercise_row_id INTEGER NOT NULL "
    "REFERENCES session_exercises(id) ON DELETE CASCADE,"
    "position INTEGER NOT NULL CHECK(position >= 0),"
    "reps INTEGER CHECK(reps >= 0),"
    "duration_seconds INTEGER CHECK(duration_seconds > 0),"
    "weight_kg REAL CHECK(weight_kg >= 0.0),"
    "UNIQUE(session_exercise_row_id,position),"
    "CHECK((reps IS NOT NULL AND duration_seconds IS NULL) OR "
    "(reps IS NULL AND duration_seconds IS NOT NULL))"
    ");"
    "INSERT INTO performed_sets("
    "id,session_exercise_row_id,position,reps,duration_seconds,weight_kg"
    ") SELECT id,session_exercise_row_id,position,reps,duration_seconds,weight_kg "
    "FROM performed_sets_v9;"
    "DROP TABLE performed_sets_v9;"
    "PRAGMA user_version = 10;"
    "COMMIT;"
    "PRAGMA foreign_keys = ON;";

/*
 * WHY: one text column cannot preserve a primary plus multiple secondary
 * zones, and parent groups are derivable catalogue metadata.
 * CONTRACT: v11 is additive and touches no exercise/session/history identity.
 * INVARIANT: the primary key forbids duplicate roles for one zone and the
 * partial unique index permits at most one primary relation per exercise.
 */
static const char *const CREATE_BODY_ZONE_RELATIONS_SQL =
    "CREATE TABLE exercise_body_zones("
    "exercise_row_id INTEGER NOT NULL "
    "REFERENCES exercises(id) ON DELETE CASCADE,"
    "zone_id TEXT NOT NULL,"
    "role TEXT NOT NULL CHECK(role IN('primary','secondary')),"
    "PRIMARY KEY(exercise_row_id,zone_id)"
    ");"
    "CREATE UNIQUE INDEX exercise_body_zones_one_primary "
    "ON exercise_body_zones(exercise_row_id) WHERE role='primary';"
    "CREATE TABLE exercise_body_zone_sync("
    "exercise_row_id INTEGER PRIMARY KEY "
    "REFERENCES exercises(id) ON DELETE CASCADE,"
    "synced_state TEXT NOT NULL"
    ");";

/* WHY: deleted duplicate catalogue rows still arrive from offline peers.
 * CONTRACT: aliases are a separate identity artifact, never an overload of a
 * frozen mobile-export field. INVARIANT: canonical IDs always name a live
 * exercise row and mappings are stored collapsed, so cycles/chains cannot be
 * represented by valid database writes. */
static const char *const MIGRATE_V11_TO_V12_SQL =
    "BEGIN IMMEDIATE;"
    "CREATE TABLE exercise_aliases("
    "source_exercise_id TEXT PRIMARY KEY,"
    "canonical_exercise_id TEXT NOT NULL "
    "REFERENCES exercises(exercise_id) ON DELETE RESTRICT,"
    "CHECK(source_exercise_id<>canonical_exercise_id)"
    ");"
    "CREATE INDEX exercise_aliases_canonical "
    "ON exercise_aliases(canonical_exercise_id);"
    "PRAGMA user_version = 12;COMMIT;";

static const char *const MIGRATE_V1_TO_V3_SQL =
    "BEGIN IMMEDIATE;"
    "ALTER TABLE sessions "
    "ADD COLUMN session_type TEXT NOT NULL DEFAULT 'training' "
    "CHECK (session_type IN ('training', 'max_test'));"
    "ALTER TABLE exercises "
    "ADD COLUMN recording_mode TEXT NOT NULL DEFAULT 'sets' "
    "CHECK (recording_mode IN ('sets', 'continuous'));"
    "ALTER TABLE exercises "
    "ADD COLUMN data_fields INTEGER NOT NULL DEFAULT 0 "
    "CHECK (data_fields >= 0 AND (data_fields & ~3) = 0);"
    "ALTER TABLE session_exercises "
    "ADD COLUMN recording_mode TEXT NOT NULL DEFAULT 'sets' "
    "CHECK (recording_mode IN ('sets', 'continuous'));"
    "ALTER TABLE session_exercises "
    "ADD COLUMN data_fields INTEGER NOT NULL DEFAULT 0 "
    "CHECK (data_fields >= 0 AND (data_fields & ~3) = 0);"
    "CREATE TABLE continuous_activity ("
    "id INTEGER PRIMARY KEY,"
    "session_exercise_row_id INTEGER NOT NULL UNIQUE "
    "REFERENCES session_exercises(id) ON DELETE CASCADE,"
    "duration_seconds INTEGER NOT NULL CHECK (duration_seconds > 0),"
    "speed_kmh REAL CHECK (speed_kmh > 0.0),"
    "distance_km REAL CHECK (distance_km > 0.0)"
    ");"
    "PRAGMA user_version = 3;"
    "COMMIT;";

static const char *const MIGRATE_V2_TO_V3_SQL =
    "BEGIN IMMEDIATE;"
    "ALTER TABLE exercises "
    "ADD COLUMN recording_mode TEXT NOT NULL DEFAULT 'sets' "
    "CHECK (recording_mode IN ('sets', 'continuous'));"
    "ALTER TABLE exercises "
    "ADD COLUMN data_fields INTEGER NOT NULL DEFAULT 0 "
    "CHECK (data_fields >= 0 AND (data_fields & ~3) = 0);"
    "ALTER TABLE session_exercises "
    "ADD COLUMN recording_mode TEXT NOT NULL DEFAULT 'sets' "
    "CHECK (recording_mode IN ('sets', 'continuous'));"
    "ALTER TABLE session_exercises "
    "ADD COLUMN data_fields INTEGER NOT NULL DEFAULT 0 "
    "CHECK (data_fields >= 0 AND (data_fields & ~3) = 0);"
    "CREATE TABLE continuous_activity ("
    "id INTEGER PRIMARY KEY,"
    "session_exercise_row_id INTEGER NOT NULL UNIQUE "
    "REFERENCES session_exercises(id) ON DELETE CASCADE,"
    "duration_seconds INTEGER NOT NULL CHECK (duration_seconds > 0),"
    "speed_kmh REAL CHECK (speed_kmh > 0.0),"
    "distance_km REAL CHECK (distance_km > 0.0)"
    ");"
    "PRAGMA user_version = 3;"
    "COMMIT;";

static const char *const MIGRATE_V3_TO_V4_SQL_A =
    "BEGIN IMMEDIATE;"

    "CREATE TABLE session_exercises_v4 ("
    "  id INTEGER PRIMARY KEY,"
    "  session_row_id INTEGER NOT NULL"
    "    REFERENCES sessions(id) ON DELETE CASCADE,"
    "  exercise_row_id INTEGER NOT NULL"
    "    REFERENCES exercises(id) ON DELETE RESTRICT,"
    "  recording_mode TEXT NOT NULL"
    "    CHECK (recording_mode IN ('sets', 'continuous')),"
    "  data_fields INTEGER NOT NULL DEFAULT 0"
    "    CHECK (data_fields >= 0 AND (data_fields & ~3) = 0),"
    "  position INTEGER NOT NULL CHECK (position >= 0),"
    "  load_mode TEXT NOT NULL"
    "    CHECK (load_mode IN ('none', 'external', 'assistance')),"
    "  rest_seconds INTEGER NOT NULL CHECK (rest_seconds >= 0),"
    "  target_sets INTEGER CHECK (target_sets > 0),"
    "  target_reps INTEGER CHECK (target_reps >= 1),"
    "  target_duration_seconds INTEGER"
    "    CHECK (target_duration_seconds > 0),"
    "  target_weight_kg REAL CHECK (target_weight_kg > 0.0),"
    "  notes TEXT,"
    "  UNIQUE (session_row_id, position),"
    "  UNIQUE (session_row_id, exercise_row_id),"
    "  CHECK ("
    "    (recording_mode = 'sets' AND"
    "     target_sets IS NOT NULL AND"
    "     ((target_reps IS NOT NULL AND"
    "       target_duration_seconds IS NULL) OR"
    "      (target_reps IS NULL AND"
    "       target_duration_seconds IS NOT NULL))) OR"
    "    (recording_mode = 'continuous' AND"
    "     target_sets IS NULL AND"
    "     target_reps IS NULL AND"
    "     target_duration_seconds IS NULL AND"
    "     load_mode = 'none' AND"
    "     rest_seconds = 0 AND"
    "     target_weight_kg IS NULL)"
    "  ),"
    "  CHECK ("
    "    (load_mode = 'none' AND target_weight_kg IS NULL) OR"
    "    (load_mode IN ('external', 'assistance') AND"
    "     target_weight_kg IS NOT NULL)"
    "  )"
    ");"

    "INSERT INTO session_exercises_v4("
    "id, session_row_id, exercise_row_id, recording_mode, data_fields,"
    "position, load_mode, rest_seconds, target_sets, target_reps,"
    "target_duration_seconds, target_weight_kg, notes"
    ") SELECT "
    "id, session_row_id, exercise_row_id, recording_mode, data_fields,"
    "position, load_mode, rest_seconds, target_sets, target_reps,"
    "target_duration_seconds, target_weight_kg, notes "
    "FROM session_exercises;"

    "CREATE TABLE performed_sets_v4 ("
    "  id INTEGER PRIMARY KEY,"
    "  session_exercise_row_id INTEGER NOT NULL"
    "    REFERENCES session_exercises_v4(id) ON DELETE CASCADE,"
    "  position INTEGER NOT NULL CHECK (position >= 0),"
    "  reps INTEGER CHECK (reps >= 0),"
    "  duration_seconds INTEGER CHECK (duration_seconds > 0),"
    "  weight_kg REAL CHECK (weight_kg > 0.0),"
    "  UNIQUE (session_exercise_row_id, position),"
    "  CHECK ("
    "    (reps IS NOT NULL AND duration_seconds IS NULL) OR"
    "    (reps IS NULL AND duration_seconds IS NOT NULL)"
    "  )"
    ");"

    "INSERT INTO performed_sets_v4("
    "id, session_exercise_row_id, position, reps,"
    "duration_seconds, weight_kg"
    ") SELECT "
    "id, session_exercise_row_id, position, reps,"
    "duration_seconds, weight_kg "
    "FROM performed_sets;";

static const char *const MIGRATE_V3_TO_V4_SQL_B =
    "CREATE TABLE continuous_activity_v4 ("
    "  id INTEGER PRIMARY KEY,"
    "  session_exercise_row_id INTEGER NOT NULL UNIQUE"
    "    REFERENCES session_exercises_v4(id) ON DELETE CASCADE,"
    "  duration_seconds INTEGER NOT NULL CHECK (duration_seconds > 0),"
    "  speed_kmh REAL CHECK (speed_kmh > 0.0),"
    "  distance_km REAL CHECK (distance_km > 0.0)"
    ");"

    "INSERT INTO continuous_activity_v4("
    "id, session_exercise_row_id, duration_seconds,"
    "speed_kmh, distance_km"
    ") SELECT "
    "id, session_exercise_row_id, duration_seconds,"
    "speed_kmh, distance_km "
    "FROM continuous_activity;"

    "DROP TABLE continuous_activity;"
    "DROP TABLE performed_sets;"
    "DROP TABLE session_exercises;"

    "ALTER TABLE session_exercises_v4"
    " RENAME TO session_exercises;"
    "ALTER TABLE performed_sets_v4"
    " RENAME TO performed_sets;"
    "ALTER TABLE continuous_activity_v4"
    " RENAME TO continuous_activity;"

    "PRAGMA user_version = 4;"
    "COMMIT;";

static const char *const MIGRATE_V4_TO_V5_SQL_A =
    "BEGIN IMMEDIATE;"

    "CREATE TABLE session_exercises_v5 ("
    "  id INTEGER PRIMARY KEY,"
    "  session_row_id INTEGER NOT NULL"
    "    REFERENCES sessions(id) ON DELETE CASCADE,"
    "  exercise_row_id INTEGER NOT NULL"
    "    REFERENCES exercises(id) ON DELETE RESTRICT,"
    "  recording_mode TEXT NOT NULL DEFAULT 'sets'"
    "    CHECK (recording_mode IN ('sets', 'continuous')),"
    "  data_fields INTEGER NOT NULL DEFAULT 0"
    "    CHECK (data_fields >= 0 AND (data_fields & ~3) = 0),"
    "  position INTEGER NOT NULL CHECK (position >= 0),"
    "  load_mode TEXT NOT NULL"
    "    CHECK (load_mode IN ('none', 'external', 'assistance')),"
    "  rest_seconds INTEGER NOT NULL CHECK (rest_seconds >= 0),"
    "  target_sets INTEGER CHECK (target_sets > 0),"
    "  target_reps INTEGER CHECK (target_reps >= 1),"
    "  target_duration_seconds INTEGER"
    "    CHECK (target_duration_seconds > 0),"
    "  target_weight_kg REAL CHECK (target_weight_kg > 0.0),"
    "  notes TEXT,"
    "  UNIQUE (session_row_id, position),"
    "  UNIQUE (session_row_id, exercise_row_id),"
    "  CHECK ("
    "    (recording_mode = 'sets' AND"
    "     ("
    "       (target_sets IS NULL AND"
    "        target_reps IS NULL AND"
    "        target_duration_seconds IS NULL) OR"
    "       (target_sets IS NOT NULL AND"
    "        ((target_reps IS NOT NULL AND"
    "          target_duration_seconds IS NULL) OR"
    "         (target_reps IS NULL AND"
    "          target_duration_seconds IS NOT NULL)))"
    "     )) OR"
    "    (recording_mode = 'continuous' AND"
    "     target_sets IS NULL AND"
    "     target_reps IS NULL AND"
    "     target_duration_seconds IS NULL AND"
    "     load_mode = 'none' AND"
    "     rest_seconds = 0 AND"
    "     target_weight_kg IS NULL)"
    "  ),"
    "  CHECK ("
    "    (load_mode = 'none' AND target_weight_kg IS NULL) OR"
    "    (load_mode IN ('external', 'assistance') AND"
    "     target_weight_kg IS NOT NULL)"
    "  )"
    ");"

    "INSERT INTO session_exercises_v5("
    "id, session_row_id, exercise_row_id, recording_mode, data_fields,"
    "position, load_mode, rest_seconds, target_sets, target_reps,"
    "target_duration_seconds, target_weight_kg, notes"
    ") SELECT "
    "id, session_row_id, exercise_row_id, recording_mode, data_fields,"
    "position, load_mode, rest_seconds, target_sets, target_reps,"
    "target_duration_seconds, target_weight_kg, notes "
    "FROM session_exercises;"

    "CREATE TABLE performed_sets_v5 ("
    "  id INTEGER PRIMARY KEY,"
    "  session_exercise_row_id INTEGER NOT NULL"
    "    REFERENCES session_exercises_v5(id) ON DELETE CASCADE,"
    "  position INTEGER NOT NULL CHECK (position >= 0),"
    "  reps INTEGER CHECK (reps >= 0),"
    "  duration_seconds INTEGER CHECK (duration_seconds > 0),"
    "  weight_kg REAL CHECK (weight_kg > 0.0),"
    "  UNIQUE (session_exercise_row_id, position),"
    "  CHECK ("
    "    (reps IS NOT NULL AND duration_seconds IS NULL) OR"
    "    (reps IS NULL AND duration_seconds IS NOT NULL)"
    "  )"
    ");"

    "INSERT INTO performed_sets_v5("
    "id, session_exercise_row_id, position, reps,"
    "duration_seconds, weight_kg"
    ") SELECT "
    "id, session_exercise_row_id, position, reps,"
    "duration_seconds, weight_kg "
    "FROM performed_sets;";

static const char *const MIGRATE_V4_TO_V5_SQL_B =
    "CREATE TABLE continuous_activity_v5 ("
    "  id INTEGER PRIMARY KEY,"
    "  session_exercise_row_id INTEGER NOT NULL UNIQUE"
    "    REFERENCES session_exercises_v5(id) ON DELETE CASCADE,"
    "  duration_seconds INTEGER NOT NULL CHECK (duration_seconds > 0),"
    "  speed_kmh REAL CHECK (speed_kmh > 0.0),"
    "  distance_km REAL CHECK (distance_km > 0.0)"
    ");"

    "INSERT INTO continuous_activity_v5("
    "id, session_exercise_row_id, duration_seconds,"
    "speed_kmh, distance_km"
    ") SELECT "
    "id, session_exercise_row_id, duration_seconds,"
    "speed_kmh, distance_km "
    "FROM continuous_activity;"

    "DROP TABLE continuous_activity;"
    "DROP TABLE performed_sets;"
    "DROP TABLE session_exercises;"

    "ALTER TABLE session_exercises_v5"
    " RENAME TO session_exercises;"
    "ALTER TABLE performed_sets_v5"
    " RENAME TO performed_sets;"
    "ALTER TABLE continuous_activity_v5"
    " RENAME TO continuous_activity;"

    "PRAGMA user_version = 5;"
    "COMMIT;";

static TrainlogStatus execute_sql(
    TrainlogDatabase *database,
    const char *sql
)
{
    if (database == NULL || database->connection == NULL || sql == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    if (sqlite3_exec(database->connection, sql, NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    return TRAINLOG_STATUS_OK;
}

static bool bind_initial_body_zone(
    sqlite3_stmt *statement,
    const char *exercise_id,
    const char *zone_id,
    const char *role
)
{
    int rc;
    if (sqlite3_reset(statement) != SQLITE_OK ||
        sqlite3_clear_bindings(statement) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, zone_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, role, -1, SQLITE_STATIC) != SQLITE_OK) {
        return false;
    }
    rc = sqlite3_step(statement);
    return rc == SQLITE_DONE;
}

static TrainlogStatus migrate_v10_to_v11(TrainlogDatabase *database)
{
    static const char *const INSERT_SQL =
        "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) "
        "SELECT id,?2,?3 FROM exercises WHERE exercise_id=?1;";
    sqlite3_stmt *statement = NULL;
    size_t index;
    TrainlogStatus status = TRAINLOG_STATUS_DATABASE_ERROR;

    if (execute_sql(database, "BEGIN IMMEDIATE;") != TRAINLOG_STATUS_OK ||
        execute_sql(database, CREATE_BODY_ZONE_RELATIONS_SQL) != TRAINLOG_STATUS_OK ||
        sqlite3_prepare_v2(database->connection, INSERT_SQL, -1, &statement, NULL) != SQLITE_OK) {
        goto rollback;
    }

    for (index = 0U; index < trainlog_body_zone_initial_mapping_count(); ++index) {
        const TrainlogBodyZoneInitialMapping *mapping =
            trainlog_body_zone_initial_mapping_at(index);
        const char *cursor;
        if (mapping == NULL ||
            !bind_initial_body_zone(statement, mapping->exercise_id,
                mapping->primary_zone_id, "primary")) {
            goto rollback;
        }
        cursor = mapping->secondary_zone_ids;
        while (cursor != NULL && cursor[0] != '\0') {
            const char *end = strchr(cursor, '\n');
            size_t length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
            char zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
            if (length == 0U || length > TRAINLOG_ZONE_ID_MAX) goto rollback;
            (void)memcpy(zone_id, cursor, length);
            zone_id[length] = '\0';
            if (!bind_initial_body_zone(statement, mapping->exercise_id,
                    zone_id, "secondary")) {
                goto rollback;
            }
            cursor = end == NULL ? NULL : end + 1;
        }
    }

    /* CONTRACT: synced_state is an internal comparison baseline, not domain
     * data. `primary|secondary,...` is deterministic because zone IDs exclude
     * delimiters and secondary IDs are sorted bytewise. */
    if (execute_sql(database,
            "INSERT INTO exercise_body_zone_sync(exercise_row_id,synced_state) "
            "SELECT e.id,COALESCE((SELECT p.zone_id FROM exercise_body_zones p "
            "WHERE p.exercise_row_id=e.id AND p.role='primary'),'')||'|'||"
            "COALESCE((SELECT group_concat(s.zone_id,',') FROM "
            "(SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=e.id "
            "AND role='secondary' ORDER BY zone_id) s),'') FROM exercises e;"
        ) != TRAINLOG_STATUS_OK) goto rollback;

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL;
        goto rollback;
    }
    statement = NULL;
    if (execute_sql(database, "PRAGMA user_version = 11;COMMIT;") != TRAINLOG_STATUS_OK) {
        goto rollback;
    }
    return TRAINLOG_STATUS_OK;

rollback:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    (void)sqlite3_exec(database->connection, "ROLLBACK;", NULL, NULL, NULL);
    return status;
}

static TrainlogStatus read_single_int_pragma(
    TrainlogDatabase *database,
    const char *sql,
    int *output
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        sql == NULL ||
        output == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output = sqlite3_column_int(statement, 0);

    rc = sqlite3_finalize(statement);
    return rc == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus initialize_or_validate_schema(
    TrainlogDatabase *database,
    char *output_diagnostic,
    size_t output_diagnostic_capacity
)
{
    int version = 0;
    TrainlogStatus status;

    status =
        trainlog_database_schema_version(
            database,
            &version
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        set_open_diagnostic(
            output_diagnostic,
            output_diagnostic_capacity,
            "read schema version",
            database->connection,
            SQLITE_ERROR
        );
        return status;
    }

    if (
        version >
        TRAINLOG_DATABASE_SCHEMA_VERSION
    ) {
        char message[128];
        (void)snprintf(
            message, sizeof(message),
            "schema version %d is newer than supported version %d",
            version, TRAINLOG_DATABASE_SCHEMA_VERSION);
        set_application_diagnostic(
            output_diagnostic, output_diagnostic_capacity, message);
        return
            TRAINLOG_STATUS_SCHEMA_UNSUPPORTED;
    }

    if (
        version ==
        TRAINLOG_DATABASE_SCHEMA_VERSION
    ) {
        return TRAINLOG_STATUS_OK;
    }

    if (version == 0) {
        status =
            execute_sql(
                database,
                SCHEMA_V7_SQL_A
            );

        if (
            status ==
            TRAINLOG_STATUS_OK
        ) {
            status =
                execute_sql(
                    database,
                    SCHEMA_V7_SQL_B
                );
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V7_TO_V8_SQL);
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V8_TO_V9_SQL);
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V9_TO_V10_SQL);
        }
    } else if (version == 7) {
        /* CONTRACT: v7 is the immediate historic schema and must open through
         * its lossless custom-equipment-table migration. */
        status = execute_sql(database, MIGRATE_V7_TO_V8_SQL);
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V8_TO_V9_SQL);
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V9_TO_V10_SQL);
        }
    } else if (version == 8) {
        status = execute_sql(database, MIGRATE_V8_TO_V9_SQL);
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V9_TO_V10_SQL);
        }
    } else if (version == 9) {
        status = execute_sql(database, MIGRATE_V9_TO_V10_SQL);
    } else if (version == 10) {
        status = TRAINLOG_STATUS_OK;
    } else if (version == 11) {
        status = TRAINLOG_STATUS_OK;
    } else {
        if (version == 1) {
            status =
                execute_sql(
                    database,
                    MIGRATE_V1_TO_V3_SQL
                );

            if (
                status ==
                TRAINLOG_STATUS_OK
            ) {
                status =
                    execute_sql(
                        database,
                        MIGRATE_V3_TO_V4_SQL_A
                    );
            }

            if (
                status ==
                TRAINLOG_STATUS_OK
            ) {
                status =
                    execute_sql(
                        database,
                        MIGRATE_V3_TO_V4_SQL_B
                    );
            }
        } else if (version == 2) {
            status =
                execute_sql(
                    database,
                    MIGRATE_V2_TO_V3_SQL
                );

            if (
                status ==
                TRAINLOG_STATUS_OK
            ) {
                status =
                    execute_sql(
                        database,
                        MIGRATE_V3_TO_V4_SQL_A
                    );
            }

            if (
                status ==
                TRAINLOG_STATUS_OK
            ) {
                status =
                    execute_sql(
                        database,
                        MIGRATE_V3_TO_V4_SQL_B
                    );
            }
        } else if (version == 3) {
            status =
                execute_sql(
                    database,
                    MIGRATE_V3_TO_V4_SQL_A
                );

            if (
                status ==
                TRAINLOG_STATUS_OK
            ) {
                status =
                    execute_sql(
                        database,
                        MIGRATE_V3_TO_V4_SQL_B
                    );
            }
        } else if (version == 4) {
            status =
                TRAINLOG_STATUS_OK;
        } else if (version == 5) {
            status = TRAINLOG_STATUS_OK;
        } else if (version == 6) {
            status = TRAINLOG_STATUS_OK;
        } else {
            char message[128];
            (void)snprintf(
                message, sizeof(message),
                "schema version %d is unsupported (supported through version %d)",
                version, TRAINLOG_DATABASE_SCHEMA_VERSION);
            set_application_diagnostic(
                output_diagnostic, output_diagnostic_capacity, message);
            return
                TRAINLOG_STATUS_SCHEMA_UNSUPPORTED;
        }

        if (
            status ==
            TRAINLOG_STATUS_OK
        ) {
            status =
                execute_sql(
                    database,
                    MIGRATE_V4_TO_V5_SQL_A
                );
        }

        if (
            status ==
            TRAINLOG_STATUS_OK
        ) {
            status =
                execute_sql(
                    database,
                    MIGRATE_V4_TO_V5_SQL_B
                );
        }

        if (status == TRAINLOG_STATUS_OK && version < 6) {
            status = execute_sql(database, MIGRATE_V5_TO_V6_SQL);
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V6_TO_V7_SQL);
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V7_TO_V8_SQL);
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V8_TO_V9_SQL);
        }
        if (status == TRAINLOG_STATUS_OK) {
            status = execute_sql(database, MIGRATE_V9_TO_V10_SQL);
        }
    }

    if (status == TRAINLOG_STATUS_OK) {
        if (version < 11) status = migrate_v10_to_v11(database);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = execute_sql(database, MIGRATE_V11_TO_V12_SQL);
    }

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        set_open_diagnostic(
            output_diagnostic,
            output_diagnostic_capacity,
            version == 0 ? "create schema v12" : "migrate database to schema v12",
            database->connection,
            SQLITE_ERROR
        );
        (void)sqlite3_exec(
            database->connection,
            "ROLLBACK;",
            NULL,
            NULL,
            NULL
        );
        /* MIGRATE_V9_TO_V10_SQL disables foreign keys outside its transaction;
         * a failed statement must not leave the live handle with enforcement
         * disabled after the rollback. */
        (void)sqlite3_exec(
            database->connection,
            "PRAGMA foreign_keys = ON;",
            NULL,
            NULL,
            NULL
        );
    }

    return status;
}

TrainlogStatus trainlog_database_open(
    const char *path,
    TrainlogDatabase **output_database
)
{
    return trainlog_database_open_with_diagnostic(
        path,
        output_database,
        NULL,
        0U
    );
}

TrainlogStatus trainlog_database_open_with_diagnostic(
    const char *path,
    TrainlogDatabase **output_database,
    char *output_diagnostic,
    size_t output_diagnostic_capacity
)
{
    TrainlogDatabase *database;
    int rc;
    TrainlogStatus status;

    if (path == NULL || path[0] == '\0' || output_database == NULL) {
        set_open_diagnostic(
            output_diagnostic,
            output_diagnostic_capacity,
            "validate database path",
            NULL,
            SQLITE_MISUSE
        );
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_database = NULL;
    if (output_diagnostic != NULL && output_diagnostic_capacity > 0U) {
        output_diagnostic[0] = '\0';
    }

    database = calloc(1U, sizeof(*database));
    if (database == NULL) {
        set_open_diagnostic(
            output_diagnostic,
            output_diagnostic_capacity,
            "allocate database handle",
            NULL,
            SQLITE_NOMEM
        );
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    rc = sqlite3_open_v2(
        path,
        &database->connection,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        NULL
    );
    if (rc != SQLITE_OK) {
        set_open_diagnostic(
            output_diagnostic,
            output_diagnostic_capacity,
            "open database",
            database->connection,
            rc
        );
        trainlog_database_close(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_busy_timeout(database->connection, 5000) != SQLITE_OK) {
        set_open_diagnostic(
            output_diagnostic,
            output_diagnostic_capacity,
            "configure database busy timeout",
            database->connection,
            SQLITE_ERROR
        );
        trainlog_database_close(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    status = execute_sql(database, "PRAGMA foreign_keys = ON;");
    if (status != TRAINLOG_STATUS_OK) {
        set_open_diagnostic(
            output_diagnostic,
            output_diagnostic_capacity,
            "enable foreign-key enforcement",
            database->connection,
            SQLITE_ERROR
        );
        trainlog_database_close(database);
        return status;
    }

    status = initialize_or_validate_schema(
        database,
        output_diagnostic,
        output_diagnostic_capacity
    );
    if (status != TRAINLOG_STATUS_OK) {
        trainlog_database_close(database);
        return status;
    }

    *output_database = database;
    return TRAINLOG_STATUS_OK;
}

void trainlog_database_close(TrainlogDatabase *database)
{
    if (database == NULL) {
        return;
    }

    if (database->connection != NULL) {
        (void)sqlite3_close(database->connection);
    }

    free(database);
}

TrainlogStatus trainlog_database_schema_version(
    TrainlogDatabase *database,
    int *output_version
)
{
    return read_single_int_pragma(
        database,
        "PRAGMA user_version;",
        output_version
    );
}

TrainlogStatus trainlog_database_foreign_keys_enabled(
    TrainlogDatabase *database,
    int *output_enabled
)
{
    return read_single_int_pragma(
        database,
        "PRAGMA foreign_keys;",
        output_enabled
    );
}

TrainlogStatus trainlog_database_create_custom_equipment(
    TrainlogDatabase *database,
    const TrainlogCustomEquipment *equipment
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    /* CONTRACT: custom IDs must not shadow frozen manifest identities. */
    if (database == NULL || equipment == NULL || equipment->equipment_id[0] == '\0' ||
        equipment->display_name[0] == '\0' || equipment->equipment_type[0] == '\0' ||
        trainlog_equipment_catalog_lookup(equipment->equipment_id) != NULL ||
        (strcmp(equipment->load_semantics, "none") != 0 &&
         strcmp(equipment->load_semantics, "external") != 0 &&
         strcmp(equipment->load_semantics, "assistance") != 0)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    rc = sqlite3_prepare_v2(database->connection,
        "INSERT INTO custom_equipment(equipment_id,display_name,label_name,equipment_type,load_semantics) VALUES(?1,?2,?3,?4,?5);",
        -1, &statement, NULL);
    if (rc != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    (void)sqlite3_bind_text(statement, 1, equipment->equipment_id, -1, SQLITE_TRANSIENT);
    (void)sqlite3_bind_text(statement, 2, equipment->display_name, -1, SQLITE_TRANSIENT);
    (void)sqlite3_bind_text(statement, 3, equipment->label_name, -1, SQLITE_TRANSIENT);
    (void)sqlite3_bind_text(statement, 4, equipment->equipment_type, -1, SQLITE_TRANSIENT);
    (void)sqlite3_bind_text(statement, 5, equipment->load_semantics, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(statement);
    (void)sqlite3_finalize(statement);
    return rc == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_list_custom_equipment(
    TrainlogDatabase *database, TrainlogCustomEquipment *output,
    size_t capacity, size_t *output_count
)
{
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;
    if (database == NULL || output_count == NULL || (capacity > 0U && output == NULL))
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_count = 0U;
    rc = sqlite3_prepare_v2(database->connection,
        "SELECT equipment_id,display_name,label_name,equipment_type,load_semantics FROM custom_equipment ORDER BY display_name COLLATE NOCASE,equipment_id;",
        -1, &statement, NULL);
    if (rc != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < capacity) {
            TrainlogCustomEquipment *item = &output[count];
            (void)memset(item, 0, sizeof(*item));
            (void)snprintf(item->equipment_id, sizeof(item->equipment_id), "%s", sqlite3_column_text(statement, 0));
            (void)snprintf(item->display_name, sizeof(item->display_name), "%s", sqlite3_column_text(statement, 1));
            (void)snprintf(item->label_name, sizeof(item->label_name), "%s", sqlite3_column_text(statement, 2));
            (void)snprintf(item->equipment_type, sizeof(item->equipment_type), "%s", sqlite3_column_text(statement, 3));
            (void)snprintf(item->load_semantics, sizeof(item->load_semantics), "%s", sqlite3_column_text(statement, 4));
        }
        ++count;
    }
    (void)sqlite3_finalize(statement);
    *output_count = count;
    return rc == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

static bool copy_custom_equipment_page_column(
    sqlite3_stmt *statement, int column, char *output, size_t capacity)
{
    const unsigned char *source;
    int byte_count;
    size_t length;

    if (sqlite3_column_type(statement, column) != SQLITE_TEXT ||
        output == NULL || capacity == 0U) return false;
    source = sqlite3_column_text(statement, column);
    byte_count = sqlite3_column_bytes(statement, column);
    if (source == NULL || byte_count < 0) return false;
    length = (size_t)byte_count;
    /* SQLite TEXT may contain embedded NUL bytes; never silently truncate it. */
    if (length >= capacity || memchr(source, '\0', length) != NULL) return false;
    (void)memcpy(output, source, length);
    output[length] = '\0';
    return true;
}

TrainlogStatus trainlog_database_list_custom_equipment_page(
    TrainlogDatabase *database, size_t offset, TrainlogCustomEquipment *output,
    size_t capacity, size_t *output_count, bool *output_more)
{
    static const char *const SQL =
        "SELECT equipment_id,display_name,label_name,equipment_type,load_semantics "
        "FROM custom_equipment ORDER BY display_name COLLATE NOCASE,equipment_id "
        "LIMIT ?1 OFFSET ?2;";
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;

    if (output != NULL && output_count != NULL && output_more != NULL &&
        capacity >= 1U && capacity <= TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX) {
        *output_count = 0U;
        *output_more = false;
    }
    if (database == NULL || output == NULL || output_count == NULL ||
        output_more == NULL || capacity == 0U ||
        capacity > TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX ||
        (uintmax_t)offset > (uintmax_t)INT64_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    if (sqlite3_bind_int64(statement, 1, (sqlite3_int64)(capacity + 1U)) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)offset) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogCustomEquipment item;
        (void)memset(&item, 0, sizeof(item));
        if (!copy_custom_equipment_page_column(statement, 0, item.equipment_id,
                sizeof(item.equipment_id)) ||
            !copy_custom_equipment_page_column(statement, 1, item.display_name,
                sizeof(item.display_name)) ||
            !copy_custom_equipment_page_column(statement, 2, item.label_name,
                sizeof(item.label_name)) ||
            !copy_custom_equipment_page_column(statement, 3, item.equipment_type,
                sizeof(item.equipment_type)) ||
            !copy_custom_equipment_page_column(statement, 4, item.load_semantics,
                sizeof(item.load_semantics))) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        if (count == capacity) {
            *output_more = true;
            break;
        }
        output[count++] = item;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) return TRAINLOG_STATUS_DATABASE_ERROR;
    *output_count = count;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_resolve_equipment(
    TrainlogDatabase *database, const char *equipment_id,
    TrainlogResolvedEquipment *output
)
{
    const TrainlogEquipment *supplied;
    sqlite3_stmt *statement = NULL;
    int rc;
    if (database == NULL || equipment_id == NULL || equipment_id[0] == '\0' ||
        output == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)memset(output, 0, sizeof(*output));
    (void)snprintf(output->equipment_id, sizeof(output->equipment_id), "%s", equipment_id);
    supplied = trainlog_equipment_catalog_lookup(equipment_id);
    if (supplied != NULL) {
        (void)snprintf(output->display_name, sizeof(output->display_name), "%s", supplied->display_name);
        (void)snprintf(output->label_name, sizeof(output->label_name), "%s", supplied->label_name);
        (void)snprintf(output->equipment_type, sizeof(output->equipment_type), "%s", supplied->equipment_type);
        (void)snprintf(output->load_semantics, sizeof(output->load_semantics), "%s", supplied->load_semantics);
        output->origin = TRAINLOG_EQUIPMENT_SUPPLIED;
        return TRAINLOG_STATUS_OK;
    }
    rc = sqlite3_prepare_v2(database->connection,
        "SELECT display_name,label_name,equipment_type,load_semantics FROM custom_equipment WHERE equipment_id=?1;",
        -1, &statement, NULL);
    if (rc != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    (void)sqlite3_bind_text(statement, 1, equipment_id, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        (void)snprintf(output->display_name, sizeof(output->display_name), "%s", sqlite3_column_text(statement, 0));
        (void)snprintf(output->label_name, sizeof(output->label_name), "%s", sqlite3_column_text(statement, 1));
        (void)snprintf(output->equipment_type, sizeof(output->equipment_type), "%s", sqlite3_column_text(statement, 2));
        (void)snprintf(output->load_semantics, sizeof(output->load_semantics), "%s", sqlite3_column_text(statement, 3));
        output->origin = TRAINLOG_EQUIPMENT_CUSTOM;
    } else if (rc == SQLITE_DONE) {
        /* INVARIANT: historic references survive catalogue changes visibly. */
        (void)snprintf(output->display_name, sizeof(output->display_name), "Inconnu (%s)", equipment_id);
        output->origin = TRAINLOG_EQUIPMENT_UNKNOWN;
        rc = SQLITE_ROW;
    }
    (void)sqlite3_finalize(statement);
    return rc == SQLITE_ROW ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_list_exercise_equipment(
    TrainlogDatabase *database, const char *exercise_id,
    TrainlogResolvedEquipment *output, size_t capacity, size_t *output_count
)
{
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;
    if (database == NULL || exercise_id == NULL || output_count == NULL ||
        (capacity > 0U && output == NULL)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_count = 0U;
    rc = sqlite3_prepare_v2(database->connection,
        "SELECT DISTINCT se.equipment_id FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id WHERE e.exercise_id=?1 AND se.equipment_id IS NOT NULL AND se.equipment_id<>'' ORDER BY se.equipment_id;",
        -1, &statement, NULL);
    if (rc != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    (void)sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(statement, 0);
        if (count < capacity && trainlog_database_resolve_equipment(database, id, &output[count]) != TRAINLOG_STATUS_OK) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        ++count;
    }
    (void)sqlite3_finalize(statement);
    *output_count = count;
    return rc == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_begin(TrainlogDatabase *database)
{
    return execute_sql(database, "BEGIN IMMEDIATE;");
}

TrainlogStatus trainlog_database_commit(TrainlogDatabase *database)
{
    return execute_sql(database, "COMMIT;");
}

TrainlogStatus trainlog_database_rollback(TrainlogDatabase *database)
{
    return execute_sql(database, "ROLLBACK;");
}

static const char *tracking_mode_to_sql(TrainlogTrackingMode mode)
{
    switch (mode) {
    case TRAINLOG_TRACKING_REPS:
        return "reps";
    case TRAINLOG_TRACKING_DURATION:
        return "duration";
    default:
        return NULL;
    }
}

static const char *recording_mode_to_sql(
    TrainlogRecordingMode mode
)
{
    switch (mode) {
    case TRAINLOG_RECORDING_SETS:
        return "sets";
    case TRAINLOG_RECORDING_CONTINUOUS:
        return "continuous";
    default:
        return NULL;
    }
}

static bool exercise_profile_valid(
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields
)
{
    if ((data_fields &
         ~TRAINLOG_EXERCISE_DATA_KNOWN_MASK) != 0U) {
        return false;
    }

    if (recording_mode == TRAINLOG_RECORDING_CONTINUOUS) {
        return tracking_mode == TRAINLOG_TRACKING_DURATION;
    }

    return
        recording_mode == TRAINLOG_RECORDING_SETS &&
        (tracking_mode == TRAINLOG_TRACKING_REPS ||
         tracking_mode == TRAINLOG_TRACKING_DURATION);
}


static const char *load_mode_to_sql(TrainlogLoadMode mode)
{
    switch (mode) {
    case TRAINLOG_LOAD_NONE:
        return "none";
    case TRAINLOG_LOAD_EXTERNAL:
        return "external";
    case TRAINLOG_LOAD_ASSISTANCE:
        return "assistance";
    default:
        return NULL;
    }
}

static const char *session_type_to_sql(
    TrainlogSessionType type
)
{
    switch (type) {
    case TRAINLOG_SESSION_TRAINING:
        return "training";

    case TRAINLOG_SESSION_MAX_TEST:
        return "max_test";

    default:
        return NULL;
    }
}

TrainlogStatus trainlog_database_insert_exercise_profiled(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *name,
    const char *normalized_name,
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields
)
{
    static const char *const SQL =
        "INSERT INTO exercises("
        "exercise_id, name, normalized_name, tracking_mode, "
        "recording_mode, data_fields"
        ") VALUES(?1, ?2, ?3, ?4, ?5, ?6);";

    sqlite3_stmt *statement = NULL;
    const char *tracking;
    const char *recording;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        exercise_id == NULL ||
        exercise_id[0] == '\0' ||
        name == NULL ||
        name[0] == '\0' ||
        normalized_name == NULL ||
        normalized_name[0] == '\0' ||
        !exercise_profile_valid(
            tracking_mode,
            recording_mode,
            data_fields
        )) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    tracking = tracking_mode_to_sql(tracking_mode);
    recording = recording_mode_to_sql(recording_mode);

    if (tracking == NULL || recording == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        SQL,
        -1,
        &statement,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, normalized_name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, tracking, -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_text(statement, 5, recording, -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 6, (sqlite3_int64)data_fields) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc == SQLITE_CONSTRAINT) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_insert_exercise(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *name,
    const char *normalized_name,
    TrainlogTrackingMode tracking_mode
)
{
    return trainlog_database_insert_exercise_profiled(
        database,
        exercise_id,
        name,
        normalized_name,
        tracking_mode,
        TRAINLOG_RECORDING_SETS,
        0U
    );
}

TrainlogStatus trainlog_database_update_exercise_profiled(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *name,
    const char *normalized_name,
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields,
    const char *primary_zone_id,
    const char *const *secondary_zone_ids,
    size_t secondary_count
)
{
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 row_id;
    TrainlogStatus status;
    int rc;
    bool profile_changed;
    const char *tracking;
    const char *recording;

    if (database == NULL || exercise_id == NULL || name == NULL ||
        normalized_name == NULL || name[0] == '\0' || normalized_name[0] == '\0' ||
        strlen(name) > TRAINLOG_NAME_MAX ||
        !exercise_profile_valid(tracking_mode, recording_mode, data_fields))
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    tracking = tracking_mode_to_sql(tracking_mode);
    recording = recording_mode_to_sql(recording_mode);
    status = lookup_exercise_row_id(database, exercise_id, &row_id);
    if (status != TRAINLOG_STATUS_OK) return status;
    if (execute_sql(database, "BEGIN IMMEDIATE;") != TRAINLOG_STATUS_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;

    rc = sqlite3_prepare_v2(database->connection,
        "SELECT tracking_mode,recording_mode,data_fields FROM exercises WHERE id=?1;",
        -1, &statement, NULL);
    if (rc != SQLITE_OK || sqlite3_bind_int64(statement, 1, row_id) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto rollback;
    }
    profile_changed = strcmp((const char *)sqlite3_column_text(statement, 0), tracking) != 0 ||
        strcmp((const char *)sqlite3_column_text(statement, 1), recording) != 0 ||
        sqlite3_column_int64(statement, 2) != (sqlite3_int64)data_fields;
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL;
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto rollback;
    }
    statement = NULL;
    if (profile_changed) {
        rc = sqlite3_prepare_v2(database->connection,
            "SELECT 1 FROM session_exercises WHERE exercise_row_id=?1 LIMIT 1;",
            -1, &statement, NULL);
        if (rc != SQLITE_OK || sqlite3_bind_int64(statement, 1, row_id) != SQLITE_OK) {
            status = TRAINLOG_STATUS_DATABASE_ERROR;
            goto rollback;
        }
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW) {
            status = TRAINLOG_STATUS_CONFLICT;
            goto rollback;
        }
        if (rc != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
            statement = NULL;
            status = TRAINLOG_STATUS_DATABASE_ERROR;
            goto rollback;
        }
        statement = NULL;
    }
    rc = sqlite3_prepare_v2(database->connection,
        "UPDATE exercises SET name=?1,normalized_name=?2,tracking_mode=?3,"
        "recording_mode=?4,data_fields=?5 WHERE id=?6;", -1, &statement, NULL);
    if (rc != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, normalized_name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, tracking, -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, recording, -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 5, (sqlite3_int64)data_fields) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 6, row_id) != SQLITE_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto rollback;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE) {
        status = rc == SQLITE_CONSTRAINT ? TRAINLOG_STATUS_CONFLICT :
            TRAINLOG_STATUS_DATABASE_ERROR;
        goto rollback;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL;
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto rollback;
    }
    statement = NULL;
    status = trainlog_database_replace_exercise_body_zones(database, exercise_id,
        primary_zone_id, secondary_zone_ids, secondary_count);
    if (status != TRAINLOG_STATUS_OK) goto rollback;
    if (execute_sql(database, "COMMIT;") != TRAINLOG_STATUS_OK)
    {
        (void)sqlite3_exec(database->connection, "ROLLBACK;", NULL, NULL, NULL);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;

rollback:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    (void)sqlite3_exec(database->connection, "ROLLBACK;", NULL, NULL, NULL);
    return status;
}

static TrainlogStatus count_query(
    TrainlogDatabase *database,
    const char *sql,
    size_t *output_count
)
{
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 count;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        sql == NULL ||
        output_count == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    count = sqlite3_column_int64(statement, 0);
    if (count < 0 || (uint64_t)count > (uint64_t)SIZE_MAX) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_count = (size_t)count;

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_exercise_count(
    TrainlogDatabase *database,
    size_t *output_count
)
{
    return count_query(
        database,
        "SELECT COUNT(*) FROM exercises;",
        output_count
    );
}

TrainlogStatus trainlog_database_session_count(
    TrainlogDatabase *database,
    size_t *output_count
)
{
    return count_query(
        database,
        "SELECT COUNT(*) FROM sessions;",
        output_count
    );
}

static TrainlogTrackingMode tracking_mode_from_sql(const char *text)
{
    return text != NULL && strcmp(text, "duration") == 0
        ? TRAINLOG_TRACKING_DURATION
        : TRAINLOG_TRACKING_REPS;
}

static TrainlogRecordingMode recording_mode_from_sql(
    const char *text
)
{
    return
        text != NULL &&
        strcmp(text, "continuous") == 0
            ? TRAINLOG_RECORDING_CONTINUOUS
            : TRAINLOG_RECORDING_SETS;
}


static bool session_type_from_sql(
    const char *text,
    TrainlogSessionType *output
)
{
    if (text == NULL ||
        output == NULL) {
        return false;
    }

    if (strcmp(text, "training") == 0) {
        *output = TRAINLOG_SESSION_TRAINING;
        return true;
    }

    if (strcmp(text, "max_test") == 0) {
        *output = TRAINLOG_SESSION_MAX_TEST;
        return true;
    }

    return false;
}

TrainlogStatus trainlog_database_list_exercises(
    TrainlogDatabase *database,
    TrainlogExercise *output,
    size_t capacity,
    size_t *output_count
)
{
    static const char *const SQL =
        "SELECT exercise_id, name, tracking_mode, "
        "recording_mode, data_fields "
        "FROM exercises "
        "ORDER BY name COLLATE NOCASE, exercise_id;";

    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        output_count == NULL ||
        (output == NULL && capacity != 0U)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = 0U;

    rc = sqlite3_prepare_v2(
        database->connection,
        SQL,
        -1,
        &statement,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (output != NULL && count < capacity) {
            const unsigned char *exercise_id =
                sqlite3_column_text(statement, 0);
            const unsigned char *name =
                sqlite3_column_text(statement, 1);
            const unsigned char *tracking =
                sqlite3_column_text(statement, 2);
            const unsigned char *recording =
                sqlite3_column_text(statement, 3);
            sqlite3_int64 data_fields =
                sqlite3_column_int64(statement, 4);

            if (exercise_id == NULL ||
                name == NULL ||
                tracking == NULL ||
                recording == NULL ||
                data_fields < 0 ||
                (uint64_t)data_fields > (uint64_t)UINT32_MAX ||
                (((TrainlogExerciseDataFields)data_fields) &
                 ~TRAINLOG_EXERCISE_DATA_KNOWN_MASK) != 0U) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)memset(
                &output[count],
                0,
                sizeof(output[count])
            );

            (void)snprintf(
                output[count].exercise_id,
                sizeof(output[count].exercise_id),
                "%s",
                (const char *)exercise_id
            );

            (void)snprintf(
                output[count].name,
                sizeof(output[count].name),
                "%s",
                (const char *)name
            );

            output[count].tracking_mode =
                tracking_mode_from_sql(
                    (const char *)tracking
                );

            output[count].recording_mode =
                recording_mode_from_sql(
                    (const char *)recording
                );

            output[count].data_fields =
                (TrainlogExerciseDataFields)data_fields;
        }

        ++count;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_count = count;

    if (output != NULL && count > capacity) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_resolve_exercise_id(
    TrainlogDatabase *database,
    const char *exercise_id,
    char *output_canonical_id,
    size_t output_capacity
)
{
    static const char *const SQL =
        "SELECT exercise_id FROM exercises WHERE exercise_id=?1 "
        "UNION ALL SELECT canonical_exercise_id FROM exercise_aliases "
        "WHERE source_exercise_id=?1 LIMIT 1;";
    sqlite3_stmt *statement = NULL;
    const unsigned char *canonical;
    int rc;
    if (database == NULL || database->connection == NULL || exercise_id == NULL ||
        exercise_id[0] == '\0' || output_canonical_id == NULL ||
        output_capacity < TRAINLOG_ID_MAX + 1U)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK || sqlite3_bind_text(statement, 1, exercise_id, -1,
            SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    rc = sqlite3_step(statement);
    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    canonical = rc == SQLITE_ROW ? sqlite3_column_text(statement, 0) : NULL;
    if (canonical == NULL || strlen((const char *)canonical) > TRAINLOG_ID_MAX) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    (void)snprintf(output_canonical_id, output_capacity, "%s", canonical);
    return sqlite3_finalize(statement) == SQLITE_OK ? TRAINLOG_STATUS_OK :
        TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus merge_bind_ids(
    TrainlogDatabase *database,
    const char *sql,
    sqlite3_int64 source_row_id,
    sqlite3_int64 canonical_row_id
)
{
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK || sqlite3_bind_parameter_count(statement) < 1 ||
        sqlite3_bind_int64(statement, 1, source_row_id) != SQLITE_OK ||
        (sqlite3_bind_parameter_count(statement) >= 2 &&
         sqlite3_bind_int64(statement, 2, canonical_row_id) != SQLITE_OK)) {
        if (statement != NULL) (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return rc == SQLITE_CONSTRAINT ? TRAINLOG_STATUS_CONFLICT :
            TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return sqlite3_finalize(statement) == SQLITE_OK ? TRAINLOG_STATUS_OK :
        TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_merge_exercises(
    TrainlogDatabase *database,
    const char *source_exercise_id,
    const char *canonical_exercise_id
)
{
    static const char *const PROFILE_SQL =
        "SELECT s.id,c.id,s.tracking_mode,c.tracking_mode,"
        "s.recording_mode,c.recording_mode,s.data_fields,c.data_fields,"
        "(SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=s.id "
        "AND role='primary'),(SELECT zone_id FROM exercise_body_zones "
        "WHERE exercise_row_id=c.id AND role='primary') "
        "FROM exercises s JOIN exercises c ON s.exercise_id=?1 "
        "AND c.exercise_id=?2;";
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 source_row_id, canonical_row_id;
    const char *source_primary = NULL, *canonical_primary = NULL;
    char primary[TRAINLOG_ZONE_ID_MAX + 1U] = "";
    TrainlogStatus status = TRAINLOG_STATUS_DATABASE_ERROR;
    int rc;
    if (database == NULL || database->connection == NULL ||
        source_exercise_id == NULL || canonical_exercise_id == NULL ||
        source_exercise_id[0] == '\0' || canonical_exercise_id[0] == '\0' ||
        strcmp(source_exercise_id, canonical_exercise_id) == 0)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    if (execute_sql(database, "BEGIN IMMEDIATE;") != TRAINLOG_STATUS_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    rc = sqlite3_prepare_v2(database->connection, PROFILE_SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, source_exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, canonical_exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) goto rollback;
    rc = sqlite3_step(statement);
    if (rc == SQLITE_DONE) { status = TRAINLOG_STATUS_NOT_FOUND; goto rollback; }
    if (rc != SQLITE_ROW) goto rollback;
    source_row_id = sqlite3_column_int64(statement, 0);
    canonical_row_id = sqlite3_column_int64(statement, 1);
    if (strcmp((const char *)sqlite3_column_text(statement, 2),
               (const char *)sqlite3_column_text(statement, 3)) != 0 ||
        strcmp((const char *)sqlite3_column_text(statement, 4),
               (const char *)sqlite3_column_text(statement, 5)) != 0 ||
        sqlite3_column_int64(statement, 6) != sqlite3_column_int64(statement, 7)) {
        status = TRAINLOG_STATUS_CONFLICT; goto rollback;
    }
    if (sqlite3_column_type(statement, 8) != SQLITE_NULL)
        source_primary = (const char *)sqlite3_column_text(statement, 8);
    if (sqlite3_column_type(statement, 9) != SQLITE_NULL)
        canonical_primary = (const char *)sqlite3_column_text(statement, 9);
    if (source_primary != NULL && canonical_primary != NULL &&
        strcmp(source_primary, canonical_primary) != 0) {
        status = TRAINLOG_STATUS_CONFLICT; goto rollback;
    }
    if (canonical_primary != NULL || source_primary != NULL)
        (void)snprintf(primary, sizeof(primary), "%s",
            canonical_primary != NULL ? canonical_primary : source_primary);
    if (sqlite3_finalize(statement) != SQLITE_OK) { statement = NULL; goto rollback; }
    statement = NULL;

    /* INVARIANT: promote the chosen primary before unioning secondaries, so a
     * former secondary cannot mask it through the (exercise,zone) key. */
    if (primary[0] != '\0') {
        status = merge_bind_ids(database,
            "DELETE FROM exercise_body_zones WHERE exercise_row_id=?2 "
            "AND zone_id=(SELECT zone_id FROM exercise_body_zones "
            "WHERE exercise_row_id=?1 AND role='primary');",
            source_row_id, canonical_row_id);
        if (status != TRAINLOG_STATUS_OK) goto rollback;
        rc = sqlite3_prepare_v2(database->connection,
            "INSERT OR REPLACE INTO exercise_body_zones(exercise_row_id,zone_id,role) "
            "VALUES(?1,?2,'primary');", -1, &statement, NULL);
        if (rc != SQLITE_OK || sqlite3_bind_int64(statement, 1, canonical_row_id) != SQLITE_OK ||
            sqlite3_bind_text(statement, 2, primary, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
            statement = NULL; goto rollback;
        }
        statement = NULL;
    }
    status = merge_bind_ids(database,
        "INSERT OR IGNORE INTO exercise_body_zones(exercise_row_id,zone_id,role) "
        "SELECT ?2,zone_id,'secondary' FROM exercise_body_zones "
        "WHERE exercise_row_id=?1 AND role='secondary' "
        "AND zone_id<>(SELECT COALESCE((SELECT zone_id FROM exercise_body_zones "
        "WHERE exercise_row_id=?2 AND role='primary'),''));",
        source_row_id, canonical_row_id);
    if (status != TRAINLOG_STATUS_OK) goto rollback;
    status = merge_bind_ids(database,
        "UPDATE session_exercises SET exercise_row_id=?2 WHERE exercise_row_id=?1;",
        source_row_id, canonical_row_id);
    if (status != TRAINLOG_STATUS_OK) goto rollback;

    rc = sqlite3_prepare_v2(database->connection,
        "UPDATE exercise_aliases SET canonical_exercise_id=?2 "
        "WHERE canonical_exercise_id=?1;", -1, &statement, NULL);
    if (rc != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, source_exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, canonical_exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL; goto rollback;
    }
    statement = NULL;
    rc = sqlite3_prepare_v2(database->connection,
        "INSERT INTO exercise_aliases(source_exercise_id,canonical_exercise_id) "
        "VALUES(?1,?2);", -1, &statement, NULL);
    if (rc != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, source_exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, canonical_exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL; goto rollback;
    }
    statement = NULL;
    status = merge_bind_ids(database,
        "DELETE FROM exercise_body_zone_sync WHERE exercise_row_id IN(?1,?2);",
        source_row_id, canonical_row_id);
    if (status != TRAINLOG_STATUS_OK) goto rollback;
    status = merge_bind_ids(database, "DELETE FROM exercises WHERE id=?1;",
        source_row_id, canonical_row_id);
    if (status != TRAINLOG_STATUS_OK) goto rollback;
    if (execute_sql(database, "COMMIT;") != TRAINLOG_STATUS_OK) goto rollback;
    return TRAINLOG_STATUS_OK;

rollback:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    (void)sqlite3_exec(database->connection, "ROLLBACK;", NULL, NULL, NULL);
    return status;
}

TrainlogStatus trainlog_database_preview_exercise_merge(
    TrainlogDatabase *database,
    const char *source_exercise_id,
    TrainlogExerciseMergePreview *output
)
{
    static const char *const SQL =
        "SELECT "
        "(SELECT COUNT(*) FROM session_exercises se WHERE se.exercise_row_id=e.id),"
        "(SELECT COUNT(*) FROM performed_sets ps JOIN session_exercises se "
        "ON se.id=ps.session_exercise_row_id WHERE se.exercise_row_id=e.id),"
        "(SELECT COUNT(*) FROM continuous_activity ca JOIN session_exercises se "
        "ON se.id=ca.session_exercise_row_id WHERE se.exercise_row_id=e.id),"
        "(SELECT COUNT(*) FROM max_results mr JOIN session_exercises se "
        "ON se.id=mr.session_exercise_row_id WHERE se.exercise_row_id=e.id),"
        "(SELECT COUNT(DISTINCT se.equipment_id) FROM session_exercises se "
        "WHERE se.exercise_row_id=e.id AND se.equipment_id IS NOT NULL "
        "AND se.equipment_id<>''),"
        "(SELECT COUNT(*) FROM exercise_body_zones z WHERE z.exercise_row_id=e.id) "
        "FROM exercises e WHERE e.exercise_id=?1;";
    sqlite3_stmt *statement = NULL;
    TrainlogExerciseMergePreview preview = {0};
    int rc;
    if (database == NULL || database->connection == NULL ||
        source_exercise_id == NULL || source_exercise_id[0] == '\0' ||
        output == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK || sqlite3_bind_text(statement, 1, source_exercise_id,
            -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    rc = sqlite3_step(statement);
    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    preview.occurrences = (size_t)sqlite3_column_int64(statement, 0);
    preview.performed_sets = (size_t)sqlite3_column_int64(statement, 1);
    preview.continuous_activities = (size_t)sqlite3_column_int64(statement, 2);
    preview.max_results = (size_t)sqlite3_column_int64(statement, 3);
    preview.associated_equipment = (size_t)sqlite3_column_int64(statement, 4);
    preview.body_zones = (size_t)sqlite3_column_int64(statement, 5);
    if (sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    *output = preview;
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus lookup_exercise_row_id(
    TrainlogDatabase *database,
    const char *exercise_id,
    sqlite3_int64 *output_row_id
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    rc = sqlite3_prepare_v2(
        database->connection,
        "SELECT id FROM exercises WHERE exercise_id = ?1;",
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_bind_text(
            statement,
            1,
            exercise_id,
            -1,
            SQLITE_TRANSIENT
        ) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_row_id = sqlite3_column_int64(statement, 0);

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

static bool assignable_body_zone(const char *zone_id)
{
    const TrainlogBodyZone *zone;
    if (zone_id == NULL || zone_id[0] == '\0' ||
        strlen(zone_id) > TRAINLOG_ZONE_ID_MAX) return false;
    zone = trainlog_body_zone_catalog_lookup(zone_id);
    return zone != NULL && !zone->is_group;
}

static TrainlogStatus insert_body_zone_relation(
    TrainlogDatabase *database,
    sqlite3_int64 exercise_row_id,
    const char *zone_id,
    const char *role
)
{
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(database->connection,
        "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) "
        "VALUES(?1,?2,?3);", -1, &statement, NULL);
    if (rc != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    if (sqlite3_bind_int64(statement, 1, exercise_row_id) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, zone_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, role, -1, SQLITE_STATIC) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    rc = sqlite3_step(statement);
    (void)sqlite3_finalize(statement);
    return rc == SQLITE_DONE ? TRAINLOG_STATUS_OK :
        rc == SQLITE_CONSTRAINT ? TRAINLOG_STATUS_CONFLICT :
        TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_replace_exercise_body_zones(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *primary_zone_id,
    const char *const *secondary_zone_ids,
    size_t secondary_count
)
{
    sqlite3_int64 row_id;
    sqlite3_stmt *statement = NULL;
    bool owns_transaction;
    size_t index;
    TrainlogStatus status;

    if (database == NULL || database->connection == NULL || exercise_id == NULL ||
        exercise_id[0] == '\0' || secondary_count > trainlog_body_zone_catalog_count() ||
        (secondary_count > 0U && secondary_zone_ids == NULL) ||
        (secondary_count > 0U &&
         (primary_zone_id == NULL || primary_zone_id[0] == '\0')) ||
        (primary_zone_id != NULL && primary_zone_id[0] != '\0' &&
         !assignable_body_zone(primary_zone_id))) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < secondary_count; ++index) {
        size_t earlier;
        if (!assignable_body_zone(secondary_zone_ids[index]) ||
            (primary_zone_id != NULL && strcmp(primary_zone_id, secondary_zone_ids[index]) == 0)) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        for (earlier = 0U; earlier < index; ++earlier) {
            if (strcmp(secondary_zone_ids[earlier], secondary_zone_ids[index]) == 0)
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
    }
    status = lookup_exercise_row_id(database, exercise_id, &row_id);
    if (status != TRAINLOG_STATUS_OK) return status;

    owns_transaction = sqlite3_get_autocommit(database->connection) != 0;
    if (owns_transaction && execute_sql(database, "BEGIN IMMEDIATE;") != TRAINLOG_STATUS_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    if (sqlite3_prepare_v2(database->connection,
            "DELETE FROM exercise_body_zones WHERE exercise_row_id=?1;",
            -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 1, row_id) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) {
        if (statement != NULL) (void)sqlite3_finalize(statement);
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto finish;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL;
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto finish;
    }
    statement = NULL;
    if (primary_zone_id != NULL && primary_zone_id[0] != '\0') {
        status = insert_body_zone_relation(database, row_id, primary_zone_id, "primary");
        if (status != TRAINLOG_STATUS_OK) goto finish;
    }
    for (index = 0U; index < secondary_count; ++index) {
        status = insert_body_zone_relation(database, row_id,
            secondary_zone_ids[index], "secondary");
        if (status != TRAINLOG_STATUS_OK) goto finish;
    }
    status = TRAINLOG_STATUS_OK;

finish:
    if (owns_transaction) {
        if (status == TRAINLOG_STATUS_OK) {
            if (execute_sql(database, "COMMIT;") != TRAINLOG_STATUS_OK) {
                status = TRAINLOG_STATUS_DATABASE_ERROR;
                (void)sqlite3_exec(database->connection, "ROLLBACK;", NULL, NULL, NULL);
            }
        } else {
            (void)sqlite3_exec(database->connection, "ROLLBACK;", NULL, NULL, NULL);
        }
    }
    return status;
}

TrainlogStatus trainlog_database_list_exercise_body_zones(
    TrainlogDatabase *database,
    const char *exercise_id,
    TrainlogExerciseBodyZone *output,
    size_t capacity,
    size_t *output_count
)
{
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 row_id;
    size_t count = 0U;
    bool has_primary = false;
    TrainlogStatus status;
    int rc;
    if (database == NULL || exercise_id == NULL || output_count == NULL ||
        (capacity > 0U && output == NULL)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_count = 0U;
    status = lookup_exercise_row_id(database, exercise_id, &row_id);
    if (status != TRAINLOG_STATUS_OK) return status;
    rc = sqlite3_prepare_v2(database->connection,
        "SELECT zone_id,role FROM exercise_body_zones WHERE exercise_row_id=?1 "
        "ORDER BY CASE role WHEN 'primary' THEN 0 ELSE 1 END,zone_id;",
        -1, &statement, NULL);
    if (rc != SQLITE_OK || sqlite3_bind_int64(statement, 1, row_id) != SQLITE_OK) {
        if (statement != NULL) (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *zone_id = (const char *)sqlite3_column_text(statement, 0);
        const char *role = (const char *)sqlite3_column_text(statement, 1);
        const TrainlogBodyZone *zone = trainlog_body_zone_catalog_lookup(zone_id);
        if (zone_id == NULL || role == NULL ||
            zone == NULL || zone->is_group ||
            (strcmp(role, "primary") != 0 && strcmp(role, "secondary") != 0)) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        if (count < capacity) {
            (void)memset(&output[count], 0, sizeof(output[count]));
            (void)snprintf(output[count].zone_id, sizeof(output[count].zone_id),
                "%s", zone_id);
            output[count].role = strcmp(role, "primary") == 0
                ? TRAINLOG_BODY_ZONE_PRIMARY : TRAINLOG_BODY_ZONE_SECONDARY;
        }
        if (strcmp(role, "primary") == 0) has_primary = true;
        ++count;
    }
    if (rc != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    /* INVARIANT: the explicitly unclassified state has no relations. A
     * secondary-only raw SQLite state is corruption, not partial metadata. */
    if (count > 0U && !has_primary) return TRAINLOG_STATUS_DATABASE_ERROR;
    *output_count = count;
    return count > capacity ? TRAINLOG_STATUS_INVALID_ARGUMENT : TRAINLOG_STATUS_OK;
}

static bool exercise_row_matches_body_zone(
    TrainlogDatabase *database,
    sqlite3_int64 row_id,
    const char *zone_id,
    bool include_descendants,
    bool primary_only,
    bool unclassified_only,
    bool *output_match
)
{
    sqlite3_stmt *statement = NULL;
    int rc;
    bool has_relation = false;
    bool has_primary = false;
    bool match = false;
    if (sqlite3_prepare_v2(database->connection,
            "SELECT zone_id,role FROM exercise_body_zones WHERE exercise_row_id=?1;",
            -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 1, row_id) != SQLITE_OK) {
        if (statement != NULL) (void)sqlite3_finalize(statement);
        return false;
    }
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *candidate = (const char *)sqlite3_column_text(statement, 0);
        const char *role = (const char *)sqlite3_column_text(statement, 1);
        const TrainlogBodyZone *zone = trainlog_body_zone_catalog_lookup(candidate);
        if (candidate == NULL || role == NULL ||
            zone == NULL || zone->is_group ||
            (strcmp(role, "primary") != 0 && strcmp(role, "secondary") != 0)) {
            (void)sqlite3_finalize(statement);
            return false;
        }
        has_relation = true;
        if (strcmp(role, "primary") == 0) has_primary = true;
        if (!unclassified_only &&
            (!primary_only || strcmp(role, "primary") == 0) &&
            (strcmp(candidate, zone_id) == 0 ||
             (include_descendants &&
              trainlog_body_zone_catalog_is_descendant(candidate, zone_id)))) {
            match = true;
        }
    }
    if (rc != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK ||
        (has_relation && !has_primary)) return false;
    *output_match = unclassified_only ? !has_relation : match;
    return true;
}

TrainlogStatus trainlog_database_list_exercises_filtered(
    TrainlogDatabase *database,
    const char *normalized_prefix,
    const char *zone_id,
    bool include_descendants,
    bool primary_only,
    bool unclassified_only,
    TrainlogExercise *output,
    size_t capacity,
    size_t *output_count
)
{
    static const char *const SQL =
        "SELECT id,exercise_id,name,tracking_mode,recording_mode,data_fields,normalized_name "
        "FROM exercises ORDER BY name COLLATE NOCASE,exercise_id;";
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    size_t prefix_length;
    int rc;
    if (database == NULL || normalized_prefix == NULL || output_count == NULL ||
        (capacity > 0U && output == NULL) ||
        (unclassified_only && zone_id != NULL) ||
        (!unclassified_only && zone_id != NULL &&
         trainlog_body_zone_catalog_lookup(zone_id) == NULL))
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    prefix_length = strlen(normalized_prefix);
    *output_count = 0U;
    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        sqlite3_int64 row_id = sqlite3_column_int64(statement, 0);
        const char *normalized = (const char *)sqlite3_column_text(statement, 6);
        bool matches = zone_id == NULL && !unclassified_only;
        sqlite3_int64 data_fields = sqlite3_column_int64(statement, 5);
        if (normalized == NULL || strncmp(normalized, normalized_prefix, prefix_length) != 0)
            continue;
        if (!matches && !exercise_row_matches_body_zone(database, row_id,
                zone_id != NULL ? zone_id : "", include_descendants,
                primary_only, unclassified_only, &matches)) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        if (!matches) continue;
        if (data_fields < 0 || (uint64_t)data_fields > UINT32_MAX) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        if (count < capacity) {
            TrainlogExercise *item = &output[count];
            (void)memset(item, 0, sizeof(*item));
            (void)snprintf(item->exercise_id, sizeof(item->exercise_id), "%s",
                (const char *)sqlite3_column_text(statement, 1));
            (void)snprintf(item->name, sizeof(item->name), "%s",
                (const char *)sqlite3_column_text(statement, 2));
            item->tracking_mode = tracking_mode_from_sql(
                (const char *)sqlite3_column_text(statement, 3));
            item->recording_mode = recording_mode_from_sql(
                (const char *)sqlite3_column_text(statement, 4));
            item->data_fields = (TrainlogExerciseDataFields)data_fields;
        }
        ++count;
    }
    if (rc != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    *output_count = count;
    return count > capacity ? TRAINLOG_STATUS_INVALID_ARGUMENT : TRAINLOG_STATUS_OK;
}

static TrainlogStatus lookup_session_row_id(
    TrainlogDatabase *database,
    const char *session_id,
    sqlite3_int64 *output_row_id
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        session_id == NULL ||
        session_id[0] == '\0' ||
        output_row_id == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "SELECT id FROM sessions WHERE session_id = ?1;",
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_text(
        statement,
        1,
        session_id,
        -1,
        SQLITE_TRANSIENT
    );
    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_row_id =
        sqlite3_column_int64(statement, 0);

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus insert_session_header(
    TrainlogDatabase *database,
    const TrainlogSessionInput *session,
    sqlite3_int64 *output_row_id
)
{
    static const char *const SQL =
        "INSERT INTO sessions("
        "session_id, started_at, ended_at, session_type, notes"
        ") VALUES(?1, ?2, ?3, ?4, ?5);";

    sqlite3_stmt *statement = NULL;
    const char *session_type;
    int rc;

    session_type =
        session_type_to_sql(
            session->session_type
        );

    if (session_type == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        SQL,
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_text(
        statement,
        1,
        session->session_id,
        -1,
        SQLITE_TRANSIENT
    );

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            2,
            session->started_at,
            -1,
            SQLITE_TRANSIENT
        );
    }

    if (rc == SQLITE_OK) {
        rc =
            session->ended_at[0] != '\0'
                ? sqlite3_bind_text(
                    statement,
                    3,
                    session->ended_at,
                    -1,
                    SQLITE_TRANSIENT
                )
                : sqlite3_bind_null(
                    statement,
                    3
                );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            4,
            session_type,
            -1,
            SQLITE_STATIC
        );
    }

    if (rc == SQLITE_OK) {
        rc =
            session->notes != NULL &&
            session->notes[0] != '\0'
                ? sqlite3_bind_text(
                    statement,
                    5,
                    session->notes,
                    -1,
                    SQLITE_TRANSIENT
                )
                : sqlite3_bind_null(
                    statement,
                    5
                );
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc == SQLITE_CONSTRAINT) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_row_id =
        sqlite3_last_insert_rowid(
            database->connection
        );

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus insert_session_exercise(
    TrainlogDatabase *database,
    sqlite3_int64 session_row_id,
    size_t position,
    const TrainlogSessionExerciseInput *input,
    sqlite3_int64 *output_row_id
)
{
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 exercise_row_id;
    const char *load_mode;
    const char *recording_mode;
    char generated_entry_id[TRAINLOG_GENERATED_ID_CAPACITY];
    const char *entry_id;
    int rc;
    TrainlogStatus status;

    if (input == NULL ||
        output_row_id == NULL ||
        (input->data_fields &
         ~TRAINLOG_EXERCISE_DATA_KNOWN_MASK) != 0U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    if (input->equipment_id[0] != '\0' &&
        trainlog_equipment_catalog_lookup(input->equipment_id) == NULL &&
        !custom_equipment_exists(database, input->equipment_id)) {
        /* INVARIANT: a persisted occurrence names a known canonical or custom definition. */
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    /* WHY: entry_id, not position or exercise_id, is the durable identity of
     * one occurrence. Local TUI creation therefore allocates it exactly once
     * at persistence, while imported V2 identities pass through unchanged. */
    if (input->entry_id[0] == '\0') {
        status = trainlog_id_generate("sxe", generated_entry_id,
                                     sizeof(generated_entry_id));
        if (status != TRAINLOG_STATUS_OK) {
            return status;
        }
        entry_id = generated_entry_id;
    } else {
        entry_id = input->entry_id;
    }

    load_mode =
        load_mode_to_sql(
            input->load_mode
        );

    recording_mode =
        recording_mode_to_sql(
            input->recording_mode
        );

    if (load_mode == NULL ||
        recording_mode == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    if (input->has_max_weight) {
        if (!isfinite(input->max_weight_kg) ||
            input->max_weight_kg <= 0.0 ||
            input->rest_seconds != 0 ||
            input->target_sets != 0 ||
            input->target_reps != 0 ||
            input->target_duration_seconds != 0 ||
            input->target_has_weight ||
            input->set_count != 0U ||
            input->continuous_duration_seconds != 0 ||
            input->continuous_has_speed ||
            input->continuous_has_distance) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
    } else if (input->recording_mode ==
        TRAINLOG_RECORDING_CONTINUOUS) {
        bool speed_required =
            (input->data_fields &
             TRAINLOG_EXERCISE_DATA_SPEED_KMH) != 0U;

        bool distance_required =
            (input->data_fields &
             TRAINLOG_EXERCISE_DATA_DISTANCE_KM) != 0U;

        if (input->load_mode != TRAINLOG_LOAD_NONE ||
            input->rest_seconds != 0 ||
            input->target_sets != 0 ||
            input->target_reps != 0 ||
            input->target_duration_seconds != 0 ||
            input->target_has_weight ||
            input->set_count != 0U ||
            input->continuous_duration_seconds <= 0 ||
            input->continuous_has_speed != speed_required ||
            input->continuous_has_distance != distance_required ||
            (input->continuous_has_speed &&
             input->continuous_speed_kmh <= 0.0) ||
            (input->continuous_has_distance &&
             input->continuous_distance_km <= 0.0)) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
    } else if (
        input->recording_mode ==
        TRAINLOG_RECORDING_SETS
    ) {
        bool has_target_sets =
            input->target_sets > 0;

        bool has_target_reps =
            input->target_reps > 0;

        bool has_target_duration =
            input->target_duration_seconds > 0;

        bool any_target =
            has_target_sets ||
            has_target_reps ||
            has_target_duration;

        bool target_valid =
            !any_target ||
            (
                has_target_sets &&
                (
                    has_target_reps !=
                    has_target_duration
                )
            );

        if (
            input->target_sets < 0 ||
            input->target_reps < 0 ||
            input->target_duration_seconds < 0 ||
            !target_valid ||
            input->continuous_duration_seconds != 0 ||
            input->continuous_has_speed ||
            input->continuous_has_distance
        ) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
    } else {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status = lookup_exercise_row_id(
        database,
        input->exercise_id,
        &exercise_row_id
    );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "INSERT INTO session_exercises("
        "entry_id, session_row_id, exercise_row_id, "
        "recording_mode, data_fields, "
        "position, load_mode, rest_seconds, "
        "target_sets, target_reps, "
        "target_duration_seconds, "
        "target_weight_kg, notes, equipment_id"
        ") VALUES("
        "?1, ?2, ?3, ?4, ?5, ?6, ?7, "
        "?8, ?9, ?10, ?11, ?12, ?13, ?14"
        ");",
        -1,
        &statement,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_text(statement, 1, entry_id, -1, SQLITE_TRANSIENT);

    if (rc == SQLITE_OK) {
    rc = sqlite3_bind_int64(
        statement,
        2,
        session_row_id
    );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(
            statement,
            3,
            exercise_row_id
        );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            4,
            recording_mode,
            -1,
            SQLITE_STATIC
        );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(
            statement,
            5,
            (sqlite3_int64)input->data_fields
        );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(
            statement,
            6,
            (sqlite3_int64)position
        );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            7,
            load_mode,
            -1,
            SQLITE_STATIC
        );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int(
            statement,
            8,
            input->rest_seconds
        );
    }

    if (rc == SQLITE_OK) {
        rc = input->target_sets > 0
            ? sqlite3_bind_int(
                statement,
                9,
                input->target_sets
            )
            : sqlite3_bind_null(
                statement,
                9
            );
    }

    if (rc == SQLITE_OK) {
        rc = input->target_reps > 0
            ? sqlite3_bind_int(
                statement,
                10,
                input->target_reps
            )
            : sqlite3_bind_null(
                statement,
                10
            );
    }

    if (rc == SQLITE_OK) {
        rc = input->target_duration_seconds > 0
            ? sqlite3_bind_int(
                statement,
                11,
                input->target_duration_seconds
            )
            : sqlite3_bind_null(
                statement,
                11
            );
    }

    if (rc == SQLITE_OK) {
        rc = input->target_has_weight
            ? sqlite3_bind_double(
                statement,
                12,
                input->target_weight_kg
            )
            : sqlite3_bind_null(
                statement,
                12
            );
    }

    if (rc == SQLITE_OK) {
        rc =
            input->notes != NULL &&
            input->notes[0] != '\0'
                ? sqlite3_bind_text(
                    statement,
                    13,
                    input->notes,
                    -1,
                    SQLITE_TRANSIENT
                )
                : sqlite3_bind_null(
                    statement,
                    13
                );
    }

    if (rc == SQLITE_OK) {
        rc = input->equipment_id[0] != '\0'
            ? sqlite3_bind_text(statement, 14, input->equipment_id, -1,
                                SQLITE_TRANSIENT)
            : sqlite3_bind_null(statement, 14);
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc == SQLITE_CONSTRAINT) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_row_id =
        sqlite3_last_insert_rowid(
            database->connection
        );

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus insert_continuous_activity(
    TrainlogDatabase *database,
    sqlite3_int64 session_exercise_row_id,
    const TrainlogSessionExerciseInput *input
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        input == NULL ||
        input->recording_mode != TRAINLOG_RECORDING_CONTINUOUS ||
        input->continuous_duration_seconds <= 0) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "INSERT INTO continuous_activity("
        "session_exercise_row_id, "
        "duration_seconds, speed_kmh, "
        "distance_km"
        ") VALUES(?1, ?2, ?3, ?4);",
        -1,
        &statement,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_int64(
        statement,
        1,
        session_exercise_row_id
    );

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int(
            statement,
            2,
            input->continuous_duration_seconds
        );
    }

    if (rc == SQLITE_OK) {
        rc = input->continuous_has_speed
            ? sqlite3_bind_double(
                statement,
                3,
                input->continuous_speed_kmh
            )
            : sqlite3_bind_null(
                statement,
                3
            );
    }

    if (rc == SQLITE_OK) {
        rc = input->continuous_has_distance
            ? sqlite3_bind_double(
                statement,
                4,
                input->continuous_distance_km
            )
            : sqlite3_bind_null(
                statement,
                4
            );
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc == SQLITE_CONSTRAINT) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus insert_performed_set(
    TrainlogDatabase *database,
    sqlite3_int64 session_exercise_row_id,
    size_t position,
    const TrainlogSetInput *input
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    if (database == NULL || database->connection == NULL || input == NULL ||
        position > (size_t)INT64_MAX ||
        (input->has_weight &&
         (!isfinite(input->weight_kg) || input->weight_kg < 0.0))) {
        /* CONTRACT: blank is represented by has_weight=false/SQL NULL;
         * explicit zero is a finite, observed load and must survive exactly. */
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "INSERT INTO performed_sets("
        "session_exercise_row_id, position, reps, duration_seconds, weight_kg"
        ") VALUES(?1, ?2, ?3, ?4, ?5);",
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_int64(statement, 1, session_exercise_row_id);
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(statement, 2, (sqlite3_int64)position);
    }
    if (rc == SQLITE_OK) {
        rc = input->duration_seconds > 0
            ? sqlite3_bind_null(statement, 3)
            : sqlite3_bind_int(statement, 3, input->reps);
    }
    if (rc == SQLITE_OK) {
        rc = input->duration_seconds > 0
            ? sqlite3_bind_int(statement, 4, input->duration_seconds)
            : sqlite3_bind_null(statement, 4);
    }
    if (rc == SQLITE_OK) {
        rc = input->has_weight
            ? sqlite3_bind_double(statement, 5, input->weight_kg)
            : sqlite3_bind_null(statement, 5);
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_CONSTRAINT) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }
    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus insert_max_result(
    TrainlogDatabase *database,
    sqlite3_int64 session_exercise_row_id,
    double max_weight_kg
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    if (database == NULL || database->connection == NULL ||
        !isfinite(max_weight_kg) || max_weight_kg <= 0.0) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "INSERT INTO max_results(session_exercise_row_id,max_weight_kg) "
        "VALUES(?1,?2);",
        -1, &statement, NULL
    );
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(statement, 1, session_exercise_row_id);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_double(statement, 2, max_weight_kg);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
    }
    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return rc == SQLITE_CONSTRAINT
            ? TRAINLOG_STATUS_CONFLICT
            : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus insert_session_children(
    TrainlogDatabase *database,
    sqlite3_int64 session_row_id,
    TrainlogSessionType session_type,
    const TrainlogSessionExerciseInput *exercises,
    size_t exercise_count
)
{
    size_t exercise_index;

    if (database == NULL ||
        database->connection == NULL ||
        (exercise_count > 0U &&
         exercises == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    for (exercise_index = 0U;
         exercise_index < exercise_count;
         ++exercise_index) {
        const TrainlogSessionExerciseInput *exercise =
            &exercises[exercise_index];

        sqlite3_int64 session_exercise_row_id;
        size_t set_index;
        TrainlogStatus status;

        if (exercise->has_max_weight &&
            session_type != TRAINLOG_SESSION_MAX_TEST) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        status = insert_session_exercise(
            database,
            session_row_id,
            exercise_index,
            exercise,
            &session_exercise_row_id
        );

        if (status != TRAINLOG_STATUS_OK) {
            return status;
        }

        if (exercise->has_max_weight) {
            status = insert_max_result(
                database,
                session_exercise_row_id,
                exercise->max_weight_kg
            );
            if (status != TRAINLOG_STATUS_OK) {
                return status;
            }
            continue;
        }

        if (exercise->recording_mode ==
            TRAINLOG_RECORDING_CONTINUOUS) {
            status = insert_continuous_activity(
                database,
                session_exercise_row_id,
                exercise
            );

            if (status != TRAINLOG_STATUS_OK) {
                return status;
            }

            continue;
        }

        for (set_index = 0U;
             set_index < exercise->set_count;
             ++set_index) {
            if (exercise->sets == NULL) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }

            status = insert_performed_set(
                database,
                session_exercise_row_id,
                set_index,
                &exercise->sets[set_index]
            );

            if (status != TRAINLOG_STATUS_OK) {
                return status;
            }
        }
    }

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus validate_actual_set_weights(
    const TrainlogSessionExerciseInput *exercises,
    size_t exercise_count
)
{
    size_t exercise_index;

    if ((exercise_count > 0U && exercises == NULL) ||
        exercise_count > (size_t)INT64_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    for (exercise_index = 0U; exercise_index < exercise_count;
         ++exercise_index) {
        const TrainlogSessionExerciseInput *exercise =
            &exercises[exercise_index];
        size_t set_index;

        if (exercise->set_count > (size_t)INT64_MAX ||
            (exercise->set_count > 0U && exercise->sets == NULL)) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        for (set_index = 0U; set_index < exercise->set_count; ++set_index) {
            const TrainlogSetInput *set = &exercise->sets[set_index];
            if (set->has_weight &&
                (!isfinite(set->weight_kg) || set->weight_kg < 0.0)) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
        }
    }

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_insert_session(
    TrainlogDatabase *database,
    const TrainlogSessionInput *session
)
{
    sqlite3_int64 session_row_id;
    TrainlogStatus status;

    if (database == NULL ||
        database->connection == NULL ||
        session == NULL ||
        session->session_id[0] == '\0' ||
        session->started_at[0] == '\0' ||
        (session->exercise_count > 0U &&
         session->exercises == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status = validate_actual_set_weights(
        session->exercises,
        session->exercise_count
    );
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status = trainlog_database_begin(database);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status = insert_session_header(
        database,
        session,
        &session_row_id
    );

    if (status == TRAINLOG_STATUS_OK) {
        status = insert_session_children(
            database,
            session_row_id,
            session->session_type,
            session->exercises,
            session->exercise_count
        );
    }

    if (status != TRAINLOG_STATUS_OK) {
        (void)trainlog_database_rollback(database);
        return status;
    }

    status = trainlog_database_commit(database);

    if (status != TRAINLOG_STATUS_OK) {
        (void)trainlog_database_rollback(database);
    }

    return status;
}

TrainlogStatus trainlog_database_replace_session_exercises(
    TrainlogDatabase *database,
    const char *session_id,
    const TrainlogSessionExerciseInput *exercises,
    size_t exercise_count
)
{
    sqlite3_int64 session_row_id;
    sqlite3_stmt *statement = NULL;
    TrainlogSessionType session_type;
    TrainlogStatus status;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        session_id == NULL ||
        session_id[0] == '\0' ||
        (exercise_count > 0U && exercises == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status = validate_actual_set_weights(exercises, exercise_count);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status = trainlog_database_begin(database);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status = lookup_session_row_id(
        database,
        session_id,
        &session_row_id
    );

    if (status != TRAINLOG_STATUS_OK) {
        (void)trainlog_database_rollback(database);
        return status;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "SELECT session_type FROM sessions WHERE id=?1;",
        -1, &statement, NULL
    );
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(statement, 1, session_row_id);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
    }
    if (rc != SQLITE_ROW ||
        !session_type_from_sql(
            (const char *)sqlite3_column_text(statement, 0),
            &session_type)) {
        (void)sqlite3_finalize(statement);
        (void)trainlog_database_rollback(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    rc = sqlite3_finalize(statement);
    statement = NULL;
    if (rc != SQLITE_OK) {
        (void)trainlog_database_rollback(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "DELETE FROM session_exercises "
        "WHERE session_row_id = ?1;",
        -1,
        &statement,
        NULL
    );

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(
            statement,
            1,
            session_row_id
        );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        (void)trainlog_database_rollback(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        (void)trainlog_database_rollback(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    statement = NULL;

    status = insert_session_children(
        database,
        session_row_id,
        session_type,
        exercises,
        exercise_count
    );

    if (status != TRAINLOG_STATUS_OK) {
        (void)trainlog_database_rollback(database);
        return status;
    }

    status = trainlog_database_commit(database);

    if (status != TRAINLOG_STATUS_OK) {
        (void)trainlog_database_rollback(database);
    }

    return status;
}

TrainlogStatus trainlog_database_list_sessions(
    TrainlogDatabase *database,
    TrainlogSessionSummary *output,
    size_t capacity,
    size_t *output_count
)
{
    static const char *const SQL =
        "SELECT "
        "s.session_id, "
        "s.started_at, "
        "COALESCE(s.ended_at, ''), "
        "s.session_type, "
        "COUNT(se.id) "
        "FROM sessions AS s "
        "LEFT JOIN session_exercises AS se "
        "ON se.session_row_id = s.id "
        "GROUP BY s.id "
        "ORDER BY s.started_at DESC, s.id DESC;";

    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        output_count == NULL ||
        (capacity > 0U && output == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        SQL,
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < capacity) {
            const unsigned char *id =
                sqlite3_column_text(statement, 0);

            const unsigned char *started =
                sqlite3_column_text(statement, 1);

            const unsigned char *ended =
                sqlite3_column_text(statement, 2);

            const unsigned char *session_type =
                sqlite3_column_text(statement, 3);

            sqlite3_int64 exercise_count =
                sqlite3_column_int64(statement, 4);

            if (id == NULL ||
                started == NULL ||
                ended == NULL ||
                session_type == NULL ||
                exercise_count < 0) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)snprintf(
                output[count].session_id,
                sizeof(output[count].session_id),
                "%s",
                (const char *)id
            );

            (void)snprintf(
                output[count].started_at,
                sizeof(output[count].started_at),
                "%s",
                (const char *)started
            );

            (void)snprintf(
                output[count].ended_at,
                sizeof(output[count].ended_at),
                "%s",
                (const char *)ended
            );

            if (!session_type_from_sql(
                    (const char *)session_type,
                    &output[count].session_type
                )) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            output[count].exercise_count =
                (size_t)exercise_count;
        }

        ++count;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_count =
        count < capacity
            ? count
            : capacity;

    return TRAINLOG_STATUS_OK;
}

static int bind_optional_double(
    sqlite3_stmt *statement,
    int index,
    bool present,
    double value
)
{
    return present
        ? sqlite3_bind_double(statement, index, value)
        : sqlite3_bind_null(statement, index);
}

TrainlogStatus trainlog_database_insert_body_observation(
    TrainlogDatabase *database,
    const TrainlogBodyObservationInput *observation
)
{
    static const char *const SQL =
        "INSERT INTO body_observations("
        "observation_id, observed_at, session_row_id, body_weight_kg, "
        "neck_cm, shoulders_cm, chest_cm, waist_cm, hips_cm, "
        "left_arm_cm, right_arm_cm, left_forearm_cm, right_forearm_cm, "
        "left_thigh_cm, right_thigh_cm, left_calf_cm, right_calf_cm, notes"
        ") VALUES("
        "?1, ?2, "
        "(SELECT id FROM sessions WHERE session_id = ?3), "
        "?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18"
        ");";
    sqlite3_stmt *statement = NULL;
    bool any_metric;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        observation == NULL ||
        observation->observation_id[0] == '\0' ||
        observation->observed_at[0] == '\0') {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    any_metric =
        observation->has_body_weight ||
        observation->has_neck ||
        observation->has_shoulders ||
        observation->has_chest ||
        observation->has_waist ||
        observation->has_hips ||
        observation->has_left_arm ||
        observation->has_right_arm ||
        observation->has_left_forearm ||
        observation->has_right_forearm ||
        observation->has_left_thigh ||
        observation->has_right_thigh ||
        observation->has_left_calf ||
        observation->has_right_calf;

    if (!any_metric) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_text(
        statement,
        1,
        observation->observation_id,
        -1,
        SQLITE_TRANSIENT
    );
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            2,
            observation->observed_at,
            -1,
            SQLITE_TRANSIENT
        );
    }

    if (rc == SQLITE_OK) {
        rc = observation->session_id != NULL &&
             observation->session_id[0] != '\0'
            ? sqlite3_bind_text(
                statement,
                3,
                observation->session_id,
                -1,
                SQLITE_TRANSIENT
            )
            : sqlite3_bind_null(statement, 3);
    }

#define BIND_METRIC(index_, flag_, value_)                                   \
    do {                                                                     \
        if (rc == SQLITE_OK) {                                               \
            rc = bind_optional_double(                                       \
                statement,                                                   \
                (index_),                                                    \
                (flag_),                                                     \
                (value_)                                                     \
            );                                                               \
        }                                                                    \
    } while (0)

    BIND_METRIC(4, observation->has_body_weight, observation->body_weight_kg);
    BIND_METRIC(5, observation->has_neck, observation->neck_cm);
    BIND_METRIC(6, observation->has_shoulders, observation->shoulders_cm);
    BIND_METRIC(7, observation->has_chest, observation->chest_cm);
    BIND_METRIC(8, observation->has_waist, observation->waist_cm);
    BIND_METRIC(9, observation->has_hips, observation->hips_cm);
    BIND_METRIC(10, observation->has_left_arm, observation->left_arm_cm);
    BIND_METRIC(11, observation->has_right_arm, observation->right_arm_cm);
    BIND_METRIC(
        12,
        observation->has_left_forearm,
        observation->left_forearm_cm
    );
    BIND_METRIC(
        13,
        observation->has_right_forearm,
        observation->right_forearm_cm
    );
    BIND_METRIC(14, observation->has_left_thigh, observation->left_thigh_cm);
    BIND_METRIC(15, observation->has_right_thigh, observation->right_thigh_cm);
    BIND_METRIC(16, observation->has_left_calf, observation->left_calf_cm);
    BIND_METRIC(17, observation->has_right_calf, observation->right_calf_cm);

#undef BIND_METRIC

    if (rc == SQLITE_OK) {
        rc = observation->notes != NULL && observation->notes[0] != '\0'
            ? sqlite3_bind_text(
                statement,
                18,
                observation->notes,
                -1,
                SQLITE_TRANSIENT
            )
            : sqlite3_bind_null(statement, 18);
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_CONSTRAINT) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }
    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_list_weight_points(
    TrainlogDatabase *database,
    TrainlogWeightPoint *output,
    size_t capacity,
    size_t *output_count
)
{
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        output_count == NULL ||
        (capacity > 0U && output == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        "SELECT observed_at, body_weight_kg "
        "FROM body_observations "
        "WHERE body_weight_kg IS NOT NULL "
        "ORDER BY observed_at ASC, id ASC;",
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < capacity) {
            const unsigned char *observed = sqlite3_column_text(statement, 0);
            if (observed == NULL) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)snprintf(
                output[count].observed_at,
                sizeof(output[count].observed_at),
                "%s",
                (const char *)observed
            );
            output[count].body_weight_kg =
                sqlite3_column_double(statement, 1);
        }
        ++count;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_count = count < capacity ? count : capacity;
    return TRAINLOG_STATUS_OK;
}

/* TRAINLOG_SESSION_DETAILS_IMPLEMENTATION */

static TrainlogLoadMode detail_load_mode_from_text(const char *text)
{
    if (text != NULL && strcmp(text, "external") == 0) {
        return TRAINLOG_LOAD_EXTERNAL;
    }

    if (text != NULL && strcmp(text, "assistance") == 0) {
        return TRAINLOG_LOAD_ASSISTANCE;
    }

    return TRAINLOG_LOAD_NONE;
}

static TrainlogStatus detail_append_text(
    char *output,
    size_t output_size,
    size_t *used,
    const char *text
)
{
    size_t remaining;
    int written;

    if (output == NULL ||
        used == NULL ||
        text == NULL ||
        *used >= output_size) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    remaining = output_size - *used;

    written = snprintf(
        output + *used,
        remaining,
        "%s",
        text
    );

    if (written < 0) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if ((size_t)written >= remaining) {
        if (output_size >= 4U) {
            output[output_size - 4U] = '.';
            output[output_size - 3U] = '.';
            output[output_size - 2U] = '.';
            output[output_size - 1U] = '\0';
        }

        *used = output_size - 1U;
        return TRAINLOG_STATUS_OK;
    }

    *used += (size_t)written;
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus detail_fill_sets(
    TrainlogDatabase *database,
    sqlite3_int64 session_exercise_row_id,
    TrainlogPersistedExerciseDetail *detail
)
{
    static const char *const SQL =
        "SELECT reps, duration_seconds, weight_kg "
        "FROM performed_sets "
        "WHERE session_exercise_row_id = ?1 "
        "ORDER BY position ASC;";

    sqlite3_stmt *statement = NULL;
    sqlite3_stmt *count_statement = NULL;
    size_t used = 0U;
    size_t count = 0U;
    int rc;

    rc = sqlite3_prepare_v2(database->connection,
        "SELECT COUNT(*) FROM performed_sets "
        "WHERE session_exercise_row_id = ?1;", -1, &count_statement, NULL);
    if (rc != SQLITE_OK ||
        sqlite3_bind_int64(count_statement, 1, session_exercise_row_id) !=
            SQLITE_OK ||
        sqlite3_step(count_statement) != SQLITE_ROW) {
        (void)sqlite3_finalize(count_statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    {
        sqlite3_int64 persisted_count = sqlite3_column_int64(count_statement, 0);
        if (persisted_count < 0 ||
            (uint64_t)persisted_count >
                (uint64_t)(SIZE_MAX / sizeof(*detail->actual_sets))) {
            (void)sqlite3_finalize(count_statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        count = (size_t)persisted_count;
    }
    if (sqlite3_finalize(count_statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    count_statement = NULL;

    /* WHY: performed_sets has no schema/domain maximum. Exact occurrence-owned
     * storage bounds memory to the selected session without limiting history. */
    if (count > 0U) {
        detail->actual_sets = calloc(count, sizeof(*detail->actual_sets));
        if (detail->actual_sets == NULL) {
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    }
    detail->actual_set_count = count;
    count = 0U;

    rc = sqlite3_prepare_v2(
        database->connection,
        SQL,
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        free(detail->actual_sets);
        detail->actual_sets = NULL;
        detail->actual_set_count = 0U;
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_int64(
        statement,
        1,
        session_exercise_row_id
    );
    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        free(detail->actual_sets);
        detail->actual_sets = NULL;
        detail->actual_set_count = 0U;
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    detail->actual_summary[0] = '\0';

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        char fragment[96];
        int written;
        int has_reps =
            sqlite3_column_type(statement, 0) != SQLITE_NULL;
        int has_duration =
            sqlite3_column_type(statement, 1) != SQLITE_NULL;
        int has_weight =
            sqlite3_column_type(statement, 2) != SQLITE_NULL;

        /* INVARIANT: COUNT and SELECT observe the same connection operation;
         * any mismatch is corruption/concurrent mutation, never truncation. */
        if (count >= detail->actual_set_count) {
            (void)sqlite3_finalize(statement);
            free(detail->actual_sets);
            detail->actual_sets = NULL;
            detail->actual_set_count = 0U;
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        /* INVARIANT: array index is persisted position order. Nullable load
         * stays explicit instead of inheriting occurrence target metadata. */
        detail->actual_sets[count].reps =
            has_reps != 0
                ? sqlite3_column_int(statement, 0)
                : 0;
        detail->actual_sets[count].duration_seconds =
            has_duration != 0
                ? sqlite3_column_int(statement, 1)
                : 0;
        detail->actual_sets[count].has_weight =
            has_weight != 0;
        detail->actual_sets[count].weight_kg =
            has_weight != 0
                ? sqlite3_column_double(statement, 2)
                : 0.0;

        if (count > 0U) {
            TrainlogStatus status = detail_append_text(
                detail->actual_summary,
                sizeof(detail->actual_summary),
                &used,
                " / "
            );

            if (status != TRAINLOG_STATUS_OK) {
                (void)sqlite3_finalize(statement);
                free(detail->actual_sets);
                detail->actual_sets = NULL;
                detail->actual_set_count = 0U;
                return status;
            }
        }

        if (has_reps != 0) {
            int reps = sqlite3_column_int(statement, 0);

            if (has_weight != 0) {
                written = snprintf(
                    fragment,
                    sizeof(fragment),
                    "%d@%.1f",
                    reps,
                    sqlite3_column_double(statement, 2)
                );
            } else {
                written = snprintf(
                    fragment,
                    sizeof(fragment),
                    "%d",
                    reps
                );
            }
        } else if (has_duration != 0) {
            int duration = sqlite3_column_int(statement, 1);

            char duration_text[64];

            if (trainlog_duration_format(
                    duration,
                    duration_text,
                    sizeof(duration_text)
                ) != TRAINLOG_STATUS_OK) {
                (void)sqlite3_finalize(statement);
                free(detail->actual_sets);
                detail->actual_sets = NULL;
                detail->actual_set_count = 0U;
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            if (has_weight != 0) {
                written = snprintf(
                    fragment,
                    sizeof(fragment),
                    "%s@%.1f",
                    duration_text,
                    sqlite3_column_double(statement, 2)
                );
            } else {
                written = snprintf(
                    fragment,
                    sizeof(fragment),
                    "%s",
                    duration_text
                );
            }
        } else {
            (void)sqlite3_finalize(statement);
            free(detail->actual_sets);
            detail->actual_sets = NULL;
            detail->actual_set_count = 0U;
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        if (written < 0 ||
            (size_t)written >= sizeof(fragment)) {
            (void)sqlite3_finalize(statement);
            free(detail->actual_sets);
            detail->actual_sets = NULL;
            detail->actual_set_count = 0U;
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        {
            TrainlogStatus status = detail_append_text(
                detail->actual_summary,
                sizeof(detail->actual_summary),
                &used,
                fragment
            );

            if (status != TRAINLOG_STATUS_OK) {
                (void)sqlite3_finalize(statement);
                free(detail->actual_sets);
                detail->actual_sets = NULL;
                detail->actual_set_count = 0U;
                return status;
            }
        }

        ++count;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        free(detail->actual_sets);
        detail->actual_sets = NULL;
        detail->actual_set_count = 0U;
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        free(detail->actual_sets);
        detail->actual_sets = NULL;
        detail->actual_set_count = 0U;
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (count != detail->actual_set_count) {
        free(detail->actual_sets);
        detail->actual_sets = NULL;
        detail->actual_set_count = 0U;
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (count == 0U) {
        (void)snprintf(
            detail->actual_summary,
            sizeof(detail->actual_summary),
            "%s",
            "aucune série réalisée"
        );
    }

    return TRAINLOG_STATUS_OK;
}

void trainlog_database_free_session_details(
    TrainlogPersistedExerciseDetail *exercises,
    size_t exercise_count
)
{
    size_t index;

    if (exercises == NULL) {
        return;
    }
    for (index = 0U; index < exercise_count; ++index) {
        free(exercises[index].actual_sets);
        exercises[index].actual_sets = NULL;
        exercises[index].actual_set_count = 0U;
    }
}

TrainlogStatus trainlog_database_get_session_details(
    TrainlogDatabase *database,
    const char *session_id,
    TrainlogSessionSummary *output_session,
    TrainlogPersistedExerciseDetail *output_exercises,
    size_t exercise_capacity,
    size_t *output_exercise_count
)
{
    static const char *const HEADER_SQL =
        "SELECT started_at, COALESCE(ended_at, ''), session_type "
        "FROM sessions "
        "WHERE session_id = ?1;";

    static const char *const EXERCISE_SQL =
        "SELECT "
        "e.name, e.tracking_mode, "
        "se.recording_mode, se.data_fields, "
        "se.load_mode, se.rest_seconds, "
        "COALESCE(se.target_sets, 0), "
        "COALESCE(se.target_reps, 0), "
        "COALESCE(se.target_duration_seconds, 0), "
        "se.target_weight_kg, "
        "ca.duration_seconds, ca.speed_kmh, ca.distance_km, "
        "mr.max_weight_kg, "
        "se.equipment_id, "
        "se.entry_id, "
        "se.id "
        "FROM session_exercises AS se "
        "JOIN sessions AS s "
        "  ON s.id = se.session_row_id "
        "JOIN exercises AS e "
        "  ON e.id = se.exercise_row_id "
        "LEFT JOIN continuous_activity AS ca "
        "  ON ca.session_exercise_row_id = se.id "
        "LEFT JOIN max_results AS mr "
        "  ON mr.session_exercise_row_id = se.id "
        "WHERE s.session_id = ?1 "
        "ORDER BY se.position ASC;";

    sqlite3_stmt *header = NULL;
    sqlite3_stmt *exercises = NULL;
    size_t copied = 0U;
    size_t total = 0U;
    int rc;

    if (output_exercise_count != NULL) {
        *output_exercise_count = 0U;
    }

    if (database == NULL ||
        database->connection == NULL ||
        session_id == NULL ||
        session_id[0] == '\0' ||
        output_session == NULL ||
        output_exercise_count == NULL ||
        (exercise_capacity > 0U &&
         output_exercises == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(
        output_session,
        0,
        sizeof(*output_session)
    );

    rc = sqlite3_prepare_v2(
        database->connection,
        HEADER_SQL,
        -1,
        &header,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_text(
        header,
        1,
        session_id,
        -1,
        SQLITE_TRANSIENT
    );

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(header);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(header);

    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(header);
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(header);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    {
        const unsigned char *started =
            sqlite3_column_text(header, 0);

        const unsigned char *ended =
            sqlite3_column_text(header, 1);

        const unsigned char *session_type =
            sqlite3_column_text(header, 2);

        if (started == NULL ||
            ended == NULL ||
            session_type == NULL) {
            (void)sqlite3_finalize(header);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        (void)snprintf(
            output_session->session_id,
            sizeof(output_session->session_id),
            "%s",
            session_id
        );

        (void)snprintf(
            output_session->started_at,
            sizeof(output_session->started_at),
            "%s",
            (const char *)started
        );

        (void)snprintf(
            output_session->ended_at,
            sizeof(output_session->ended_at),
            "%s",
            (const char *)ended
        );

        if (!session_type_from_sql(
                (const char *)session_type,
                &output_session->session_type
            )) {
            (void)sqlite3_finalize(header);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }

    if (sqlite3_finalize(header) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        EXERCISE_SQL,
        -1,
        &exercises,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_text(
        exercises,
        1,
        session_id,
        -1,
        SQLITE_TRANSIENT
    );

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(exercises);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(exercises)) == SQLITE_ROW) {
        if (copied < exercise_capacity) {
            TrainlogPersistedExerciseDetail *detail =
                &output_exercises[copied];

            const unsigned char *name =
                sqlite3_column_text(exercises, 0);

            const unsigned char *tracking =
                sqlite3_column_text(exercises, 1);

            const unsigned char *recording =
                sqlite3_column_text(exercises, 2);

            sqlite3_int64 data_fields =
                sqlite3_column_int64(exercises, 3);

            const unsigned char *load =
                sqlite3_column_text(exercises, 4);

            sqlite3_int64 session_exercise_row_id =
                sqlite3_column_int64(exercises, 16);
            const unsigned char *equipment_id =
                sqlite3_column_text(exercises, 14);
            const unsigned char *entry_id = sqlite3_column_text(exercises, 15);

            TrainlogStatus status;

            if (name == NULL ||
                tracking == NULL ||
                recording == NULL ||
                load == NULL ||
                data_fields < 0 ||
                (uint64_t)data_fields >
                    (uint64_t)UINT32_MAX ||
                (((TrainlogExerciseDataFields)data_fields) &
                 ~TRAINLOG_EXERCISE_DATA_KNOWN_MASK) != 0U) {
                (void)sqlite3_finalize(exercises);
                trainlog_database_free_session_details(output_exercises, copied);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)memset(
                detail,
                0,
                sizeof(*detail)
            );

            if (equipment_id != NULL) {
                (void)snprintf(detail->equipment_id,
                               sizeof(detail->equipment_id), "%s",
                               (const char *)equipment_id);
            }
            if (entry_id == NULL) {
                (void)sqlite3_finalize(exercises);
                trainlog_database_free_session_details(output_exercises, copied);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            (void)snprintf(detail->entry_id, sizeof(detail->entry_id), "%s",
                           (const char *)entry_id);

            (void)snprintf(
                detail->name,
                sizeof(detail->name),
                "%s",
                (const char *)name
            );

            detail->tracking_mode =
                strcmp(
                    (const char *)tracking,
                    "duration"
                ) == 0
                    ? TRAINLOG_TRACKING_DURATION
                    : TRAINLOG_TRACKING_REPS;

            detail->recording_mode =
                recording_mode_from_sql(
                    (const char *)recording
                );

            detail->data_fields =
                (TrainlogExerciseDataFields)
                    data_fields;

            detail->load_mode =
                detail_load_mode_from_text(
                    (const char *)load
                );

            detail->rest_seconds =
                sqlite3_column_int(
                    exercises,
                    5
                );

            detail->target_sets =
                sqlite3_column_int(
                    exercises,
                    6
                );

            detail->target_reps =
                sqlite3_column_int(
                    exercises,
                    7
                );

            detail->target_duration_seconds =
                sqlite3_column_int(
                    exercises,
                    8
                );

            detail->has_target_weight =
                sqlite3_column_type(
                    exercises,
                    9
                ) != SQLITE_NULL;

            detail->target_weight_kg =
                detail->has_target_weight != 0
                    ? sqlite3_column_double(
                        exercises,
                        9
                    )
                    : 0.0;

            detail->has_max_weight =
                sqlite3_column_type(exercises, 13) != SQLITE_NULL;
            detail->max_weight_kg =
                detail->has_max_weight != 0
                    ? sqlite3_column_double(exercises, 13)
                    : 0.0;

            if (detail->has_max_weight != 0) {
                detail->actual_set_count = 0U;
                (void)snprintf(
                    detail->actual_summary,
                    sizeof(detail->actual_summary),
                    "Max %.2f kg",
                    detail->max_weight_kg
                );
            } else if (detail->recording_mode ==
                TRAINLOG_RECORDING_CONTINUOUS) {
                bool speed_required =
                    (detail->data_fields &
                     TRAINLOG_EXERCISE_DATA_SPEED_KMH) != 0U;

                bool distance_required =
                    (detail->data_fields &
                     TRAINLOG_EXERCISE_DATA_DISTANCE_KM) != 0U;

                if (sqlite3_column_type(
                        exercises,
                        10
                    ) == SQLITE_NULL) {
                    (void)sqlite3_finalize(exercises);
                    trainlog_database_free_session_details(output_exercises, copied);
                    return TRAINLOG_STATUS_DATABASE_ERROR;
                }

                detail->continuous_duration_seconds =
                    sqlite3_column_int(
                        exercises,
                        10
                    );

                detail->has_continuous_speed =
                    sqlite3_column_type(
                        exercises,
                        11
                    ) != SQLITE_NULL;

                detail->continuous_speed_kmh =
                    detail->has_continuous_speed != 0
                        ? sqlite3_column_double(
                            exercises,
                            11
                        )
                        : 0.0;

                detail->has_continuous_distance =
                    sqlite3_column_type(
                        exercises,
                        12
                    ) != SQLITE_NULL;

                detail->continuous_distance_km =
                    detail->has_continuous_distance != 0
                        ? sqlite3_column_double(
                            exercises,
                            12
                        )
                        : 0.0;

                if ((detail->has_continuous_speed != 0) !=
                        speed_required ||
                    (detail->has_continuous_distance != 0) !=
                        distance_required) {
                    (void)sqlite3_finalize(exercises);
                    trainlog_database_free_session_details(output_exercises, copied);
                    return TRAINLOG_STATUS_DATABASE_ERROR;
                }

                detail->actual_set_count = 0U;

                (void)snprintf(
                    detail->actual_summary,
                    sizeof(detail->actual_summary),
                    "%s",
                    "activité continue"
                );
            } else {
                status = detail_fill_sets(
                    database,
                    session_exercise_row_id,
                    detail
                );

                if (status != TRAINLOG_STATUS_OK) {
                    (void)sqlite3_finalize(exercises);
                    trainlog_database_free_session_details(output_exercises, copied);
                    return status;
                }
            }

            ++copied;
        }

        ++total;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(exercises);
        trainlog_database_free_session_details(output_exercises, copied);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(exercises) != SQLITE_OK) {
        trainlog_database_free_session_details(output_exercises, copied);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    output_session->exercise_count = total;
    *output_exercise_count = copied;

    return TRAINLOG_STATUS_OK;
}

/* TRAINLOG_BODY_METRIC_HISTORY_IMPLEMENTATION */

static const char *body_metric_column(TrainlogBodyMetric metric)
{
    switch (metric) {
    case TRAINLOG_BODY_METRIC_WEIGHT:
        return "body_weight_kg";
    case TRAINLOG_BODY_METRIC_NECK:
        return "neck_cm";
    case TRAINLOG_BODY_METRIC_SHOULDERS:
        return "shoulders_cm";
    case TRAINLOG_BODY_METRIC_CHEST:
        return "chest_cm";
    case TRAINLOG_BODY_METRIC_WAIST:
        return "waist_cm";
    case TRAINLOG_BODY_METRIC_HIPS:
        return "hips_cm";
    case TRAINLOG_BODY_METRIC_LEFT_ARM:
        return "left_arm_cm";
    case TRAINLOG_BODY_METRIC_RIGHT_ARM:
        return "right_arm_cm";
    case TRAINLOG_BODY_METRIC_LEFT_FOREARM:
        return "left_forearm_cm";
    case TRAINLOG_BODY_METRIC_RIGHT_FOREARM:
        return "right_forearm_cm";
    case TRAINLOG_BODY_METRIC_LEFT_THIGH:
        return "left_thigh_cm";
    case TRAINLOG_BODY_METRIC_RIGHT_THIGH:
        return "right_thigh_cm";
    case TRAINLOG_BODY_METRIC_LEFT_CALF:
        return "left_calf_cm";
    case TRAINLOG_BODY_METRIC_RIGHT_CALF:
        return "right_calf_cm";
    case TRAINLOG_BODY_METRIC_COUNT:
    default:
        return NULL;
    }
}

TrainlogStatus trainlog_database_list_body_metric_points(
    TrainlogDatabase *database,
    TrainlogBodyMetric metric,
    TrainlogBodyMetricPoint *output,
    size_t capacity,
    size_t *output_count
)
{
    const char *column;
    char sql[256];
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int written;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        output_count == NULL ||
        (capacity > 0U && output == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    column = body_metric_column(metric);
    if (column == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    written = snprintf(
        sql,
        sizeof(sql),
        "SELECT observed_at, %s "
        "FROM body_observations "
        "WHERE %s IS NOT NULL "
        "ORDER BY observed_at ASC, id ASC;",
        column,
        column
    );

    if (written < 0 ||
        (size_t)written >= sizeof(sql)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        sql,
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < capacity) {
            const unsigned char *observed =
                sqlite3_column_text(statement, 0);

            if (observed == NULL) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)snprintf(
                output[count].observed_at,
                sizeof(output[count].observed_at),
                "%s",
                (const char *)observed
            );

            output[count].value =
                sqlite3_column_double(statement, 1);
        }

        ++count;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_count =
        count < capacity
            ? count
            : capacity;

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_latest_body_pair(
    TrainlogDatabase *database,
    TrainlogBodyMetric left_metric,
    TrainlogBodyMetric right_metric,
    TrainlogBodyPairPoint *output
)
{
    const char *left_column;
    const char *right_column;
    char sql[384];
    sqlite3_stmt *statement = NULL;
    int written;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        output == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    left_column = body_metric_column(left_metric);
    right_column = body_metric_column(right_metric);

    if (left_column == NULL ||
        right_column == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(output, 0, sizeof(*output));

    written = snprintf(
        sql,
        sizeof(sql),
        "SELECT observed_at, %s, %s "
        "FROM body_observations "
        "WHERE %s IS NOT NULL AND %s IS NOT NULL "
        "ORDER BY observed_at DESC, id DESC "
        "LIMIT 1;",
        left_column,
        right_column,
        left_column,
        right_column
    );

    if (written < 0 ||
        (size_t)written >= sizeof(sql)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        sql,
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        output->found = false;
        return TRAINLOG_STATUS_OK;
    }

    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    {
        const unsigned char *observed =
            sqlite3_column_text(statement, 0);

        if (observed == NULL) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        output->found = true;

        (void)snprintf(
            output->observed_at,
            sizeof(output->observed_at),
            "%s",
            (const char *)observed
        );

        output->left_value =
            sqlite3_column_double(statement, 1);

        output->right_value =
            sqlite3_column_double(statement, 2);
    }

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

/* TRAINLOG_EXERCISE_PERFORMANCE_IMPLEMENTATION */

static TrainlogLoadMode performance_load_mode_from_sql(
    const char *text
)
{
    if (text != NULL &&
        strcmp(text, "external") == 0) {
        return TRAINLOG_LOAD_EXTERNAL;
    }

    if (text != NULL &&
        strcmp(text, "assistance") == 0) {
        return TRAINLOG_LOAD_ASSISTANCE;
    }

    return TRAINLOG_LOAD_NONE;
}

static bool performance_candidate_better(
    const TrainlogExercisePerformancePoint *current,
    TrainlogLoadMode load_mode,
    int metric_value,
    int has_weight,
    double weight_kg
)
{
    if (current == NULL ||
        metric_value <= 0) {
        return false;
    }

    if (current->has_performance == 0) {
        return true;
    }

    switch (load_mode) {
    case TRAINLOG_LOAD_EXTERNAL:
        if (has_weight == 0) {
            return false;
        }

        if (weight_kg > current->weight_kg) {
            return true;
        }

        return
            weight_kg == current->weight_kg &&
            metric_value > current->metric_value;

    case TRAINLOG_LOAD_ASSISTANCE:
        if (has_weight == 0) {
            return false;
        }

        if (weight_kg < current->weight_kg) {
            return true;
        }

        return
            weight_kg == current->weight_kg &&
            metric_value > current->metric_value;

    case TRAINLOG_LOAD_NONE:
    default:
        return metric_value > current->metric_value;
    }
}

/* TRAINLOG_MEASURED_MAX_SESSION_TYPE_V1 */
TrainlogStatus trainlog_database_list_exercise_performance(
    TrainlogDatabase *database,
    const char *exercise_id,
    TrainlogExercisePerformancePoint *output,
    size_t capacity,
    size_t *output_count
)
{
    static const char *const SQL =
        "SELECT "
        "s.session_id, "
        "s.started_at, "
        "s.session_type, "
        "e.tracking_mode, "
        "se.load_mode, "
        "ps.id, "
        "ps.reps, "
        "ps.duration_seconds, "
        "ps.weight_kg, "
        "mr.max_weight_kg, "
        "COALESCE(se.equipment_id, '') "
        "FROM session_exercises AS se "
        "JOIN sessions AS s "
        "  ON s.id = se.session_row_id "
        "JOIN exercises AS e "
        "  ON e.id = se.exercise_row_id "
        "LEFT JOIN performed_sets AS ps "
        "  ON ps.session_exercise_row_id = se.id "
        "LEFT JOIN max_results AS mr "
        "  ON mr.session_exercise_row_id = se.id "
        "WHERE e.exercise_id = ?1 "
        "ORDER BY "
        "s.started_at DESC, "
        "s.id DESC, "
        "se.position ASC, ps.position ASC;";

    sqlite3_stmt *statement = NULL;
    TrainlogExercisePerformancePoint *current = NULL;
    char current_session_id[TRAINLOG_ID_MAX + 1U];
    size_t copied = 0U;
    int rc;

    if (
        database == NULL ||
        database->connection == NULL ||
        exercise_id == NULL ||
        exercise_id[0] == '\0' ||
        output_count == NULL ||
        (
            capacity > 0U &&
            output == NULL
        )
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = 0U;
    current_session_id[0] = '\0';

    rc = sqlite3_prepare_v2(
        database->connection,
        SQL,
        -1,
        &statement,
        NULL
    );

    if (rc != SQLITE_OK) {
        return
            TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_text(
        statement,
        1,
        exercise_id,
        -1,
        SQLITE_TRANSIENT
    );

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(
            statement
        );

        return
            TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while (
        (rc = sqlite3_step(statement)) ==
        SQLITE_ROW
    ) {
        const unsigned char *session_id =
            sqlite3_column_text(
                statement,
                0
            );

        const unsigned char *started_at =
            sqlite3_column_text(
                statement,
                1
            );

        const unsigned char *session_type =
            sqlite3_column_text(
                statement,
                2
            );

        const unsigned char *tracking_mode =
            sqlite3_column_text(
                statement,
                3
            );

        const unsigned char *load_mode =
            sqlite3_column_text(
                statement,
                4
            );

        bool new_session;
        const unsigned char *equipment_id =
            sqlite3_column_text(statement, 10);

        if (
            session_id == NULL ||
            started_at == NULL ||
            session_type == NULL ||
            tracking_mode == NULL ||
            load_mode == NULL
            || equipment_id == NULL
            || (size_t)sqlite3_column_bytes(statement, 10) > TRAINLOG_ID_MAX
        ) {
            (void)sqlite3_finalize(
                statement
            );

            return
                TRAINLOG_STATUS_DATABASE_ERROR;
        }

        new_session =
            current_session_id[0] == '\0' ||
            strcmp(
                current_session_id,
                (const char *)session_id
            ) != 0;

        if (new_session) {
            (void)snprintf(
                current_session_id,
                sizeof(current_session_id),
                "%s",
                (const char *)session_id
            );

            current = NULL;

            if (copied < capacity) {
                current =
                    &output[copied];

                (void)memset(
                    current,
                    0,
                    sizeof(*current)
                );

                (void)snprintf(
                    current->session_id,
                    sizeof(current->session_id),
                    "%s",
                    (const char *)session_id
                );

                (void)snprintf(
                    current->started_at,
                    sizeof(current->started_at),
                    "%s",
                    (const char *)started_at
                );

                if (
                    !session_type_from_sql(
                        (const char *)session_type,
                        &current->session_type
                    )
                ) {
                    (void)sqlite3_finalize(
                        statement
                    );

                    return
                        TRAINLOG_STATUS_DATABASE_ERROR;
                }

                current->tracking_mode =
                    tracking_mode_from_sql(
                        (const char *)tracking_mode
                    );

                current->load_mode =
                    performance_load_mode_from_sql(
                        (const char *)load_mode
                    );

                ++copied;
            }
        }

        if (current != NULL &&
            sqlite3_column_type(statement, 9) != SQLITE_NULL) {
            double max_weight = sqlite3_column_double(statement, 9);
            if (!isfinite(max_weight) || max_weight <= 0.0) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            /* Explicit max results participate in measured-max history without
             * synthesizing a rep/set count. Repeated movement occurrences in
             * one session reduce to the greatest recorded max. */
            if (current->has_performance == 0 ||
                max_weight > current->weight_kg) {
                current->load_mode = TRAINLOG_LOAD_EXTERNAL;
                current->has_performance = 1;
                current->has_explicit_max = 1;
                current->metric_value = 1;
                current->has_weight = 1;
                current->weight_kg = max_weight;
                (void)snprintf(current->equipment_id,
                    sizeof(current->equipment_id), "%s",
                    (const char *)equipment_id);
            }
        } else if (
            current != NULL &&
            sqlite3_column_type(
                statement,
                5
            ) != SQLITE_NULL
        ) {
            int metric_value;
            int has_weight;
            double weight_kg;

            ++current->actual_set_count;

            if (
                current->tracking_mode ==
                TRAINLOG_TRACKING_REPS
            ) {
                metric_value =
                    sqlite3_column_type(
                        statement,
                        6
                    ) != SQLITE_NULL
                        ? sqlite3_column_int(
                            statement,
                            6
                        )
                        : 0;
            } else {
                metric_value =
                    sqlite3_column_type(
                        statement,
                        7
                    ) != SQLITE_NULL
                        ? sqlite3_column_int(
                            statement,
                            7
                        )
                        : 0;
            }

            has_weight =
                sqlite3_column_type(
                    statement,
                    8
                ) != SQLITE_NULL;

            weight_kg =
                has_weight != 0
                    ? sqlite3_column_double(
                        statement,
                        8
                    )
                    : 0.0;

            if (
                performance_candidate_better(
                    current,
                    current->load_mode,
                    metric_value,
                    has_weight,
                    weight_kg
                )
            ) {
                current->has_performance = 1;
                current->has_explicit_max = 0;
                current->metric_value =
                    metric_value;
                current->has_weight =
                    has_weight;
                current->weight_kg =
                    weight_kg;
                (void)snprintf(current->equipment_id,
                    sizeof(current->equipment_id), "%s",
                    (const char *)equipment_id);
            }
        }
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(
            statement
        );

        return
            TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (
        sqlite3_finalize(
            statement
        ) != SQLITE_OK
    ) {
        return
            TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_count = copied;

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_load_session_editable(
    TrainlogDatabase *database,
    const char *session_id,
    TrainlogSessionSummary *output_session,
    TrainlogEditableExerciseRecord *output_exercises,
    size_t exercise_capacity,
    size_t *output_exercise_count,
    TrainlogSetInput *output_sets,
    size_t set_capacity,
    size_t *output_set_count
)
{
    static const char *const HEADER_SQL =
        "SELECT "
        "started_at, "
        "COALESCE(ended_at, ''), "
        "session_type "
        "FROM sessions "
        "WHERE session_id = ?1;";

    static const char *const EXERCISE_SQL =
        "SELECT "
        "se.id, "
        "e.exercise_id, "
        "e.name, "
        "e.tracking_mode, "
        "se.load_mode, "
        "se.rest_seconds, "
        "se.target_sets, "
        "COALESCE(se.target_reps, 0), "
        "COALESCE(se.target_duration_seconds, 0), "
        "se.target_weight_kg, "
        "COALESCE(se.notes, ''), "
        "COALESCE(se.equipment_id, ''), se.entry_id, mr.max_weight_kg, "
        "se.recording_mode, se.data_fields, "
        "ca.duration_seconds, ca.speed_kmh, ca.distance_km "
        "FROM session_exercises AS se "
        "JOIN sessions AS s "
        "ON s.id = se.session_row_id "
        "JOIN exercises AS e "
        "ON e.id = se.exercise_row_id "
        "LEFT JOIN max_results AS mr "
        "ON mr.session_exercise_row_id = se.id "
        "LEFT JOIN continuous_activity AS ca "
        "ON ca.session_exercise_row_id = se.id "
        "WHERE s.session_id = ?1 "
        "ORDER BY se.position ASC;";

    static const char *const SET_SQL =
        "SELECT reps, duration_seconds, weight_kg "
        "FROM performed_sets "
        "WHERE session_exercise_row_id = ?1 "
        "ORDER BY position ASC;";

    sqlite3_stmt *header = NULL;
    sqlite3_stmt *exercise_statement = NULL;
    sqlite3_stmt *set_statement = NULL;
    size_t exercise_count = 0U;
    size_t set_count = 0U;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        session_id == NULL ||
        session_id[0] == '\0' ||
        output_session == NULL ||
        output_exercise_count == NULL ||
        output_set_count == NULL ||
        (exercise_capacity > 0U &&
         output_exercises == NULL) ||
        (set_capacity > 0U &&
         output_sets == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_exercise_count = 0U;
    *output_set_count = 0U;
    (void)memset(
        output_session,
        0,
        sizeof(*output_session)
    );

    rc = sqlite3_prepare_v2(
        database->connection,
        HEADER_SQL,
        -1,
        &header,
        NULL
    );

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            header,
            1,
            session_id,
            -1,
            SQLITE_TRANSIENT
        );
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(header);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(header);

    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(header);
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(header);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    {
        const unsigned char *started =
            sqlite3_column_text(header, 0);

        const unsigned char *ended =
            sqlite3_column_text(header, 1);

        const unsigned char *session_type =
            sqlite3_column_text(header, 2);

        if (started == NULL ||
            ended == NULL ||
            session_type == NULL) {
            (void)sqlite3_finalize(header);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        (void)snprintf(
            output_session->session_id,
            sizeof(output_session->session_id),
            "%s",
            session_id
        );

        (void)snprintf(
            output_session->started_at,
            sizeof(output_session->started_at),
            "%s",
            (const char *)started
        );

        (void)snprintf(
            output_session->ended_at,
            sizeof(output_session->ended_at),
            "%s",
            (const char *)ended
        );

        if (!session_type_from_sql(
                (const char *)session_type,
                &output_session->session_type
            )) {
            (void)sqlite3_finalize(header);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }

    if (sqlite3_finalize(header) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    header = NULL;

    rc = sqlite3_prepare_v2(
        database->connection,
        EXERCISE_SQL,
        -1,
        &exercise_statement,
        NULL
    );

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            exercise_statement,
            1,
            session_id,
            -1,
            SQLITE_TRANSIENT
        );
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(exercise_statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(exercise_statement)) == SQLITE_ROW) {
        TrainlogEditableExerciseRecord *record;
        sqlite3_int64 session_exercise_row_id;
        const unsigned char *exercise_id;
        const unsigned char *name;
        const unsigned char *tracking;
        const unsigned char *load;
        const unsigned char *notes;
        int note_bytes;

        if (exercise_count >= exercise_capacity) {
            (void)sqlite3_finalize(exercise_statement);
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        record =
            &output_exercises[exercise_count];

        session_exercise_row_id =
            sqlite3_column_int64(
                exercise_statement,
                0
            );

        exercise_id =
            sqlite3_column_text(
                exercise_statement,
                1
            );

        name =
            sqlite3_column_text(
                exercise_statement,
                2
            );

        tracking =
            sqlite3_column_text(
                exercise_statement,
                3
            );

        load =
            sqlite3_column_text(
                exercise_statement,
                4
            );

        notes =
            sqlite3_column_text(
                exercise_statement,
                10
            );

        note_bytes =
            sqlite3_column_bytes(
                exercise_statement,
                10
            );

        if (exercise_id == NULL ||
            name == NULL ||
            tracking == NULL ||
            load == NULL ||
            notes == NULL ||
            note_bytes < 0 ||
            (size_t)note_bytes >
                TRAINLOG_NOTE_MAX) {
            (void)sqlite3_finalize(exercise_statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        (void)memset(
            record,
            0,
            sizeof(*record)
        );

        if (sqlite3_column_type(exercise_statement, 11) != SQLITE_NULL) {
            const unsigned char *equipment_id = sqlite3_column_text(exercise_statement, 11);
            if (equipment_id == NULL) {
                (void)sqlite3_finalize(exercise_statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            (void)snprintf(record->equipment_id, sizeof(record->equipment_id),
                           "%s", (const char *)equipment_id);
        }

        if (sqlite3_column_type(exercise_statement, 12) == SQLITE_NULL) {
            (void)sqlite3_finalize(exercise_statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        (void)snprintf(record->entry_id, sizeof(record->entry_id), "%s",
                       (const char *)sqlite3_column_text(exercise_statement, 12));

        (void)snprintf(
            record->exercise_id,
            sizeof(record->exercise_id),
            "%s",
            (const char *)exercise_id
        );

        (void)snprintf(
            record->name,
            sizeof(record->name),
            "%s",
            (const char *)name
        );

        record->tracking_mode =
            tracking_mode_from_sql(
                (const char *)tracking
            );

        record->recording_mode = recording_mode_from_sql(
            (const char *)sqlite3_column_text(exercise_statement, 14));
        {
            sqlite3_int64 data_fields =
                sqlite3_column_int64(exercise_statement, 15);
            if (data_fields < 0 ||
                (uint64_t)data_fields > (uint64_t)UINT32_MAX ||
                (((TrainlogExerciseDataFields)data_fields) &
                 ~TRAINLOG_EXERCISE_DATA_KNOWN_MASK) != 0U) {
                (void)sqlite3_finalize(exercise_statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            record->data_fields =
                (TrainlogExerciseDataFields)data_fields;
        }

        record->load_mode =
            detail_load_mode_from_text(
                (const char *)load
            );

        record->rest_seconds =
            sqlite3_column_int(
                exercise_statement,
                5
            );

        record->target_sets =
            sqlite3_column_int(
                exercise_statement,
                6
            );

        record->target_reps =
            sqlite3_column_int(
                exercise_statement,
                7
            );

        record->target_duration_seconds =
            sqlite3_column_int(
                exercise_statement,
                8
            );

        record->has_target_weight =
            sqlite3_column_type(
                exercise_statement,
                9
            ) != SQLITE_NULL;

        record->target_weight_kg =
            record->has_target_weight != 0
                ? sqlite3_column_double(
                    exercise_statement,
                    9
                )
                : 0.0;

        record->has_max_weight =
            sqlite3_column_type(exercise_statement, 13) != SQLITE_NULL;
        record->max_weight_kg =
            record->has_max_weight != 0
                ? sqlite3_column_double(exercise_statement, 13)
                : 0.0;

        /*
         * CONTRACT: persisted-session editing must round-trip a continuous
         * occurrence exactly; otherwise replacing a mixed max-test session
         * could discard or invalidate its warm-up entry.
         */
        if (sqlite3_column_type(exercise_statement, 16) != SQLITE_NULL) {
            record->continuous_duration_seconds =
                sqlite3_column_int(exercise_statement, 16);
            record->has_continuous_speed =
                sqlite3_column_type(exercise_statement, 17) != SQLITE_NULL;
            record->continuous_speed_kmh =
                record->has_continuous_speed != 0
                    ? sqlite3_column_double(exercise_statement, 17)
                    : 0.0;
            record->has_continuous_distance =
                sqlite3_column_type(exercise_statement, 18) != SQLITE_NULL;
            record->continuous_distance_km =
                record->has_continuous_distance != 0
                    ? sqlite3_column_double(exercise_statement, 18)
                    : 0.0;
        }

        (void)snprintf(
            record->notes,
            sizeof(record->notes),
            "%s",
            (const char *)notes
        );

        record->set_offset = set_count;

        rc = sqlite3_prepare_v2(
            database->connection,
            SET_SQL,
            -1,
            &set_statement,
            NULL
        );

        if (rc == SQLITE_OK) {
            rc = sqlite3_bind_int64(
                set_statement,
                1,
                session_exercise_row_id
            );
        }

        if (rc != SQLITE_OK) {
            (void)sqlite3_finalize(set_statement);
            (void)sqlite3_finalize(exercise_statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        while ((rc = sqlite3_step(set_statement)) == SQLITE_ROW) {
            TrainlogSetInput *set;
            int has_reps;
            int has_duration;

            if (set_count >= set_capacity) {
                (void)sqlite3_finalize(set_statement);
                (void)sqlite3_finalize(exercise_statement);
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }

            set = &output_sets[set_count];

            has_reps =
                sqlite3_column_type(
                    set_statement,
                    0
                ) != SQLITE_NULL;

            has_duration =
                sqlite3_column_type(
                    set_statement,
                    1
                ) != SQLITE_NULL;

            if (has_reps == has_duration) {
                (void)sqlite3_finalize(set_statement);
                (void)sqlite3_finalize(exercise_statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)memset(
                set,
                0,
                sizeof(*set)
            );

            if (has_reps != 0) {
                set->reps =
                    sqlite3_column_int(
                        set_statement,
                        0
                    );
            } else {
                set->duration_seconds =
                    sqlite3_column_int(
                        set_statement,
                        1
                    );
            }

            set->has_weight =
                sqlite3_column_type(
                    set_statement,
                    2
                ) != SQLITE_NULL;

            set->weight_kg =
                set->has_weight
                    ? sqlite3_column_double(
                        set_statement,
                        2
                    )
                    : 0.0;

            ++set_count;
            ++record->set_count;
        }

        if (rc != SQLITE_DONE) {
            (void)sqlite3_finalize(set_statement);
            (void)sqlite3_finalize(exercise_statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        if (sqlite3_finalize(set_statement) != SQLITE_OK) {
            (void)sqlite3_finalize(exercise_statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        set_statement = NULL;
        ++exercise_count;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(exercise_statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(exercise_statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    output_session->exercise_count =
        exercise_count;

    *output_exercise_count =
        exercise_count;

    *output_set_count =
        set_count;

    return TRAINLOG_STATUS_OK;
}

/* TRAINLOG_BODY_OBSERVATION_RECORD_IMPLEMENTATION */

static bool body_record_fill(
    sqlite3_stmt *statement,
    TrainlogBodyObservationRecord *record
)
{
    const unsigned char *observation_id;
    const unsigned char *observed_at;
    const unsigned char *session_id;
    const unsigned char *notes;
    int note_bytes;

    if (statement == NULL ||
        record == NULL) {
        return false;
    }

    observation_id =
        sqlite3_column_text(statement, 0);

    observed_at =
        sqlite3_column_text(statement, 1);

    session_id =
        sqlite3_column_text(statement, 2);

    notes =
        sqlite3_column_text(statement, 17);

    note_bytes =
        sqlite3_column_bytes(statement, 17);

    if (observation_id == NULL ||
        observed_at == NULL ||
        session_id == NULL ||
        notes == NULL ||
        note_bytes < 0 ||
        (size_t)note_bytes > TRAINLOG_NOTE_MAX) {
        return false;
    }

    (void)memset(
        record,
        0,
        sizeof(*record)
    );

    (void)snprintf(
        record->observation_id,
        sizeof(record->observation_id),
        "%s",
        (const char *)observation_id
    );

    (void)snprintf(
        record->observed_at,
        sizeof(record->observed_at),
        "%s",
        (const char *)observed_at
    );

    (void)snprintf(
        record->session_id,
        sizeof(record->session_id),
        "%s",
        (const char *)session_id
    );

#define READ_BODY_VALUE(column_, flag_, value_)                              \
    do {                                                                     \
        (flag_) =                                                            \
            sqlite3_column_type(                                             \
                statement,                                                   \
                (column_)                                                    \
            ) != SQLITE_NULL;                                                \
        (value_) =                                                           \
            (flag_)                                                          \
                ? sqlite3_column_double(                                     \
                    statement,                                               \
                    (column_)                                                \
                )                                                            \
                : 0.0;                                                       \
    } while (0)

    READ_BODY_VALUE(
        3,
        record->has_body_weight,
        record->body_weight_kg
    );

    READ_BODY_VALUE(
        4,
        record->has_neck,
        record->neck_cm
    );

    READ_BODY_VALUE(
        5,
        record->has_shoulders,
        record->shoulders_cm
    );

    READ_BODY_VALUE(
        6,
        record->has_chest,
        record->chest_cm
    );

    READ_BODY_VALUE(
        7,
        record->has_waist,
        record->waist_cm
    );

    READ_BODY_VALUE(
        8,
        record->has_hips,
        record->hips_cm
    );

    READ_BODY_VALUE(
        9,
        record->has_left_arm,
        record->left_arm_cm
    );

    READ_BODY_VALUE(
        10,
        record->has_right_arm,
        record->right_arm_cm
    );

    READ_BODY_VALUE(
        11,
        record->has_left_forearm,
        record->left_forearm_cm
    );

    READ_BODY_VALUE(
        12,
        record->has_right_forearm,
        record->right_forearm_cm
    );

    READ_BODY_VALUE(
        13,
        record->has_left_thigh,
        record->left_thigh_cm
    );

    READ_BODY_VALUE(
        14,
        record->has_right_thigh,
        record->right_thigh_cm
    );

    READ_BODY_VALUE(
        15,
        record->has_left_calf,
        record->left_calf_cm
    );

    READ_BODY_VALUE(
        16,
        record->has_right_calf,
        record->right_calf_cm
    );

#undef READ_BODY_VALUE

    (void)snprintf(
        record->notes,
        sizeof(record->notes),
        "%s",
        (const char *)notes
    );

    return true;
}

static const char *const BODY_RECORD_SELECT =
    "SELECT "
    "bo.observation_id, "
    "bo.observed_at, "
    "COALESCE(s.session_id, ''), "
    "bo.body_weight_kg, "
    "bo.neck_cm, "
    "bo.shoulders_cm, "
    "bo.chest_cm, "
    "bo.waist_cm, "
    "bo.hips_cm, "
    "bo.left_arm_cm, "
    "bo.right_arm_cm, "
    "bo.left_forearm_cm, "
    "bo.right_forearm_cm, "
    "bo.left_thigh_cm, "
    "bo.right_thigh_cm, "
    "bo.left_calf_cm, "
    "bo.right_calf_cm, "
    "COALESCE(bo.notes, '') "
    "FROM body_observations AS bo "
    "LEFT JOIN sessions AS s "
    "ON s.id = bo.session_row_id ";

TrainlogStatus trainlog_database_list_body_observations(
    TrainlogDatabase *database,
    TrainlogBodyObservationRecord *output,
    size_t capacity,
    size_t *output_count
)
{
    char sql[1024];
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int written;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        output_count == NULL ||
        (capacity > 0U && output == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    written = snprintf(
        sql,
        sizeof(sql),
        "%s"
        "ORDER BY bo.observed_at DESC, bo.id DESC;",
        BODY_RECORD_SELECT
    );

    if (written < 0 ||
        (size_t)written >= sizeof(sql)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        sql,
        -1,
        &statement,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < capacity) {
            if (!body_record_fill(
                    statement,
                    &output[count]
                )) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }
        }

        ++count;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    *output_count =
        count < capacity
            ? count
            : capacity;

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_database_get_body_observation(
    TrainlogDatabase *database,
    const char *observation_id,
    TrainlogBodyObservationRecord *output
)
{
    char sql[1024];
    sqlite3_stmt *statement = NULL;
    int written;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        observation_id == NULL ||
        observation_id[0] == '\0' ||
        output == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    written = snprintf(
        sql,
        sizeof(sql),
        "%s"
        "WHERE bo.observation_id = ?1;",
        BODY_RECORD_SELECT
    );

    if (written < 0 ||
        (size_t)written >= sizeof(sql)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        sql,
        -1,
        &statement,
        NULL
    );

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            1,
            observation_id,
            -1,
            SQLITE_TRANSIENT
        );
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    if (rc != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (!body_record_fill(
            statement,
            output
        )) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_update_body_observation(
    TrainlogDatabase *database,
    const TrainlogBodyObservationInput *observation
)
{
    static const char *const SQL =
        "UPDATE body_observations SET "
        "body_weight_kg = ?1, "
        "neck_cm = ?2, "
        "shoulders_cm = ?3, "
        "chest_cm = ?4, "
        "waist_cm = ?5, "
        "hips_cm = ?6, "
        "left_arm_cm = ?7, "
        "right_arm_cm = ?8, "
        "left_forearm_cm = ?9, "
        "right_forearm_cm = ?10, "
        "left_thigh_cm = ?11, "
        "right_thigh_cm = ?12, "
        "left_calf_cm = ?13, "
        "right_calf_cm = ?14, "
        "notes = ?15 "
        "WHERE observation_id = ?16;";

    sqlite3_stmt *statement = NULL;
    bool any_metric;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        observation == NULL ||
        observation->observation_id[0] == '\0') {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    any_metric =
        observation->has_body_weight ||
        observation->has_neck ||
        observation->has_shoulders ||
        observation->has_chest ||
        observation->has_waist ||
        observation->has_hips ||
        observation->has_left_arm ||
        observation->has_right_arm ||
        observation->has_left_forearm ||
        observation->has_right_forearm ||
        observation->has_left_thigh ||
        observation->has_right_thigh ||
        observation->has_left_calf ||
        observation->has_right_calf;

    if (!any_metric) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(
        database->connection,
        SQL,
        -1,
        &statement,
        NULL
    );

    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

#define UPDATE_BODY_VALUE(index_, flag_, value_)                             \
    do {                                                                     \
        if (rc == SQLITE_OK) {                                               \
            rc = bind_optional_double(                                       \
                statement,                                                   \
                (index_),                                                    \
                (flag_),                                                     \
                (value_)                                                     \
            );                                                               \
        }                                                                    \
    } while (0)

    UPDATE_BODY_VALUE(
        1,
        observation->has_body_weight,
        observation->body_weight_kg
    );

    UPDATE_BODY_VALUE(
        2,
        observation->has_neck,
        observation->neck_cm
    );

    UPDATE_BODY_VALUE(
        3,
        observation->has_shoulders,
        observation->shoulders_cm
    );

    UPDATE_BODY_VALUE(
        4,
        observation->has_chest,
        observation->chest_cm
    );

    UPDATE_BODY_VALUE(
        5,
        observation->has_waist,
        observation->waist_cm
    );

    UPDATE_BODY_VALUE(
        6,
        observation->has_hips,
        observation->hips_cm
    );

    UPDATE_BODY_VALUE(
        7,
        observation->has_left_arm,
        observation->left_arm_cm
    );

    UPDATE_BODY_VALUE(
        8,
        observation->has_right_arm,
        observation->right_arm_cm
    );

    UPDATE_BODY_VALUE(
        9,
        observation->has_left_forearm,
        observation->left_forearm_cm
    );

    UPDATE_BODY_VALUE(
        10,
        observation->has_right_forearm,
        observation->right_forearm_cm
    );

    UPDATE_BODY_VALUE(
        11,
        observation->has_left_thigh,
        observation->left_thigh_cm
    );

    UPDATE_BODY_VALUE(
        12,
        observation->has_right_thigh,
        observation->right_thigh_cm
    );

    UPDATE_BODY_VALUE(
        13,
        observation->has_left_calf,
        observation->left_calf_cm
    );

    UPDATE_BODY_VALUE(
        14,
        observation->has_right_calf,
        observation->right_calf_cm
    );

#undef UPDATE_BODY_VALUE

    if (rc == SQLITE_OK) {
        rc =
            observation->notes != NULL &&
            observation->notes[0] != '\0'
                ? sqlite3_bind_text(
                    statement,
                    15,
                    observation->notes,
                    -1,
                    SQLITE_TRANSIENT
                )
                : sqlite3_bind_null(
                    statement,
                    15
                );
    }

    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            16,
            observation->observation_id,
            -1,
            SQLITE_TRANSIENT
        );
    }

    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_step(statement);

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);

        return rc == SQLITE_CONSTRAINT
            ? TRAINLOG_STATUS_CONFLICT
            : TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_changes(
            database->connection
        ) != 1) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    return sqlite3_finalize(statement) == SQLITE_OK
        ? TRAINLOG_STATUS_OK
        : TRAINLOG_STATUS_DATABASE_ERROR;
}

/* TRAINING_KNOWLEDGE_RUNTIME_READ_V1 */
static bool knowledge_copy_column(sqlite3_stmt *statement, int column, char *output, size_t capacity)
{
    const unsigned char *value;
    size_t length;
    if (statement == NULL || sqlite3_column_type(statement, column) != SQLITE_TEXT ||
            output == NULL || capacity == 0U) return false;
    value = sqlite3_column_text(statement, column);
    if (value == NULL) return false;
    length = (size_t)sqlite3_column_bytes(statement, column);
    if (length >= capacity) return false;
    (void)memcpy(output, value, length + 1U);
    return true;
}

static bool knowledge_numeric_column(sqlite3_stmt *statement, int column)
{
    int type = sqlite3_column_type(statement, column);
    return type == SQLITE_INTEGER || type == SQLITE_FLOAT;
}

static bool knowledge_bounded_string(const char *value, size_t capacity, size_t *length)
{
    const char *end;
    if (value == NULL || capacity == 0U) return false;
    end = memchr(value, '\0', capacity);
    if (end == NULL || end == value) return false;
    if (length != NULL) *length = (size_t)(end - value);
    return true;
}

typedef struct KnowledgeTemporalCandidate {
    sqlite3_int64 row_id;
    char *started_at;
    size_t started_at_length;
    char *session_id;
    size_t session_id_length;
    char *entry_id;
    size_t entry_id_length;
    TrainlogTimestampKey timestamp;
} KnowledgeTemporalCandidate;

static void knowledge_temporal_candidate_release(KnowledgeTemporalCandidate *candidate)
{
    if (candidate == NULL) return;
    free(candidate->started_at);
    free(candidate->session_id);
    free(candidate->entry_id);
    (void)memset(candidate, 0, sizeof(*candidate));
}

static TrainlogStatus knowledge_copy_dynamic_column(
    sqlite3_stmt *statement, int column, char **output, size_t *output_length)
{
    const unsigned char *source;
    size_t length;
    char *copy;
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) return TRAINLOG_STATUS_DATABASE_ERROR;
    source = sqlite3_column_text(statement, column);
    if (source == NULL) return TRAINLOG_STATUS_DATABASE_ERROR;
    length = (size_t)sqlite3_column_bytes(statement, column);
    if (length == 0U || memchr(source, '\0', length) != NULL || length == SIZE_MAX)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    copy = malloc(length + 1U);
    if (copy == NULL) return TRAINLOG_STATUS_SYSTEM_ERROR;
    (void)memcpy(copy, source, length);
    copy[length] = '\0';
    *output = copy;
    *output_length = length;
    return TRAINLOG_STATUS_OK;
}

static int knowledge_bytes_compare(
    const char *left, size_t left_length, const char *right, size_t right_length)
{
    size_t common = left_length < right_length ? left_length : right_length;
    int result = memcmp(left, right, common);
    if (result != 0) return result;
    if (left_length == right_length) return 0;
    return left_length < right_length ? -1 : 1;
}

/* INVARIANT: This is the single ordering relation used for selection and the
 * exclusive cursor. Source timestamp spelling never participates in a tie. */
static int knowledge_temporal_candidate_compare(
    const KnowledgeTemporalCandidate *left, const KnowledgeTemporalCandidate *right)
{
    int result = trainlog_timestamp_compare(&left->timestamp, &right->timestamp);
    if (result != 0) return result;
    result = knowledge_bytes_compare(left->session_id, left->session_id_length,
        right->session_id, right->session_id_length);
    if (result != 0) return result;
    return knowledge_bytes_compare(left->entry_id, left->entry_id_length,
        right->entry_id, right->entry_id_length);
}

static TrainlogStatus knowledge_temporal_candidate_read(
    sqlite3_stmt *statement, KnowledgeTemporalCandidate *output)
{
    TrainlogStatus status;
    (void)memset(output, 0, sizeof(*output));
    if (sqlite3_column_type(statement, 0) != SQLITE_INTEGER) return TRAINLOG_STATUS_DATABASE_ERROR;
    status = knowledge_copy_dynamic_column(statement, 1, &output->session_id, &output->session_id_length);
    if (status == TRAINLOG_STATUS_OK)
        status = knowledge_copy_dynamic_column(statement, 2, &output->entry_id, &output->entry_id_length);
    if (status == TRAINLOG_STATUS_OK)
        status = knowledge_copy_dynamic_column(statement, 3, &output->started_at, &output->started_at_length);
    if (status != TRAINLOG_STATUS_OK ||
            !trainlog_timestamp_parse(output->started_at, output->started_at_length, &output->timestamp)) {
        knowledge_temporal_candidate_release(output);
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
    }
    output->row_id = sqlite3_column_int64(statement, 0);
    return TRAINLOG_STATUS_OK;
}

/* Retain a descending prefix. Capacity is at most page limit + one, so a
 * sorted bounded array keeps memory independent of history cardinality. */
static void knowledge_temporal_candidate_insert(
    KnowledgeTemporalCandidate *items, size_t *count, size_t capacity,
    KnowledgeTemporalCandidate *candidate)
{
    size_t position = 0U;
    size_t index;
    while (position < *count &&
            knowledge_temporal_candidate_compare(candidate, &items[position]) <= 0) ++position;
    if (position >= capacity) {
        knowledge_temporal_candidate_release(candidate);
        return;
    }
    if (*count == capacity) knowledge_temporal_candidate_release(&items[capacity - 1U]);
    else ++*count;
    for (index = *count - 1U; index > position; --index) items[index] = items[index - 1U];
    items[position] = *candidate;
    (void)memset(candidate, 0, sizeof(*candidate));
}

static bool knowledge_tracking_mode(const char *value, TrainlogTrackingMode *output)
{
    if (value == NULL || output == NULL) return false;
    if (strcmp(value,"reps")==0) {*output=TRAINLOG_TRACKING_REPS;return true;}
    if (strcmp(value,"duration")==0) {*output=TRAINLOG_TRACKING_DURATION;return true;}
    return false;
}

static bool knowledge_recording_mode(const char *value, TrainlogRecordingMode *output)
{
    if (value == NULL || output == NULL) return false;
    if (strcmp(value,"sets")==0) {*output=TRAINLOG_RECORDING_SETS;return true;}
    if (strcmp(value,"continuous")==0) {*output=TRAINLOG_RECORDING_CONTINUOUS;return true;}
    return false;
}

static bool knowledge_load_mode(const char *value, TrainlogLoadMode *output)
{
    if (value == NULL || output == NULL) return false;
    if (strcmp(value,"none")==0) {*output=TRAINLOG_LOAD_NONE;return true;}
    if (strcmp(value,"external")==0) {*output=TRAINLOG_LOAD_EXTERNAL;return true;}
    if (strcmp(value,"assistance")==0) {*output=TRAINLOG_LOAD_ASSISTANCE;return true;}
    return false;
}

TrainlogStatus trainlog_database_get_exercise_profile(
    TrainlogDatabase *database, const char *exercise_id, TrainlogExercise *output)
{
    sqlite3_stmt *statement = NULL;
    int rc;
    if (database == NULL || database->connection == NULL || exercise_id == NULL || exercise_id[0] == '\0' ||
            output == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)memset(output, 0, sizeof(*output));
    rc = sqlite3_prepare_v2(database->connection,
        "SELECT exercise_id,name,tracking_mode,recording_mode,data_fields FROM exercises WHERE exercise_id=?1;",
        -1, &statement, NULL);
    if (rc == SQLITE_OK) rc = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR; }
    rc = sqlite3_step(statement);
    if (rc == SQLITE_DONE) { (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_NOT_FOUND; }
    if (rc != SQLITE_ROW || !knowledge_copy_column(statement, 0, output->exercise_id, sizeof(output->exercise_id)) ||
            !knowledge_copy_column(statement, 1, output->name, sizeof(output->name)) ||
            sqlite3_column_type(statement, 4) != SQLITE_INTEGER) {
        (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_column_type(statement,2)!=SQLITE_TEXT || sqlite3_column_type(statement,3)!=SQLITE_TEXT ||
            !knowledge_tracking_mode((const char *)sqlite3_column_text(statement,2),&output->tracking_mode) ||
            !knowledge_recording_mode((const char *)sqlite3_column_text(statement,3),&output->recording_mode)) {
        (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if ((sqlite3_column_int64(statement, 4) < 0) ||
            ((sqlite3_uint64)sqlite3_column_int64(statement, 4) > UINT32_MAX)) {
        (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    output->data_fields = (TrainlogExerciseDataFields)sqlite3_column_int64(statement, 4);
    return sqlite3_finalize(statement) == SQLITE_OK ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_database_list_exercise_occurrences_page(
    TrainlogDatabase *database, const char *exercise_id, const TrainlogExerciseOccurrenceCursor *after,
    size_t limit, TrainlogExerciseOccurrence *output, size_t *output_count, bool *output_has_more,
    TrainlogExerciseOccurrenceCursor *output_next)
{
    static const char *const SCAN_SQL =
        "SELECT se.id,s.session_id,se.entry_id,s.started_at FROM session_exercises se "
        "JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id "
        "WHERE e.exercise_id=?1;";
    static const char *const HYDRATE_SQL =
        "SELECT s.session_id,se.entry_id,e.exercise_id,s.started_at,COALESCE(se.equipment_id,''),"
        "s.session_type,e.tracking_mode,se.recording_mode,se.data_fields,se.load_mode,"
        "(SELECT COUNT(*) FROM performed_sets ps WHERE ps.session_exercise_row_id=se.id),"
        "ca.duration_seconds,ca.speed_kmh,ca.distance_km FROM session_exercises se "
        "JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id "
        "LEFT JOIN continuous_activity ca ON ca.session_exercise_row_id=se.id WHERE se.id=?1;";
    sqlite3_stmt *statement = NULL;
    KnowledgeTemporalCandidate selected[TRAINLOG_OCCURRENCE_PAGE_MAX + 1U] = {{0}};
    KnowledgeTemporalCandidate cursor = {0};
    TrainlogExerciseOccurrenceCursor after_value;
    TrainlogExercise profile;
    TrainlogStatus status;
    size_t selected_count = 0U, count = 0U, index;
    size_t cursor_started_length = 0U, cursor_session_length = 0U, cursor_entry_length = 0U;
    int rc;
    bool snapshot = false;
    if (output_count != NULL) *output_count = 0U;
    if (output_has_more != NULL) *output_has_more = false;
    if (database == NULL || database->connection == NULL || exercise_id == NULL || exercise_id[0] == '\0' ||
            limit == 0U || limit > TRAINLOG_OCCURRENCE_PAGE_MAX || output == NULL || output_count == NULL ||
            output_has_more == NULL || output_next == NULL ||
            (after != NULL && (!knowledge_bounded_string(after->started_at, sizeof(after->started_at),
                    &cursor_started_length) ||
                !knowledge_bounded_string(after->session_id, sizeof(after->session_id), &cursor_session_length) ||
                !knowledge_bounded_string(after->entry_id, sizeof(after->entry_id), &cursor_entry_length) ||
                !trainlog_timestamp_parse(after->started_at, cursor_started_length, &cursor.timestamp))))
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    if (after != NULL) {
        after_value = *after;
        cursor.started_at = after_value.started_at; cursor.started_at_length = cursor_started_length;
        cursor.session_id = after_value.session_id; cursor.session_id_length = cursor_session_length;
        cursor.entry_id = after_value.entry_id; cursor.entry_id_length = cursor_entry_length;
        if (!trainlog_timestamp_parse(cursor.started_at, cursor_started_length, &cursor.timestamp))
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) return status;
    snapshot = true;
    status = trainlog_database_get_exercise_profile(database, exercise_id, &profile);
    if (status != TRAINLOG_STATUS_OK) goto done;
    (void)memset(output_next, 0, sizeof(*output_next));
    rc = sqlite3_prepare_v2(database->connection, SCAN_SQL, -1, &statement, NULL);
    if (rc == SQLITE_OK) rc = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        KnowledgeTemporalCandidate candidate;
        status = knowledge_temporal_candidate_read(statement, &candidate);
        if (status != TRAINLOG_STATUS_OK) goto done;
        if (after == NULL || knowledge_temporal_candidate_compare(&candidate, &cursor) < 0)
            knowledge_temporal_candidate_insert(selected, &selected_count, limit + 1U, &candidate);
        knowledge_temporal_candidate_release(&candidate);
    }
    if (rc != SQLITE_DONE) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
    rc = sqlite3_finalize(statement); statement = NULL;
    if (rc != SQLITE_OK) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
    statement = NULL;
    *output_has_more = selected_count > limit;
    count = *output_has_more ? limit : selected_count;
    for (index = 0U; index < count; ++index) {
        TrainlogExerciseOccurrence *item;
        sqlite3_int64 fields;
        sqlite3_int64 sets;
        rc = sqlite3_prepare_v2(database->connection, HYDRATE_SQL, -1, &statement, NULL);
        if (rc == SQLITE_OK) rc = sqlite3_bind_int64(statement, 1, selected[index].row_id);
        if (rc == SQLITE_OK) rc = sqlite3_step(statement);
        if (rc != SQLITE_ROW) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
        item = &output[index]; (void)memset(item, 0, sizeof(*item));
        if (!knowledge_copy_column(statement,0,item->session_id,sizeof(item->session_id)) ||
            !knowledge_copy_column(statement,1,item->entry_id,sizeof(item->entry_id)) ||
            !knowledge_copy_column(statement,2,item->exercise_id,sizeof(item->exercise_id)) ||
            !knowledge_copy_column(statement,3,item->started_at,sizeof(item->started_at)) ||
            !knowledge_copy_column(statement,4,item->equipment_id,sizeof(item->equipment_id)) ||
            sqlite3_column_type(statement,5)!=SQLITE_TEXT ||
            !session_type_from_sql((const char *)sqlite3_column_text(statement,5),&item->session_type)) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        if(sqlite3_column_type(statement,6)!=SQLITE_TEXT || sqlite3_column_type(statement,7)!=SQLITE_TEXT ||
                sqlite3_column_type(statement,9)!=SQLITE_TEXT ||
                !knowledge_tracking_mode((const char *)sqlite3_column_text(statement,6),&item->tracking_mode) ||
                !knowledge_recording_mode((const char *)sqlite3_column_text(statement,7),&item->recording_mode) ||
                !knowledge_load_mode((const char *)sqlite3_column_text(statement,9),&item->load_mode)) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        if (sqlite3_column_type(statement,8)!=SQLITE_INTEGER || sqlite3_column_type(statement,10)!=SQLITE_INTEGER) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        fields=sqlite3_column_int64(statement,8); sets=sqlite3_column_int64(statement,10);
        if (fields<0 || (sqlite3_uint64)fields>UINT32_MAX || sets<0 || (sqlite3_uint64)sets>SIZE_MAX) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        item->data_fields=(TrainlogExerciseDataFields)fields;
        item->set_count=(size_t)sets;
        if (sqlite3_column_type(statement,11)!=SQLITE_NULL) {
            sqlite3_int64 duration;
            if (sqlite3_column_type(statement,11)!=SQLITE_INTEGER) {
                status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
            }
            duration=sqlite3_column_int64(statement,11);
            if (duration<=0 || duration>INT_MAX) {
                status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
            }
            item->continuous_duration_seconds=(int)duration;
        }
        item->continuous_has_speed=sqlite3_column_type(statement,12)!=SQLITE_NULL;
        if (item->continuous_has_speed && !knowledge_numeric_column(statement,12)) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        item->continuous_speed_kmh=sqlite3_column_double(statement,12);
        item->continuous_has_distance=sqlite3_column_type(statement,13)!=SQLITE_NULL;
        if (item->continuous_has_distance && !knowledge_numeric_column(statement,13)) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        item->continuous_distance_km=sqlite3_column_double(statement,13);
        if ((item->continuous_has_speed && !isfinite(item->continuous_speed_kmh)) ||
                (item->continuous_has_distance && !isfinite(item->continuous_distance_km))) {
            status = TRAINLOG_STATUS_DATABASE_ERROR; goto done;
        }
        rc = sqlite3_step(statement);
        if (rc != SQLITE_DONE) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
        rc = sqlite3_finalize(statement); statement = NULL;
        if (rc != SQLITE_OK) { status = TRAINLOG_STATUS_DATABASE_ERROR; goto done; }
    }
    *output_count=count;
    if (count > 0U) {
        (void)snprintf(output_next->started_at,sizeof(output_next->started_at),"%s",output[count-1U].started_at);
        (void)snprintf(output_next->session_id,sizeof(output_next->session_id),"%s",output[count-1U].session_id);
        (void)snprintf(output_next->entry_id,sizeof(output_next->entry_id),"%s",output[count-1U].entry_id);
    }
    status = TRAINLOG_STATUS_OK;
done:
    (void)sqlite3_finalize(statement);
    for (index = 0U; index < selected_count; ++index)
        knowledge_temporal_candidate_release(&selected[index]);
    if (snapshot) {
        TrainlogStatus end_status = trainlog_database_read_snapshot_end(database, status == TRAINLOG_STATUS_OK);
        if (status == TRAINLOG_STATUS_OK) status = end_status;
    }
    return status;
}

TrainlogStatus trainlog_database_list_occurrence_sets_page(
    TrainlogDatabase *database, const char *entry_id, int after_position, size_t limit,
    TrainlogOccurrenceSet *output, size_t *output_count, bool *output_has_more, int *output_next_position)
{
    sqlite3_stmt *statement = NULL;
    sqlite3_stmt *identity = NULL;
    size_t count=0U;
    int rc;
    if (output_count != NULL) *output_count=0U;
    if (output_has_more != NULL) *output_has_more=false;
    if (database==NULL || database->connection==NULL || entry_id==NULL || entry_id[0]=='\0' ||
            after_position < -1 || limit==0U || limit>TRAINLOG_OCCURRENCE_SET_PAGE_MAX || output==NULL ||
            output_count==NULL || output_has_more==NULL || output_next_position==NULL)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_next_position=after_position;
    rc=sqlite3_prepare_v2(database->connection,"SELECT 1 FROM session_exercises WHERE entry_id=?1;",-1,&identity,NULL);
    if(rc==SQLITE_OK)rc=sqlite3_bind_text(identity,1,entry_id,-1,SQLITE_TRANSIENT);
    if(rc!=SQLITE_OK){(void)sqlite3_finalize(identity);return TRAINLOG_STATUS_DATABASE_ERROR;}
    rc=sqlite3_step(identity);
    if(rc==SQLITE_DONE){(void)sqlite3_finalize(identity);return TRAINLOG_STATUS_NOT_FOUND;}
    if(rc!=SQLITE_ROW || sqlite3_finalize(identity)!=SQLITE_OK)return TRAINLOG_STATUS_DATABASE_ERROR;
    rc=sqlite3_prepare_v2(database->connection,
        "SELECT ps.position,ps.reps,ps.duration_seconds,ps.weight_kg FROM performed_sets ps "
        "JOIN session_exercises se ON se.id=ps.session_exercise_row_id WHERE se.entry_id=?1 AND ps.position>?2 "
        "ORDER BY ps.position ASC LIMIT ?3;",-1,&statement,NULL);
    if(rc==SQLITE_OK) rc=sqlite3_bind_text(statement,1,entry_id,-1,SQLITE_TRANSIENT);
    if(rc==SQLITE_OK) rc=sqlite3_bind_int(statement,2,after_position);
    if(rc==SQLITE_OK) rc=sqlite3_bind_int64(statement,3,(sqlite3_int64)(limit+1U));
    if(rc!=SQLITE_OK){(void)sqlite3_finalize(statement);return TRAINLOG_STATUS_DATABASE_ERROR;}
    while((rc=sqlite3_step(statement))==SQLITE_ROW){ TrainlogOccurrenceSet *item; sqlite3_int64 position;
        if(count==limit){*output_has_more=true;break;} item=&output[count];(void)memset(item,0,sizeof(*item));
        if(sqlite3_column_type(statement,0)!=SQLITE_INTEGER){(void)sqlite3_finalize(statement);return TRAINLOG_STATUS_DATABASE_ERROR;}
        position=sqlite3_column_int64(statement,0); if(position<0 || position>INT_MAX){(void)sqlite3_finalize(statement);return TRAINLOG_STATUS_DATABASE_ERROR;}
        item->position=(size_t)position; item->has_reps=sqlite3_column_type(statement,1)!=SQLITE_NULL;
        item->has_duration=sqlite3_column_type(statement,2)!=SQLITE_NULL;
        if (item->has_reps) {
            sqlite3_int64 reps;
            if (sqlite3_column_type(statement,1)!=SQLITE_INTEGER) {
                (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            reps=sqlite3_column_int64(statement,1);
            if (reps<0 || reps>INT_MAX) {
                (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            item->reps=(int)reps;
        }
        if (item->has_duration) {
            sqlite3_int64 duration;
            if (sqlite3_column_type(statement,2)!=SQLITE_INTEGER) {
                (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            duration=sqlite3_column_int64(statement,2);
            if (duration<=0 || duration>INT_MAX) {
                (void)sqlite3_finalize(statement); return TRAINLOG_STATUS_DATABASE_ERROR;
            }
            item->duration_seconds=(int)duration;
        }
        item->has_weight=sqlite3_column_type(statement,3)!=SQLITE_NULL;
        if(item->has_weight && !knowledge_numeric_column(statement,3)){(void)sqlite3_finalize(statement);return TRAINLOG_STATUS_DATABASE_ERROR;}
        item->weight_kg=sqlite3_column_double(statement,3);
        if(item->has_weight && !isfinite(item->weight_kg)){(void)sqlite3_finalize(statement);return TRAINLOG_STATUS_DATABASE_ERROR;}
        *output_next_position=(int)position; ++count;
    }
    if(rc!=SQLITE_DONE && rc!=SQLITE_ROW){(void)sqlite3_finalize(statement);return TRAINLOG_STATUS_DATABASE_ERROR;}
    if(sqlite3_finalize(statement)!=SQLITE_OK)return TRAINLOG_STATUS_DATABASE_ERROR;
    *output_count=count; return TRAINLOG_STATUS_OK;
}

static TrainlogStatus database_latest_explicit_max_context(
    TrainlogDatabase *database, const char *exercise_id, const char *equipment_id,
    TrainlogLatestExplicitMax *output)
{
    static const char *const SCAN_SQL =
        "SELECT se.id,s.session_id,se.entry_id,s.started_at FROM max_results mr "
        "JOIN session_exercises se ON se.id=mr.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id "
        "WHERE e.exercise_id=?1 AND s.session_type='max_test' "
        "AND (?2 IS NULL OR se.equipment_id=?2);";
    static const char *const HYDRATE_SQL =
        "SELECT s.session_id,se.entry_id,s.started_at,COALESCE(se.equipment_id,''),se.load_mode,mr.max_weight_kg "
        "FROM max_results mr JOIN session_exercises se ON se.id=mr.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id WHERE se.id=?1;";
    sqlite3_stmt *statement=NULL;
    KnowledgeTemporalCandidate selected[1] = {{0}};
    size_t selected_count = 0U;
    int rc;
    TrainlogExercise profile;
    TrainlogStatus status;
    bool snapshot = false;
    if(database==NULL || database->connection==NULL || exercise_id==NULL || exercise_id[0]=='\0' || output==NULL)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)memset(output,0,sizeof(*output));
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) return status;
    snapshot = true;
    status=trainlog_database_get_exercise_profile(database,exercise_id,&profile);
    if(status!=TRAINLOG_STATUS_OK)goto done;
    rc=sqlite3_prepare_v2(database->connection,SCAN_SQL,-1,&statement,NULL);
    if(rc==SQLITE_OK)rc=sqlite3_bind_text(statement,1,exercise_id,-1,SQLITE_TRANSIENT);
    if(rc==SQLITE_OK)rc=equipment_id==NULL ? sqlite3_bind_null(statement,2)
        : sqlite3_bind_text(statement,2,equipment_id,-1,SQLITE_TRANSIENT);
    if(rc!=SQLITE_OK){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
    while((rc=sqlite3_step(statement))==SQLITE_ROW){
        KnowledgeTemporalCandidate candidate;
        status=knowledge_temporal_candidate_read(statement,&candidate);
        if(status!=TRAINLOG_STATUS_OK)goto done;
        knowledge_temporal_candidate_insert(selected,&selected_count,1U,&candidate);
        knowledge_temporal_candidate_release(&candidate);
    }
    if(rc!=SQLITE_DONE){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
    rc=sqlite3_finalize(statement); statement=NULL;
    if(rc!=SQLITE_OK){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
    if(selected_count==0U){status=TRAINLOG_STATUS_OK;goto done;}
    rc=sqlite3_prepare_v2(database->connection,HYDRATE_SQL,-1,&statement,NULL);
    if(rc==SQLITE_OK)rc=sqlite3_bind_int64(statement,1,selected[0].row_id);
    if(rc==SQLITE_OK)rc=sqlite3_step(statement);
    if(rc!=SQLITE_ROW || !knowledge_copy_column(statement,0,output->session_id,sizeof(output->session_id)) ||
        !knowledge_copy_column(statement,1,output->entry_id,sizeof(output->entry_id)) ||
        !knowledge_copy_column(statement,2,output->started_at,sizeof(output->started_at)) ||
        !knowledge_copy_column(statement,3,output->equipment_id,sizeof(output->equipment_id))){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
    if(sqlite3_column_type(statement,4)!=SQLITE_TEXT ||
            !knowledge_load_mode((const char *)sqlite3_column_text(statement,4),&output->load_mode)){
        status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;
    }
    if(!knowledge_numeric_column(statement,5)){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
    output->max_weight_kg=sqlite3_column_double(statement,5);
    if(!isfinite(output->max_weight_kg) || output->max_weight_kg<=0.0){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
    output->found=true;
    rc=sqlite3_step(statement);
    if(rc!=SQLITE_DONE){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
    rc=sqlite3_finalize(statement); statement=NULL;
    status=rc==SQLITE_OK?TRAINLOG_STATUS_OK:TRAINLOG_STATUS_DATABASE_ERROR;
done:
    (void)sqlite3_finalize(statement);
    knowledge_temporal_candidate_release(&selected[0]);
    if(snapshot){
        TrainlogStatus end_status=trainlog_database_read_snapshot_end(database,status==TRAINLOG_STATUS_OK);
        if(status==TRAINLOG_STATUS_OK)status=end_status;
    }
    return status;
}

TrainlogStatus trainlog_database_latest_explicit_max_context(
    TrainlogDatabase *database, const char *exercise_id, TrainlogLatestExplicitMax *output)
{
    return database_latest_explicit_max_context(database, exercise_id, NULL, output);
}

TrainlogStatus trainlog_database_latest_explicit_max_equipment_context(
    TrainlogDatabase *database, const char *exercise_id, const char *equipment_id,
    TrainlogLatestExplicitMax *output)
{
    if (equipment_id == NULL || equipment_id[0] == '\0')
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    return database_latest_explicit_max_context(database, exercise_id, equipment_id, output);
}
