#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "trainlog/database.h"
#include "trainlog/web_programs.h"
#include "trainlog/web_sessions.h"

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static const char PROGRAM_JSON[] =
    "{\"format\":\"trainlog-program\",\"version\":1,\"program\":{"
    "\"program_id\":\"pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\","
    "\"title\":\"Cycle lisible\",\"note\":null,\"state\":\"active\","
    "\"start_date\":\"2026-09-21\",\"end_date\":null,\"sessions\":[{"
    "\"program_session_id\":\"pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb\","
    "\"title\":\"Séance A\",\"session_type\":\"training\","
    "\"planned_for\":null,\"note\":null,\"occurrences\":[{"
    "\"entry_id\":\"pge_cccccccc-cccc-4ccc-8ccc-cccccccccccc\","
    "\"exercise_id\":\"ex_dddddddd-dddd-4ddd-8ddd-dddddddddddd\","
    "\"equipment_id\":null,\"load_mode\":\"external\",\"rest_seconds\":90,"
    "\"target_sets\":3,\"target_reps\":8,\"target_duration_seconds\":null,"
    "\"target_weight_kg\":40.0,\"notes\":null}]}]}}";

static const char MIXED_PROGRAM_JSON[] =
    "{\"format\":\"trainlog-program\",\"version\":1,\"program\":{"
    "\"program_id\":\"pg_10000000-0000-4000-8000-000000000001\","
    "\"title\":\"Mixed profile cycle\",\"note\":null,\"state\":\"active\","
    "\"start_date\":\"2026-09-21\",\"end_date\":null,\"sessions\":[{"
    "\"program_session_id\":\"pgs_10000000-0000-4000-8000-000000000002\","
    "\"title\":\"Mixed profile session\",\"session_type\":\"training\","
    "\"planned_for\":\"2026-09-21\",\"note\":\"Regression fixture\","
    "\"occurrences\":[{"
    "\"entry_id\":\"pge_10000000-0000-4000-8000-000000000003\","
    "\"exercise_id\":\"ex_10000000-0000-4000-8000-000000000004\","
    "\"equipment_id\":null,\"load_mode\":\"none\",\"rest_seconds\":0,"
    "\"target_sets\":null,\"target_reps\":null,\"target_duration_seconds\":600,"
    "\"target_weight_kg\":null,\"notes\":\"Warm-up\"},{"
    "\"entry_id\":\"pge_10000000-0000-4000-8000-000000000005\","
    "\"exercise_id\":\"ex_10000000-0000-4000-8000-000000000006\","
    "\"equipment_id\":null,\"load_mode\":\"external\",\"rest_seconds\":90,"
    "\"target_sets\":3,\"target_reps\":10,\"target_duration_seconds\":null,"
    "\"target_weight_kg\":52,\"notes\":null},{"
    "\"entry_id\":\"pge_10000000-0000-4000-8000-000000000007\","
    "\"exercise_id\":\"ex_10000000-0000-4000-8000-000000000008\","
    "\"equipment_id\":null,\"load_mode\":\"none\",\"rest_seconds\":60,"
    "\"target_sets\":2,\"target_reps\":12,\"target_duration_seconds\":null,"
    "\"target_weight_kg\":null,\"notes\":\"Bodyweight\"}] }]}}";

static int scalar(TrainlogDatabase *database, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int value = -1;

    if (sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int(statement, 0);
    }
    (void)sqlite3_finalize(statement);
    return value;
}

static bool
extract_string(const char *json, const char *key, char *output, size_t output_capacity) {
    char marker[96];
    const char *start;
    const char *end;
    size_t length;

    if (snprintf(marker, sizeof(marker), "\"%s\":\"", key) < 0) {
        return false;
    }
    start = strstr(json, marker);
    if (start == NULL) {
        return false;
    }
    start += strlen(marker);
    end = strchr(start, '"');
    if (end == NULL) {
        return false;
    }
    length = (size_t)(end - start);
    if (length == 0U || length >= output_capacity) {
        return false;
    }
    (void)memcpy(output, start, length);
    output[length] = '\0';
    return true;
}

static char *replace_once(const char *source, const char *needle, const char *replacement) {
    const char *match = strstr(source, needle);
    size_t prefix_size;
    size_t output_size;
    char *output;

    if (match == NULL) {
        return NULL;
    }
    prefix_size = (size_t)(match - source);
    output_size = strlen(source) - strlen(needle) + strlen(replacement) + 1U;
    output = malloc(output_size);
    if (output == NULL) {
        return NULL;
    }
    (void)snprintf(output,
                   output_size,
                   "%.*s%s%s",
                   (int)prefix_size,
                   source,
                   replacement,
                   match + strlen(needle));
    return output;
}

