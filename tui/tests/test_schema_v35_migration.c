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
    static const char *const DROP_TRIGGERS =
        "DROP TRIGGER sleep_diary_revisions_immutable_update;"
        "DROP TRIGGER sleep_diary_revisions_immutable_delete;"
        "DROP TRIGGER sleep_diary_events_immutable_update;"
        "DROP TRIGGER sleep_diary_events_immutable_delete;"
        "DROP TRIGGER sleep_medication_intakes_immutable_update;"
        "DROP TRIGGER sleep_medication_intakes_immutable_delete;"
        "DROP TRIGGER sleep_medication_revisions_immutable_update;"
        "DROP TRIGGER sleep_medication_revisions_immutable_delete;"
        "PRAGMA user_version=35;";
    char path[] = "/tmp/trainlog-schema-v35-XXXXXX";
    TrainlogDatabase *production = NULL;
    sqlite3 *raw = NULL;
    int descriptor = mkstemp(path);
    int version = 0;
    int result = EXIT_FAILURE;

    CHECK(descriptor >= 0 && close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    trainlog_database_close(production);
    production = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, DROP_TRIGGERS, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(production, &version) == TRAINLOG_STATUS_OK);
    CHECK(version == TRAINLOG_DATABASE_SCHEMA_VERSION);
    trainlog_database_close(production);
    production = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' AND name LIKE "
                 "'sleep_%_immutable_%'") == 8);
    CHECK(sqlite3_exec(
              raw,
              "INSERT INTO sleep_diary_entries VALUES('sl_10000000-0000-4000-8000-000000000001',"
              "'2026-10-24','2026-10-25','2026-10-24T22:00:00Z',"
              "'2026-10-25T07:00:00Z','slr_20000000-0000-4000-8000-000000000002',0);"
              "INSERT INTO sleep_diary_revisions VALUES("
              "'slr_20000000-0000-4000-8000-000000000002',"
              "'sl_10000000-0000-4000-8000-000000000001',NULL,"
              "'2026-10-25T07:00:00Z',NULL,NULL,NULL,'');",
              NULL,
              NULL,
              NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(
              raw, "UPDATE sleep_diary_revisions SET sleep_quality='TB';", NULL, NULL, NULL) ==
          SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(raw, "DELETE FROM sleep_diary_revisions;", NULL, NULL, NULL) ==
          SQLITE_CONSTRAINT);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM sleep_diary_revisions") == 1);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0);
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
        puts("PASS schema v35 to v36 Sleep revision immutability migration");
    }
    return result;
}
