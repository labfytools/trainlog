#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "trainlog/database.h"
#include "trainlog/status.h"
#include "trainlog/web_programs.h"
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
                       "'2026-09-18T10:00:00Z','mu_active',NULL,'active',"
                       "'{\"exercises\":[{\"entry_id\":\"entry_live\"}]}');",
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

static bool empty_active_draft_is_causally_deleted_and_replay_safe(void) {
    char path[] = "/tmp/trainlog-web-delete-empty-active-XXXXXX";
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
                       "INSERT INTO execution_drafts VALUES("
                       "'se_56565656-5656-4565-8565-565656565656','training',NULL,"
                       "'2026-09-20T10:00:00Z','mu_empty',NULL,'active',"
                       "'{\"exercises\":[]}');",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_session_delete_json(database,
                                           "draft",
                                           "se_56565656-5656-4565-8565-565656565656",
                                           "mu_empty",
                                           "request-delete-empty",
                                           &response,
                                           &response_size) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_web_session_delete_json(database,
                                           "draft",
                                           "se_56565656-5656-4565-8565-565656565656",
                                           "mu_empty",
                                           "request-delete-empty",
                                           &replayed,
                                           &replayed_size) == TRAINLOG_STATUS_OK);
    CHECK(response_size == replayed_size && memcmp(response, replayed, response_size) == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM execution_drafts") == 0);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sync_causal_state WHERE target_kind='execution_draft' "
                 "AND target_id='se_56565656-5656-4565-8565-565656565656' AND deleted=1") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sync_causal_operations") == 1);
    free(replayed);
    free(response);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

static bool completed_program_execution_deletion_is_atomic_and_idempotent(void) {
    char path[] = "/tmp/trainlog-web-delete-program-execution-XXXXXX";
    TrainlogDatabase *database = NULL;
    char *response = NULL;
    char *replayed = NULL;
    char *program_detail = NULL;
    size_t response_size = 0U;
    size_t replayed_size = 0U;
    size_t program_detail_size = 0U;
    int descriptor = mkstemp(path);

    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO programs(program_id,title,state,created_at,updated_at,"
                       "revision_id,source_format,source_version,source_payload_sha256) VALUES("
                       "'pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','Disposable Program','active',"
                       "'2026-09-18T10:00:00Z','2026-09-18T10:00:00Z','pgr_fixture',"
                       "'trainlog-program',1,"
                       "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');"
                       "INSERT INTO program_sessions(program_session_id,program_id,position,title,"
                       "session_type) VALUES("
                       "'pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb',"
                       "'pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',0,'Executed','training'),("
                       "'pgs_cccccccc-cccc-4ccc-8ccc-cccccccccccc',"
                       "'pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',1,'Untouched','training');"
                       "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES("
                       "'se_dddddddd-dddd-4ddd-8ddd-dddddddddddd','2026-09-18T10:00:00Z',"
                       "'2026-09-18T11:00:00Z','training'),("
                       "'se_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee','2026-09-17T10:00:00Z',"
                       "'2026-09-17T11:00:00Z','training');"
                       "INSERT INTO sync_causal_state VALUES("
                       "'session','se_dddddddd-dddd-4ddd-8ddd-dddddddddddd','lv_fixture',0,NULL);"
                       "INSERT INTO program_session_executions VALUES("
                       "'pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb',"
                       "'pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',"
                       "'se_dddddddd-dddd-4ddd-8ddd-dddddddddddd','completed',"
                       "'2026-09-18T11:00:00Z');",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_session_delete_json(database,
                                           "history",
                                           "se_dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                                           "lv_fixture",
                                           "request-delete-program-history",
                                           &response,
                                           &response_size) == TRAINLOG_STATUS_OK);
    CHECK(response != NULL && strstr(response, "\"state\":\"deleted\"") != NULL);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sessions WHERE "
                 "session_id='se_dddddddd-dddd-4ddd-8ddd-dddddddddddd'") == 0);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sessions WHERE "
                 "session_id='se_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee'") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM programs") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM program_sessions") == 2);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM program_session_executions WHERE state='deleted' AND "
                 "session_id='se_dddddddd-dddd-4ddd-8ddd-dddddddddddd' AND "
                 "observed_at='2026-09-18T11:00:00Z'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sync_causal_operations WHERE target_kind='session' AND "
                 "target_id='se_dddddddd-dddd-4ddd-8ddd-dddddddddddd'") == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sync_causal_state WHERE target_kind='session' AND "
                 "target_id='se_dddddddd-dddd-4ddd-8ddd-dddddddddddd' AND deleted=1") == 1);
    CHECK(trainlog_web_programs_detail_json(database,
                                            "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                            &program_detail,
                                            &program_detail_size) == TRAINLOG_STATUS_OK);
    CHECK(program_detail != NULL && program_detail_size > 0U &&
          strstr(program_detail, "\"execution_state\":\"deleted\"") != NULL &&
          strstr(program_detail, "\"execution_session_id\":null") != NULL);
    CHECK(trainlog_web_session_delete_json(database,
                                           "history",
                                           "se_dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                                           "lv_fixture",
                                           "request-delete-program-history-replay",
                                           &replayed,
                                           &replayed_size) == TRAINLOG_STATUS_OK);
    CHECK(replayed != NULL && response_size == replayed_size && strcmp(response, replayed) == 0);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sync_causal_operations WHERE target_kind='session' AND "
                 "target_id='se_dddddddd-dddd-4ddd-8ddd-dddddddddddd'") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM web_session_deletion_requests") == 1);
    free(program_detail);
    free(replayed);
    free(response);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void) {
    return proposal_deletion_is_durable_and_preserves_derivatives() &&
                   draft_and_history_use_causal_deletion() &&
                   active_draft_conflicts_without_mutation() &&
                   empty_active_draft_is_causally_deleted_and_replay_safe() &&
                   completed_program_execution_deletion_is_atomic_and_idempotent()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