static bool strict_import_rejections(void) {
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    size_t response_size = 0U;
    char *unknown_exercise;
    char *unknown_equipment;
    char *performed_data;
    char *divergent;
    char *oversized;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     "ex_dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                                                     "Exercise fixture",
                                                     "exercise fixture",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_web_programs_import_json(database, "{", 1U, true, &response, &response_size) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_web_programs_import_json(
              database,
              "{\"format\":\"trainlog-program\",\"format\":\"trainlog-program\","
              "\"version\":1,\"program\":{}}",
              strlen("{\"format\":\"trainlog-program\",\"format\":\"trainlog-program\","
                     "\"version\":1,\"program\":{}}"),
              true,
              &response,
              &response_size) == TRAINLOG_STATUS_INVALID_ARGUMENT);

    unknown_exercise = replace_once(PROGRAM_JSON,
                                    "ex_dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                                    "ex_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee");
    unknown_equipment =
        replace_once(PROGRAM_JSON, "\"equipment_id\":null", "\"equipment_id\":\"missing\"");
    performed_data = replace_once(PROGRAM_JSON, "\"notes\":null", "\"notes\":null,\"sets\":[]");
    divergent = replace_once(PROGRAM_JSON, "Cycle lisible", "Cycle divergent");
    CHECK(unknown_exercise != NULL && unknown_equipment != NULL && performed_data != NULL &&
          divergent != NULL);
    CHECK(trainlog_web_programs_import_json(database,
                                            unknown_exercise,
                                            strlen(unknown_exercise),
                                            true,
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_web_programs_import_json(database,
                                            unknown_equipment,
                                            strlen(unknown_equipment),
                                            true,
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_web_programs_import_json(
              database, performed_data, strlen(performed_data), true, &response, &response_size) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 0);

    CHECK(trainlog_web_programs_import_json(
              database, PROGRAM_JSON, strlen(PROGRAM_JSON), true, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    free(response);
    response = NULL;
    CHECK(trainlog_web_programs_import_json(
              database, divergent, strlen(divergent), true, &response, &response_size) ==
          TRAINLOG_STATUS_CONFLICT);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 1);

    oversized = malloc(TRAINLOG_WEB_PROGRAM_BYTES_MAX + 2U);
    CHECK(oversized != NULL);
    (void)memset(oversized, ' ', TRAINLOG_WEB_PROGRAM_BYTES_MAX + 1U);
    oversized[TRAINLOG_WEB_PROGRAM_BYTES_MAX + 1U] = '\0';
    CHECK(trainlog_web_programs_import_json(database,
                                            oversized,
                                            TRAINLOG_WEB_PROGRAM_BYTES_MAX + 1U,
                                            true,
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_INVALID_ARGUMENT);

    free(oversized);
    free(divergent);
    free(performed_data);
    free(unknown_equipment);
    free(unknown_exercise);
    trainlog_database_close(database);
    return true;
}

static bool import_archive_and_prepare(void) {
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    size_t response_size = 0U;
    char revision[128];
    char preparation_id[128];
    char *second_program;
    char *second_session;
    char *second_entry;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(scalar(database, "PRAGMA user_version") == TRAINLOG_DATABASE_SCHEMA_VERSION);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM pragma_table_info('program_deletions') WHERE "
                 "name IN('generation_id','acknowledged_at')") == 2);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND "
                 "name='program_deletions_pending'") == 1);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     "ex_dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                                                     "Exercise fixture",
                                                     "exercise fixture",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0U) == TRAINLOG_STATUS_OK);

    CHECK(trainlog_web_programs_import_json(
              database, PROGRAM_JSON, strlen(PROGRAM_JSON), false, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(response_size > 0U);
    CHECK(strstr(response, "\"imported\":false") != NULL);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 0);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_import_json(
              database, PROGRAM_JSON, strlen(PROGRAM_JSON), true, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"imported\":true") != NULL);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_session_entries") == 1);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_import_json(
              database, PROGRAM_JSON, strlen(PROGRAM_JSON), true, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"imported\":false") != NULL);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 1);
    free(response);
    response = NULL;

    second_program = replace_once(PROGRAM_JSON,
                                  "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                  "pg_11111111-1111-4111-8111-111111111111");
    CHECK(second_program != NULL);
    second_session = replace_once(second_program,
                                  "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                                  "pgs_22222222-2222-4222-8222-222222222222");
    free(second_program);
    CHECK(second_session != NULL);
    second_entry = replace_once(second_session,
                                "pge_cccccccc-cccc-4ccc-8ccc-cccccccccccc",
                                "pge_33333333-3333-4333-8333-333333333333");
    free(second_session);
    CHECK(second_entry != NULL);
    CHECK(trainlog_web_programs_import_json(
              database, second_entry, strlen(second_entry), true, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    free(second_entry);
    free(response);
    response = NULL;
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 2);
    CHECK(trainlog_web_programs_list_json(
              database, "Cycle", "active", 0U, 1U, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"more\":true") != NULL);
    CHECK(strstr(response, "\"next_offset\":1") != NULL);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_detail_json(
              database, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(extract_string(response, "revision_id", revision, sizeof(revision)));
    CHECK(strstr(response, "Exercise fixture") == NULL);
    CHECK(strstr(response, "\"execution_state\":\"todo\"") != NULL);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_prepare_json(database,
                                             "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                             "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                                             "request-prepare-readable",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_OK);
    CHECK(extract_string(response, "preparation_id", preparation_id, sizeof(preparation_id)));
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparations "
                 "WHERE source_program_id='pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'") == 1);
    free(response);
    response = NULL;
    CHECK(trainlog_web_programs_detail_json(
              database, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"execution_state\":\"prepared\"") != NULL);
    free(response);
    response = NULL;

    /* The linked workout owns its actual timestamp; planned_for must not be
     * rewritten to make the Program calendar agree with history. */
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO sessions(session_id,started_at,ended_at,session_type) "
                       "VALUES('se_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee',"
                       "'2026-09-23T23:30:00+02:00',"
                       "'2026-09-24T00:30:00+02:00','training');"
                       "INSERT INTO program_session_executions("
                       "program_session_id,program_id,session_id,state,observed_at) VALUES("
                       "'pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb',"
                       "'pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',"
                       "'se_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee','completed',"
                       "'2026-09-21T12:00:00Z')",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_programs_detail_json(
              database, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"execution_state\":\"completed\"") != NULL);
    CHECK(strstr(response,
                 "\"execution_session_id\":"
                 "\"se_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee\"") != NULL);
    CHECK(strstr(response, "\"execution_started_at\":\"2026-09-23T23:30:00+02:00\"") != NULL);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sessions WHERE session_id="
                 "'se_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee' AND "
                 "started_at='2026-09-23T23:30:00+02:00'") == 1);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_archive_json(database,
                                             "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                             revision,
                                             "request-archive-readable",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"state\":\"archived\"") != NULL);
    CHECK(extract_string(response, "revision_id", revision, sizeof(revision)));
    free(response);
    response = NULL;
    CHECK(trainlog_web_programs_prepare_json(database,
                                             "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                             "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                                             "request-prepare-after-archive",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_CONFLICT);

    CHECK(trainlog_web_programs_archive_json(database,
                                             "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                             revision,
                                             "request-archive-readable",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"state\":\"archived\"") != NULL);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_delete_json(database,
                                            "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                            revision,
                                            "request-delete-readable",
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"deleted_at\":") != NULL);
    free(response);
    response = NULL;
    CHECK(trainlog_web_programs_delete_json(database,
                                            "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                            revision,
                                            "request-delete-readable",
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_OK);
    free(response);
    response = NULL;
    CHECK(trainlog_web_programs_delete_json(database,
                                            "pg_11111111-1111-4111-8111-111111111111",
                                            revision,
                                            "request-delete-readable",
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_CONFLICT);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 2);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_sessions") == 2);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_session_entries") == 2);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparations") == 1);
    CHECK(trainlog_web_programs_list_json(database, "", "", 0U, 24U, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa") == NULL);
    CHECK(strstr(response, "\"preparation_count\":0") != NULL);
    free(response);
    response = NULL;
    CHECK(trainlog_web_programs_detail_json(
              database, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &response, &response_size) ==
          TRAINLOG_STATUS_CONFLICT);
    CHECK(trainlog_web_programs_prepare_json(database,
                                             "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                             "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                                             "request-prepare-after-delete",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_CONFLICT);
    CHECK(trainlog_web_programs_import_json(
              database, PROGRAM_JSON, strlen(PROGRAM_JSON), true, &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"imported\":false") != NULL);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs WHERE deleted_at IS NOT NULL") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_deletions WHERE request_id LIKE "
                 "'request-delete-%' AND generation_id IS NULL AND acknowledged_at IS NULL") == 1);
    free(response);
    trainlog_database_close(database);
    return true;
}

