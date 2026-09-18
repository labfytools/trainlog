#include "trainlog/web_sessions.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"
#include "trainlog/id.h"

typedef struct ListDefinition {
    const char *kind;
    const char *sql;
} ListDefinition;

static const ListDefinition LISTS[] = {
    {"preparation",
     "SELECT p.preparation_id,r.title,r.planned_for,p.editing_state,COUNT(e.entry_id),p.updated_at "
     "FROM session_preparations p JOIN session_preparation_revisions r ON "
     "r.revision_id=p.current_revision_id "
     "LEFT JOIN session_preparation_entries e ON e.revision_id=r.revision_id "
     "WHERE (?1='' OR r.title LIKE '%'||?1||'%' COLLATE NOCASE OR EXISTS(SELECT 1 FROM "
     "session_preparation_entries pe JOIN exercises x ON x.exercise_id=pe.exercise_id WHERE "
     "pe.revision_id=r.revision_id AND x.name LIKE '%'||?1||'%' COLLATE NOCASE)) "
     "GROUP BY p.preparation_id ORDER BY "
     "COALESCE(r.planned_for,'9999-12-31'),p.updated_at,p.preparation_id LIMIT ?2 OFFSET ?3"},
    {"proposal",
     "SELECT d.draft_id,COALESCE(d.title,''),d.planned_for,CASE WHEN d.published_at IS NULL THEN "
     "'local' ELSE 'published' END,COUNT(e.entry_id),d.created_at "
     "FROM ai_session_drafts d LEFT JOIN ai_session_draft_entries e ON e.draft_row_id=d.id "
     "WHERE (?1='' OR d.title LIKE '%'||?1||'%' COLLATE NOCASE OR EXISTS(SELECT 1 FROM "
     "ai_session_draft_entries ae JOIN exercises x ON x.id=ae.exercise_row_id WHERE "
     "ae.draft_row_id=d.id AND x.name LIKE '%'||?1||'%' COLLATE NOCASE)) "
     "GROUP BY d.id ORDER BY COALESCE(d.planned_for,'9999-12-31'),d.created_at,d.draft_id LIMIT ?2 "
     "OFFSET ?3"},
    {"draft",
     "SELECT "
     "d.session_id,d.session_type,substr(d.started_at,1,10),d.state,COALESCE(json_array_length(d."
     "payload_json,'$.exercises'),0),COALESCE(d.started_at,'') "
     "FROM execution_drafts d WHERE NOT EXISTS(SELECT 1 FROM execution_draft_finalizations f WHERE "
     "f.session_id=d.session_id) "
     "AND NOT EXISTS(SELECT 1 FROM sync_causal_state c WHERE c.target_kind='execution_draft' AND "
     "c.target_id=d.session_id AND c.deleted=1) "
     "AND (?1='' OR d.session_type LIKE '%'||?1||'%' COLLATE NOCASE) ORDER BY CASE d.state WHEN "
     "'active' THEN 0 ELSE 1 END,COALESCE(d.started_at,''),d.session_id LIMIT ?2 OFFSET ?3"},
    {"history",
     "SELECT "
     "s.session_id,s.session_type,substr(s.started_at,1,10),'completed',COUNT(se.entry_id),s."
     "started_at "
     "FROM sessions s LEFT JOIN session_exercises se ON se.session_row_id=s.id "
     "WHERE (?1='' OR s.session_type LIKE '%'||?1||'%' COLLATE NOCASE OR EXISTS(SELECT 1 FROM "
     "session_exercises he JOIN exercises x ON x.id=he.exercise_row_id WHERE "
     "he.session_row_id=s.id AND x.name LIKE '%'||?1||'%' COLLATE NOCASE)) "
     "GROUP BY s.id ORDER BY s.started_at DESC,s.session_id LIMIT ?2 OFFSET ?3"},
};

static bool add_nullable_text(yyjson_mut_doc *document,
                              yyjson_mut_val *object,
                              const char *key,
                              sqlite3_stmt *statement,
                              int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return yyjson_mut_obj_add_null(document, object, key);
    }
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) {
        return false;
    }
    return yyjson_mut_obj_add_strcpy(
        document, object, key, (const char *)sqlite3_column_text(statement, column));
}

