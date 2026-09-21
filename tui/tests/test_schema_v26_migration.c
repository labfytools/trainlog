/**
 * @file test_schema_v26_migration.c
 * @brief Lossless populated v25 to v26 migration evidence.
 */
#define _POSIX_C_SOURCE 200809L

#include <sqlite3.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "trainlog/database.h"

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct Projection {
    char *text;
    size_t length;
    size_t capacity;
} Projection;

typedef struct ProjectionSpec {
    const char *label;
    const char *sql;
} ProjectionSpec;

static bool projection_append(Projection *projection, const char *value, size_t length) {
    size_t required;
    size_t capacity;
    char *resized;

    if (length > SIZE_MAX - projection->length - 1U) {
        return false;
    }
    required = projection->length + length + 1U;
    if (required > projection->capacity) {
        capacity = projection->capacity == 0U ? 4096U : projection->capacity;
        while (capacity < required) {
            if (capacity > SIZE_MAX / 2U) {
                return false;
            }
            capacity *= 2U;
        }
        resized = realloc(projection->text, capacity);
        if (resized == NULL) {
            return false;
        }
        projection->text = resized;
        projection->capacity = capacity;
    }
    (void)memcpy(projection->text + projection->length, value, length);
    projection->length += length;
    projection->text[projection->length] = '\0';
    return true;
}

static bool projection_append_value(Projection *projection, sqlite3_stmt *statement, int column) {
    char number[128];
    const unsigned char *text;
    int length;
    int written;

    switch (sqlite3_column_type(statement, column)) {
    case SQLITE_NULL:
        return projection_append(projection, "N;", 2U);
    case SQLITE_INTEGER:
        written = snprintf(
            number, sizeof(number), "I%lld;", (long long)sqlite3_column_int64(statement, column));
        return written > 0 && (size_t)written < sizeof(number) &&
               projection_append(projection, number, (size_t)written);
    case SQLITE_FLOAT:
        written =
            snprintf(number, sizeof(number), "F%.17g;", sqlite3_column_double(statement, column));
        return written > 0 && (size_t)written < sizeof(number) &&
               projection_append(projection, number, (size_t)written);
    case SQLITE_TEXT:
        text = sqlite3_column_text(statement, column);
        length = sqlite3_column_bytes(statement, column);
        written = snprintf(number, sizeof(number), "T%d:", length);
        return text != NULL && length >= 0 && written > 0 && (size_t)written < sizeof(number) &&
               projection_append(projection, number, (size_t)written) &&
               projection_append(projection, (const char *)text, (size_t)length) &&
               projection_append(projection, ";", 1U);
    default:
        return false;
    }
}

/* WHY: row counts cannot prove that nullable targets, floating-point loads, or
 * ordered identities survived. CONTRACT: this helper records SQLite types,
 * byte lengths, values, columns, and row boundaries for an explicitly ordered
 * business query. INVARIANT: equal projections imply field-for-field equality. */
static bool capture_projection(sqlite3 *database,
                               const ProjectionSpec *specifications,
                               size_t specification_count,
                               Projection *projection) {
    sqlite3_stmt *statement = NULL;
    size_t index;
    int column;
    int result;

    for (index = 0U; index < specification_count; ++index) {
        if (!projection_append(
                projection, specifications[index].label, strlen(specifications[index].label)) ||
            !projection_append(projection, "\n", 1U) ||
            sqlite3_prepare_v2(database, specifications[index].sql, -1, &statement, NULL) !=
                SQLITE_OK) {
            (void)sqlite3_finalize(statement);
            return false;
        }
        while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
            for (column = 0; column < sqlite3_column_count(statement); ++column) {
                if (!projection_append_value(projection, statement, column)) {
                    (void)sqlite3_finalize(statement);
                    return false;
                }
            }
            if (!projection_append(projection, "\n", 1U)) {
                (void)sqlite3_finalize(statement);
                return false;
            }
        }
        if (result != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
            statement = NULL;
            return false;
        }
        statement = NULL;
    }
    return true;
}

static bool execute(sqlite3 *database, const char *sql) {
    char *error = NULL;
    int result = sqlite3_exec(database, sql, NULL, NULL, &error);

    if (result != SQLITE_OK) {
        (void)fprintf(stderr, "fixture SQL failed: %s\n", error == NULL ? "no diagnostic" : error);
    }
    sqlite3_free(error);
    return result == SQLITE_OK;
}