static bool insert_mixed_profile_exercises(TrainlogDatabase *database) {
    return trainlog_database_insert_exercise_profiled(database,
                                                      "ex_10000000-0000-4000-8000-000000000004",
                                                      "Continuous duration fixture",
                                                      "continuous duration fixture",
                                                      TRAINLOG_TRACKING_DURATION,
                                                      TRAINLOG_RECORDING_CONTINUOUS,
                                                      0U) == TRAINLOG_STATUS_OK &&
           trainlog_database_insert_exercise_profiled(database,
                                                      "ex_10000000-0000-4000-8000-000000000006",
                                                      "External reps fixture",
                                                      "external reps fixture",
                                                      TRAINLOG_TRACKING_REPS,
                                                      TRAINLOG_RECORDING_SETS,
                                                      0U) == TRAINLOG_STATUS_OK &&
           trainlog_database_insert_exercise_profiled(database,
                                                      "ex_10000000-0000-4000-8000-000000000008",
                                                      "Bodyweight reps fixture",
                                                      "bodyweight reps fixture",
                                                      TRAINLOG_TRACKING_REPS,
                                                      TRAINLOG_RECORDING_SETS,
                                                      0U) == TRAINLOG_STATUS_OK;
}

static bool mixed_profile_prepare_regression(void) {
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    char *replayed = NULL;
    char *detail = NULL;
    char *saved_response = NULL;
    size_t response_size = 0U;
    size_t replayed_size = 0U;
    size_t detail_size = 0U;
    char preparation_id[128];
    const char *continuous_position;
    const char *external_position;
    const char *bodyweight_position;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(insert_mixed_profile_exercises(database));
    CHECK(trainlog_web_programs_import_json(database,
                                            MIXED_PROGRAM_JSON,
                                            strlen(MIXED_PROGRAM_JSON),
                                            false,
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"imported\":false") != NULL);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 0);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_import_json(database,
                                            MIXED_PROGRAM_JSON,
                                            strlen(MIXED_PROGRAM_JSON),
                                            true,
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"imported\":true") != NULL);
    free(response);
    response = NULL;
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_session_entries") == 3);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_session_entries WHERE position=1 AND "
                 "typeof(target_weight_kg)='real' AND target_weight_kg=52.0") == 1);

    CHECK(trainlog_web_programs_prepare_json(database,
                                             "pg_10000000-0000-4000-8000-000000000001",
                                             "pgs_10000000-0000-4000-8000-000000000002",
                                             "request-mixed-prepare",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_OK);
    CHECK(extract_string(response, "preparation_id", preparation_id, sizeof(preparation_id)));
    saved_response = strdup(response);
    CHECK(saved_response != NULL);
    free(response);
    response = NULL;

    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparations") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_revisions") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_entries") == 3);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparations WHERE "
                 "source_program_id='pg_10000000-0000-4000-8000-000000000001' AND "
                 "source_program_session_id='pgs_10000000-0000-4000-8000-000000000002'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparation_entries WHERE "
                 "position=0 AND recording_mode='continuous' AND tracking_mode='duration' AND "
                 "load_mode='none' AND target_sets IS NULL AND target_reps IS NULL AND "
                 "target_duration_seconds=600 AND target_weight_kg IS NULL") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparation_entries WHERE "
                 "position=1 AND recording_mode='sets' AND tracking_mode='reps' AND "
                 "load_mode='external' AND target_sets=3 AND target_reps=10 AND "
                 "target_duration_seconds IS NULL AND target_weight_kg=52.0") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparation_entries WHERE "
                 "position=2 AND recording_mode='sets' AND tracking_mode='reps' AND "
                 "load_mode='none' AND target_sets=2 AND target_reps=12 AND "
                 "target_duration_seconds IS NULL AND target_weight_kg IS NULL") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparation_entries WHERE "
                 "entry_id LIKE 'spe_%'") == 3);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sessions") == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_deliveries") == 0);

    CHECK(trainlog_web_sessions_detail_json(
              database, "preparation", preparation_id, &detail, &detail_size) ==
          TRAINLOG_STATUS_OK);
    continuous_position = strstr(detail, "ex_10000000-0000-4000-8000-000000000004");
    external_position = strstr(detail, "ex_10000000-0000-4000-8000-000000000006");
    bodyweight_position = strstr(detail, "ex_10000000-0000-4000-8000-000000000008");
    CHECK(continuous_position != NULL && external_position != NULL && bodyweight_position != NULL);
    CHECK(continuous_position < external_position && external_position < bodyweight_position);
    free(detail);
    detail = NULL;

    CHECK(trainlog_web_programs_prepare_json(database,
                                             "pg_10000000-0000-4000-8000-000000000001",
                                             "pgs_10000000-0000-4000-8000-000000000002",
                                             "request-mixed-prepare",
                                             &replayed,
                                             &replayed_size) == TRAINLOG_STATUS_OK);
    CHECK(strcmp(saved_response, replayed) == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparations") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_revisions") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_entries") == 3);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_requests") == 1);

    free(replayed);
    free(saved_response);
    trainlog_database_close(database);
    return true;
}

