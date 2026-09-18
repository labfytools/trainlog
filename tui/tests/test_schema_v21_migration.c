#define _POSIX_C_SOURCE 200809L

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "trainlog/database.h"

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            return 1;                                                                              \
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
    static const char DOWNGRADE_TO_V20[] = "PRAGMA foreign_keys=OFF;"
                                           "DROP TABLE program_requests;"
                                           "DROP TABLE program_session_entries;"
                                           "DROP TABLE program_sessions;"
                                           "DROP TABLE programs;"
                                           "DROP TABLE session_preparation_withdrawals;"
                                           "DROP TABLE session_preparation_deliveries;"
                                           "DROP TABLE session_preparation_requests;"
                                           "DROP TABLE session_preparation_entries;"
                                           "DROP TABLE session_preparation_revisions;"
                                           "DROP TABLE session_preparations;"
                                           "DROP TABLE sync_generation_archives;"
                                           "DROP TABLE sync_causal_publications;"
                                           "DROP TABLE sync_acknowledgements;"
                                           "DROP TABLE sync_consumed_generations;"
                                           "DROP TABLE sync_generation_artifacts;"
                                           "DROP TABLE sync_generations;"
                                           "DROP TABLE sync_peer_identity;"
                                           "PRAGMA user_version=20;";
    char path[] = "/tmp/trainlog-schema-v21-XXXXXX";
    int file_descriptor = mkstemp(path);
    TrainlogDatabase *database = NULL;
    sqlite3 *raw = NULL;
    size_t exercises = 0U;

    CHECK(file_descriptor >= 0);
    CHECK(close(file_descriptor) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     "ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                                     "Migration sentinel",
                                                     "migration sentinel",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0U) == TRAINLOG_STATUS_OK);
    trainlog_database_close(database);
    database = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, DOWNGRADE_TO_V20, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_exercise_count(database, &exercises) == TRAINLOG_STATUS_OK);
    CHECK(exercises > 0U);
    trainlog_database_close(database);
    database = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(scalar(raw, "PRAGMA user_version") == TRAINLOG_DATABASE_SCHEMA_VERSION);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_preparation_withdrawals") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM sync_generations") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM sync_consumed_generations") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM sync_acknowledgements") == 0);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM sync_generation_archives") == 0);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(unlink(path) == 0);
    return 0;
}
