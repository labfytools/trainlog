#define _POSIX_C_SOURCE 200809L

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "trainlog/database.h"

#define CHECK(value) do { if (!(value)) {     fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #value);     goto cleanup; } } while (0)

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
    char path[] = "/tmp/trainlog-schema-v34-XXXXXX";
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
    CHECK(sqlite3_exec(raw,
                       "DROP TABLE cardio_guidance_events;"
                       "DROP TABLE cardio_guidance_phases;"
                       "DROP TABLE cardio_guidance_runs;"
                       "PRAGMA user_version=33;",
                       NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(production, &version) == TRAINLOG_STATUS_OK);
    CHECK(version == 34);
    trainlog_database_close(production);
    production = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN("
                 "'cardio_guidance_runs','cardio_guidance_phases','cardio_guidance_events')") == 3);
    CHECK(scalar(raw,
                 "SELECT COUNT(*) FROM sqlite_master WHERE type='index' "
                 "AND name IN('cardio_guidance_session','cardio_guidance_event_time')") == 2);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM cardio_guidance_runs") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0);
    result = EXIT_SUCCESS;

cleanup:
    if (production != NULL) trainlog_database_close(production);
    if (raw != NULL) (void)sqlite3_close(raw);
    (void)unlink(path);
    if (result == EXIT_SUCCESS) puts("PASS schema v33 to v34 cardio guidance migration");
    return result;
}
