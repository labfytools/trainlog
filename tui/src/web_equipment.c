#include "trainlog/web_equipment.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"
#include "trainlog/equipment_catalog.h"

static bool add_count(sqlite3 *connection,
                      yyjson_mut_doc *document,
                      yyjson_mut_val *item,
                      const char *key,
                      const char *sql,
                      const char *equipment_id) {
    sqlite3_stmt *statement = NULL;
    bool ok = sqlite3_prepare_v2(connection, sql, -1, &statement, NULL) == SQLITE_OK &&
              sqlite3_bind_text(statement, 1, equipment_id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
              sqlite3_step(statement) == SQLITE_ROW &&
              yyjson_mut_obj_add_sint(document, item, key, sqlite3_column_int64(statement, 0)) &&
              sqlite3_step(statement) == SQLITE_DONE;
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK) {
        ok = false;
    }
    return ok;
}

static bool add_item(TrainlogDatabase *database,
                     yyjson_mut_doc *document,
                     yyjson_mut_val *items,
                     const TrainlogResolvedEquipment *equipment) {
    static const char EXERCISES[] =
        "SELECT COUNT(DISTINCT exercise_row_id) FROM session_exercises WHERE equipment_id=?1";
    static const char HISTORY[] = "SELECT COUNT(*) FROM session_exercises WHERE equipment_id=?1";
    static const char PREPARATIONS[] =
        "SELECT (SELECT COUNT(*) FROM session_preparation_entries WHERE equipment_id=?1)+"
        "(SELECT COUNT(*) FROM ai_session_draft_entries WHERE equipment_id=?1)";
    static const char PROGRAMS[] =
        "SELECT COUNT(*) FROM program_session_entries WHERE equipment_id=?1";
    const char *origin = equipment->origin == TRAINLOG_EQUIPMENT_SUPPLIED ? "supplied"
                         : equipment->origin == TRAINLOG_EQUIPMENT_CUSTOM ? "custom"
                                                                          : "unknown";
    yyjson_mut_val *item = yyjson_mut_obj(document);
    return item != NULL &&
           yyjson_mut_obj_add_strcpy(document, item, "equipment_id", equipment->equipment_id) &&
           yyjson_mut_obj_add_strcpy(document, item, "display_name", equipment->display_name) &&
           yyjson_mut_obj_add_strcpy(document, item, "equipment_type", equipment->equipment_type) &&
           yyjson_mut_obj_add_strcpy(document, item, "load_semantics", equipment->load_semantics) &&
           yyjson_mut_obj_add_strcpy(document, item, "origin", origin) &&
           add_count(database->connection,
                     document,
                     item,
                     "exercise_count",
                     EXERCISES,
                     equipment->equipment_id) &&
           add_count(database->connection,
                     document,
                     item,
                     "historical_occurrences",
                     HISTORY,
                     equipment->equipment_id) &&
           add_count(database->connection,
                     document,
                     item,
                     "preparation_references",
                     PREPARATIONS,
                     equipment->equipment_id) &&
           add_count(database->connection,
                     document,
                     item,
                     "program_references",
                     PROGRAMS,
                     equipment->equipment_id) &&
           yyjson_mut_arr_add_val(items, item);
}

