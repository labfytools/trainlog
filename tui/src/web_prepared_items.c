#include "trainlog/web_prepared_items.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sqlite3.h>

#include "database_internal.h"

static bool copy_column(sqlite3_stmt *statement, int column, char *output, size_t capacity) {
    const unsigned char *text;
    int bytes;

    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        output[0] = '\0';
        return true;
    }
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) {
        return false;
    }
    text = sqlite3_column_text(statement, column);
    bytes = sqlite3_column_bytes(statement, column);
    if (text == NULL || bytes < 0 || (size_t)bytes >= capacity) {
        return false;
    }
    (void)memcpy(output, text, (size_t)bytes);
    output[bytes] = '\0';
    return true;
}

static bool read_count(sqlite3_stmt *statement, int column, size_t *output) {
    sqlite3_int64 value;

    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) {
        return false;
    }
    value = sqlite3_column_int64(statement, column);
    if (value < 0 || (uint64_t)value > (uint64_t)SIZE_MAX) {
        return false;
    }
    *output = (size_t)value;
    return true;
}

static bool format_now(char output[TRAINLOG_TIMESTAMP_MAX + 1U]) {
    time_t now = time(NULL);
    struct tm utc;

    return now != (time_t)-1 && gmtime_r(&now, &utc) != NULL &&
           strftime(output, TRAINLOG_TIMESTAMP_MAX + 1U, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0U;
}

static TrainlogStatus load_ai_proposals(TrainlogDatabase *database,
                                        TrainlogWebPreparedItems *output) {
    static const char SQL[] =
        "SELECT d.draft_id,COALESCE(d.title,''),COALESCE(d.planned_for,''),"
        "COUNT(e.id),CASE WHEN d.published_at IS NULL THEN 'pending_publication' ELSE 'published' "
        "END,d.created_at FROM ai_session_drafts d LEFT JOIN ai_session_draft_entries e ON "
        "e.draft_row_id=d.id WHERE d.withdrawn_at IS NULL GROUP BY d.id ORDER BY "
        "COALESCE(d.planned_for,'9999-12-31'),"
        "d.created_at,d.draft_id LIMIT ?1;";
    sqlite3_stmt *statement = NULL;
    int step;

    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(
            statement, 1, (sqlite3_int64)(TRAINLOG_WEB_PREPARED_ITEM_CAPACITY + 1U)) != SQLITE_OK) {
        goto fail;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogWebPreparedItem *item;

        if (output->item_count == TRAINLOG_WEB_PREPARED_ITEM_CAPACITY) {
            output->partial = true;
            continue;
        }
        item = &output->items[output->item_count];
        item->kind = TRAINLOG_WEB_PREPARED_AI_PROPOSAL;
        if (!copy_column(statement, 0, item->identity, sizeof(item->identity)) ||
            !copy_column(statement, 1, item->title, sizeof(item->title)) ||
            !copy_column(statement, 2, item->planned_for, sizeof(item->planned_for)) ||
            !read_count(statement, 3, &item->occurrence_count) ||
            !copy_column(statement, 4, item->state, sizeof(item->state)) ||
            !copy_column(statement, 5, item->sort_timestamp, sizeof(item->sort_timestamp))) {
            goto fail;
        }
        (void)snprintf(item->provenance, sizeof(item->provenance), "%s", "ai_import");
        ++output->item_count;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;

fail:
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus load_manual_preparations(TrainlogDatabase *database,
                                               TrainlogWebPreparedItems *output) {
    static const char SQL[] =
        "SELECT p.preparation_id,r.title,COALESCE(r.planned_for,''),COUNT(e.entry_id),"
        "p.editing_state,p.updated_at FROM session_preparations p JOIN "
        "session_preparation_revisions r ON r.revision_id=p.current_revision_id LEFT JOIN "
        "session_preparation_entries e ON e.revision_id=r.revision_id WHERE "
        "p.withdrawn_at IS NULL GROUP BY p.preparation_id ORDER BY "
        "COALESCE(r.planned_for,'9999-12-31'),p.updated_at,p.preparation_id LIMIT ?1;";
    sqlite3_stmt *statement = NULL;
    int step;

    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(
            statement, 1, (sqlite3_int64)(TRAINLOG_WEB_PREPARED_ITEM_CAPACITY + 1U)) != SQLITE_OK) {
        goto fail;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogWebPreparedItem *item;

        if (output->item_count == TRAINLOG_WEB_PREPARED_ITEM_CAPACITY) {
            output->partial = true;
            continue;
        }
        item = &output->items[output->item_count];
        item->kind = TRAINLOG_WEB_PREPARED_MANUAL_PREPARATION;
        if (!copy_column(statement, 0, item->identity, sizeof(item->identity)) ||
            !copy_column(statement, 1, item->title, sizeof(item->title)) ||
            !copy_column(statement, 2, item->planned_for, sizeof(item->planned_for)) ||
            !read_count(statement, 3, &item->occurrence_count) ||
            !copy_column(statement, 4, item->state, sizeof(item->state)) ||
            !copy_column(statement, 5, item->sort_timestamp, sizeof(item->sort_timestamp))) {
            goto fail;
        }
        (void)snprintf(item->provenance, sizeof(item->provenance), "%s", "manual_preparation");
        ++output->item_count;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;

fail:
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus load_execution_drafts(TrainlogDatabase *database,
                                            TrainlogWebPreparedItems *output) {
    static const char SQL[] =
        "SELECT d.session_id,d.state,d.session_type,COALESCE(substr(d.started_at,1,10),''),"
        "COALESCE(json_array_length(d.payload_json,'$.exercises'),0),COALESCE(d.started_at,'') "
        "FROM execution_drafts d "
        "WHERE NOT EXISTS(SELECT 1 FROM execution_draft_finalizations f WHERE "
        "f.session_id=d.session_id) AND NOT EXISTS(SELECT 1 FROM sync_causal_state c WHERE "
        "c.target_kind='execution_draft' AND c.target_id=d.session_id AND c.deleted=1) "
        "ORDER BY CASE d.state WHEN 'active' THEN 0 ELSE 1 END,d.session_id LIMIT ?1;";
    sqlite3_stmt *statement = NULL;
    int step;
    size_t remaining = TRAINLOG_WEB_PREPARED_ITEM_CAPACITY - output->item_count;

    if (remaining == 0U) {
        return TRAINLOG_STATUS_OK;
    }
    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 1, (sqlite3_int64)(remaining + 1U)) != SQLITE_OK) {
        goto fail;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogWebPreparedItem *item;

        if (output->item_count == TRAINLOG_WEB_PREPARED_ITEM_CAPACITY) {
            output->partial = true;
            continue;
        }
        item = &output->items[output->item_count];
        item->kind = TRAINLOG_WEB_PREPARED_EXECUTION_DRAFT;
        if (!copy_column(statement, 0, item->identity, sizeof(item->identity)) ||
            !copy_column(statement, 1, item->state, sizeof(item->state)) ||
            !copy_column(statement, 2, item->title, sizeof(item->title)) ||
            !copy_column(statement, 3, item->planned_for, sizeof(item->planned_for)) ||
            !read_count(statement, 4, &item->occurrence_count) ||
            !copy_column(statement, 5, item->sort_timestamp, sizeof(item->sort_timestamp))) {
            goto fail;
        }
        (void)snprintf(item->provenance, sizeof(item->provenance), "%s", "execution_store");
        ++output->item_count;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;

fail:
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_web_prepared_items_load(TrainlogDatabase *database,
                                                TrainlogWebPreparedItems *output) {
    TrainlogStatus status;
    TrainlogStatus end_status;

    if (database == NULL || output == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(output, 0, sizeof(*output));
    if (!format_now(output->generated_at)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    status = load_manual_preparations(database, output);
    if (status == TRAINLOG_STATUS_OK) {
        status = load_ai_proposals(database, output);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = load_execution_drafts(database, output);
    }
    end_status = trainlog_database_read_snapshot_end(database, status == TRAINLOG_STATUS_OK);
    return status == TRAINLOG_STATUS_OK ? end_status : status;
}
