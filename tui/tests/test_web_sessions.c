#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "trainlog/database.h"
#include "trainlog/web_sessions.h"

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static int scalar(TrainlogDatabase *database, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int result = -1;

    if (sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        result = sqlite3_column_int(statement, 0);
    }
    (void)sqlite3_finalize(statement);
    return result;
}

static bool creation_replay_revision_and_pagination(void) {
    static const char CREATE_BODY[] =
        "{\"title\":\"Séance échappée \\\"A\\\"\",\"session_type\":\"training\","
        "\"planned_for\":\"2026-09-20\",\"notes\":\"Dos & épaules\","
        "\"editing_state\":\"ready\",\"occurrences\":["
        "{\"exercise_id\":\"ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\","
        "\"load_mode\":\"external\",\"rest_seconds\":90,\"target_sets\":3,"
        "\"target_reps\":8,\"target_duration_seconds\":null,\"target_weight_kg\":42.5},"
        "{\"exercise_id\":\"ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\","
        "\"load_mode\":\"none\",\"rest_seconds\":0,\"target_sets\":1,"
        "\"target_reps\":12,\"target_duration_seconds\":null,\"target_weight_kg\":null}]}";
    static const char UPDATE_BODY[] =
        "{\"title\":\"Réordonnée\",\"session_type\":\"training\","
        "\"planned_for\":null,\"notes\":null,\"editing_state\":\"draft\","
        "\"occurrences\":[]}";
    static const char DERIVED_BODY[] =
        "{\"title\":\"Derived\",\"session_type\":\"training\","
        "\"planned_for\":null,\"notes\":null,\"editing_state\":\"draft\","
        "\"occurrences\":[],\"source_proposal_id\":"
        "\"aid_11111111-1111-4111-8111-111111111111\","
        "\"source_payload_sha256\":"
        "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}";
    TrainlogDatabase *database = NULL;
    TrainlogWebSessionsPageQuery query = {TRAINLOG_WEB_SESSION_PREPARATIONS, 0U, 1U, "échappée"};
    char *created = NULL;
    char *replayed = NULL;
    char *detail = NULL;
    char *page = NULL;
    char *updated = NULL;
    size_t created_size = 0U;
    size_t replayed_size = 0U;
    size_t detail_size = 0U;
    size_t page_size = 0U;
    size_t updated_size = 0U;
    char preparation_id[128];
    char revision_id[128];
    const char *preparation_start;
    const char *revision_start;
    const char *end;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     "ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                                     "Développé test",
                                                     "developpe test",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0U) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO ai_session_drafts(draft_id,created_at,session_type,title) "
                       "VALUES('aid_11111111-1111-4111-8111-111111111111','2026-09-18T00:00:00Z',"
                       "'training','Source');"
                       "INSERT INTO ai_session_draft_imports VALUES("
                       "last_insert_rowid(),'aid_11111111-1111-4111-8111-111111111111',"
                       "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',"
                       "'2026-09-18T00:00:00Z');",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(trainlog_web_sessions_save_json(database,
                                          NULL,
                                          "",
                                          "request-create-1",
                                          CREATE_BODY,
                                          strlen(CREATE_BODY),
                                          &created,
                                          &created_size) == TRAINLOG_STATUS_OK);
    CHECK(created_size > 0U);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparations") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_entries") == 2);
    CHECK(trainlog_web_sessions_save_json(database,
                                          NULL,
                                          "",
                                          "request-create-1",
                                          CREATE_BODY,
                                          strlen(CREATE_BODY),
                                          &replayed,
                                          &replayed_size) == TRAINLOG_STATUS_OK);
    CHECK(created_size == replayed_size && strcmp(created, replayed) == 0);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparations") == 1);
    preparation_start = strstr(created, "\"preparation_id\":\"");
    revision_start = strstr(created, "\"revision_id\":\"");
    CHECK(preparation_start != NULL && revision_start != NULL);
    preparation_start += strlen("\"preparation_id\":\"");
    end = strchr(preparation_start, '"');
    CHECK(end != NULL && (size_t)(end - preparation_start) < sizeof(preparation_id));
    (void)memcpy(preparation_id, preparation_start, (size_t)(end - preparation_start));
    preparation_id[end - preparation_start] = '\0';
    revision_start += strlen("\"revision_id\":\"");
    end = strchr(revision_start, '"');
    CHECK(end != NULL && (size_t)(end - revision_start) < sizeof(revision_id));
    (void)memcpy(revision_id, revision_start, (size_t)(end - revision_start));
    revision_id[end - revision_start] = '\0';
    CHECK(trainlog_web_sessions_detail_json(
              database, "preparation", preparation_id, &detail, &detail_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(detail, "Séance échappée") != NULL);
    CHECK(strstr(detail, "Développé test") != NULL);
    CHECK(trainlog_web_sessions_list_json(database, &query, &page, &page_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(strstr(page, preparation_id) != NULL);
    CHECK(trainlog_web_sessions_save_json(database,
                                          preparation_id,
                                          revision_id,
                                          "request-update-1",
                                          UPDATE_BODY,
                                          strlen(UPDATE_BODY),
                                          &updated,
                                          &updated_size) == TRAINLOG_STATUS_OK);
    CHECK(updated_size > 0U);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_revisions") == 2);
    CHECK(trainlog_web_sessions_save_json(database,
                                          NULL,
                                          "",
                                          "request-derived-1",
                                          DERIVED_BODY,
                                          strlen(DERIVED_BODY),
                                          &detail,
                                          &detail_size) == TRAINLOG_STATUS_OK);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_preparations WHERE source_proposal_id="
                 "'aid_11111111-1111-4111-8111-111111111111'") == 1);
    free(detail);
    detail = NULL;
    detail_size = 0U;
    CHECK(trainlog_web_sessions_save_json(database,
                                          preparation_id,
                                          revision_id,
                                          "request-stale",
                                          UPDATE_BODY,
                                          strlen(UPDATE_BODY),
                                          &detail,
                                          &detail_size) == TRAINLOG_STATUS_CONFLICT);
    CHECK(scalar(database, "SELECT COUNT(*) FROM session_preparation_revisions") == 3);
    free(created);
    free(replayed);
    free(detail);
    free(page);
    free(updated);
    trainlog_database_close(database);
    return true;
}

int main(void) {
    return creation_replay_revision_and_pagination() ? 0 : 1;
}
