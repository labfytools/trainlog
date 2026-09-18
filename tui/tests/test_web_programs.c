#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "trainlog/database.h"
#include "trainlog/web_programs.h"

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
    CHECK(scalar(database, "PRAGMA user_version") == 28);
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

int main(void) {
    return import_archive_and_prepare() && strict_import_rejections() ? EXIT_SUCCESS : EXIT_FAILURE;
}
