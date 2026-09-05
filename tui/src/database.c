/**
 * @file database.c
 * @brief SQLite persistence implementation for Trainlog.
 */

#include "trainlog/database.h"
#include "trainlog/duration.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

struct TrainlogDatabase {
    sqlite3 *connection;
};

static const char *const SCHEMA_V1_SQL =
    "BEGIN IMMEDIATE;"

    "CREATE TABLE IF NOT EXISTS exercises ("
    "  id INTEGER PRIMARY KEY,"
    "  exercise_id TEXT NOT NULL UNIQUE,"
    "  name TEXT NOT NULL,"
    "  normalized_name TEXT NOT NULL UNIQUE,"
    "  tracking_mode TEXT NOT NULL"
    "    CHECK (tracking_mode IN ('reps', 'duration'))"
    ");"

    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id INTEGER PRIMARY KEY,"
    "  session_id TEXT NOT NULL UNIQUE,"
    "  started_at TEXT NOT NULL,"
    "  ended_at TEXT,"
    "  notes TEXT"
    ");"

    "CREATE TABLE IF NOT EXISTS session_exercises ("
    "  id INTEGER PRIMARY KEY,"
    "  session_row_id INTEGER NOT NULL"
    "    REFERENCES sessions(id) ON DELETE CASCADE,"
    "  exercise_row_id INTEGER NOT NULL"
    "    REFERENCES exercises(id) ON DELETE RESTRICT,"
    "  position INTEGER NOT NULL CHECK (position >= 0),"
    "  load_mode TEXT NOT NULL"
    "    CHECK (load_mode IN ('none', 'external', 'assistance')),"
    "  rest_seconds INTEGER NOT NULL CHECK (rest_seconds >= 0),"
    "  target_sets INTEGER NOT NULL CHECK (target_sets > 0),"
    "  target_reps INTEGER CHECK (target_reps >= 1),"
    "  target_duration_seconds INTEGER"
    "    CHECK (target_duration_seconds > 0),"
    "  target_weight_kg REAL CHECK (target_weight_kg > 0.0),"
    "  notes TEXT,"
    "  UNIQUE (session_row_id, position),"
    "  UNIQUE (session_row_id, exercise_row_id),"
    "  CHECK ("
    "    (target_reps IS NOT NULL AND"
    "     target_duration_seconds IS NULL) OR"
    "    (target_reps IS NULL AND"
    "     target_duration_seconds IS NOT NULL)"
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

    "PRAGMA user_version = 1;"
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
    TrainlogDatabase *database
)
{
    int version = 0;
    TrainlogStatus status;

    status = trainlog_database_schema_version(database, &version);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    if (version > TRAINLOG_DATABASE_SCHEMA_VERSION) {
        return TRAINLOG_STATUS_SCHEMA_UNSUPPORTED;
    }

    if (version == TRAINLOG_DATABASE_SCHEMA_VERSION) {
        return TRAINLOG_STATUS_OK;
    }

    if (version != 0) {
        return TRAINLOG_STATUS_SCHEMA_UNSUPPORTED;
    }

    status = execute_sql(database, SCHEMA_V1_SQL);
    if (status != TRAINLOG_STATUS_OK) {
        (void)sqlite3_exec(database->connection, "ROLLBACK;", NULL, NULL, NULL);
    }

    return status;
}