static int scalar(sqlite3 *database, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int value = -1;

    if (sqlite3_prepare_v2(database, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int(statement, 0);
    }
    (void)sqlite3_finalize(statement);
    return value;
}

static bool insert_program_sessions(sqlite3 *database) {
    static const char INSERT_SESSION[] =
        "INSERT INTO program_sessions(program_session_id,program_id,position,title,session_type,"
        "planned_for,note) VALUES(?1,'pg_fixture',?2,?3,?4,?5,?6)";
    static const char INSERT_ENTRY[] =
        "INSERT INTO program_session_entries(program_session_id,entry_id,position,exercise_id,"
        "equipment_id,recording_mode,tracking_mode,data_fields,load_mode,rest_seconds,target_sets,"
        "target_reps,target_duration_seconds,target_weight_kg,notes)"
        " VALUES(?1,?2,?3,?4,?5,'sets',?6,?7,?8,?9,?10,?11,?12,?13,?14)";
    sqlite3_stmt *session = NULL;
    sqlite3_stmt *entry = NULL;
    char session_id[32];
    char entry_id[32];
    char title[64];
    char planned_for[32];
    char note[96];
    int position;
    int occurrence;
    bool ok = false;

    if (sqlite3_prepare_v2(database, INSERT_SESSION, -1, &session, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(database, INSERT_ENTRY, -1, &entry, NULL) != SQLITE_OK) {
        goto cleanup;
    }
    for (position = 0; position < 24; ++position) {
        (void)snprintf(session_id, sizeof(session_id), "pgs_fixture_%02d", position + 1);
        (void)snprintf(
            title, sizeof(title), "Week %d · Session %d", position / 3 + 1, position + 1);
        (void)snprintf(planned_for, sizeof(planned_for), "2026-10-%02dT18:00:00Z", position + 1);
        (void)snprintf(
            note, sizeof(note), "Progression block %d, session %d", position / 6 + 1, position + 1);
        (void)sqlite3_bind_text(session, 1, session_id, -1, SQLITE_TRANSIENT);
        (void)sqlite3_bind_int(session, 2, position);
        (void)sqlite3_bind_text(session, 3, title, -1, SQLITE_TRANSIENT);
        (void)sqlite3_bind_text(
            session, 4, position == 23 ? "max_test" : "training", -1, SQLITE_STATIC);
        (void)sqlite3_bind_text(session, 5, planned_for, -1, SQLITE_TRANSIENT);
        (void)sqlite3_bind_text(session, 6, note, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(session) != SQLITE_DONE || sqlite3_reset(session) != SQLITE_OK ||
            sqlite3_clear_bindings(session) != SQLITE_OK) {
            goto cleanup;
        }
        for (occurrence = 0; occurrence < 2; ++occurrence) {
            (void)snprintf(entry_id, sizeof(entry_id), "pge_%02d_%d", position + 1, occurrence + 1);
            (void)snprintf(note,
                           sizeof(note),
                           occurrence == 0 ? "Controlled eccentric, RPE %d" : "Timed brace, RPE %d",
                           7 + position % 3);
            (void)sqlite3_bind_text(entry, 1, session_id, -1, SQLITE_TRANSIENT);
            (void)sqlite3_bind_text(entry, 2, entry_id, -1, SQLITE_TRANSIENT);
            (void)sqlite3_bind_int(entry, 3, occurrence);
            (void)sqlite3_bind_text(entry,
                                    4,
                                    occurrence == 0 ? "ex_fixture_press" : "ex_fixture_hold",
                                    -1,
                                    SQLITE_STATIC);
            (void)sqlite3_bind_text(entry,
                                    5,
                                    occurrence == 0 ? "eq_fixture_stack" : "eq_fixture_mat",
                                    -1,
                                    SQLITE_STATIC);
            (void)sqlite3_bind_text(
                entry, 6, occurrence == 0 ? "reps" : "duration", -1, SQLITE_STATIC);
            (void)sqlite3_bind_int(entry, 7, occurrence == 0 ? 1 : 2);
            (void)sqlite3_bind_text(
                entry, 8, occurrence == 0 ? "external" : "none", -1, SQLITE_STATIC);
            (void)sqlite3_bind_int(entry, 9, occurrence == 0 ? 90 + position : 45 + position);
            (void)sqlite3_bind_int(entry, 10, occurrence == 0 ? 4 : 3);
            if (occurrence == 0) {
                (void)sqlite3_bind_int(entry, 11, 8 + position % 5);
                (void)sqlite3_bind_null(entry, 12);
                (void)sqlite3_bind_double(entry, 13, 42.5 + (double)position * 1.25);
            } else {
                (void)sqlite3_bind_null(entry, 11);
                (void)sqlite3_bind_int(entry, 12, 30 + position);
                (void)sqlite3_bind_null(entry, 13);
            }
            (void)sqlite3_bind_text(entry, 14, note, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(entry) != SQLITE_DONE || sqlite3_reset(entry) != SQLITE_OK ||
                sqlite3_clear_bindings(entry) != SQLITE_OK) {
                goto cleanup;
            }
        }
    }
    ok = true;
cleanup:
    (void)sqlite3_finalize(entry);
    (void)sqlite3_finalize(session);
    return ok;
}

/* CONTRACT: construct a populated v25 database without consulting any user
 * path. The current initializer supplies all older schema objects; removing
 * exactly the v26 additions recreates the immediate predecessor boundary. */
static bool build_v25_fixture(const char *path) {
    TrainlogDatabase *production = NULL;
    sqlite3 *database = NULL;
    bool ok = false;

    if (trainlog_database_open(path, &production) != TRAINLOG_STATUS_OK) {
        return false;
    }
    if (trainlog_database_insert_exercise_profiled(production,
                                                   "ex_fixture_press",
                                                   "Cable press",
                                                   "cable press",
                                                   TRAINLOG_TRACKING_REPS,
                                                   TRAINLOG_RECORDING_SETS,
                                                   1U) != TRAINLOG_STATUS_OK ||
        trainlog_database_insert_exercise_profiled(production,
                                                   "ex_fixture_hold",
                                                   "Pallof hold",
                                                   "pallof hold",
                                                   TRAINLOG_TRACKING_DURATION,
                                                   TRAINLOG_RECORDING_SETS,
                                                   2U) != TRAINLOG_STATUS_OK) {
        trainlog_database_close(production);
        return false;
    }
    trainlog_database_close(production);
    if (sqlite3_open(path, &database) != SQLITE_OK ||
        !execute(database,
                 "PRAGMA foreign_keys=OFF;"
                 "DROP INDEX program_deletions_pending;"
                 "DROP TABLE program_deletions;"
                 "DROP INDEX program_session_executions_program;"
                 "DROP TABLE program_session_executions;"
                 "ALTER TABLE programs DROP COLUMN deleted_at;"
                 "PRAGMA user_version=25;"
                 "PRAGMA foreign_keys=ON;") ||
        !execute(
            database,
            "BEGIN IMMEDIATE;"
            "INSERT INTO custom_equipment VALUES"
            "('eq_fixture_stack','Cable stack','Stack','machine','external'),"
            "('eq_fixture_mat','Exercise mat','Mat','surface','none');"
            "INSERT INTO sessions(id,session_id,started_at,ended_at,session_type,notes) VALUES"
            "(201,'se_fixture_history','2026-09-12T17:00:00Z','2026-09-12T17:52:00Z',"
            "'training','Completed history sentinel');"
            "INSERT INTO session_exercises(id,entry_id,session_row_id,exercise_row_id,"
            "recording_mode,data_fields,position,load_mode,rest_seconds,target_sets,target_reps,"
            "target_duration_seconds,target_weight_kg,equipment_id,notes,tracking_mode) VALUES"
            "(301,'sxe_fixture_press',201,(SELECT id FROM exercises WHERE "
            "exercise_id='ex_fixture_press'),'sets',1,0,'external',105,4,10,NULL,47.5,"
            "'eq_fixture_stack','Historic occurrence sentinel','reps'),"
            "(302,'sxe_fixture_hold',201,(SELECT id FROM exercises WHERE "
            "exercise_id='ex_fixture_hold'),'sets',2,1,'none',45,3,NULL,35,NULL,"
            "'eq_fixture_mat','Historic duration sentinel','duration');"
            "INSERT INTO performed_sets(id,session_exercise_row_id,position,reps,duration_seconds,"
            "weight_kg) VALUES(401,301,0,10,NULL,47.5),(402,301,1,9,NULL,47.5),"
            "(403,302,0,NULL,35,NULL);"
            "INSERT INTO body_observations(id,observation_id,observed_at,session_row_id,"
            "body_weight_kg,waist_cm,left_arm_cm,right_arm_cm,notes) VALUES"
            "(501,'bo_fixture','2026-09-12T16:55:00Z',201,78.4,82.1,35.2,35.4,"
            "'Historical body sentinel');"
            "INSERT INTO programs(program_id,title,note,state,start_date,end_date,created_at,"
            "updated_at,revision_id,source_format,source_version,source_payload_sha256) VALUES"
            "('pg_fixture','Eight-week strength progression','Imported coach plan','active',"
            "'2026-10-01','2026-11-30','2026-09-15T08:00:00Z','2026-09-16T09:30:00Z',"
            "'pgr_fixture_2','trainlog-program',1,"
            "'0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef');") ||
        !insert_program_sessions(database) ||
        !execute(
            database,
            "INSERT INTO session_preparations(preparation_id,current_revision_id,created_at,"
            "updated_at,editing_state,delivery_state,source_proposal_id,source_payload_sha256,"
            "withdrawn_at,source_program_id,source_program_session_id) VALUES"
            "('prep_fixture','prepr_fixture_2','2026-09-17T08:00:00Z',"
            "'2026-09-17T08:15:00Z','ready','local',NULL,NULL,NULL,'pg_fixture','pgs_fixture_07');"
            "INSERT INTO session_preparation_revisions(revision_id,preparation_id,"
            "parent_revision_id,title,session_type,planned_for,notes,created_at) VALUES"
            "('prepr_fixture_1','prep_fixture',NULL,'Week 3 · Session 7','training',"
            "'2026-10-07T18:00:00Z','Initial Program-derived revision','2026-09-17T08:00:00Z'),"
            "('prepr_fixture_2','prep_fixture','prepr_fixture_1','Week 3 · Session 7 adjusted',"
            "'training','2026-10-07T18:30:00Z','Adjusted after review','2026-09-17T08:15:00Z');"
            "INSERT INTO session_preparation_entries(revision_id,entry_id,position,exercise_id,"
            "equipment_id,recording_mode,tracking_mode,data_fields,load_mode,rest_seconds,"
            "target_sets,target_reps,target_duration_seconds,target_weight_kg,notes) VALUES"
            "('prepr_fixture_1','prepe_fixture_1a',0,'ex_fixture_press','eq_fixture_stack',"
            "'sets','reps',1,'external',96,4,10,NULL,50.0,'Program provenance press'),"
            "('prepr_fixture_1','prepe_fixture_1b',1,'ex_fixture_hold','eq_fixture_mat',"
            "'sets','duration',2,'none',51,3,NULL,36,NULL,'Program provenance hold'),"
            "('prepr_fixture_2','prepe_fixture_2a',0,'ex_fixture_press','eq_fixture_stack',"
            "'sets','reps',1,'external',100,5,8,NULL,52.5,'Adjusted press'),"
            "('prepr_fixture_2','prepe_fixture_2b',1,'ex_fixture_hold','eq_fixture_mat',"
            "'sets','duration',2,'none',55,3,NULL,40,NULL,'Adjusted hold');"
            "COMMIT;") ||
        scalar(database, "PRAGMA user_version") != 25 ||
        scalar(database, "SELECT COUNT(*) FROM program_sessions") != 24 ||
        scalar(database, "SELECT COUNT(*) FROM program_session_entries") != 48 ||
        scalar(database, "SELECT COUNT(*) FROM pragma_foreign_key_check") != 0) {
        goto cleanup;
    }
    ok = true;
cleanup:
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) {
        ok = false;
    }
    return ok;
}

static int run_test(void) {
    static const ProjectionSpec BUSINESS_PROJECTIONS[] = {
        {"programs",
         "SELECT program_id,title,note,state,start_date,end_date,created_at,updated_at,revision_id,"
         "source_format,source_version,source_payload_sha256 FROM programs ORDER BY program_id"},
        {"program_sessions",
         "SELECT * FROM program_sessions ORDER BY program_id,position,program_session_id"},
        {"program_session_entries",
         "SELECT * FROM program_session_entries ORDER BY program_session_id,position,entry_id"},
        {"session_preparations", "SELECT * FROM session_preparations ORDER BY preparation_id"},
        {"session_preparation_revisions",
         "SELECT * FROM session_preparation_revisions ORDER BY "
         "preparation_id,created_at,revision_id"},
        {"session_preparation_entries",
         "SELECT * FROM session_preparation_entries ORDER BY revision_id,position,entry_id"},
        {"exercises", "SELECT * FROM exercises ORDER BY id"},
        {"custom_equipment", "SELECT * FROM custom_equipment ORDER BY equipment_id"},
        {"sessions", "SELECT * FROM sessions ORDER BY id"},
        {"session_exercises", "SELECT * FROM session_exercises ORDER BY id"},
        {"performed_sets", "SELECT * FROM performed_sets ORDER BY id"},
        {"body_observations", "SELECT * FROM body_observations ORDER BY id"},
        {"profile_state", "SELECT * FROM exercise_profile_state ORDER BY exercise_row_id"},
        {"profile_revisions",
         "SELECT * FROM exercise_profile_revisions ORDER BY exercise_row_id,revision_id"},
    };
    static const ProjectionSpec UNCHANGED_SCHEMA[] = {
        {"schema",
         "SELECT type,name,tbl_name,sql FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' "
         "AND name NOT IN('programs','program_deletions','program_deletions_pending',"
         "'program_session_executions','program_session_executions_program') "
         "ORDER BY type,name"},
    };
    char path[] = "/tmp/trainlog-v26-populated-XXXXXX";
    char diagnostic[512] = {0};
    int file_descriptor = -1;
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    Projection business_before = {0};
    Projection business_after = {0};
    Projection schema_before = {0};
    Projection schema_after = {0};
    int version = 0;
    int schema_object_count_before = 0;
    bool ok = false;

    file_descriptor = mkstemp(path);
    CHECK(file_descriptor >= 0);
    CHECK(close(file_descriptor) == 0);
    file_descriptor = -1;
    CHECK(build_v25_fixture(path));
    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "PRAGMA user_version") == 25);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('programs')") == 12);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE name IN('program_deletions',"
                 "'program_deletions_pending')") == 0);
    schema_object_count_before =
        scalar(raw, "SELECT COUNT(*) FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'");
    CHECK(schema_object_count_before > 0);
    CHECK(capture_projection(raw,
                             BUSINESS_PROJECTIONS,
                             sizeof(BUSINESS_PROJECTIONS) / sizeof(BUSINESS_PROJECTIONS[0]),
                             &business_before));
    CHECK(capture_projection(raw, UNCHANGED_SCHEMA, 1U, &schema_before));
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(trainlog_database_open_with_diagnostic(path, &database, diagnostic, sizeof(diagnostic)) ==
          TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(database, &version) == TRAINLOG_STATUS_OK);
    CHECK(version == TRAINLOG_DATABASE_SCHEMA_VERSION);
    trainlog_database_close(database);
    database = NULL;

    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "PRAGMA user_version") == TRAINLOG_DATABASE_SCHEMA_VERSION);
    CHECK(capture_projection(raw,
                             BUSINESS_PROJECTIONS,
                             sizeof(BUSINESS_PROJECTIONS) / sizeof(BUSINESS_PROJECTIONS[0]),
                             &business_after));
    CHECK(capture_projection(raw, UNCHANGED_SCHEMA, 1U, &schema_after));
    CHECK(business_before.length == business_after.length);
    CHECK(memcmp(business_before.text, business_after.text, business_before.length) == 0);
    CHECK(schema_before.length == schema_after.length);
    CHECK(memcmp(schema_before.text, schema_after.text, schema_before.length) == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_integrity_check WHERE integrity_check='ok'") ==
          1);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM programs WHERE deleted_at IS NOT NULL") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM program_deletions") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM program_session_executions") == 0);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM pragma_table_info('programs') WHERE name='deleted_at' AND "
                 "type='TEXT' AND \"notnull\"=0") == 1);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE (type='table' AND "
                 "name='program_deletions') OR (type='index' AND "
                 "name='program_deletions_pending')") == 2);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('programs')") == 13);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'") ==
          schema_object_count_before + 4);
    ok = true;
cleanup:
    if (!ok && diagnostic[0] != '\0') {
        (void)fprintf(stderr, "migration diagnostic: %s\n", diagnostic);
    }
    free(schema_after.text);
    free(schema_before.text);
    free(business_after.text);
    free(business_before.text);
    if (raw != NULL) {
        (void)sqlite3_close(raw);
    }
    if (database != NULL) {
        trainlog_database_close(database);
    }
    if (file_descriptor >= 0) {
        (void)close(file_descriptor);
    }
    if (unlink(path) != 0) {
        ok = false;
    }
    return ok ? 0 : 1;
}

int main(void) {
    return run_test();
}
