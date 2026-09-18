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

static bool import_archive_and_prepare(void) {
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    size_t response_size = 0U;
    char revision[128];
    char preparation_id[128];

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
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

    CHECK(trainlog_web_programs_detail_json(
              database, "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &response, &response_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(extract_string(response, "revision_id", revision, sizeof(revision)));
    CHECK(strstr(response, "Exercise fixture") == NULL);
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

    CHECK(trainlog_web_programs_archive_json(database,
                                             "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                             revision,
                                             "request-archive-readable",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"state\":\"archived\"") != NULL);
    free(response);
    response = NULL;

    CHECK(trainlog_web_programs_archive_json(database,
                                             "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                             revision,
                                             "request-archive-readable",
                                             &response,
                                             &response_size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(response, "\"state\":\"archived\"") != NULL);
    free(response);
    trainlog_database_close(database);
    return true;
}

int main(void) {
    return import_archive_and_prepare() ? EXIT_SUCCESS : EXIT_FAILURE;
}
