#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "trainlog/database.h"
#include "trainlog/status.h"
#include "trainlog/web_session_deletions.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);  \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static sqlite3_int64 scalar(TrainlogDatabase *database, const char *sql) {
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 value = -1;

    if (sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int64(statement, 0);
    }
    (void)sqlite3_finalize(statement);
    return value;
}

static bool proposal_deletion_is_durable_and_preserves_derivatives(void) {
    char path[] = "/tmp/trainlog-web-delete-proposal-XXXXXX";
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    char *replayed = NULL;
    size_t response_size = 0U;
    size_t replayed_size = 0U;
    int descriptor = mkstemp(path);

    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO ai_session_drafts("
                       "draft_id,created_at,session_type,title,archive_status,published_at) "
                       "VALUES('aid_11111111-1111-4111-8111-111111111111',"
                       "'2026-09-18T10:00:00Z','training','Proposal','archived',"
                       "'2026-09-18T11:00:00Z');"
                       "INSERT INTO ai_session_draft_imports VALUES("
                       "last_insert_rowid(),'aid_11111111-1111-4111-8111-111111111111',"
                       "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',"
                       "'2026-09-18T10:00:00Z');"
                       "INSERT INTO session_preparations VALUES("
                       "'sp_22222222-2222-4222-8222-222222222222','spr_source',"
                       "'2026-09-18T10:00:00Z','2026-09-18T10:00:00Z','draft','local',"
                       "'aid_11111111-1111-4111-8111-111111111111',"
                       "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',"
                       "NULL,NULL,NULL);",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_session_delete_json(
              database,
              "proposal",
              "aid_11111111-1111-4111-8111-111111111111",
              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
              "request-delete-proposal",
              &response,
              &response_size) == TRAINLOG_STATUS_OK);
    CHECK(response != NULL && strstr(response, "\"state\":\"deleted\"") != NULL);
    CHECK(trainlog_web_session_delete_json(
              database,
              "proposal",
              "aid_11111111-1111-4111-8111-111111111111",
              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
              "request-delete-proposal",
              &replayed,
              &replayed_size) == TRAINLOG_STATUS_OK);
    CHECK(response_size == replayed_size && strcmp(response, replayed) == 0);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM ai_session_drafts WHERE withdrawn_at IS NOT NULL") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparations") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM web_session_deletion_requests") == 1);
    free(replayed);
    free(response);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

static bool draft_and_history_use_causal_deletion(void) {
    char path[] = "/tmp/trainlog-web-delete-causal-XXXXXX";
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    size_t response_size = 0U;
    int descriptor = mkstemp(path);

    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO execution_drafts VALUES("
                       "'se_33333333-3333-4333-8333-333333333333','training',NULL,"
                       "'2026-09-18T10:00:00Z','mu_draft',NULL,'pending','{}');"
                       "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES("
                       "'se_44444444-4444-4444-8444-444444444444',"
                       "'2026-09-17T10:00:00Z','2026-09-17T11:00:00Z','training');"
                       "INSERT INTO sync_causal_state VALUES("
                       "'session','se_44444444-4444-4444-8444-444444444444',"
                       "'mu_history',0,NULL);",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_session_delete_json(database,
                                           "draft",
                                           "se_33333333-3333-4333-8333-333333333333",
                                           "mu_draft",
                                           "request-delete-draft",
                                           &response,
                                           &response_size) == TRAINLOG_STATUS_OK);
    free(response);
    response = NULL;
    CHECK(trainlog_web_session_delete_json(database,
                                           "history",
                                           "se_44444444-4444-4444-8444-444444444444",
                                           "mu_history",
                                           "request-delete-history",
                                           &response,
                                           &response_size) == TRAINLOG_STATUS_OK);
    CHECK(scalar(database, "SELECT COUNT(*) FROM execution_drafts") == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sessions") == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sync_causal_operations") == 2);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sync_causal_state WHERE deleted=1") == 2);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM execution_draft_finalizations "
                 "WHERE session_id='se_44444444-4444-4444-8444-444444444444' "
                 "AND final_revision_id LIKE 'deleted:del_%'") == 1);
    free(response);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

static bool active_draft_conflicts_without_mutation(void) {
    char path[] = "/tmp/trainlog-web-delete-active-XXXXXX";
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    size_t response_size = 0U;
    int descriptor = mkstemp(path);

    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO execution_drafts VALUES("
                       "'se_55555555-5555-4555-8555-555555555555','training',NULL,"
                       "'2026-09-18T10:00:00Z','mu_active',NULL,'active','{}');",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_session_delete_json(database,
                                           "draft",
                                           "se_55555555-5555-4555-8555-555555555555",
                                           "mu_active",
                                           "request-delete-active",
                                           &response,
                                           &response_size) == TRAINLOG_STATUS_CONFLICT);
    CHECK(response == NULL && response_size == 0U);
    CHECK(scalar(database, "SELECT COUNT(*) FROM execution_drafts") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sync_causal_operations") == 0);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void) {
    return proposal_deletion_is_durable_and_preserves_derivatives() &&
                   draft_and_history_use_causal_deletion() &&
                   active_draft_conflicts_without_mutation()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
