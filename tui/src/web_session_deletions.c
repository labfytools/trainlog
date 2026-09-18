#include "trainlog/web_session_deletions.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/evp.h>
#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"
#include "trainlog/id.h"
#include "trainlog/model.h"

#define CAUSAL_OPERATION_MAX 4096
#define CAUSAL_TEXT_BUDGET (3 * 1024 * 1024)
#define PROPOSAL_WITHDRAWAL_MAX 256

typedef struct DeleteContext {
    const char *kind;
    const char *target_id;
    const char *expected_revision;
    const char *request_id;
    char created_at[TRAINLOG_TIMESTAMP_MAX + 1U];
} DeleteContext;

static bool timestamp_now(char output[TRAINLOG_TIMESTAMP_MAX + 1U]) {
    time_t now = time(NULL);
    struct tm utc;

    return now != (time_t)-1 && gmtime_r(&now, &utc) != NULL &&
           strftime(output, TRAINLOG_TIMESTAMP_MAX + 1U, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0U;
}

static bool copy_column(sqlite3_stmt *statement, int column, char **output, size_t *output_size) {
    const char *value = (const char *)sqlite3_column_text(statement, column);
    int bytes = sqlite3_column_bytes(statement, column);

    if (value == NULL || bytes <= 0) {
        return false;
    }
    *output = malloc((size_t)bytes + 1U);
    if (*output == NULL) {
        return false;
    }
    (void)memcpy(*output, value, (size_t)bytes);
    (*output)[bytes] = '\0';
    *output_size = (size_t)bytes;
    return true;
}

static TrainlogStatus replay_request(TrainlogDatabase *database,
                                     const DeleteContext *context,
                                     char **output_json,
                                     size_t *output_size,
                                     bool *replayed) {
    static const char SQL[] =
        "SELECT resource_kind,target_id,expected_revision,response_json "
        "FROM web_session_deletion_requests WHERE request_id=?1 OR "
        "(resource_kind=?2 AND target_id=?3) ORDER BY request_id=?1 DESC LIMIT 1";
    sqlite3_stmt *statement = NULL;
    int result;

    *replayed = false;
    result = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, context->request_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, context->kind, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, context->target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_OK;
    }
    if (result != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (strcmp((const char *)sqlite3_column_text(statement, 0), context->kind) != 0 ||
        strcmp((const char *)sqlite3_column_text(statement, 1), context->target_id) != 0 ||
        strcmp((const char *)sqlite3_column_text(statement, 2), context->expected_revision) != 0) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }
    if (!copy_column(statement, 3, output_json, output_size)) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        free(*output_json);
        *output_json = NULL;
        *output_size = 0U;
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    *replayed = true;
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus load_current_revision(TrainlogDatabase *database,
                                            const char *kind,
                                            const char *target_id,
                                            char revision[TRAINLOG_ID_MAX + 1U],
                                            bool *active_draft) {
    static const char DRAFT_SQL[] =
        "SELECT revision_id,state FROM execution_drafts WHERE session_id=?1 AND NOT EXISTS("
        "SELECT 1 FROM sync_causal_state WHERE target_kind='execution_draft' AND target_id=?1 "
        "AND deleted=1)";
    static const char HISTORY_SQL[] =
        "SELECT c.current_revision_id,'' FROM sessions s JOIN sync_causal_state c ON "
        "c.target_kind='session' AND c.target_id=s.session_id AND c.deleted=0 "
        "WHERE s.session_id=?1";
    sqlite3_stmt *statement = NULL;
    const char *sql = strcmp(kind, "draft") == 0 ? DRAFT_SQL : HISTORY_SQL;
    const char *current;
    int result;

    *active_draft = false;
    result = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return result == SQLITE_DONE ? TRAINLOG_STATUS_NOT_FOUND : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    current = (const char *)sqlite3_column_text(statement, 0);
    if (current == NULL || strlen(current) > TRAINLOG_ID_MAX) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    (void)snprintf(revision, TRAINLOG_ID_MAX + 1U, "%s", current);
    *active_draft = strcmp((const char *)sqlite3_column_text(statement, 1), "active") == 0;
    return sqlite3_finalize(statement) == SQLITE_OK ? TRAINLOG_STATUS_OK
                                                    : TRAINLOG_STATUS_DATABASE_ERROR;
}

static bool sha256_hex(const char *input, char output[65]) {
    unsigned char digest[32];
    unsigned int digest_size = 0U;
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    size_t index;
    bool success;

    if (context == NULL) {
        return false;
    }
    success = EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1 &&
              EVP_DigestUpdate(context, input, strlen(input)) == 1 &&
              EVP_DigestFinal_ex(context, digest, &digest_size) == 1 && digest_size == 32U;
    EVP_MD_CTX_free(context);
    if (!success) {
        return false;
    }
    for (index = 0U; index < 32U; ++index) {
        (void)snprintf(output + (index * 2U), 3U, "%02x", digest[index]);
    }
    output[64] = '\0';
    return true;
}

static TrainlogStatus causal_capacity_available(TrainlogDatabase *database) {
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 count;
    sqlite3_int64 bytes;
    int result;

    result = sqlite3_prepare_v2(
        database->connection,
        "SELECT COUNT(*),COALESCE(SUM(length(operation_id)+length(target_kind)+"
        "length(target_id)+length(creator_id)+length(predecessor_revision_id)+"
        "length(created_at)+length(payload_sha256)+128),0) FROM sync_causal_operations",
        -1,
        &statement,
        NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    count = sqlite3_column_int64(statement, 0);
    bytes = sqlite3_column_int64(statement, 1);
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return count < CAUSAL_OPERATION_MAX && bytes < CAUSAL_TEXT_BUDGET ? TRAINLOG_STATUS_OK
                                                                      : TRAINLOG_STATUS_CONFLICT;
}

static TrainlogStatus insert_causal_operation(TrainlogDatabase *database,
                                              const DeleteContext *context,
                                              const char *causal_kind,
                                              const char *predecessor_revision,
                                              const char *operation_id) {
    static const char CREATOR[] = "peer_desktop_web";
    sqlite3_stmt *statement = NULL;
    char payload[1024];
    char digest[65];
    int written;
    int result;

    written = snprintf(payload,
                       sizeof(payload),
                       "{\"created_at\":\"%s\",\"creator_id\":\"%s\","
                       "\"operation_id\":\"%s\",\"predecessor_revision_id\":\"%s\","
                       "\"publication_context\":null,\"target_id\":\"%s\","
                       "\"target_kind\":\"%s\"}",
                       context->created_at,
                       CREATOR,
                       operation_id,
                       predecessor_revision,
                       context->target_id,
                       causal_kind);
    if (written < 0 || (size_t)written >= sizeof(payload) || !sha256_hex(payload, digest)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    result =
        sqlite3_prepare_v2(database->connection,
                           "INSERT INTO sync_causal_operations VALUES(?1,?2,?3,?4,?5,?6,?7,NULL)",
                           -1,
                           &statement,
                           NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, operation_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, causal_kind, -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, context->target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, CREATOR, -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 5, predecessor_revision, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 6, context->created_at, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 7, digest, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_DONE) {
        result = SQLITE_ERROR;
    }
    return result == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus apply_causal_effect(TrainlogDatabase *database,
                                          const DeleteContext *context,
                                          const char *causal_kind,
                                          const char *operation_id) {
    sqlite3_stmt *statement = NULL;
    int result;

    if (strcmp(context->kind, "history") == 0) {
        result = sqlite3_prepare_v2(
            database->connection,
            "UPDATE body_observations SET session_row_id=NULL WHERE session_row_id=(SELECT id "
            "FROM sessions WHERE session_id=?1)",
            -1,
            &statement,
            NULL);
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 1, context->target_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_step(statement);
        }
        if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK &&
            result == SQLITE_DONE) {
            result = SQLITE_ERROR;
        }
        statement = NULL;
        if (result != SQLITE_DONE) {
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    result = sqlite3_prepare_v2(database->connection,
                                strcmp(context->kind, "draft") == 0
                                    ? "DELETE FROM execution_drafts WHERE session_id=?1"
                                    : "DELETE FROM sessions WHERE session_id=?1",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, context->target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_DONE) {
        result = SQLITE_ERROR;
    }
    statement = NULL;
    if (result != SQLITE_DONE || sqlite3_changes(database->connection) != 1) {
        return result == SQLITE_DONE ? TRAINLOG_STATUS_CONFLICT : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (strcmp(context->kind, "history") == 0) {
        char final_revision[TRAINLOG_GENERATED_ID_CAPACITY + sizeof("deleted:")];
        int written = snprintf(final_revision, sizeof(final_revision), "deleted:%s", operation_id);

        if (written < 0 || (size_t)written >= sizeof(final_revision)) {
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
        result = sqlite3_prepare_v2(
            database->connection,
            "INSERT OR IGNORE INTO execution_draft_finalizations VALUES(?1,?2,?3)",
            -1,
            &statement,
            NULL);
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 1, context->target_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 2, final_revision, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 3, context->created_at, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_step(statement);
        }
        if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK &&
            result == SQLITE_DONE) {
            result = SQLITE_ERROR;
        }
        statement = NULL;
        if (result != SQLITE_DONE) {
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    result = sqlite3_prepare_v2(database->connection,
                                "INSERT OR REPLACE INTO sync_causal_state VALUES(?1,?2,?3,1,?3)",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, causal_kind, -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, context->target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, operation_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_DONE) {
        result = SQLITE_ERROR;
    }
    return result == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus delete_causal_resource(TrainlogDatabase *database,
                                             const DeleteContext *context) {
    const char *causal_kind = strcmp(context->kind, "draft") == 0 ? "execution_draft" : "session";
    char current_revision[TRAINLOG_ID_MAX + 1U];
    char operation_id[TRAINLOG_GENERATED_ID_CAPACITY];
    bool active_draft;
    TrainlogStatus status;

    status = load_current_revision(
        database, context->kind, context->target_id, current_revision, &active_draft);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    if (active_draft || strcmp(current_revision, context->expected_revision) != 0) {
        return TRAINLOG_STATUS_CONFLICT;
    }
    status = causal_capacity_available(database);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    if (trainlog_id_generate("del", operation_id, sizeof(operation_id)) != TRAINLOG_STATUS_OK) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    status =
        insert_causal_operation(database, context, causal_kind, current_revision, operation_id);
    if (status == TRAINLOG_STATUS_OK) {
        status = apply_causal_effect(database, context, causal_kind, operation_id);
    }
    return status;
}

static TrainlogStatus delete_proposal(TrainlogDatabase *database, const DeleteContext *context) {
    sqlite3_stmt *statement = NULL;
    const char *revision;
    int result;

    result = sqlite3_prepare_v2(
        database->connection,
        "SELECT COALESCE(i.payload_sha256,d.draft_id) FROM ai_session_drafts d LEFT JOIN "
        "ai_session_draft_imports i ON i.draft_row_id=d.id WHERE d.draft_id=?1 AND "
        "d.withdrawn_at IS NULL",
        -1,
        &statement,
        NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, context->target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return result == SQLITE_DONE ? TRAINLOG_STATUS_NOT_FOUND : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    revision = (const char *)sqlite3_column_text(statement, 0);
    if (revision == NULL || strcmp(revision, context->expected_revision) != 0) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    statement = NULL;
    result = sqlite3_prepare_v2(database->connection,
                                "SELECT COUNT(*) FROM ai_session_drafts "
                                "WHERE withdrawn_at IS NOT NULL",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_column_int64(statement, 0) >= PROPOSAL_WITHDRAWAL_MAX) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_CONFLICT;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    statement = NULL;
    result = sqlite3_prepare_v2(database->connection,
                                "UPDATE ai_session_drafts SET withdrawn_at=?1 WHERE draft_id=?2 "
                                "AND withdrawn_at IS NULL",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, context->created_at, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, context->target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_DONE) {
        result = SQLITE_ERROR;
    }
    if (result != SQLITE_DONE || sqlite3_changes(database->connection) != 1) {
        return result == SQLITE_DONE ? TRAINLOG_STATUS_CONFLICT : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus
build_response(const DeleteContext *context, char **output_json, size_t *output_size) {
    yyjson_mut_doc *document = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = document == NULL ? NULL : yyjson_mut_obj(document);

    if (document == NULL || root == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_strcpy(document, root, "kind", context->kind) ||
        !yyjson_mut_obj_add_strcpy(document, root, "identity", context->target_id) ||
        !yyjson_mut_obj_add_strcpy(document, root, "state", "deleted") ||
        !yyjson_mut_obj_add_strcpy(document, root, "deleted_at", context->created_at)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    *output_json = yyjson_mut_write(document, 0U, output_size);
    yyjson_mut_doc_free(document);
    return *output_json != NULL ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_SYSTEM_ERROR;
}

static TrainlogStatus
store_request(TrainlogDatabase *database, const DeleteContext *context, const char *response_json) {
    sqlite3_stmt *statement = NULL;
    int result;

    result = sqlite3_prepare_v2(database->connection,
                                "INSERT INTO web_session_deletion_requests "
                                "VALUES(?1,?2,?3,?4,?5,?6)",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, context->request_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, context->kind, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, context->target_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, context->expected_revision, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 5, response_json, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 6, context->created_at, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_DONE) {
        result = SQLITE_ERROR;
    }
    return result == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_web_session_delete_json(TrainlogDatabase *database,
                                                const char *kind,
                                                const char *target_id,
                                                const char *expected_revision,
                                                const char *request_id,
                                                char **output_json,
                                                size_t *output_size) {
    DeleteContext context;
    TrainlogStatus status;
    bool replayed;

    if (database == NULL || kind == NULL || target_id == NULL || expected_revision == NULL ||
        request_id == NULL || output_json == NULL || output_size == NULL || target_id[0] == '\0' ||
        expected_revision[0] == '\0' || request_id[0] == '\0' ||
        strlen(target_id) > TRAINLOG_ID_MAX || strlen(expected_revision) > TRAINLOG_ID_MAX ||
        strlen(request_id) > 128U ||
        (strcmp(kind, "proposal") != 0 && strcmp(kind, "draft") != 0 &&
         strcmp(kind, "history") != 0)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    context.kind = kind;
    context.target_id = target_id;
    context.expected_revision = expected_revision;
    context.request_id = request_id;
    if (!timestamp_now(context.created_at)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = replay_request(database, &context, output_json, output_size, &replayed);
    if (status == TRAINLOG_STATUS_OK && replayed) {
        status = sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) == SQLITE_OK
                     ? TRAINLOG_STATUS_OK
                     : TRAINLOG_STATUS_DATABASE_ERROR;
        return status;
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = strcmp(kind, "proposal") == 0 ? delete_proposal(database, &context)
                                               : delete_causal_resource(database, &context);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = build_response(&context, output_json, output_size);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = store_request(database, &context, *output_json);
    }
    if (status == TRAINLOG_STATUS_OK &&
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) {
        return TRAINLOG_STATUS_OK;
    }
    (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
    free(*output_json);
    *output_json = NULL;
    *output_size = 0U;
    return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
}