static bool legacy_integer_weight_zero_prepares_editable_draft(void) {
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    size_t response_size = 0U;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(insert_mixed_profile_exercises(database));
    CHECK(trainlog_web_programs_import_json(database,
                                            MIXED_PROGRAM_JSON,
                                            strlen(MIXED_PROGRAM_JSON),
                                            true,
                                            &response,
                                            &response_size) == TRAINLOG_STATUS_OK);
    free(response);
    response = NULL;

    /* This impossible Program V1 value reproduces rows written by the former
     * integer-weight import bug. Production compatibility must not rewrite
     * the source row merely to create an editable derived draft. */
    CHECK(sqlite3_exec(database->connection,
                       "UPDATE program_session_entries SET target_weight_kg=0.0 "
                       "WHERE position=1",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_programs_prepare_json(database,
                                             "pg_10000000-0000-4000-8000-000000000001",
                                             "pgs_10000000-0000-4000-8000-000000000002",
                                             "request-legacy-zero-prepare",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_OK);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparation_entries WHERE "
                 "position=1 AND load_mode='external' AND target_weight_kg IS NULL") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_session_entries WHERE "
                 "position=1 AND target_weight_kg=0.0") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_deliveries") == 0);

    free(response);
    trainlog_database_close(database);
    return true;
}