TrainlogStatus
trainlog_web_equipment_list_json(TrainlogDatabase *database, char **output, size_t *output_size) {
    TrainlogCustomEquipment custom[TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX];
    size_t custom_count = 0U;
    size_t index;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *items;
    if (database == NULL || output == NULL || output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output = NULL;
    *output_size = 0U;
    if (trainlog_database_list_custom_equipment(
            database, custom, TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX, &custom_count) !=
        TRAINLOG_STATUS_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    items = document == NULL ? NULL : yyjson_mut_arr(document);
    if (root == NULL || items == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    for (index = 0U; index < trainlog_equipment_catalog_count(); ++index) {
        const TrainlogEquipment *source = trainlog_equipment_catalog_at(index);
        TrainlogResolvedEquipment resolved = {0};
        if (source == NULL) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        (void)snprintf(
            resolved.equipment_id, sizeof(resolved.equipment_id), "%s", source->equipment_id);
        (void)snprintf(
            resolved.display_name, sizeof(resolved.display_name), "%s", source->display_name);
        (void)snprintf(
            resolved.equipment_type, sizeof(resolved.equipment_type), "%s", source->equipment_type);
        (void)snprintf(
            resolved.load_semantics, sizeof(resolved.load_semantics), "%s", source->load_semantics);
        resolved.origin = TRAINLOG_EQUIPMENT_SUPPLIED;
        if (!add_item(database, document, items, &resolved)) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    for (index = 0U; index < custom_count; ++index) {
        TrainlogResolvedEquipment resolved;
        if (trainlog_database_resolve_equipment(database, custom[index].equipment_id, &resolved) !=
                TRAINLOG_STATUS_OK ||
            !add_item(database, document, items, &resolved)) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    if (!yyjson_mut_obj_add_val(document, root, "items", items)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    *output = yyjson_mut_write(document, 0U, output_size);
    yyjson_mut_doc_free(document);
    return *output == NULL ? TRAINLOG_STATUS_DATABASE_ERROR : TRAINLOG_STATUS_OK;
}

static bool
execute_bound(sqlite3 *connection, const char *sql, const char *canonical, const char *duplicate) {
    sqlite3_stmt *statement = NULL;
    bool ok = sqlite3_prepare_v2(connection, sql, -1, &statement, NULL) == SQLITE_OK &&
              sqlite3_bind_text(statement, 1, canonical, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
              sqlite3_bind_text(statement, 2, duplicate, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
              sqlite3_step(statement) == SQLITE_DONE;
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK) {
        ok = false;
    }
    return ok;
}

static bool embedded_draft_references(sqlite3 *connection, const char *equipment_id) {
    static const char SQL[] =
        "SELECT (SELECT COUNT(*) FROM execution_drafts WHERE "
        "instr(payload_json,json_quote(?1))>0)+(SELECT COUNT(*) FROM "
        "execution_draft_revisions WHERE instr(payload_json,json_quote(?1))>0)";
    sqlite3_stmt *statement = NULL;
    bool referenced = true;
    if (sqlite3_prepare_v2(connection, SQL, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_bind_text(statement, 1, equipment_id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        referenced = sqlite3_column_int64(statement, 0) > 0;
    }
    (void)sqlite3_finalize(statement);
    return referenced;
}

static bool
merge_already_applied(sqlite3 *connection, const char *canonical, const char *duplicate) {
    static const char SQL[] =
        "SELECT 1 FROM sync_causal_state WHERE target_kind='custom_equipment' AND "
        "target_id=?2 AND deleted=1 AND current_revision_id='equipment_merge_v1:'||?1";
    sqlite3_stmt *statement = NULL;
    bool applied = sqlite3_prepare_v2(connection, SQL, -1, &statement, NULL) == SQLITE_OK &&
                   sqlite3_bind_text(statement, 1, canonical, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
                   sqlite3_bind_text(statement, 2, duplicate, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
                   sqlite3_step(statement) == SQLITE_ROW;
    (void)sqlite3_finalize(statement);
    return applied;
}

TrainlogStatus trainlog_web_equipment_merge_json(TrainlogDatabase *database,
                                                 const char *input,
                                                 size_t input_size,
                                                 char **output,
                                                 size_t *output_size) {
    static const char *const UPDATES[] = {
        "UPDATE session_exercises SET equipment_id=?1 WHERE equipment_id=?2",
        "UPDATE ai_session_draft_entries SET equipment_id=?1 WHERE equipment_id=?2",
        "UPDATE session_preparation_entries SET equipment_id=?1 WHERE equipment_id=?2",
        "UPDATE program_session_entries SET equipment_id=?1 WHERE equipment_id=?2",
        "UPDATE exercises SET legacy_equipment_id=?1 WHERE legacy_equipment_id=?2",
    };
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *canonical_value;
    yyjson_val *duplicate_value;
    const char *canonical;
    const char *duplicate;
    TrainlogResolvedEquipment resolved;
    size_t index;
    bool ok = true;
    if (database == NULL || input == NULL || input_size == 0U || output == NULL ||
        output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    document = yyjson_read_opts((char *)input, input_size, YYJSON_READ_NOFLAG, NULL, NULL);
    root = document == NULL ? NULL : yyjson_doc_get_root(document);
    canonical_value = root == NULL ? NULL : yyjson_obj_get(root, "canonical_id");
    duplicate_value = root == NULL ? NULL : yyjson_obj_get(root, "duplicate_id");
    canonical = yyjson_is_str(canonical_value) ? yyjson_get_str(canonical_value) : NULL;
    duplicate = yyjson_is_str(duplicate_value) ? yyjson_get_str(duplicate_value) : NULL;
    if (canonical == NULL || duplicate == NULL || canonical[0] == '\0' || duplicate[0] == '\0' ||
        strcmp(canonical, duplicate) == 0 || strlen(canonical) > TRAINLOG_ID_MAX ||
        strlen(duplicate) > TRAINLOG_ID_MAX ||
        trainlog_database_resolve_equipment(database, canonical, &resolved) != TRAINLOG_STATUS_OK ||
        resolved.origin == TRAINLOG_EQUIPMENT_UNKNOWN) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (merge_already_applied(database->connection, canonical, duplicate)) {
        yyjson_doc_free(document);
        return trainlog_web_equipment_list_json(database, output, output_size);
    }
    if (trainlog_database_resolve_equipment(database, duplicate, &resolved) != TRAINLOG_STATUS_OK ||
        resolved.origin != TRAINLOG_EQUIPMENT_CUSTOM) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (embedded_draft_references(database->connection, duplicate)) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_CONFLICT;
    }
    /* CONTRACT: embedded JSON revisions are immutable evidence. Refuse a merge
     * if one mentions the duplicate instead of corrupting or partially moving it. */
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    for (index = 0U; index < sizeof(UPDATES) / sizeof(UPDATES[0]); ++index) {
        if (!execute_bound(database->connection, UPDATES[index], canonical, duplicate)) {
            ok = false;
            break;
        }
    }
    if (ok) {
        ok = execute_bound(database->connection,
                           "DELETE FROM custom_equipment WHERE equipment_id=?2",
                           canonical,
                           duplicate);
    }
    if (ok) {
        ok = execute_bound(database->connection,
                           "INSERT INTO sync_causal_state(target_kind,target_id,"
                           "current_revision_id,deleted,operation_id) VALUES("
                           "'custom_equipment',?2,'equipment_merge_v1:'||?1,1,NULL) ON CONFLICT("
                           "target_kind,target_id) DO UPDATE SET current_revision_id="
                           "excluded.current_revision_id,deleted=1,operation_id=NULL",
                           canonical,
                           duplicate);
    }
    if (!ok || sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    yyjson_doc_free(document);
    return trainlog_web_equipment_list_json(database, output, output_size);
}
