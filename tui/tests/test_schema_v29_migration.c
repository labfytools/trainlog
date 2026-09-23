#define _POSIX_C_SOURCE 200809L

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "trainlog/database.h"

#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #value);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

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

int main(void) {
    char path[] = "/tmp/trainlog-schema-v29-XXXXXX";
    TrainlogDatabase *production = NULL;
    sqlite3 *raw = NULL;
    int descriptor = mkstemp(path);
    int result = EXIT_FAILURE;
    CHECK(descriptor >= 0 && close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    trainlog_database_close(production);
    production = NULL;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
                       "DROP TABLE session_exercise_timeline;"
                       "DROP TABLE heart_rate_rr_intervals;"
                       "DROP TABLE heart_rate_samples;"
                       "DROP TABLE heart_rate_captures;"
                       "DROP TABLE sleep_diary_events;DROP TABLE sleep_diary_revisions;"
                       "DROP TABLE sleep_diary_entries;PRAGMA user_version=28;",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    trainlog_database_close(production);
    production = NULL;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(scalar(raw, "PRAGMA user_version") == 33);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN "
                 "('sleep_diary_entries','sleep_diary_revisions','sleep_diary_events',"
                 "'sleep_diary_publication_state','sleep_diary_generation_entries')") == 5);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND "
                 "name='sleep_diary_one_night'") == 1);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN "
                 "('sleep_medications','sleep_medication_revisions',"
                 "'sleep_medication_intakes')") == 3);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN "
                 "('heart_rate_captures','heart_rate_samples','heart_rate_rr_intervals')") == 3);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_integrity_check WHERE integrity_check='ok'") ==
          1);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    trainlog_database_close(production);
    production = NULL;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
                       "DROP TABLE session_exercise_timeline;"
                       "DROP TABLE heart_rate_rr_intervals;"
                       "DROP TABLE heart_rate_samples;"
                       "DROP TABLE heart_rate_captures;"
                       "DROP TABLE sleep_medication_intakes;"
                       "DROP TABLE sleep_medication_revisions;"
                       "DROP TABLE sleep_medications;"
                       "PRAGMA user_version=29;",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    /* A valid v29 may already exist locally. Opening it completes the historical
     * v29 repair and then advances additively to the current v30 schema. */
    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    trainlog_database_close(production);
    production = NULL;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(scalar(raw, "PRAGMA user_version") == 33);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name LIKE "
                 "'sleep_medication%'") == 3);
    result = EXIT_SUCCESS;
cleanup:
    if (production != NULL) {
        trainlog_database_close(production);
    }
    if (raw != NULL) {
        (void)sqlite3_close(raw);
    }
    (void)unlink(path);
    if (result == EXIT_SUCCESS) {
        puts("PASS schema v28 through v29 to current migration and reopen");
    }
    return result;
}
