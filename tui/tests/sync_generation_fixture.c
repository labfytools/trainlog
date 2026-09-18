/* Test-only producer fixture through the public desktop persistence API. */
#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trainlog/database.h"
#include "trainlog/web_programs.h"

static const char PROGRAM_JSON[] =
    "{\"format\":\"trainlog-program\",\"version\":1,\"program\":{"
    "\"program_id\":\"pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\","
    "\"title\":\"Generation program\",\"note\":null,\"state\":\"active\","
    "\"start_date\":\"2026-09-21\",\"end_date\":null,\"sessions\":[{"
    "\"program_session_id\":\"pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb\","
    "\"title\":\"Session A\",\"session_type\":\"training\","
    "\"planned_for\":null,\"note\":null,\"occurrences\":[{"
    "\"entry_id\":\"pge_cccccccc-cccc-4ccc-8ccc-cccccccccccc\","
    "\"exercise_id\":\"ex_dddddddd-dddd-4ddd-8ddd-dddddddddddd\","
    "\"equipment_id\":null,\"load_mode\":\"external\",\"rest_seconds\":90,"
    "\"target_sets\":3,\"target_reps\":8,\"target_duration_seconds\":null,"
    "\"target_weight_kg\":40.0,\"notes\":null}]}]}}";

static int create_program(TrainlogDatabase *database) {
    char *response = NULL;
    size_t response_size = 0U;
    TrainlogStatus status;

    status = trainlog_database_insert_exercise_profiled(database,
                                                        "ex_dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                                                        "Program exercise fixture",
                                                        "program exercise fixture",
                                                        TRAINLOG_TRACKING_REPS,
                                                        TRAINLOG_RECORDING_SETS,
                                                        0U);
    if (status != TRAINLOG_STATUS_OK) {
        return 3;
    }
    status = trainlog_web_programs_import_json(
        database, PROGRAM_JSON, strlen(PROGRAM_JSON), true, &response, &response_size);
    free(response);
    response = NULL;
    if (status != TRAINLOG_STATUS_OK ||
        trainlog_web_programs_detail_json(
            database, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &response, &response_size) !=
            TRAINLOG_STATUS_OK) {
        free(response);
        return 4;
    }
    (void)printf("PROGRAM_CREATE=%s\n", response);
    free(response);
    return 0;
}

static int
delete_program(TrainlogDatabase *database, const char *expected_revision, const char *request_id) {
    char *response = NULL;
    size_t response_size = 0U;
    TrainlogStatus status =
        trainlog_web_programs_delete_json(database,
                                          "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                          expected_revision,
                                          request_id,
                                          &response,
                                          &response_size);

    if (status != TRAINLOG_STATUS_OK) {
        free(response);
        return 5;
    }
    (void)printf("PROGRAM_DELETE=%s\n", response);
    free(response);
    return 0;
}

static int print_program_detail(TrainlogDatabase *database) {
    char *response = NULL;
    size_t response_size = 0U;
    TrainlogStatus status = trainlog_web_programs_detail_json(
        database, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &response, &response_size);

    if (status != TRAINLOG_STATUS_OK) {
        free(response);
        return 6;
    }
    (void)printf("PROGRAM_DETAIL=%s\n", response);
    free(response);
    return 0;
}

static int create_session_fixture(TrainlogDatabase *database) {
    TrainlogSetInput set = {.reps = 9, .has_weight = true, .weight_kg = 32.5};
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;

    if (trainlog_database_insert_exercise_profiled(database,
                                                   "ex_77777777-7777-4777-8777-777777777777",
                                                   "Desktop generation fixture",
                                                   "desktop generation fixture",
                                                   TRAINLOG_TRACKING_REPS,
                                                   TRAINLOG_RECORDING_SETS,
                                                   0U) != TRAINLOG_STATUS_OK) {
        return 3;
    }
    (void)memset(&exercise, 0, sizeof(exercise));
    (void)snprintf(exercise.entry_id,
                   sizeof(exercise.entry_id),
                   "%s",
                   "sxe_88888888-8888-4888-8888-888888888888");
    (void)snprintf(exercise.exercise_id,
                   sizeof(exercise.exercise_id),
                   "%s",
                   "ex_77777777-7777-4777-8777-777777777777");
    exercise.recording_mode = TRAINLOG_RECORDING_SETS;
    exercise.tracking_mode = TRAINLOG_TRACKING_REPS;
    exercise.sets = &set;
    exercise.set_count = 1U;
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(session.session_id,
                   sizeof(session.session_id),
                   "%s",
                   "se_99999999-9999-4999-8999-999999999999");
    (void)snprintf(
        session.started_at, sizeof(session.started_at), "%s", "2026-09-17T10:00:00+02:00");
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s", "2026-09-17T10:30:00+02:00");
    session.exercises = &exercise;
    session.exercise_count = 1U;
    if (trainlog_database_insert_session(database, &session) != TRAINLOG_STATUS_OK) {
        return 4;
    }
    (void)puts("SYNC_GENERATION_FIXTURE=PASS");
    return 0;
}