static bool ordered_reschedule_is_atomic_and_idempotent(void) {
    static const char *const DATES[] = {
        "2026-09-21", "2026-09-22", "2026-09-23", "2026-09-24", "2026-09-25", "2026-09-26",
        "2026-09-28", "2026-09-29", "2026-09-30", "2026-10-01", "2026-10-02", "2026-10-03",
        "2026-10-05", "2026-10-06", "2026-10-07", "2026-10-08", "2026-10-09", "2026-10-10",
        "2026-10-12", "2026-10-13", "2026-10-14", "2026-10-15", "2026-10-16", "2026-10-17",
    };
    static const char REQUEST[] = "{\"start_session_id\":\"pgs_fixture_07\","
                                  "\"through_session_id\":\"pgs_fixture_22\","
                                  "\"start_date\":\"2026-10-01\","
                                  "\"ceded_session_ids\":[\"pgs_fixture_11\",\"pgs_fixture_17\"],"
                                  "\"expected_revision\":\"pgr_fixture\"}";
    TrainlogDatabase *database = NULL;
    char *preview = NULL;
    char *applied = NULL;
    char *replayed = NULL;
    char *request = NULL;
    size_t size = 0U;
    size_t applied_size = 0U;
    char fingerprint[80];
    char sql[1024];

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO programs(program_id,title,state,start_date,end_date,"
                       "created_at,updated_at,revision_id,source_format,source_version,"
                       "source_payload_sha256) VALUES('pg_fixture','Fixture','active',"
                       "'2026-09-21','2026-10-18','2026-09-20T00:00:00Z',"
                       "'2026-09-20T00:00:00Z','pgr_fixture','trainlog-program',1,"
                       "'0000000000000000000000000000000000000000000000000000000000000000')",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    for (size_t index = 0U; index < 24U; ++index) {
        (void)snprintf(sql,
                       sizeof(sql),
                       "INSERT INTO program_sessions VALUES('pgs_fixture_%02zu','pg_fixture',"
                       "%zu,'Session %zu','training','%s',NULL)",
                       index,
                       index,
                       index,
                       DATES[index]);
        CHECK(sqlite3_exec(database->connection, sql, NULL, NULL, NULL) == SQLITE_OK);
        if (index < 7U) {
            const char *actual = index == 5U   ? "2026-09-28T09:00:00+02:00"
                                 : index == 6U ? "2026-09-30T17:00:00+02:00"
                                               : "2026-09-21T09:00:00+02:00";
            (void)snprintf(sql,
                           sizeof(sql),
                           "INSERT INTO sessions(session_id,started_at) "
                           "VALUES('se_fixture_%02zu','%s')",
                           index,
                           actual);
            CHECK(sqlite3_exec(database->connection, sql, NULL, NULL, NULL) == SQLITE_OK);
            (void)snprintf(sql,
                           sizeof(sql),
                           "INSERT INTO program_session_executions VALUES("
                           "'pgs_fixture_%02zu','pg_fixture','se_fixture_%02zu',"
                           "'completed','2026-09-30T00:00:00Z')",
                           index,
                           index);
            CHECK(sqlite3_exec(database->connection, sql, NULL, NULL, NULL) == SQLITE_OK);
        }
    }
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO sessions(session_id,started_at) "
                       "VALUES('se_evening_stretch','2026-10-01T20:00:00+02:00')",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    char *invalid_date = replace_once(REQUEST, "2026-10-01", "2026-02-30");
    CHECK(invalid_date != NULL);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                NULL,
                                                invalid_date,
                                                strlen(invalid_date),
                                                false,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    free(invalid_date);
    free(replayed);
    replayed = NULL;
    char *too_late = replace_once(REQUEST, "2026-10-01", "2026-10-16");
    CHECK(too_late != NULL);
    CHECK(trainlog_web_programs_reschedule_json(
              database, "pg_fixture", NULL, too_late, strlen(too_late), false, &replayed, &size) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(strstr(replayed, "insufficient_or_duplicate_slots") != NULL);
    free(too_late);
    free(replayed);
    replayed = NULL;
    // The DST transition on October 25 is a rest day in this fixture.
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO programs(program_id,title,state,start_date,end_date,"
                       "created_at,updated_at,revision_id,source_format,source_version,"
                       "source_payload_sha256) VALUES('pg_dst','DST','active','2026-10-24',"
                       "'2026-10-27','2026-10-20T00:00:00Z','2026-10-20T00:00:00Z',"
                       "'pgr_dst','trainlog-program',1,"
                       "'0000000000000000000000000000000000000000000000000000000000000000');"
                       "INSERT INTO program_sessions VALUES"
                       "('pgs_dst_0','pg_dst',0,'Before','training','2026-10-24',NULL),"
                       "('pgs_dst_1','pg_dst',1,'After','training','2026-10-26',NULL),"
                       "('pgs_dst_2','pg_dst',2,'Next','training','2026-10-27',NULL)",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    static const char DST_REQUEST[] =
        "{\"start_session_id\":\"pgs_dst_0\",\"through_session_id\":\"pgs_dst_2\","
        "\"start_date\":\"2026-10-24\",\"ceded_session_ids\":[],"
        "\"expected_revision\":\"pgr_dst\"}";
    CHECK(
        trainlog_web_programs_reschedule_json(
            database, "pg_dst", NULL, DST_REQUEST, strlen(DST_REQUEST), false, &replayed, &size) ==
        TRAINLOG_STATUS_OK);
    CHECK(strstr(replayed, "\"new_for\":\"2026-10-26\"") != NULL);
    CHECK(strstr(replayed, "2026-10-25") == NULL);
    free(replayed);
    replayed = NULL;
    CHECK(trainlog_web_programs_reschedule_json(
              database, "pg_fixture", NULL, REQUEST, strlen(REQUEST), false, &preview, &size) ==
          TRAINLOG_STATUS_OK);
    CHECK(extract_string(preview, "preview_sha256", fingerprint, sizeof(fingerprint)));
    CHECK(strstr(preview,
                 "\"program_session_id\":\"pgs_fixture_07\","
                 "\"original_for\":\"2026-09-29\","
                 "\"old_for\":\"2026-09-29\","
                 "\"new_for\":\"2026-10-01\"") != NULL);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_session_planning") == 0);
    request = malloc(strlen(REQUEST) + strlen(fingerprint) + 32U);
    CHECK(request != NULL);
    (void)snprintf(request,
                   strlen(REQUEST) + strlen(fingerprint) + 32U,
                   "%.*s,\"preview_sha256\":\"%s\"}",
                   (int)strlen(REQUEST) - 1,
                   REQUEST,
                   fingerprint);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO sync_generations(generation_id,run_id,producer_peer_id,"
                       "consumer_peer_id,producer_kind,generated_at,manifest_sha256,status,"
                       "manifest_json,staging_path) VALUES('gen_fixture','sy_fixture',"
                       "'peer_desktop','peer_phone','desktop',"
                       "strftime('%Y-%m-%dT%H:%M:%SZ','now'),'digest','captured','{}','/tmp')",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO sync_generation_artifacts VALUES('gen_fixture','programs-v2',"
                       "'trainlog-programs',2,'programs-v2.json',0,'digest',0)",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                "op_fixture",
                                                request,
                                                strlen(request),
                                                true,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_CONFLICT);
    CHECK(strstr(replayed, "program_peer_not_fresh") != NULL);
    free(replayed);
    replayed = NULL;
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_session_planning") == 0);
    CHECK(sqlite3_exec(database->connection,
                       "UPDATE sync_generations SET status='acknowledged',"
                       "acknowledged_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') "
                       "WHERE generation_id='gen_fixture'",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                "op_fixture",
                                                request,
                                                strlen(request),
                                                true,
                                                &applied,
                                                &applied_size) == TRAINLOG_STATUS_OK);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_session_planning") == 11);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_session_planning WHERE state='ceded'") ==
          2);
    CHECK(scalar(database,
                 "SELECT COUNT(DISTINCT current_for) FROM program_session_planning "
                 "WHERE state='active'") == 9);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_session_executions "
                 "WHERE state='completed'") == 7);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sessions WHERE "
                 "session_id='se_fixture_05' AND started_at='2026-09-28T09:00:00+02:00'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sessions WHERE "
                 "session_id='se_fixture_06' AND started_at='2026-09-30T17:00:00+02:00'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sessions WHERE "
                 "session_id='se_evening_stretch' AND started_at='2026-10-01T20:00:00+02:00'") ==
          1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_sessions WHERE "
                 "planned_for='2026-09-29' AND program_session_id='pgs_fixture_07'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_session_planning WHERE "
                 "program_session_id='pgs_fixture_09' AND current_for='2026-10-03'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_session_planning WHERE "
                 "program_session_id='pgs_fixture_22'") == 0);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                "op_fixture",
                                                request,
                                                strlen(request),
                                                true,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_OK);
    CHECK(strcmp(applied, replayed) == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_reschedule_operations") == 1);
    free(replayed);
    replayed = NULL;
    char *different_request = replace_once(request, "2026-10-01", "2026-10-02");
    CHECK(different_request != NULL);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                "op_fixture",
                                                different_request,
                                                strlen(different_request),
                                                true,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_CONFLICT);
    free(different_request);
    free(replayed);
    replayed = NULL;
    char new_revision[80];
    char next_fingerprint[80];
    CHECK(extract_string(applied, "new_revision_id", new_revision, sizeof(new_revision)));
    CHECK(scalar(database,
                 "SELECT revision_sequence FROM program_revision_sequences "
                 "WHERE program_id='pg_fixture'") == 1);
    char second_choice[512];
    (void)snprintf(second_choice,
                   sizeof(second_choice),
                   "{\"start_session_id\":\"pgs_fixture_07\","
                   "\"through_session_id\":\"pgs_fixture_09\","
                   "\"start_date\":\"2026-10-01\",\"ceded_session_ids\":[],"
                   "\"expected_revision\":\"%s\"}",
                   new_revision);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                NULL,
                                                second_choice,
                                                strlen(second_choice),
                                                false,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(replayed,
                 "\"program_session_id\":\"pgs_fixture_09\","
                 "\"original_for\":\"2026-10-01\","
                 "\"old_for\":\"2026-10-03\","
                 "\"new_for\":\"2026-10-03\"") != NULL);
    free(replayed);
    replayed = NULL;
    char *next_choice = replace_once(REQUEST, "pgr_fixture", new_revision);
    CHECK(next_choice != NULL);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                NULL,
                                                next_choice,
                                                strlen(next_choice),
                                                false,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_OK);
    CHECK(extract_string(replayed, "preview_sha256", next_fingerprint, sizeof(next_fingerprint)));
    free(replayed);
    replayed = NULL;
    char *next_request = malloc(strlen(next_choice) + strlen(next_fingerprint) + 32U);
    CHECK(next_request != NULL);
    (void)snprintf(next_request,
                   strlen(next_choice) + strlen(next_fingerprint) + 32U,
                   "%.*s,\"preview_sha256\":\"%s\"}",
                   (int)strlen(next_choice) - 1,
                   next_choice,
                   next_fingerprint);
    CHECK(sqlite3_exec(database->connection,
                       "CREATE TRIGGER fail_reschedule_fixture BEFORE INSERT ON "
                       "program_reschedule_operations WHEN NEW.operation_id='op_failure' "
                       "BEGIN SELECT RAISE(ABORT,'fixture failure'); END",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                "op_failure",
                                                next_request,
                                                strlen(next_request),
                                                true,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_DATABASE_ERROR);
    free(replayed);
    replayed = NULL;
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_reschedule_operations") == 1);
    (void)snprintf(sql,
                   sizeof(sql),
                   "SELECT COUNT(*) FROM programs WHERE program_id='pg_fixture' "
                   "AND revision_id='%s'",
                   new_revision);
    CHECK(scalar(database, sql) == 1);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO program_session_executions VALUES("
                       "'pgs_fixture_07','pg_fixture','se_late','completed',"
                       "'2026-10-01T10:00:00Z')",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_programs_reschedule_json(database,
                                                "pg_fixture",
                                                "op_stale",
                                                next_request,
                                                strlen(next_request),
                                                true,
                                                &replayed,
                                                &size) == TRAINLOG_STATUS_CONFLICT);
    CHECK(strstr(replayed, "prepared_or_executed_session") != NULL);
    free(replayed);
    free(next_request);
    free(next_choice);
    free(applied);
    free(preview);
    free(request);
    trainlog_database_close(database);
    return true;
}

