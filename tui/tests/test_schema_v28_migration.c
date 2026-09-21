/**
 * @file test_schema_v28_migration.c
 * @brief Populated v27 to v28 Program-execution lifecycle migration.
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

static bool execute(sqlite3 *database, const char *sql) {
    char *error = NULL;
    int result = sqlite3_exec(database, sql, NULL, NULL, &error);

    if (result != SQLITE_OK) {
        (void)fprintf(stderr, "fixture SQL failed: %s\n", error == NULL ? "unknown" : error);
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

static bool execution_matches(sqlite3 *database, const char *state) {
    sqlite3_stmt *statement = NULL;
    bool matches = false;

    if (sqlite3_prepare_v2(database,
                           "SELECT program_session_id,program_id,session_id,state,observed_at "
                           "FROM program_session_executions",
                           -1,
                           &statement,
                           NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        matches =
            strcmp((const char *)sqlite3_column_text(statement, 0), "pgs_v28_fixture") == 0 &&
            strcmp((const char *)sqlite3_column_text(statement, 1), "pg_v28_fixture") == 0 &&
            strcmp((const char *)sqlite3_column_text(statement, 2), "se_v28_fixture") == 0 &&
            strcmp((const char *)sqlite3_column_text(statement, 3), state) == 0 &&
            strcmp((const char *)sqlite3_column_text(statement, 4), "2026-09-18T20:00:00Z") == 0;
    }
    (void)sqlite3_finalize(statement);
    return matches;
}

static int run_test(void) {
    char path[] = "/tmp/trainlog-schema-v28-XXXXXX";
    TrainlogDatabase *production = NULL;
    sqlite3 *database = NULL;
    int descriptor = mkstemp(path);
    int version = 0;
    int result = EXIT_FAILURE;

    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    trainlog_database_close(production);
    production = NULL;

    CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CHECK(execute(database,
                  "PRAGMA foreign_keys=OFF;BEGIN IMMEDIATE;"
                  "CREATE TABLE program_session_executions_v27("
                  "program_session_id TEXT PRIMARY KEY REFERENCES program_sessions("
                  "program_session_id) ON DELETE RESTRICT,"
                  "program_id TEXT NOT NULL REFERENCES programs(program_id) ON DELETE RESTRICT,"
                  "session_id TEXT NOT NULL UNIQUE,"
                  "state TEXT NOT NULL CHECK(state IN('in_progress','completed')) ,"
                  "observed_at TEXT NOT NULL,"
                  "CHECK(length(program_id)>0 AND length(session_id)>0));"
                  "DROP INDEX program_session_executions_program;"
                  "DROP TABLE program_session_executions;"
                  "ALTER TABLE program_session_executions_v27 RENAME TO "
                  "program_session_executions;"
                  "CREATE INDEX program_session_executions_program "
                  "ON program_session_executions(program_id,state);"
                  "INSERT INTO programs(program_id,title,state,created_at,updated_at,revision_id,"
                  "source_format,source_version,source_payload_sha256) VALUES("
                  "'pg_v28_fixture','Migration fixture','active','2026-09-18T19:00:00Z',"
                  "'2026-09-18T19:00:00Z','pgr_v28_fixture','trainlog-program',1,"
                  "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');"
                  "INSERT INTO program_sessions(program_session_id,program_id,position,title,"
                  "session_type) VALUES('pgs_v28_fixture','pg_v28_fixture',0,'Fixture session',"
                  "'training');"
                  "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES("
                  "'se_v28_fixture','2026-09-18T19:30:00Z','2026-09-18T20:00:00Z','training');"
                  "INSERT INTO program_session_executions VALUES('pgs_v28_fixture',"
                  "'pg_v28_fixture','se_v28_fixture','completed','2026-09-18T20:00:00Z');"
                  "PRAGMA user_version=27;COMMIT;PRAGMA foreign_keys=ON;"));
    CHECK(scalar(database, "PRAGMA user_version") == 27);
    CHECK(execution_matches(database, "completed"));
    CHECK(scalar(database, "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0);
    CHECK(sqlite3_close(database) == SQLITE_OK);
    database = NULL;

    CHECK(trainlog_database_open(path, &production) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(production, &version) == TRAINLOG_STATUS_OK);
    CHECK(version == TRAINLOG_DATABASE_SCHEMA_VERSION);
    trainlog_database_close(production);
    production = NULL;

    CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM pragma_integrity_check WHERE integrity_check='ok'") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0);
    CHECK(execution_matches(database, "completed"));
    CHECK(scalar(database, "SELECT COUNT(*) FROM sync_causal_operations") == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sync_causal_state WHERE deleted=1") == 0);
    CHECK(execute(database,
                  "UPDATE program_session_executions SET state='deleted' "
                  "WHERE session_id='se_v28_fixture';"));
    CHECK(execution_matches(database, "deleted"));
    result = EXIT_SUCCESS;

cleanup:
    if (production != NULL) {
        trainlog_database_close(production);
    }
    if (database != NULL) {
        (void)sqlite3_close(database);
    }
    (void)unlink(path);
    return result;
}

int main(void) {
    int result = run_test();

    if (result == EXIT_SUCCESS) {
        (void)puts("PASS populated schema v27 to v28 migration");
    }
    return result;
}