static TrainlogStatus
write_document(yyjson_mut_doc *document, char **output_json, size_t *output_size) {
    yyjson_write_err error;
    char *json;

    json = yyjson_mut_write_opts(document, YYJSON_WRITE_NOFLAG, NULL, output_size, &error);
    yyjson_mut_doc_free(document);
    if (json == NULL || *output_size == 0U || *output_size > TRAINLOG_WEB_SESSIONS_JSON_MAX) {
        free(json);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    *output_json = json;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_sessions_list_json(TrainlogDatabase *database,
                                               const TrainlogWebSessionsPageQuery *query,
                                               char **output_json,
                                               size_t *output_size) {
    yyjson_mut_doc *document = NULL;
    yyjson_mut_val *root;
    yyjson_mut_val *items;
    sqlite3_stmt *statement = NULL;
    TrainlogStatus status;
    TrainlogStatus end_status;
    size_t count = 0U;
    int step;
    const ListDefinition *definition;

    if (database == NULL || query == NULL || output_json == NULL || output_size == NULL ||
        query->collection > TRAINLOG_WEB_SESSION_HISTORY || query->limit == 0U ||
        query->limit > TRAINLOG_WEB_SESSIONS_PAGE_MAX || query->offset > INT64_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    definition = &LISTS[(size_t)query->collection];
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    if (sqlite3_prepare_v2(database->connection, definition->sql, -1, &statement, NULL) !=
            SQLITE_OK ||
        sqlite3_bind_text(
            statement, 1, query->search == NULL ? "" : query->search, -1, SQLITE_TRANSIENT) !=
            SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)(query->limit + 1U)) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)query->offset) != SQLITE_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto finish;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    items = document == NULL ? NULL : yyjson_mut_arr(document);
    if (document == NULL || root == NULL || items == NULL) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto finish;
    }
    yyjson_mut_doc_set_root(document, root);
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_strcpy(document, root, "kind", definition->kind) ||
        !yyjson_mut_obj_add_uint(document, root, "offset", query->offset) ||
        !yyjson_mut_obj_add_val(document, root, "items", items)) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto finish;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        yyjson_mut_val *item;
        sqlite3_int64 occurrence_count;

        if (count == query->limit) {
            ++count;
            break;
        }
        occurrence_count = sqlite3_column_int64(statement, 4);
        item = yyjson_mut_obj(document);
        if (item == NULL || occurrence_count < 0 ||
            !add_nullable_text(document, item, "identity", statement, 0) ||
            !add_nullable_text(document, item, "title", statement, 1) ||
            !add_nullable_text(document, item, "date", statement, 2) ||
            !add_nullable_text(document, item, "state", statement, 3) ||
            !yyjson_mut_obj_add_sint(document, item, "occurrence_count", occurrence_count) ||
            !add_nullable_text(document, item, "sort_timestamp", statement, 5) ||
            !yyjson_mut_arr_add_val(items, item)) {
            status = TRAINLOG_STATUS_DATABASE_ERROR;
            goto finish;
        }
        ++count;
    }
    if (step != SQLITE_DONE && count <= query->limit) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto finish;
    }
    if (!yyjson_mut_obj_add_bool(document, root, "more", count > query->limit) ||
        !yyjson_mut_obj_add_uint(document, root, "next_offset", query->offset + query->limit)) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto finish;
    }
    status = TRAINLOG_STATUS_OK;

finish:
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK &&
        status == TRAINLOG_STATUS_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    end_status = trainlog_database_read_snapshot_end(database, status == TRAINLOG_STATUS_OK);
    if (status == TRAINLOG_STATUS_OK) {
        status = end_status;
    }
    if (status != TRAINLOG_STATUS_OK) {
        yyjson_mut_doc_free(document);
        return status;
    }
    return write_document(document, output_json, output_size);
}