static bool calendar_move_preserves_other_sessions_and_restores_ceded(void) {
    /* CONTRACT: an occupied target and a legacy ceded optional identity are
     * independent Program sessions. The repeated command proves durable
     * idempotence without changing the imported definitions. */
    TrainlogDatabase *database = NULL;
    char *preview = NULL;
    char *applied = NULL;
    char *replay = NULL;
    char *request = NULL;
    size_t size = 0U;
    char fingerprint[80];
    char revision[80] = "pgr_calendar";
    static const char *const TARGETS[] = {"2026-10-04", "2026-10-03", "2026-10-10"};
    static const char *const IDENTITIES[] = {"pgs_main", "pgs_main", "pgs_optional"};

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO programs(program_id,title,state,start_date,end_date,"
                       "created_at,updated_at,revision_id,source_format,source_version,"
                       "source_payload_sha256) VALUES('pg_calendar','Fixture','active',"
                       "'2026-10-01','2026-10-18','2026-10-01T00:00:00Z',"
                       "'2026-10-01T00:00:00Z','pgr_calendar','trainlog-program',1,"
                       "'0000000000000000000000000000000000000000000000000000000000000000');"
                       "INSERT INTO program_sessions VALUES"
                       "('pgs_main','pg_calendar',0,'S2 D','training','2026-10-01',NULL),"
                       "('pgs_other','pg_calendar',1,'S2 E','training','2026-10-03',NULL),"
                       "('pgs_optional','pg_calendar',2,'S2 F FACULTATIVE','training',"
                       "'2026-10-10',NULL);"
                       "INSERT INTO program_session_planning(program_session_id,current_for,"
                       "state,operation_id,updated_at) VALUES"
                       "('pgs_optional',NULL,'ceded','pr_old','2026-10-01T00:00:00Z')",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    for (size_t index = 0U; index < 3U; ++index) {
        char choice[512];
        char operation_id[48];
        (void)snprintf(operation_id, sizeof(operation_id), "pr_calendar_move_%zu", index);
        (void)snprintf(choice,
                       sizeof(choice),
                       "{\"start_session_id\":\"%s\",\"through_session_id\":\"%s\","
                       "\"start_date\":\"%s\",\"ceded_session_ids\":[],"
                       "\"expected_revision\":\"%s\"}",
                       IDENTITIES[index],
                       IDENTITIES[index],
                       TARGETS[index],
                       revision);
        CHECK(trainlog_web_programs_reschedule_json(
                  database, "pg_calendar", NULL, choice, strlen(choice), false, &preview, &size) ==
              TRAINLOG_STATUS_OK);
        CHECK(extract_string(preview, "preview_sha256", fingerprint, sizeof(fingerprint)));
        CHECK(strstr(preview, "\"change\":\"ceded\"") == NULL);
        request = malloc(strlen(choice) + strlen(fingerprint) + 32U);
        CHECK(request != NULL);
        (void)snprintf(request,
                       strlen(choice) + strlen(fingerprint) + 32U,
                       "%.*s,\"preview_sha256\":\"%s\"}",
                       (int)strlen(choice) - 1,
                       choice,
                       fingerprint);
        CHECK(trainlog_web_programs_reschedule_json(database,
                                                    "pg_calendar",
                                                    operation_id,
                                                    request,
                                                    strlen(request),
                                                    true,
                                                    &applied,
                                                    &size) == TRAINLOG_STATUS_OK);
        CHECK(trainlog_web_programs_reschedule_json(database,
                                                    "pg_calendar",
                                                    operation_id,
                                                    request,
                                                    strlen(request),
                                                    true,
                                                    &replay,
                                                    &size) == TRAINLOG_STATUS_OK);
        CHECK(strcmp(applied, replay) == 0);
        CHECK(extract_string(applied, "new_revision_id", revision, sizeof(revision)));
        CHECK(scalar(database,
                     "SELECT COUNT(*) FROM program_sessions WHERE "
                     "program_id='pg_calendar'") == 3);
        CHECK(scalar(database,
                     "SELECT COUNT(*) FROM program_session_planning WHERE "
                     "state='ceded'") == (index < 2U ? 1 : 0));
        free(request);
        free(preview);
        free(applied);
        free(replay);
        request = NULL;
        preview = NULL;
        applied = NULL;
        replay = NULL;
    }
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_session_planning WHERE "
                 "current_for='2026-10-03'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_sessions WHERE "
                 "planned_for='2026-10-03'") == 1);
    trainlog_database_close(database);
    return true;
}

int main(void) {
    return import_archive_and_prepare() && strict_import_rejections() &&
                   mixed_profile_prepare_regression() &&
                   legacy_integer_weight_zero_prepares_editable_draft() &&
                   ordered_reschedule_is_atomic_and_idempotent() &&
                   calendar_move_preserves_other_sessions_and_restores_ceded()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