static int
check_integrity(sqlite3 *connection, bool *output_integrity_ok, bool *output_foreign_keys_ok) {
    sqlite3_stmt *statement = NULL;
    const unsigned char *integrity_result;
    int rc;

    *output_integrity_ok = false;
    *output_foreign_keys_ok = false;
    rc = sqlite3_prepare_v2(connection, "PRAGMA integrity_check;", -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return 1;
    }
    rc = sqlite3_step(statement);
    integrity_result = rc == SQLITE_ROW ? sqlite3_column_text(statement, 0) : NULL;
    *output_integrity_ok =
        integrity_result != NULL && strcmp((const char *)integrity_result, "ok") == 0;
    if (sqlite3_finalize(statement) != SQLITE_OK || !*output_integrity_ok) {
        return 1;
    }

    rc = sqlite3_prepare_v2(connection, "PRAGMA foreign_key_check;", -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        return 1;
    }
    rc = sqlite3_step(statement);
    *output_foreign_keys_ok = rc == SQLITE_DONE;
    if (sqlite3_finalize(statement) != SQLITE_OK || !*output_foreign_keys_ok) {
        return 1;
    }
    return 0;
}

static int migrate_only(const char *path) {
    TrainlogDatabase *database = NULL;
    char diagnostic[256];
    sqlite3 *inspection = NULL;
    bool integrity_ok;
    bool foreign_key_check_ok;
    int foreign_keys_enabled;
    int schema_version;
    int result = 2;

    if (trainlog_database_open_with_diagnostic(path, &database, diagnostic, sizeof(diagnostic)) !=
        TRAINLOG_STATUS_OK) {
        return result;
    }
    if (trainlog_database_schema_version(database, &schema_version) != TRAINLOG_STATUS_OK ||
        trainlog_database_foreign_keys_enabled(database, &foreign_keys_enabled) !=
            TRAINLOG_STATUS_OK) {
        goto cleanup;
    }
    trainlog_database_close(database);
    database = NULL;

    if (sqlite3_open_v2(path, &inspection, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL) !=
        SQLITE_OK) {
        goto cleanup;
    }
    if (check_integrity(inspection, &integrity_ok, &foreign_key_check_ok) != 0) {
        goto cleanup;
    }
    (void)printf(
        "MIGRATE_ONLY schema_version=%d integrity=%s foreign_keys=%s foreign_key_check=%s\n",
        schema_version,
        integrity_ok ? "ok" : "failed",
        foreign_keys_enabled != 0 ? "enabled" : "disabled",
        foreign_key_check_ok ? "ok" : "failed");
    result = 0;

cleanup:
    if (inspection != NULL) {
        (void)sqlite3_close(inspection);
    }
    if (database != NULL) {
        trainlog_database_close(database);
    }
    return result;
}

int main(int argc, char **argv) {
    TrainlogDatabase *database = NULL;
    int result;

    if (argc == 3 && strcmp(argv[2], "migrate-only") == 0) {
        return migrate_only(argv[1]);
    }
    if ((argc != 2 && argc != 3 && argc != 5) ||
        trainlog_database_open(argv[1], &database) != TRAINLOG_STATUS_OK) {
        return 2;
    }
    if (argc == 2) {
        result = create_session_fixture(database);
    } else if (argc == 3 && strcmp(argv[2], "program-create") == 0) {
        result = create_program(database);
    } else if (argc == 3 && strcmp(argv[2], "program-detail") == 0) {
        result = print_program_detail(database);
    } else if (argc == 5 && strcmp(argv[2], "program-delete") == 0) {
        result = delete_program(database, argv[3], argv[4]);
    } else {
        result = 2;
    }
    trainlog_database_close(database);
    return result;
}