static TrainlogStatus detail_for_revision(TrainlogDatabase *database,
                                          const char *identity,
                                          bool proposal,
                                          yyjson_mut_doc *document,
                                          yyjson_mut_val *root) {
    const char *sql = proposal
                          ? "SELECT "
                            "d.draft_id,COALESCE(d.title,''),d.planned_for,d.session_type,d.notes,"
                            "i.payload_sha256,d.draft_id,CASE WHEN d.published_at IS NULL THEN "
                            "'local' ELSE 'published' END FROM ai_session_drafts d LEFT JOIN "
                            "ai_session_draft_imports i ON i.draft_row_id=d.id WHERE d.draft_id=?1"
                          : "SELECT "
                            "p.preparation_id,r.title,r.planned_for,r.session_type,r.notes,"
                            "COALESCE(p.source_payload_sha256,''),p.current_revision_id,p.delivery_"
                            "state FROM session_preparations p JOIN session_preparation_revisions "
                            "r ON r.revision_id=p.current_revision_id WHERE p.preparation_id=?1";
    const char *entry_sql =
        proposal
            ? "SELECT "
              "e.entry_id,e.position,x.exercise_id,x.name,e.equipment_id,e.recording_mode,e."
              "tracking_mode,e.load_mode,e.target_sets,e.target_reps,e.target_duration_seconds,e."
              "target_weight_kg,e.rest_seconds,NULL FROM ai_session_draft_entries e JOIN exercises "
              "x ON x.id=e.exercise_row_id JOIN ai_session_drafts d ON d.id=e.draft_row_id WHERE "
              "d.draft_id=?1 ORDER BY e.position,e.entry_id"
            : "SELECT "
              "e.entry_id,e.position,e.exercise_id,COALESCE(x.name,e.exercise_id),e.equipment_id,e."
              "recording_mode,e.tracking_mode,e.load_mode,e.target_sets,e.target_reps,e.target_"
              "duration_seconds,e.target_weight_kg,e.rest_seconds,e.notes FROM "
              "session_preparation_entries e LEFT JOIN exercises x ON x.exercise_id=e.exercise_id "
              "JOIN session_preparations p ON p.current_revision_id=e.revision_id WHERE "
              "p.preparation_id=?1 ORDER BY e.position,e.entry_id";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *entries;
    int step;
    int column;

    if (sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, identity, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    for (column = 0; column < 8; ++column) {
        static const char *const keys[] = {"identity",
                                           "title",
                                           "planned_for",
                                           "session_type",
                                           "notes",
                                           "source_fingerprint",
                                           "revision_id",
                                           "state"};
        if (!add_nullable_text(document, root, keys[column], statement, column)) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    statement = NULL;
    entries = yyjson_mut_arr(document);
    if (entries == NULL || !yyjson_mut_obj_add_val(document, root, "occurrences", entries) ||
        sqlite3_prepare_v2(database->connection, entry_sql, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, identity, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        static const char *const text_keys[] = {"entry_id",
                                                NULL,
                                                "exercise_id",
                                                "exercise_name",
                                                "equipment_id",
                                                "recording_mode",
                                                "tracking_mode",
                                                "load_mode"};
        yyjson_mut_val *entry = yyjson_mut_obj(document);
        int index;
        if (entry == NULL || !yyjson_mut_obj_add_sint(
                                 document, entry, "position", sqlite3_column_int64(statement, 1))) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
        for (index = 0; index < 8; ++index) {
            if (text_keys[index] != NULL &&
                !add_nullable_text(document, entry, text_keys[index], statement, index)) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }
        }
        for (index = 8; index <= 12; ++index) {
            static const char *const number_keys[] = {"target_sets",
                                                      "target_reps",
                                                      "target_duration_seconds",
                                                      "target_weight_kg",
                                                      "rest_seconds"};
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                (void)yyjson_mut_obj_add_null(document, entry, number_keys[index - 8]);
            } else if (index == 11) {
                (void)yyjson_mut_obj_add_real(document,
                                              entry,
                                              number_keys[index - 8],
                                              sqlite3_column_double(statement, index));
            } else {
                (void)yyjson_mut_obj_add_sint(document,
                                              entry,
                                              number_keys[index - 8],
                                              sqlite3_column_int64(statement, index));
            }
        }
        if (!add_nullable_text(document, entry, "notes", statement, 13) ||
            !yyjson_mut_arr_add_val(entries, entry)) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus detail_for_history(TrainlogDatabase *database,
                                         const char *identity,
                                         yyjson_mut_doc *document,
                                         yyjson_mut_val *root) {
    static const char SESSION_SQL[] = "SELECT session_id,session_type,started_at,ended_at,notes "
                                      "FROM sessions WHERE session_id=?1";
    static const char ENTRY_SQL[] =
        "SELECT "
        "se.entry_id,se.position,x.exercise_id,x.name,se.equipment_id,se.recording_mode,se."
        "tracking_mode,se.load_mode,se.target_sets,se.target_reps,se.target_duration_seconds,se."
        "target_weight_kg,se.rest_seconds,se.notes,se.max_weight_kg FROM session_exercises se JOIN "
        "exercises x ON x.id=se.exercise_row_id JOIN sessions s ON s.id=se.session_row_id WHERE "
        "s.session_id=?1 ORDER BY se.position,se.entry_id";
    static const char SET_SQL[] =
        "SELECT ps.position,ps.reps,ps.duration_seconds,ps.weight_kg FROM performed_sets ps JOIN "
        "session_exercises se ON se.id=ps.session_exercise_row_id WHERE se.entry_id=?1 ORDER BY "
        "ps.position";
    sqlite3_stmt *statement = NULL;
    sqlite3_stmt *sets_statement = NULL;
    yyjson_mut_val *entries;
    int step;

    if (sqlite3_prepare_v2(database->connection, SESSION_SQL, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, identity, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    if (!add_nullable_text(document, root, "identity", statement, 0) ||
        !add_nullable_text(document, root, "session_type", statement, 1) ||
        !add_nullable_text(document, root, "started_at", statement, 2) ||
        !add_nullable_text(document, root, "ended_at", statement, 3) ||
        !add_nullable_text(document, root, "notes", statement, 4)) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    statement = NULL;
    entries = yyjson_mut_arr(document);
    if (entries == NULL || !yyjson_mut_obj_add_val(document, root, "occurrences", entries) ||
        sqlite3_prepare_v2(database->connection, ENTRY_SQL, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, identity, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        yyjson_mut_val *entry = yyjson_mut_obj(document);
        yyjson_mut_val *sets = yyjson_mut_arr(document);
        const char *entry_id = (const char *)sqlite3_column_text(statement, 0);
        int set_step;
        int column;
        static const char *const text_keys[] = {"entry_id",
                                                NULL,
                                                "exercise_id",
                                                "exercise_name",
                                                "equipment_id",
                                                "recording_mode",
                                                "tracking_mode",
                                                "load_mode"};
        static const char *const numeric_keys[] = {"target_sets",
                                                   "target_reps",
                                                   "target_duration_seconds",
                                                   "target_weight_kg",
                                                   "rest_seconds",
                                                   "notes",
                                                   "max_weight_kg"};

        if (entry == NULL || sets == NULL ||
            !yyjson_mut_obj_add_sint(
                document, entry, "position", sqlite3_column_int64(statement, 1))) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
        for (column = 0; column < 8; ++column) {
            if (text_keys[column] != NULL &&
                !add_nullable_text(document, entry, text_keys[column], statement, column)) {
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_DATABASE_ERROR;
            }
        }
        for (column = 8; column <= 14; ++column) {
            if (column == 13) {
                (void)add_nullable_text(
                    document, entry, numeric_keys[column - 8], statement, column);
            } else if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
                (void)yyjson_mut_obj_add_null(document, entry, numeric_keys[column - 8]);
            } else if (column == 11 || column == 14) {
                (void)yyjson_mut_obj_add_real(document,
                                              entry,
                                              numeric_keys[column - 8],
                                              sqlite3_column_double(statement, column));
            } else {
                (void)yyjson_mut_obj_add_sint(document,
                                              entry,
                                              numeric_keys[column - 8],
                                              sqlite3_column_int64(statement, column));
            }
        }
        if (!yyjson_mut_obj_add_val(document, entry, "performed_sets", sets) ||
            sqlite3_prepare_v2(database->connection, SET_SQL, -1, &sets_statement, NULL) !=
                SQLITE_OK ||
            sqlite3_bind_text(sets_statement, 1, entry_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            (void)sqlite3_finalize(statement);
            if (sets_statement != NULL) {
                (void)sqlite3_finalize(sets_statement);
            }
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        while ((set_step = sqlite3_step(sets_statement)) == SQLITE_ROW) {
            yyjson_mut_val *set = yyjson_mut_obj(document);
            if (set == NULL ||
                !yyjson_mut_obj_add_sint(
                    document, set, "position", sqlite3_column_int64(sets_statement, 0))) {
                (void)sqlite3_finalize(sets_statement);
                (void)sqlite3_finalize(statement);
                return TRAINLOG_STATUS_SYSTEM_ERROR;
            }
            for (column = 1; column <= 3; ++column) {
                static const char *const keys[] = {"reps", "duration_seconds", "weight_kg"};
                if (sqlite3_column_type(sets_statement, column) == SQLITE_NULL) {
                    (void)yyjson_mut_obj_add_null(document, set, keys[column - 1]);
                } else if (column == 3) {
                    (void)yyjson_mut_obj_add_real(document,
                                                  set,
                                                  keys[column - 1],
                                                  sqlite3_column_double(sets_statement, column));
                } else {
                    (void)yyjson_mut_obj_add_sint(document,
                                                  set,
                                                  keys[column - 1],
                                                  sqlite3_column_int64(sets_statement, column));
                }
            }
            (void)yyjson_mut_arr_add_val(sets, set);
        }
        if (set_step != SQLITE_DONE || sqlite3_finalize(sets_statement) != SQLITE_OK ||
            !yyjson_mut_arr_add_val(entries, entry)) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        sets_statement = NULL;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus detail_for_draft(TrainlogDatabase *database,
                                       const char *identity,
                                       yyjson_mut_doc *document,
                                       yyjson_mut_val *root) {
    sqlite3_stmt *statement = NULL;
    yyjson_doc *payload_document = NULL;
    yyjson_val *payload;
    yyjson_mut_val *copied_payload;

    if (sqlite3_prepare_v2(
            database->connection,
            "SELECT session_id,session_type,started_at,state,revision_id,payload_json FROM "
            "execution_drafts WHERE session_id=?1",
            -1,
            &statement,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, identity, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    if (!add_nullable_text(document, root, "identity", statement, 0) ||
        !add_nullable_text(document, root, "session_type", statement, 1) ||
        !add_nullable_text(document, root, "started_at", statement, 2) ||
        !add_nullable_text(document, root, "state", statement, 3) ||
        !add_nullable_text(document, root, "revision_id", statement, 4)) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    payload_document = yyjson_read((const char *)sqlite3_column_text(statement, 5),
                                   (size_t)sqlite3_column_bytes(statement, 5),
                                   YYJSON_READ_NOFLAG);
    payload = payload_document == NULL ? NULL : yyjson_doc_get_root(payload_document);
    copied_payload = payload == NULL ? NULL : yyjson_val_mut_copy(document, payload);
    if (copied_payload == NULL ||
        !yyjson_mut_obj_add_val(document, root, "payload", copied_payload) ||
        sqlite3_finalize(statement) != SQLITE_OK) {
        yyjson_doc_free(payload_document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    yyjson_doc_free(payload_document);
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_sessions_detail_json(TrainlogDatabase *database,
                                                 const char *kind,
                                                 const char *identity,
                                                 char **output_json,
                                                 size_t *output_size) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    TrainlogStatus status;
    TrainlogStatus end_status;

    if (database == NULL || kind == NULL || identity == NULL || identity[0] == '\0' ||
        output_json == NULL || output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    if (strcmp(kind, "preparation") != 0 && strcmp(kind, "proposal") != 0 &&
        strcmp(kind, "history") != 0 && strcmp(kind, "draft") != 0) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    if (document == NULL || root == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    (void)yyjson_mut_obj_add_uint(document, root, "api_version", 1U);
    (void)yyjson_mut_obj_add_strcpy(document, root, "kind", kind);
    status = trainlog_database_read_snapshot_begin(database);
    if (status == TRAINLOG_STATUS_OK) {
        if (strcmp(kind, "history") == 0) {
            status = detail_for_history(database, identity, document, root);
        } else if (strcmp(kind, "draft") == 0) {
            status = detail_for_draft(database, identity, document, root);
        } else {
            status = detail_for_revision(
                database, identity, strcmp(kind, "proposal") == 0, document, root);
        }
    }
    end_status = trainlog_database_read_snapshot_end(database, status == TRAINLOG_STATUS_OK);
    if (status == TRAINLOG_STATUS_OK) {
        status = end_status;
    }
    if (status != TRAINLOG_STATUS_OK) {
        yyjson_mut_doc_free(document);
        return status;
    }
    return write_document(document, output_json, output_size);
}

TrainlogStatus trainlog_web_sessions_catalog_json(TrainlogDatabase *database,
                                                  const char *search,
                                                  size_t offset,
                                                  size_t limit,
                                                  char **output_json,
                                                  size_t *output_size) {
    static const char SQL[] =
        "SELECT exercise_id,name,recording_mode,tracking_mode,data_fields FROM exercises "
        "WHERE NOT EXISTS(SELECT 1 FROM sync_causal_state c WHERE c.target_kind='exercise' "
        "AND c.target_id=exercises.exercise_id AND c.deleted=1) "
        "AND (?1='' OR name LIKE '%'||?1||'%' COLLATE NOCASE) "
        "ORDER BY name COLLATE NOCASE,exercise_id LIMIT ?2 OFFSET ?3";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *items;
    size_t count = 0U;
    int step;

    if (database == NULL || output_json == NULL || output_size == NULL || limit == 0U ||
        limit > TRAINLOG_WEB_SESSIONS_PAGE_MAX || offset > INT64_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    items = document == NULL ? NULL : yyjson_mut_arr(document);
    if (document == NULL || root == NULL || items == NULL ||
        sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, search == NULL ? "" : search, -1, SQLITE_TRANSIENT) !=
            SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)(limit + 1U)) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)offset) != SQLITE_OK) {
        yyjson_mut_doc_free(document);
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    (void)yyjson_mut_obj_add_uint(document, root, "api_version", 1U);
    (void)yyjson_mut_obj_add_val(document, root, "items", items);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        yyjson_mut_val *item;
        if (count == limit) {
            ++count;
            break;
        }
        item = yyjson_mut_obj(document);
        if (item == NULL || !add_nullable_text(document, item, "exercise_id", statement, 0) ||
            !add_nullable_text(document, item, "name", statement, 1) ||
            !add_nullable_text(document, item, "recording_mode", statement, 2) ||
            !add_nullable_text(document, item, "tracking_mode", statement, 3) ||
            !yyjson_mut_obj_add_sint(
                document, item, "data_fields", sqlite3_column_int64(statement, 4)) ||
            !yyjson_mut_arr_add_val(items, item)) {
            (void)sqlite3_finalize(statement);
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        ++count;
    }
    if ((step != SQLITE_DONE && count <= limit) || sqlite3_finalize(statement) != SQLITE_OK ||
        !yyjson_mut_obj_add_bool(document, root, "more", count > limit) ||
        !yyjson_mut_obj_add_uint(document, root, "next_offset", offset + limit)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return write_document(document, output_json, output_size);
}

static bool timestamp_now(char output[TRAINLOG_TIMESTAMP_MAX + 1U]) {
    time_t now = time(NULL);
    struct tm utc;

    return now != (time_t)-1 && gmtime_r(&now, &utc) != NULL &&
           strftime(output, TRAINLOG_TIMESTAMP_MAX + 1U, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0U;
}

static bool
json_object_has_only(yyjson_val *object, const char *const *allowed, size_t allowed_count) {
    yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
    yyjson_val *key;

    while ((key = yyjson_obj_iter_next(&iterator)) != NULL) {
        size_t index;
        bool found = false;
        const char *name = yyjson_get_str(key);
        for (index = 0U; index < allowed_count; ++index) {
            if (name != NULL && strcmp(name, allowed[index]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

static bool bounded_json_string(yyjson_val *value, size_t capacity, const char **output) {
    const char *text;
    size_t length;

    if (!yyjson_is_str(value)) {
        return false;
    }
    text = yyjson_get_str(value);
    length = yyjson_get_len(value);
    if (text == NULL || length >= capacity || memchr(text, '\0', length) != NULL) {
        return false;
    }
    *output = text;
    return true;
}

static bool bind_nullable_text(sqlite3_stmt *statement, int index, yyjson_val *value) {
    if (value == NULL || yyjson_is_null(value)) {
        return sqlite3_bind_null(statement, index) == SQLITE_OK;
    }
    return yyjson_is_str(value) && sqlite3_bind_text(statement,
                                                     index,
                                                     yyjson_get_str(value),
                                                     (int)yyjson_get_len(value),
                                                     SQLITE_TRANSIENT) == SQLITE_OK;
}

static TrainlogStatus insert_preparation_entry(TrainlogDatabase *database,
                                               const char *revision_id,
                                               yyjson_val *entry,
                                               size_t position) {
    static const char *const ALLOWED[] = {"entry_id",
                                          "exercise_id",
                                          "equipment_id",
                                          "load_mode",
                                          "rest_seconds",
                                          "target_sets",
                                          "target_reps",
                                          "target_duration_seconds",
                                          "target_weight_kg",
                                          "notes"};
    static const char INSERT_SQL[] =
        "INSERT INTO "
        "session_preparation_entries(revision_id,entry_id,position,exercise_id,equipment_id,"
        "recording_mode,tracking_mode,data_fields,load_mode,rest_seconds,target_sets,target_reps,"
        "target_duration_seconds,target_weight_kg,notes) "
        "SELECT "
        "?1,?2,?3,e.exercise_id,?5,e.recording_mode,e.tracking_mode,e.data_fields,?6,?7,?8,?9,?10,?"
        "11,?12 FROM exercises e "
        "WHERE e.exercise_id=?4 AND NOT EXISTS(SELECT 1 FROM sync_causal_state c WHERE "
        "c.target_kind='exercise' AND c.target_id=e.exercise_id AND c.deleted=1)";
    sqlite3_stmt *statement = NULL;
    const char *entry_id;
    const char *exercise_id;
    const char *load_mode;
    char generated_id[TRAINLOG_GENERATED_ID_CAPACITY];
    yyjson_val *value;
    int64_t rest_seconds;
    int rc;

    if (!yyjson_is_obj(entry) ||
        !json_object_has_only(entry, ALLOWED, sizeof(ALLOWED) / sizeof(ALLOWED[0])) ||
        !bounded_json_string(
            yyjson_obj_get(entry, "exercise_id"), TRAINLOG_ID_MAX + 1U, &exercise_id) ||
        !bounded_json_string(yyjson_obj_get(entry, "load_mode"), 16U, &load_mode) ||
        (strcmp(load_mode, "none") != 0 && strcmp(load_mode, "external") != 0 &&
         strcmp(load_mode, "assistance") != 0)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    value = yyjson_obj_get(entry, "entry_id");
    if (value == NULL) {
        if (trainlog_id_generate("spe", generated_id, sizeof(generated_id)) != TRAINLOG_STATUS_OK) {
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
        entry_id = generated_id;
    } else if (!bounded_json_string(value, TRAINLOG_ID_MAX + 1U, &entry_id) ||
               strncmp(entry_id, "spe_", 4U) != 0) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    value = yyjson_obj_get(entry, "rest_seconds");
    if (!yyjson_is_int(value)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    rest_seconds = yyjson_get_sint(value);
    if (rest_seconds < 0 || rest_seconds > 86400 || position > INT64_MAX ||
        sqlite3_prepare_v2(database->connection, INSERT_SQL, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, revision_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, entry_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)position) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        !bind_nullable_text(statement, 5, yyjson_obj_get(entry, "equipment_id")) ||
        sqlite3_bind_text(statement, 6, load_mode, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 7, rest_seconds) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    value = yyjson_obj_get(entry, "target_sets");
    if (value == NULL || yyjson_is_null(value)) {
        (void)sqlite3_bind_null(statement, 8);
    } else if (!yyjson_is_int(value) || yyjson_get_sint(value) < 1 || yyjson_get_sint(value) > 99 ||
               sqlite3_bind_int64(statement, 8, yyjson_get_sint(value)) != SQLITE_OK) {
        goto invalid;
    }
    value = yyjson_obj_get(entry, "target_reps");
    if (value == NULL || yyjson_is_null(value)) {
        (void)sqlite3_bind_null(statement, 9);
    } else if (!yyjson_is_int(value) || yyjson_get_sint(value) < 1 ||
               yyjson_get_sint(value) > 10000 ||
               sqlite3_bind_int64(statement, 9, yyjson_get_sint(value)) != SQLITE_OK) {
        goto invalid;
    }
    value = yyjson_obj_get(entry, "target_duration_seconds");
    if (value == NULL || yyjson_is_null(value)) {
        (void)sqlite3_bind_null(statement, 10);
    } else if (!yyjson_is_int(value) || yyjson_get_sint(value) < 1 ||
               yyjson_get_sint(value) > 604800 ||
               sqlite3_bind_int64(statement, 10, yyjson_get_sint(value)) != SQLITE_OK) {
        goto invalid;
    }
    value = yyjson_obj_get(entry, "target_weight_kg");
    if (value == NULL || yyjson_is_null(value)) {
        (void)sqlite3_bind_null(statement, 11);
    } else if (!yyjson_is_num(value) || !isfinite(yyjson_get_real(value)) ||
               yyjson_get_real(value) <= 0.0 || yyjson_get_real(value) > 2000.0 ||
               sqlite3_bind_double(statement, 11, yyjson_get_real(value)) != SQLITE_OK) {
        goto invalid;
    }
    if (!bind_nullable_text(statement, 12, yyjson_obj_get(entry, "notes"))) {
        goto invalid;
    }
    rc = sqlite3_step(statement);
    (void)sqlite3_finalize(statement);
    return rc == SQLITE_DONE && sqlite3_changes(database->connection) == 1
               ? TRAINLOG_STATUS_OK
               : TRAINLOG_STATUS_CONFLICT;

invalid:
    (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_INVALID_ARGUMENT;
}

TrainlogStatus trainlog_web_sessions_save_json(TrainlogDatabase *database,
                                               const char *preparation_id,
                                               const char *expected_revision,
                                               const char *request_id,
                                               const char *save_body,
                                               size_t save_body_size,
                                               char **output_json,
                                               size_t *output_size) {
    static const char *const ALLOWED[] = {"title",
                                          "session_type",
                                          "planned_for",
                                          "notes",
                                          "editing_state",
                                          "occurrences",
                                          "source_proposal_id",
                                          "source_payload_sha256"};
    yyjson_doc *input = NULL;
    yyjson_val *root;
    yyjson_val *occurrences;
    sqlite3_stmt *statement = NULL;
    char new_preparation[TRAINLOG_GENERATED_ID_CAPACITY];
    char revision[TRAINLOG_GENERATED_ID_CAPACITY];
    char now[TRAINLOG_TIMESTAMP_MAX + 1U];
    const char *actual_preparation = preparation_id;
    const char *title;
    const char *session_type;
    const char *editing_state;
    const char *source_proposal = NULL;
    const char *source_fingerprint = NULL;
    size_t index;
    TrainlogStatus status = TRAINLOG_STATUS_OK;
    yyjson_mut_doc *response;
    yyjson_mut_val *response_root;

    if (database == NULL || expected_revision == NULL || request_id == NULL ||
        request_id[0] == '\0' || save_body == NULL || save_body_size == 0U || output_json == NULL ||
        output_size == NULL || !timestamp_now(now)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    input = yyjson_read(save_body, save_body_size, YYJSON_READ_NOFLAG);
    root = input == NULL ? NULL : yyjson_doc_get_root(input);
    if (!yyjson_is_obj(root) ||
        !json_object_has_only(root, ALLOWED, sizeof(ALLOWED) / sizeof(ALLOWED[0])) ||
        !bounded_json_string(yyjson_obj_get(root, "title"), TRAINLOG_NAME_MAX + 1U, &title) ||
        !bounded_json_string(yyjson_obj_get(root, "session_type"), 16U, &session_type) ||
        !bounded_json_string(yyjson_obj_get(root, "editing_state"), 16U, &editing_state) ||
        (strcmp(session_type, "training") != 0 && strcmp(session_type, "max_test") != 0) ||
        (strcmp(editing_state, "draft") != 0 && strcmp(editing_state, "ready") != 0)) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    occurrences = yyjson_obj_get(root, "occurrences");
    if (!yyjson_is_arr(occurrences) || yyjson_arr_size(occurrences) > 64U ||
        (strcmp(editing_state, "ready") == 0 &&
         (title[0] == '\0' || yyjson_arr_size(occurrences) == 0U))) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (yyjson_obj_get(root, "source_proposal_id") != NULL &&
        (!bounded_json_string(
             yyjson_obj_get(root, "source_proposal_id"), TRAINLOG_ID_MAX + 1U, &source_proposal) ||
         !bounded_json_string(
             yyjson_obj_get(root, "source_payload_sha256"), 65U, &source_fingerprint) ||
         strlen(source_fingerprint) != 64U)) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (preparation_id != NULL && preparation_id[0] != '\0' && source_proposal != NULL) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (actual_preparation == NULL || actual_preparation[0] == '\0') {
        if (expected_revision[0] != '\0' ||
            trainlog_id_generate("sp", new_preparation, sizeof(new_preparation)) !=
                TRAINLOG_STATUS_OK) {
            yyjson_doc_free(input);
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        actual_preparation = new_preparation;
    }
    if (trainlog_id_generate("spr", revision, sizeof(revision)) != TRAINLOG_STATUS_OK ||
        sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    /* INVARIANT: a replayed request returns its committed identity and does
     * not evaluate the now-stale expected revision again. */
    if (sqlite3_prepare_v2(
            database->connection,
            "SELECT response_json FROM session_preparation_requests WHERE request_id=?1",
            -1,
            &statement,
            NULL) == SQLITE_OK &&
        sqlite3_bind_text(statement, 1, request_id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        const char *saved = (const char *)sqlite3_column_text(statement, 0);
        *output_size = saved == NULL ? 0U : strlen(saved);
        *output_json = saved == NULL ? NULL : strdup(saved);
        (void)sqlite3_finalize(statement);
        (void)sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL);
        yyjson_doc_free(input);
        return *output_json == NULL ? TRAINLOG_STATUS_SYSTEM_ERROR : TRAINLOG_STATUS_OK;
    }
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
        statement = NULL;
    }
    if ((preparation_id == NULL || preparation_id[0] == '\0') && source_proposal != NULL) {
        if (sqlite3_prepare_v2(
                database->connection,
                "SELECT 1 FROM ai_session_draft_imports WHERE draft_id=?1 AND payload_sha256=?2",
                -1,
                &statement,
                NULL) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, source_proposal, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 2, source_fingerprint, -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_ROW) {
            status = TRAINLOG_STATUS_CONFLICT;
            goto rollback;
        }
        (void)sqlite3_finalize(statement);
        statement = NULL;
    }
    if (preparation_id != NULL && preparation_id[0] != '\0') {
        const char *current = NULL;
        if (sqlite3_prepare_v2(
                database->connection,
                "SELECT current_revision_id FROM session_preparations WHERE preparation_id=?1",
                -1,
                &statement,
                NULL) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, actual_preparation, -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_ROW) {
            status = TRAINLOG_STATUS_NOT_FOUND;
            goto rollback;
        }
        current = (const char *)sqlite3_column_text(statement, 0);
        if (current == NULL || strcmp(current, expected_revision) != 0) {
            status = TRAINLOG_STATUS_CONFLICT;
            goto rollback;
        }
        (void)sqlite3_finalize(statement);
        statement = NULL;
    } else if (sqlite3_prepare_v2(
                   database->connection,
                   "INSERT INTO session_preparations VALUES(?1,?2,?3,?3,?4,'local',?5,?6)",
                   -1,
                   &statement,
                   NULL) != SQLITE_OK ||
               sqlite3_bind_text(statement, 1, actual_preparation, -1, SQLITE_TRANSIENT) !=
                   SQLITE_OK ||
               sqlite3_bind_text(statement, 2, revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
               sqlite3_bind_text(statement, 3, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
               sqlite3_bind_text(statement, 4, editing_state, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
               (source_proposal == NULL
                    ? sqlite3_bind_null(statement, 5)
                    : sqlite3_bind_text(statement, 5, source_proposal, -1, SQLITE_TRANSIENT)) !=
                   SQLITE_OK ||
               (source_fingerprint == NULL
                    ? sqlite3_bind_null(statement, 6)
                    : sqlite3_bind_text(statement, 6, source_fingerprint, -1, SQLITE_TRANSIENT)) !=
                   SQLITE_OK ||
               sqlite3_step(statement) != SQLITE_DONE) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto rollback;
    }
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
        statement = NULL;
    }
    if (sqlite3_prepare_v2(
            database->connection,
            "INSERT INTO session_preparation_revisions VALUES(?1,?2,?3,?4,?5,?6,?7,?8)",
            -1,
            &statement,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, actual_preparation, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        (expected_revision[0] == '\0'
             ? sqlite3_bind_null(statement, 3)
             : sqlite3_bind_text(statement, 3, expected_revision, -1, SQLITE_TRANSIENT)) !=
            SQLITE_OK ||
        sqlite3_bind_text(statement, 4, title, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 5, session_type, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        !bind_nullable_text(statement, 6, yyjson_obj_get(root, "planned_for")) ||
        !bind_nullable_text(statement, 7, yyjson_obj_get(root, "notes")) ||
        sqlite3_bind_text(statement, 8, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) {
        status = TRAINLOG_STATUS_INVALID_ARGUMENT;
        goto rollback;
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    for (index = 0U; index < yyjson_arr_size(occurrences); ++index) {
        status =
            insert_preparation_entry(database, revision, yyjson_arr_get(occurrences, index), index);
        if (status != TRAINLOG_STATUS_OK) {
            goto rollback;
        }
    }
    if (preparation_id != NULL && preparation_id[0] != '\0') {
        if (sqlite3_prepare_v2(database->connection,
                               "UPDATE session_preparations SET "
                               "current_revision_id=?1,updated_at=?2,editing_state=?3,delivery_"
                               "state=CASE WHEN delivery_state='local' THEN 'local' ELSE 'pending' "
                               "END WHERE preparation_id=?4 AND current_revision_id=?5",
                               -1,
                               &statement,
                               NULL) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 2, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 3, editing_state, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 4, actual_preparation, -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_bind_text(statement, 5, expected_revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(database->connection) != 1) {
            status = TRAINLOG_STATUS_CONFLICT;
            goto rollback;
        }
        (void)sqlite3_finalize(statement);
        statement = NULL;
    }
    response = yyjson_mut_doc_new(NULL);
    response_root = response == NULL ? NULL : yyjson_mut_obj(response);
    if (response == NULL || response_root == NULL) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto rollback;
    }
    yyjson_mut_doc_set_root(response, response_root);
    (void)yyjson_mut_obj_add_uint(response, response_root, "api_version", 1U);
    (void)yyjson_mut_obj_add_strcpy(response, response_root, "preparation_id", actual_preparation);
    (void)yyjson_mut_obj_add_strcpy(response, response_root, "revision_id", revision);
    (void)yyjson_mut_obj_add_strcpy(response, response_root, "editing_state", editing_state);
    status = write_document(response, output_json, output_size);
    if (status != TRAINLOG_STATUS_OK ||
        sqlite3_prepare_v2(database->connection,
                           "INSERT INTO session_preparation_requests VALUES(?1,'save',?2,?3,?4,?5)",
                           -1,
                           &statement,
                           NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, request_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, actual_preparation, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, *output_json, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 5, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK ||
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        statement = NULL;
        status = TRAINLOG_STATUS_DATABASE_ERROR;
        goto rollback;
    }
    yyjson_doc_free(input);
    return TRAINLOG_STATUS_OK;

rollback:
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
    }
    (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
    free(*output_json);
    *output_json = NULL;
    *output_size = 0U;
    yyjson_doc_free(input);
    return status;
}

TrainlogStatus trainlog_web_sessions_deliver_json(TrainlogDatabase *database,
                                                  const char *preparation_id,
                                                  const char *expected_revision,
                                                  const char *request_id,
                                                  char **output_json,
                                                  size_t *output_size) {
    sqlite3_stmt *statement = NULL;
    char delivery_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char execution_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char now[TRAINLOG_TIMESTAMP_MAX + 1U];
    yyjson_mut_doc *response = NULL;
    yyjson_mut_val *root;
    TrainlogStatus status = TRAINLOG_STATUS_DATABASE_ERROR;

    if (database == NULL || preparation_id == NULL || expected_revision == NULL ||
        request_id == NULL || request_id[0] == '\0' || output_json == NULL || output_size == NULL ||
        trainlog_id_generate("spd", delivery_id, sizeof(delivery_id)) != TRAINLOG_STATUS_OK ||
        trainlog_id_generate("se", execution_id, sizeof(execution_id)) != TRAINLOG_STATUS_OK ||
        !timestamp_now(now)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_prepare_v2(database->connection,
                           "SELECT response_json FROM session_preparation_requests WHERE "
                           "request_id=?1 AND command='deliver'",
                           -1,
                           &statement,
                           NULL) == SQLITE_OK &&
        sqlite3_bind_text(statement, 1, request_id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        const char *saved = (const char *)sqlite3_column_text(statement, 0);
        *output_size = saved == NULL ? 0U : strlen(saved);
        *output_json = saved == NULL ? NULL : strdup(saved);
        (void)sqlite3_finalize(statement);
        (void)sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL);
        return *output_json == NULL ? TRAINLOG_STATUS_SYSTEM_ERROR : TRAINLOG_STATUS_OK;
    }
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
        statement = NULL;
    }
    if (sqlite3_prepare_v2(
            database->connection,
            "SELECT 1 FROM session_preparations p WHERE p.preparation_id=?1 AND "
            "p.current_revision_id=?2 AND p.editing_state='ready' AND EXISTS(SELECT 1 FROM "
            "session_preparation_entries e WHERE e.revision_id=p.current_revision_id)",
            -1,
            &statement,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, preparation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, expected_revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW) {
        status = TRAINLOG_STATUS_CONFLICT;
        goto delivery_rollback;
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    if (sqlite3_prepare_v2(
            database->connection,
            "INSERT INTO session_preparation_deliveries VALUES(?1,?2,?3,?4,'pending',?5,NULL,NULL)",
            -1,
            &statement,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, delivery_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, preparation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, expected_revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, execution_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 5, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) {
        goto delivery_rollback;
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    response = yyjson_mut_doc_new(NULL);
    root = response == NULL ? NULL : yyjson_mut_obj(response);
    if (response == NULL || root == NULL) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto delivery_rollback;
    }
    yyjson_mut_doc_set_root(response, root);
    (void)yyjson_mut_obj_add_uint(response, root, "api_version", 1U);
    (void)yyjson_mut_obj_add_strcpy(response, root, "delivery_id", delivery_id);
    (void)yyjson_mut_obj_add_strcpy(response, root, "execution_session_id", execution_id);
    (void)yyjson_mut_obj_add_strcpy(response, root, "revision_id", expected_revision);
    status = write_document(response, output_json, output_size);
    response = NULL;
    if (status != TRAINLOG_STATUS_OK ||
        sqlite3_prepare_v2(
            database->connection,
            "INSERT INTO session_preparation_requests VALUES(?1,'deliver',?2,?3,?4,?5)",
            -1,
            &statement,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, request_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, preparation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, expected_revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, *output_json, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 5, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL;
        goto delivery_rollback;
    }
    statement = NULL;
    if (sqlite3_prepare_v2(database->connection,
                           "UPDATE session_preparations SET delivery_state='pending' WHERE "
                           "preparation_id=?1 AND current_revision_id=?2",
                           -1,
                           &statement,
                           NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, preparation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, expected_revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(database->connection) != 1 ||
        sqlite3_finalize(statement) != SQLITE_OK ||
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        statement = NULL;
        goto delivery_rollback;
    }
    return TRAINLOG_STATUS_OK;

delivery_rollback:
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
    }
    yyjson_mut_doc_free(response);
    (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
    free(*output_json);
    *output_json = NULL;
    *output_size = 0U;
    return status;
}
