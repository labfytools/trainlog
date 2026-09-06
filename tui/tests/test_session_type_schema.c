/**
 * @file test_session_type_schema.c
 * @brief Schema v2 and session type persistence tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "trainlog/database.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)fprintf(                                                   \
                stderr,                                                      \
                "CHECK failed at %s:%d: %s\n",                               \
                __FILE__,                                                    \
                __LINE__,                                                    \
                #condition                                                   \
            );                                                               \
            return false;                                                    \
        }                                                                    \
    } while (0)

static bool test_session_type_roundtrip(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogSessionInput training;
    TrainlogSessionInput max_test;
    TrainlogSessionSummary sessions[4];
    size_t count = 0U;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(&training, 0, sizeof(training));

    (void)snprintf(
        training.session_id,
        sizeof(training.session_id),
        "%s",
        "se_training"
    );

    (void)snprintf(
        training.started_at,
        sizeof(training.started_at),
        "%s",
        "2026-09-05T18:00:00+02:00"
    );

    CHECK(
        training.session_type ==
        TRAINLOG_SESSION_TRAINING
    );

    CHECK(
        trainlog_database_insert_session(
            database,
            &training
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(&max_test, 0, sizeof(max_test));

    (void)snprintf(
        max_test.session_id,
        sizeof(max_test.session_id),
        "%s",
        "se_max_test"
    );

    (void)snprintf(
        max_test.started_at,
        sizeof(max_test.started_at),
        "%s",
        "2026-09-05T19:00:00+02:00"
    );

    max_test.session_type =
        TRAINLOG_SESSION_MAX_TEST;

    CHECK(
        trainlog_database_insert_session(
            database,
            &max_test
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_list_sessions(
            database,
            sessions,
            4U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 2U);

    CHECK(
        strcmp(
            sessions[0].session_id,
            "se_max_test"
        ) == 0
    );

    CHECK(
        sessions[0].session_type ==
        TRAINLOG_SESSION_MAX_TEST
    );

    CHECK(
        strcmp(
            sessions[1].session_id,
            "se_training"
        ) == 0
    );

    CHECK(
        sessions[1].session_type ==
        TRAINLOG_SESSION_TRAINING
    );

    trainlog_database_close(database);
    return true;
}

static bool test_v1_to_v2_migration(void)
{
    char path[] =
        "/tmp/trainlog-schema-v1-XXXXXX";

    static const char *const V1_SQL =
        "CREATE TABLE sessions ("
        "id INTEGER PRIMARY KEY,"
        "session_id TEXT NOT NULL UNIQUE,"
        "started_at TEXT NOT NULL,"
        "ended_at TEXT,"
        "notes TEXT"
        ");"
        "INSERT INTO sessions("
        "session_id, started_at, ended_at, notes"
        ") VALUES("
        "'se_old',"
        "'2026-08-01T10:00:00+02:00',"
        "NULL,"
        "NULL"
        ");"
        "PRAGMA user_version = 1;";

    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;
    TrainlogDatabase *database = NULL;
    int fd;
    int version = 0;
    int rc;

    fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);

    rc = sqlite3_open(path, &raw);
    CHECK(rc == SQLITE_OK);

    CHECK(
        sqlite3_exec(
            raw,
            V1_SQL,
            NULL,
            NULL,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(
        trainlog_database_open(
            path,
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_schema_version(
            database,
            &version
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        version ==
        TRAINLOG_DATABASE_SCHEMA_VERSION
    );

    trainlog_database_close(database);
    database = NULL;

    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);

    CHECK(
        sqlite3_prepare_v2(
            raw,
            "SELECT session_type "
            "FROM sessions "
            "WHERE session_id = 'se_old';",
            -1,
            &statement,
            NULL
        ) == SQLITE_OK
    );

    CHECK(sqlite3_step(statement) == SQLITE_ROW);

    {
        const unsigned char *type =
            sqlite3_column_text(statement, 0);

        CHECK(type != NULL);

        CHECK(
            strcmp(
                (const char *)type,
                "training"
            ) == 0
        );
    }

    CHECK(
        sqlite3_finalize(statement) ==
        SQLITE_OK
    );

    statement = NULL;

    rc = sqlite3_exec(
        raw,
        "INSERT INTO sessions("
        "session_id, started_at, session_type"
        ") VALUES("
        "'se_invalid',"
        "'2026-08-02T10:00:00+02:00',"
        "'invalid'"
        ");",
        NULL,
        NULL,
        NULL
    );

    CHECK(rc == SQLITE_CONSTRAINT);

    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;

    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    CHECK(test_session_type_roundtrip());
    CHECK(test_v1_to_v2_migration());

    (void)printf("PASS session_type_schema\n");
    return 0;
}