TrainlogStatus trainlog_database_open(
    const char *path,
    TrainlogDatabase **output_database
)
{
    TrainlogDatabase *database;
    int rc;
    TrainlogStatus status;

    if (path == NULL || path[0] == '\0' || output_database == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_database = NULL;

    database = calloc(1U, sizeof(*database));
    if (database == NULL) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    rc = sqlite3_open_v2(
        path,
        &database->connection,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        NULL
    );
    if (rc != SQLITE_OK) {
        trainlog_database_close(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_busy_timeout(database->connection, 5000) != SQLITE_OK) {
        trainlog_database_close(database);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    status = execute_sql(database, "PRAGMA foreign_keys = ON;");
    if (status != TRAINLOG_STATUS_OK) {
        trainlog_database_close(database);
        return status;
    }

    status = initialize_or_validate_schema(database);
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

TrainlogStatus trainlog_database_insert_exercise(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *name,
    const char *normalized_name,
    TrainlogTrackingMode tracking_mode
)
{
    static const char *const SQL =
        "INSERT INTO exercises("
        "exercise_id, name, normalized_name, tracking_mode"
        ") VALUES(?1, ?2, ?3, ?4);";
    sqlite3_stmt *statement = NULL;
    const char *mode;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        exercise_id == NULL ||
        exercise_id[0] == '\0' ||
        name == NULL ||
        name[0] == '\0' ||
        normalized_name == NULL ||
        normalized_name[0] == '\0') {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    mode = tracking_mode_to_sql(tracking_mode);
    if (mode == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, normalized_name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, mode, -1, SQLITE_STATIC) != SQLITE_OK) {
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

TrainlogStatus trainlog_database_list_exercises(
    TrainlogDatabase *database,
    TrainlogExercise *output,
    size_t capacity,
    size_t *output_count
)
{
    static const char *const SQL =
        "SELECT exercise_id, name, tracking_mode "
        "FROM exercises ORDER BY name COLLATE NOCASE, exercise_id;";
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        output_count == NULL ||
        (capacity > 0U && output == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < capacity) {
            const unsigned char *id = sqlite3_column_text(statement, 0);
            const unsigned char *name = sqlite3_column_text(statement, 1);
            const unsigned char *mode = sqlite3_column_text(statement, 2);

            if (id == NULL || name == NULL || mode == NULL) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)snprintf(
                output[count].exercise_id,
                sizeof(output[count].exercise_id),
                "%s",
                (const char *)id
            );
            (void)snprintf(
                output[count].name,
                sizeof(output[count].name),
                "%s",
                (const char *)name
            );
            output[count].tracking_mode =
                tracking_mode_from_sql((const char *)mode);
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

static TrainlogStatus insert_session_header(
    TrainlogDatabase *database,
    const TrainlogSessionInput *session,
    sqlite3_int64 *output_row_id
)
{
    sqlite3_stmt *statement = NULL;
    int rc;

    rc = sqlite3_prepare_v2(
        database->connection,
        "INSERT INTO sessions(session_id, started_at, ended_at, notes) "
        "VALUES(?1, ?2, ?3, ?4);",
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_bind_text(statement, 1, session->session_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, session->started_at, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (session->ended_at[0] != '\0') {
        rc = sqlite3_bind_text(
            statement,
            3,
            session->ended_at,
            -1,
            SQLITE_TRANSIENT
        );
    } else {
        rc = sqlite3_bind_null(statement, 3);
    }
    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (session->notes != NULL && session->notes[0] != '\0') {
        rc = sqlite3_bind_text(statement, 4, session->notes, -1, SQLITE_TRANSIENT);
    } else {
        rc = sqlite3_bind_null(statement, 4);
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

    *output_row_id = sqlite3_last_insert_rowid(database->connection);
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
    int rc;
    TrainlogStatus status;

    load_mode = load_mode_to_sql(input->load_mode);
    if (load_mode == NULL) {
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
        "session_row_id, exercise_row_id, position, load_mode, "
        "rest_seconds, target_sets, target_reps, "
        "target_duration_seconds, target_weight_kg, notes"
        ") VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);",
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    rc = sqlite3_bind_int64(statement, 1, session_row_id);
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(statement, 2, exercise_row_id);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(statement, 3, (sqlite3_int64)position);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(statement, 4, load_mode, -1, SQLITE_STATIC);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int(statement, 5, input->rest_seconds);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int(statement, 6, input->target_sets);
    }
    if (rc == SQLITE_OK) {
        rc = input->target_reps > 0
            ? sqlite3_bind_int(statement, 7, input->target_reps)
            : sqlite3_bind_null(statement, 7);
    }
    if (rc == SQLITE_OK) {
        rc = input->target_duration_seconds > 0
            ? sqlite3_bind_int(
                statement,
                8,
                input->target_duration_seconds
            )
            : sqlite3_bind_null(statement, 8);
    }
    if (rc == SQLITE_OK) {
        rc = input->target_has_weight
            ? sqlite3_bind_double(statement, 9, input->target_weight_kg)
            : sqlite3_bind_null(statement, 9);
    }
    if (rc == SQLITE_OK) {
        rc = input->notes != NULL && input->notes[0] != '\0'
            ? sqlite3_bind_text(
                statement,
                10,
                input->notes,
                -1,
                SQLITE_TRANSIENT
            )
            : sqlite3_bind_null(statement, 10);
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

    *output_row_id = sqlite3_last_insert_rowid(database->connection);
    return TRAINLOG_STATUS_OK;
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

TrainlogStatus trainlog_database_insert_session(
    TrainlogDatabase *database,
    const TrainlogSessionInput *session
)
{
    sqlite3_int64 session_row_id;
    size_t exercise_index;
    TrainlogStatus status;

    if (database == NULL ||
        database->connection == NULL ||
        session == NULL ||
        session->session_id[0] == '\0' ||
        session->started_at[0] == '\0' ||
        (session->exercise_count > 0U && session->exercises == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status = trainlog_database_begin(database);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status = insert_session_header(database, session, &session_row_id);
    if (status != TRAINLOG_STATUS_OK) {
        (void)trainlog_database_rollback(database);
        return status;
    }

    for (exercise_index = 0U;
         exercise_index < session->exercise_count;
         ++exercise_index) {
        const TrainlogSessionExerciseInput *exercise =
            &session->exercises[exercise_index];
        sqlite3_int64 session_exercise_row_id;
        size_t set_index;

        status = insert_session_exercise(
            database,
            session_row_id,
            exercise_index,
            exercise,
            &session_exercise_row_id
        );
        if (status != TRAINLOG_STATUS_OK) {
            (void)trainlog_database_rollback(database);
            return status;
        }

        for (set_index = 0U; set_index < exercise->set_count; ++set_index) {
            status = insert_performed_set(
                database,
                session_exercise_row_id,
                set_index,
                &exercise->sets[set_index]
            );
            if (status != TRAINLOG_STATUS_OK) {
                (void)trainlog_database_rollback(database);
                return status;
            }
        }
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
        "SELECT s.session_id, s.started_at, COALESCE(s.ended_at, ''), "
        "COUNT(se.id) "
        "FROM sessions AS s "
        "LEFT JOIN session_exercises AS se ON se.session_row_id = s.id "
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

    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < capacity) {
            const unsigned char *id = sqlite3_column_text(statement, 0);
            const unsigned char *started = sqlite3_column_text(statement, 1);
            const unsigned char *ended = sqlite3_column_text(statement, 2);
            sqlite3_int64 exercise_count = sqlite3_column_int64(statement, 3);

            if (id == NULL || started == NULL || ended == NULL ||
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
            output[count].exercise_count = (size_t)exercise_count;
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
    size_t used = 0U;
    size_t count = 0U;
    int rc;

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

    rc = sqlite3_bind_int64(
        statement,
        1,
        session_exercise_row_id
    );
    if (rc != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
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

        if (count > 0U) {
            TrainlogStatus status = detail_append_text(
                detail->actual_summary,
                sizeof(detail->actual_summary),
                &used,
                " / "
            );

            if (status != TRAINLOG_STATUS_OK) {
                (void)sqlite3_finalize(statement);
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
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        if (written < 0 ||
            (size_t)written >= sizeof(fragment)) {
            (void)sqlite3_finalize(statement);
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
                return status;
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

    detail->actual_set_count = count;

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
        "SELECT started_at, COALESCE(ended_at, '') "
        "FROM sessions "
        "WHERE session_id = ?1;";

    static const char *const EXERCISE_SQL =
        "SELECT "
        "e.name, e.tracking_mode, se.load_mode, se.rest_seconds, "
        "se.target_sets, COALESCE(se.target_reps, 0), "
        "COALESCE(se.target_duration_seconds, 0), "
        "se.target_weight_kg, se.id "
        "FROM session_exercises AS se "
        "JOIN sessions AS s ON s.id = se.session_row_id "
        "JOIN exercises AS e ON e.id = se.exercise_row_id "
        "WHERE s.session_id = ?1 "
        "ORDER BY se.position ASC;";

    sqlite3_stmt *header = NULL;
    sqlite3_stmt *exercises = NULL;
    size_t copied = 0U;
    size_t total = 0U;
    int rc;

    if (database == NULL ||
        database->connection == NULL ||
        session_id == NULL ||
        session_id[0] == '\0' ||
        output_session == NULL ||
        output_exercise_count == NULL ||
        (exercise_capacity > 0U && output_exercises == NULL)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(output_session, 0, sizeof(*output_session));
    *output_exercise_count = 0U;

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

        if (started == NULL || ended == NULL) {
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

            const unsigned char *load =
                sqlite3_column_text(exercises, 2);

            sqlite3_int64 session_exercise_row_id =
                sqlite3_column_int64(exercises, 8);

            TrainlogStatus status;

            if (name == NULL ||
                tracking == NULL ||
                load == NULL) {
                (void)sqlite3_finalize(exercises);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }

            (void)memset(detail, 0, sizeof(*detail));

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

            detail->load_mode =
                detail_load_mode_from_text(
                    (const char *)load
                );

            detail->rest_seconds =
                sqlite3_column_int(exercises, 3);

            detail->target_sets =
                sqlite3_column_int(exercises, 4);

            detail->target_reps =
                sqlite3_column_int(exercises, 5);

            detail->target_duration_seconds =
                sqlite3_column_int(exercises, 6);

            detail->has_target_weight =
                sqlite3_column_type(
                    exercises,
                    7
                ) != SQLITE_NULL;

            detail->target_weight_kg =
                detail->has_target_weight != 0
                    ? sqlite3_column_double(
                        exercises,
                        7
                    )
                    : 0.0;

            status = detail_fill_sets(
                database,
                session_exercise_row_id,
                detail
            );

            if (status != TRAINLOG_STATUS_OK) {
                (void)sqlite3_finalize(exercises);
                return status;
            }

            ++copied;
        }

        ++total;
    }

    if (rc != SQLITE_DONE) {
        (void)sqlite3_finalize(exercises);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (sqlite3_finalize(exercises) != SQLITE_OK) {
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
