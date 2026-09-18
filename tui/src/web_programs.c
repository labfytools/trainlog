#include "trainlog/web_programs.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/evp.h>
#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"
#include "trainlog/id.h"
#include "trainlog/web_sessions.h"

#define PROGRAM_PAGE_MAX 64U

static bool timestamp_now(char output[32]) {
    time_t now = time(NULL);
    struct tm utc;

    return now != (time_t)-1 && gmtime_r(&now, &utc) != NULL &&
           strftime(output, 32U, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0U;
}

static TrainlogStatus
write_document(yyjson_mut_doc *document, char **output_json, size_t *output_size) {
    yyjson_write_err error;
    char *json = yyjson_mut_write_opts(document, YYJSON_WRITE_NOFLAG, NULL, output_size, &error);

    yyjson_mut_doc_free(document);
    if (json == NULL) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    *output_json = json;
    return TRAINLOG_STATUS_OK;
}

static bool
object_has_exact_keys(yyjson_val *object, const char *const *expected, size_t expected_count) {
    yyjson_obj_iter iterator;
    yyjson_val *key;
    size_t seen = 0U;

    if (!yyjson_is_obj(object) || yyjson_obj_size(object) != expected_count) {
        return false;
    }
    iterator = yyjson_obj_iter_with(object);
    while ((key = yyjson_obj_iter_next(&iterator)) != NULL) {
        const char *name = yyjson_get_str(key);
        size_t index;
        bool found = false;

        for (index = 0U; index < expected_count; ++index) {
            if (name != NULL && strcmp(name, expected[index]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
        ++seen;
    }
    return seen == expected_count;
}

static bool no_duplicate_keys(yyjson_val *value) {
    if (yyjson_is_obj(value)) {
        yyjson_obj_iter outer = yyjson_obj_iter_with(value);
        yyjson_val *key;

        while ((key = yyjson_obj_iter_next(&outer)) != NULL) {
            yyjson_obj_iter inner = yyjson_obj_iter_with(value);
            yyjson_val *other;
            size_t matches = 0U;

            while ((other = yyjson_obj_iter_next(&inner)) != NULL) {
                if (strcmp(yyjson_get_str(key), yyjson_get_str(other)) == 0) {
                    ++matches;
                }
            }
            if (matches != 1U || !no_duplicate_keys(yyjson_obj_iter_get_val(key))) {
                return false;
            }
        }
    } else if (yyjson_is_arr(value)) {
        size_t index;
        for (index = 0U; index < yyjson_arr_size(value); ++index) {
            if (!no_duplicate_keys(yyjson_arr_get(value, index))) {
                return false;
            }
        }
    }
    return true;
}

static bool bounded_string(yyjson_val *value, size_t maximum, bool nullable) {
    size_t length;
    if (nullable && yyjson_is_null(value)) {
        return true;
    }
    if (!yyjson_is_str(value)) {
        return false;
    }
    length = yyjson_get_len(value);
    return length > 0U && length <= maximum;
}

static bool nullable_date(yyjson_val *value) {
    const char *date;
    size_t index;
    static const size_t HYPHENS[] = {4U, 7U};

    if (yyjson_is_null(value)) {
        return true;
    }
    if (!yyjson_is_str(value) || yyjson_get_len(value) != 10U) {
        return false;
    }
    date = yyjson_get_str(value);
    for (index = 0U; index < 10U; ++index) {
        if ((index == HYPHENS[0] || index == HYPHENS[1]) ? date[index] != '-'
                                                         : date[index] < '0' || date[index] > '9') {
            return false;
        }
    }
    return true;
}

static bool digest_hex(const char *body, size_t body_size, char output[65]) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0U;
    size_t index;

    if (EVP_Digest(body, body_size, digest, &digest_size, EVP_sha256(), NULL) != 1 ||
        digest_size != 32U) {
        return false;
    }
    for (index = 0U; index < digest_size; ++index) {
        (void)snprintf(output + (index * 2U), 3U, "%02x", digest[index]);
    }
    output[64] = '\0';
    return true;
}

static bool add_column_text(yyjson_mut_doc *document,
                            yyjson_mut_val *object,
                            const char *key,
                            sqlite3_stmt *statement,
                            int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return yyjson_mut_obj_add_null(document, object, key);
    }
    return sqlite3_column_type(statement, column) == SQLITE_TEXT &&
           yyjson_mut_obj_add_strcpy(
               document, object, key, (const char *)sqlite3_column_text(statement, column));
}

static bool add_nullable_integer(yyjson_mut_doc *document,
                                 yyjson_mut_val *object,
                                 const char *key,
                                 sqlite3_stmt *statement,
                                 int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return yyjson_mut_obj_add_null(document, object, key);
    }
    return yyjson_mut_obj_add_sint(document, object, key, sqlite3_column_int64(statement, column));
}

static bool add_nullable_real(yyjson_mut_doc *document,
                              yyjson_mut_val *object,
                              const char *key,
                              sqlite3_stmt *statement,
                              int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return yyjson_mut_obj_add_null(document, object, key);
    }
    return yyjson_mut_obj_add_real(document, object, key, sqlite3_column_double(statement, column));
}

static bool
add_program_list_item(yyjson_mut_doc *document, yyjson_mut_val *items, sqlite3_stmt *statement) {
    yyjson_mut_val *item = yyjson_mut_obj(document);

    if (item == NULL) {
        return false;
    }
    return add_column_text(document, item, "program_id", statement, 0) &&
           add_column_text(document, item, "title", statement, 1) &&
           add_column_text(document, item, "state", statement, 2) &&
           add_column_text(document, item, "start_date", statement, 3) &&
           add_column_text(document, item, "end_date", statement, 4) &&
           yyjson_mut_obj_add_sint(
               document, item, "session_count", sqlite3_column_int64(statement, 5)) &&
           add_column_text(document, item, "provenance", statement, 6) &&
           add_column_text(document, item, "updated_at", statement, 7) &&
           yyjson_mut_arr_add_val(items, item);
}

TrainlogStatus trainlog_web_programs_list_json(TrainlogDatabase *database,
                                               const char *search,
                                               const char *state,
                                               size_t offset,
                                               size_t limit,
                                               char **output_json,
                                               size_t *output_size) {
    static const char SQL[] =
        "SELECT p.program_id,p.title,p.state,p.start_date,p.end_date,COUNT(s.program_session_id),"
        "p.source_format,p.updated_at FROM programs p LEFT JOIN program_sessions s ON "
        "s.program_id=p.program_id WHERE (?1='' OR p.title LIKE '%'||?1||'%' COLLATE NOCASE) "
        "AND (?2='' OR p.state=?2) GROUP BY p.program_id ORDER BY CASE p.state WHEN 'active' "
        "THEN 0 ELSE 1 END,p.title COLLATE NOCASE,p.program_id LIMIT ?3 OFFSET ?4";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *items;
    size_t count = 0U;
    int step;

    if (database == NULL || output_json == NULL || output_size == NULL || limit == 0U ||
        limit > PROGRAM_PAGE_MAX || offset > INT64_MAX ||
        (state != NULL && state[0] != '\0' && strcmp(state, "active") != 0 &&
         strcmp(state, "archived") != 0)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    items = document == NULL ? NULL : yyjson_mut_arr(document);
    if (document == NULL || root == NULL || items == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_bind_text(statement, 1, search == NULL ? "" : search, -1, SQLITE_TRANSIENT) !=
            SQLITE_OK ||
        sqlite3_bind_text(statement, 2, state == NULL ? "" : state, -1, SQLITE_TRANSIENT) !=
            SQLITE_OK ||
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)(limit + 1U)) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 4, (sqlite3_int64)offset) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    (void)yyjson_mut_obj_add_uint(document, root, "api_version", 1U);
    (void)yyjson_mut_obj_add_uint(document, root, "offset", offset);
    (void)yyjson_mut_obj_add_val(document, root, "items", items);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count == limit) {
            ++count;
            break;
        }
        if (!add_program_list_item(document, items, statement)) {
            (void)sqlite3_finalize(statement);
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        ++count;
    }
    if ((step != SQLITE_DONE && count <= limit) || sqlite3_finalize(statement) != SQLITE_OK) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    (void)yyjson_mut_obj_add_bool(document, root, "more", count > limit);
    (void)yyjson_mut_obj_add_uint(document, root, "next_offset", offset + limit);
    return write_document(document, output_json, output_size);
}

static bool
add_program_entry_json(yyjson_mut_doc *document, yyjson_mut_val *entries, sqlite3_stmt *statement) {
    yyjson_mut_val *entry = yyjson_mut_obj(document);

    if (entry == NULL) {
        return false;
    }
    return add_column_text(document, entry, "entry_id", statement, 0) &&
           yyjson_mut_obj_add_sint(
               document, entry, "position", sqlite3_column_int64(statement, 1)) &&
           add_column_text(document, entry, "exercise_id", statement, 2) &&
           add_column_text(document, entry, "equipment_id", statement, 3) &&
           add_column_text(document, entry, "recording_mode", statement, 4) &&
           add_column_text(document, entry, "tracking_mode", statement, 5) &&
           yyjson_mut_obj_add_sint(
               document, entry, "data_fields", sqlite3_column_int64(statement, 6)) &&
           add_column_text(document, entry, "load_mode", statement, 7) &&
           yyjson_mut_obj_add_sint(
               document, entry, "rest_seconds", sqlite3_column_int64(statement, 8)) &&
           add_nullable_integer(document, entry, "target_sets", statement, 9) &&
           add_nullable_integer(document, entry, "target_reps", statement, 10) &&
           add_nullable_integer(document, entry, "target_duration_seconds", statement, 11) &&
           add_nullable_real(document, entry, "target_weight_kg", statement, 12) &&
           add_column_text(document, entry, "notes", statement, 13) &&
           yyjson_mut_arr_add_val(entries, entry);
}

static TrainlogStatus add_program_entries(TrainlogDatabase *database,
                                          const char *session_id,
                                          yyjson_mut_doc *document,
                                          yyjson_mut_val *entries) {
    static const char SQL[] =
        "SELECT entry_id,position,exercise_id,equipment_id,recording_mode,tracking_mode,"
        "data_fields,load_mode,rest_seconds,target_sets,target_reps,target_duration_seconds,"
        "target_weight_kg,notes "
        "FROM program_session_entries "
        "WHERE program_session_id=?1 "
        "ORDER BY position,entry_id";
    sqlite3_stmt *statement = NULL;
    int step;

    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_bind_text(statement, 1, session_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (!add_program_entry_json(document, entries, statement)) {
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus add_program_session_json(TrainlogDatabase *database,
                                               yyjson_mut_doc *document,
                                               yyjson_mut_val *sessions,
                                               sqlite3_stmt *statement) {
    yyjson_mut_val *session = yyjson_mut_obj(document);
    yyjson_mut_val *entries = yyjson_mut_arr(document);
    const char *session_id = (const char *)sqlite3_column_text(statement, 0);
    TrainlogStatus status;

    if (session == NULL || entries == NULL || session_id == NULL) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (!add_column_text(document, session, "program_session_id", statement, 0) ||
        !yyjson_mut_obj_add_sint(
            document, session, "position", sqlite3_column_int64(statement, 1)) ||
        !add_column_text(document, session, "title", statement, 2) ||
        !add_column_text(document, session, "session_type", statement, 3) ||
        !add_column_text(document, session, "planned_for", statement, 4) ||
        !add_column_text(document, session, "note", statement, 5) ||
        !yyjson_mut_obj_add_val(document, session, "occurrences", entries) ||
        !yyjson_mut_arr_add_val(sessions, session)) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = add_program_entries(database, session_id, document, entries);
    return status;
}

static TrainlogStatus add_program_sessions(TrainlogDatabase *database,
                                           const char *program_id,
                                           yyjson_mut_doc *document,
                                           yyjson_mut_val *sessions) {
    static const char SESSION_SQL[] =
        "SELECT program_session_id,position,title,session_type,planned_for,note FROM "
        "program_sessions WHERE program_id=?1 ORDER BY position,program_session_id";
    sqlite3_stmt *session_statement = NULL;
    int step;

    if (sqlite3_prepare_v2(database->connection, SESSION_SQL, -1, &session_statement, NULL) !=
        SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_bind_text(session_statement, 1, program_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        (void)sqlite3_finalize(session_statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while ((step = sqlite3_step(session_statement)) == SQLITE_ROW) {
        TrainlogStatus status =
            add_program_session_json(database, document, sessions, session_statement);
        if (status != TRAINLOG_STATUS_OK) {
            (void)sqlite3_finalize(session_statement);
            return status;
        }
    }
    if (step != SQLITE_DONE || sqlite3_finalize(session_statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_programs_detail_json(TrainlogDatabase *database,
                                                 const char *program_id,
                                                 char **output_json,
                                                 size_t *output_size) {
    sqlite3_stmt *statement = NULL;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *sessions;
    TrainlogStatus status;

    if (database == NULL || program_id == NULL || program_id[0] == '\0' || output_json == NULL ||
        output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    sessions = document == NULL ? NULL : yyjson_mut_arr(document);
    if (document == NULL || root == NULL || sessions == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (sqlite3_prepare_v2(
            database->connection,
            "SELECT program_id,title,note,state,start_date,end_date,created_at,updated_at,"
            "revision_id,source_format,source_version,source_payload_sha256 "
            "FROM programs WHERE program_id=?1",
            -1,
            &statement,
            NULL) != SQLITE_OK) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_bind_text(statement, 1, program_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (sqlite3_step(statement) != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    yyjson_mut_doc_set_root(document, root);
    (void)yyjson_mut_obj_add_uint(document, root, "api_version", 1U);
    for (int column = 0; column < 12; ++column) {
        static const char *const KEYS[] = {"program_id",
                                           "title",
                                           "note",
                                           "state",
                                           "start_date",
                                           "end_date",
                                           "created_at",
                                           "updated_at",
                                           "revision_id",
                                           "source_format",
                                           "source_version",
                                           "source_payload_sha256"};
        if (column == 10) {
            (void)yyjson_mut_obj_add_sint(
                document, root, KEYS[column], sqlite3_column_int64(statement, column));
        } else {
            (void)add_column_text(document, root, KEYS[column], statement, column);
        }
    }
    (void)sqlite3_finalize(statement);
    (void)yyjson_mut_obj_add_val(document, root, "sessions", sessions);
    status = add_program_sessions(database, program_id, document, sessions);
    if (status != TRAINLOG_STATUS_OK) {
        yyjson_mut_doc_free(document);
        return status;
    }
    return write_document(document, output_json, output_size);
}

static bool equipment_is_known(TrainlogDatabase *database, yyjson_val *equipment) {
    sqlite3_stmt *statement = NULL;
    int result;

    if (yyjson_is_null(equipment)) {
        return true;
    }
    result = sqlite3_prepare_v2(
        database->connection,
        "SELECT 1 FROM custom_equipment e WHERE e.equipment_id=?1 AND NOT EXISTS("
        "SELECT 1 FROM sync_causal_state c WHERE c.target_kind='custom_equipment' "
        "AND c.target_id=e.equipment_id AND c.deleted=1)",
        -1,
        &statement,
        NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, yyjson_get_str(equipment), -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    (void)sqlite3_finalize(statement);
    return result == SQLITE_ROW;
}

static bool valid_target_weight(const char *load_mode, yyjson_val *weight) {
    double target_weight;

    if (strcmp(load_mode, "none") == 0) {
        return yyjson_is_null(weight);
    }
    if (!yyjson_is_num(weight)) {
        return false;
    }
    target_weight = yyjson_get_num(weight);
    return isfinite(target_weight) && target_weight > 0.0 && target_weight <= 2000.0;
}

static bool valid_occurrence(TrainlogDatabase *database, yyjson_val *occurrence) {
    static const char *const KEYS[] = {"entry_id",
                                       "exercise_id",
                                       "equipment_id",
                                       "load_mode",
                                       "rest_seconds",
                                       "target_sets",
                                       "target_reps",
                                       "target_duration_seconds",
                                       "target_weight_kg",
                                       "notes"};
    sqlite3_stmt *statement = NULL;
    const char *exercise_id;
    const char *load_mode;
    char recording_mode[16];
    char tracking_mode[16];
    int prepare_result;
    int bind_result;
    int step_result;
    yyjson_val *equipment;
    yyjson_val *weight;

    if (!object_has_exact_keys(occurrence, KEYS, sizeof(KEYS) / sizeof(KEYS[0])) ||
        !bounded_string(yyjson_obj_get(occurrence, "entry_id"), 128U, false) ||
        !bounded_string(yyjson_obj_get(occurrence, "exercise_id"), 128U, false) ||
        !(yyjson_is_null(yyjson_obj_get(occurrence, "equipment_id")) ||
          bounded_string(yyjson_obj_get(occurrence, "equipment_id"), 128U, false)) ||
        !bounded_string(yyjson_obj_get(occurrence, "load_mode"), 16U, false) ||
        !yyjson_is_int(yyjson_obj_get(occurrence, "rest_seconds")) ||
        yyjson_get_sint(yyjson_obj_get(occurrence, "rest_seconds")) < 0 ||
        yyjson_get_sint(yyjson_obj_get(occurrence, "rest_seconds")) > 86400 ||
        !(yyjson_is_null(yyjson_obj_get(occurrence, "notes")) ||
          bounded_string(yyjson_obj_get(occurrence, "notes"), 4000U, false))) {
        return false;
    }
    exercise_id = yyjson_get_str(yyjson_obj_get(occurrence, "exercise_id"));
    load_mode = yyjson_get_str(yyjson_obj_get(occurrence, "load_mode"));
    if (strcmp(load_mode, "none") != 0 && strcmp(load_mode, "external") != 0 &&
        strcmp(load_mode, "assistance") != 0) {
        return false;
    }
    equipment = yyjson_obj_get(occurrence, "equipment_id");
    weight = yyjson_obj_get(occurrence, "target_weight_kg");
    if (!equipment_is_known(database, equipment) || !valid_target_weight(load_mode, weight)) {
        return false;
    }
    prepare_result =
        sqlite3_prepare_v2(database->connection,
                           "SELECT recording_mode,tracking_mode "
                           "FROM exercises "
                           "WHERE exercise_id=?1 "
                           "AND NOT EXISTS(SELECT 1 FROM sync_causal_state c "
                           "WHERE c.target_kind='exercise' AND c.target_id=?1 AND c.deleted=1)",
                           -1,
                           &statement,
                           NULL);
    if (prepare_result != SQLITE_OK) {
        return false;
    }
    bind_result = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    if (bind_result != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return false;
    }
    step_result = sqlite3_step(statement);
    if (step_result != SQLITE_ROW || sqlite3_column_text(statement, 0) == NULL ||
        sqlite3_column_text(statement, 1) == NULL) {
        (void)sqlite3_finalize(statement);
        return false;
    }
    (void)snprintf(recording_mode,
                   sizeof(recording_mode),
                   "%s",
                   (const char *)sqlite3_column_text(statement, 0));
    (void)snprintf(tracking_mode,
                   sizeof(tracking_mode),
                   "%s",
                   (const char *)sqlite3_column_text(statement, 1));
    (void)sqlite3_finalize(statement);
    if (strcmp(recording_mode, "sets") == 0) {
        yyjson_val *target_sets = yyjson_obj_get(occurrence, "target_sets");
        if (!yyjson_is_int(target_sets) || yyjson_get_sint(target_sets) < 1 ||
            yyjson_get_sint(target_sets) > 99) {
            return false;
        }
    } else if (!yyjson_is_null(yyjson_obj_get(occurrence, "target_sets"))) {
        return false;
    }
    if (strcmp(tracking_mode, "reps") == 0) {
        yyjson_val *target_reps = yyjson_obj_get(occurrence, "target_reps");
        return yyjson_is_int(target_reps) && yyjson_get_sint(target_reps) >= 1 &&
               yyjson_get_sint(target_reps) <= 10000 &&
               yyjson_is_null(yyjson_obj_get(occurrence, "target_duration_seconds"));
    }
    return yyjson_is_int(yyjson_obj_get(occurrence, "target_duration_seconds")) &&
           yyjson_get_sint(yyjson_obj_get(occurrence, "target_duration_seconds")) >= 1 &&
           yyjson_get_sint(yyjson_obj_get(occurrence, "target_duration_seconds")) <= 604800 &&
           yyjson_is_null(yyjson_obj_get(occurrence, "target_reps"));
}

static bool
validate_program(TrainlogDatabase *database, yyjson_val *root, yyjson_val **program_out) {
    static const char *const ROOT_KEYS[] = {"format", "version", "program"};
    static const char *const PROGRAM_KEYS[] = {
        "program_id", "title", "note", "state", "start_date", "end_date", "sessions"};
    static const char *const SESSION_KEYS[] = {
        "program_session_id", "title", "session_type", "planned_for", "note", "occurrences"};
    yyjson_val *program;
    yyjson_val *sessions;
    size_t session_index;

    if (!no_duplicate_keys(root) || !object_has_exact_keys(root, ROOT_KEYS, 3U) ||
        !yyjson_equals_str(yyjson_obj_get(root, "format"), "trainlog-program") ||
        !yyjson_is_int(yyjson_obj_get(root, "version")) ||
        yyjson_get_sint(yyjson_obj_get(root, "version")) != 1) {
        return false;
    }
    program = yyjson_obj_get(root, "program");
    if (!object_has_exact_keys(program, PROGRAM_KEYS, 7U) ||
        !bounded_string(yyjson_obj_get(program, "program_id"), 128U, false) ||
        !bounded_string(yyjson_obj_get(program, "title"), 200U, false) ||
        !(yyjson_is_null(yyjson_obj_get(program, "note")) ||
          bounded_string(yyjson_obj_get(program, "note"), 4000U, false)) ||
        !(yyjson_equals_str(yyjson_obj_get(program, "state"), "active") ||
          yyjson_equals_str(yyjson_obj_get(program, "state"), "archived")) ||
        !nullable_date(yyjson_obj_get(program, "start_date")) ||
        !nullable_date(yyjson_obj_get(program, "end_date"))) {
        return false;
    }
    sessions = yyjson_obj_get(program, "sessions");
    if (!yyjson_is_arr(sessions) || yyjson_arr_size(sessions) == 0U ||
        yyjson_arr_size(sessions) > TRAINLOG_WEB_PROGRAM_SESSIONS_MAX) {
        return false;
    }
    for (session_index = 0U; session_index < yyjson_arr_size(sessions); ++session_index) {
        yyjson_val *session = yyjson_arr_get(sessions, session_index);
        yyjson_val *occurrences;
        size_t occurrence_index;
        if (!object_has_exact_keys(session, SESSION_KEYS, 6U) ||
            !bounded_string(yyjson_obj_get(session, "program_session_id"), 128U, false) ||
            !bounded_string(yyjson_obj_get(session, "title"), 200U, false) ||
            !(yyjson_equals_str(yyjson_obj_get(session, "session_type"), "training") ||
              yyjson_equals_str(yyjson_obj_get(session, "session_type"), "max_test")) ||
            !nullable_date(yyjson_obj_get(session, "planned_for")) ||
            !(yyjson_is_null(yyjson_obj_get(session, "note")) ||
              bounded_string(yyjson_obj_get(session, "note"), 4000U, false))) {
            return false;
        }
        occurrences = yyjson_obj_get(session, "occurrences");
        if (!yyjson_is_arr(occurrences) || yyjson_arr_size(occurrences) == 0U ||
            yyjson_arr_size(occurrences) > TRAINLOG_WEB_PROGRAM_OCCURRENCES_MAX) {
            return false;
        }
        for (occurrence_index = 0U; occurrence_index < yyjson_arr_size(occurrences);
             ++occurrence_index) {
            if (!valid_occurrence(database, yyjson_arr_get(occurrences, occurrence_index))) {
                return false;
            }
        }
    }
    *program_out = program;
    return true;
}

static int bind_json_text(sqlite3_stmt *statement, int index, yyjson_val *value) {
    return yyjson_is_null(value)
               ? sqlite3_bind_null(statement, index)
               : sqlite3_bind_text(statement, index, yyjson_get_str(value), -1, SQLITE_TRANSIENT);
}

static TrainlogStatus program_insert_header(TrainlogDatabase *database,
                                            yyjson_val *program,
                                            const char *digest,
                                            const char *now,
                                            const char *revision) {
    static const char SQL[] = "INSERT INTO programs "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?7,?8,'trainlog-program',1,?9)";
    sqlite3_stmt *statement = NULL;
    int result;

    result = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (result != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    result = sqlite3_bind_text(
        statement, 1, yyjson_get_str(yyjson_obj_get(program, "program_id")), -1, SQLITE_TRANSIENT);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(
            statement, 2, yyjson_get_str(yyjson_obj_get(program, "title")), -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = bind_json_text(statement, 3, yyjson_obj_get(program, "note"));
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(
            statement, 4, yyjson_get_str(yyjson_obj_get(program, "state")), -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = bind_json_text(statement, 5, yyjson_obj_get(program, "start_date"));
    }
    if (result == SQLITE_OK) {
        result = bind_json_text(statement, 6, yyjson_obj_get(program, "end_date"));
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 7, now, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 8, revision, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 9, digest, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        result = SQLITE_ERROR;
    }
    return result == SQLITE_OK ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

typedef struct ProgramExerciseProfile {
    char recording_mode[16];
    char tracking_mode[16];
    int64_t data_fields;
} ProgramExerciseProfile;

static TrainlogStatus program_load_profile(TrainlogDatabase *database,
                                           const char *exercise_id,
                                           ProgramExerciseProfile *profile) {
    sqlite3_stmt *statement = NULL;
    int result;

    result = sqlite3_prepare_v2(
        database->connection,
        "SELECT recording_mode,tracking_mode,data_fields FROM exercises WHERE exercise_id=?1",
        -1,
        &statement,
        NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
        (void)snprintf(profile->recording_mode,
                       sizeof(profile->recording_mode),
                       "%s",
                       (const char *)sqlite3_column_text(statement, 0));
        (void)snprintf(profile->tracking_mode,
                       sizeof(profile->tracking_mode),
                       "%s",
                       (const char *)sqlite3_column_text(statement, 1));
        profile->data_fields = sqlite3_column_int64(statement, 2);
    } else {
        result = SQLITE_ERROR;
    }
    (void)sqlite3_finalize(statement);
    return result == SQLITE_OK ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

static int
bind_optional_number(sqlite3_stmt *statement, int parameter, yyjson_val *value, bool real_value) {
    if (yyjson_is_null(value)) {
        return sqlite3_bind_null(statement, parameter);
    }
    if (real_value) {
        return sqlite3_bind_double(statement, parameter, yyjson_get_real(value));
    }
    return sqlite3_bind_int64(statement, parameter, yyjson_get_sint(value));
}

static TrainlogStatus program_insert_occurrence(TrainlogDatabase *database,
                                                const char *session_id,
                                                yyjson_val *occurrence,
                                                size_t position) {
    static const char SQL[] = "INSERT INTO program_session_entries "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15)";
    sqlite3_stmt *statement = NULL;
    ProgramExerciseProfile profile;
    const char *exercise_id = yyjson_get_str(yyjson_obj_get(occurrence, "exercise_id"));
    int result;

    if (program_load_profile(database, exercise_id, &profile) != TRAINLOG_STATUS_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    result = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, session_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement,
                                   2,
                                   yyjson_get_str(yyjson_obj_get(occurrence, "entry_id")),
                                   -1,
                                   SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 3, (sqlite3_int64)position);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, exercise_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = bind_json_text(statement, 5, yyjson_obj_get(occurrence, "equipment_id"));
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 6, profile.recording_mode, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 7, profile.tracking_mode, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 8, profile.data_fields);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement,
                                   9,
                                   yyjson_get_str(yyjson_obj_get(occurrence, "load_mode")),
                                   -1,
                                   SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(
            statement, 10, yyjson_get_sint(yyjson_obj_get(occurrence, "rest_seconds")));
    }
    if (result == SQLITE_OK) {
        result =
            bind_optional_number(statement, 11, yyjson_obj_get(occurrence, "target_sets"), false);
    }
    if (result == SQLITE_OK) {
        result =
            bind_optional_number(statement, 12, yyjson_obj_get(occurrence, "target_reps"), false);
    }
    if (result == SQLITE_OK) {
        result = bind_optional_number(
            statement, 13, yyjson_obj_get(occurrence, "target_duration_seconds"), false);
    }
    if (result == SQLITE_OK) {
        result = bind_optional_number(
            statement, 14, yyjson_obj_get(occurrence, "target_weight_kg"), true);
    }
    if (result == SQLITE_OK) {
        result = bind_json_text(statement, 15, yyjson_obj_get(occurrence, "notes"));
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        result = SQLITE_ERROR;
    }
    return result == SQLITE_OK ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus program_insert_session(TrainlogDatabase *database,
                                             const char *program_id,
                                             yyjson_val *session,
                                             size_t position) {
    sqlite3_stmt *statement = NULL;
    yyjson_val *occurrences = yyjson_obj_get(session, "occurrences");
    const char *session_id = yyjson_get_str(yyjson_obj_get(session, "program_session_id"));
    size_t occurrence_index;
    int result;

    result = sqlite3_prepare_v2(database->connection,
                                "INSERT INTO program_sessions VALUES(?1,?2,?3,?4,?5,?6,?7)",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, session_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, program_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 3, (sqlite3_int64)position);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(
            statement, 4, yyjson_get_str(yyjson_obj_get(session, "title")), -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement,
                                   5,
                                   yyjson_get_str(yyjson_obj_get(session, "session_type")),
                                   -1,
                                   SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = bind_json_text(statement, 6, yyjson_obj_get(session, "planned_for"));
    }
    if (result == SQLITE_OK) {
        result = bind_json_text(statement, 7, yyjson_obj_get(session, "note"));
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        result = SQLITE_ERROR;
    }
    if (result != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    for (occurrence_index = 0U; occurrence_index < yyjson_arr_size(occurrences);
         ++occurrence_index) {
        TrainlogStatus status = program_insert_occurrence(
            database, session_id, yyjson_arr_get(occurrences, occurrence_index), occurrence_index);
        if (status != TRAINLOG_STATUS_OK) {
            return status;
        }
    }
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus insert_program(TrainlogDatabase *database,
                                     yyjson_val *program,
                                     const char *digest,
                                     const char *now,
                                     const char *revision) {
    yyjson_val *sessions = yyjson_obj_get(program, "sessions");
    const char *program_id = yyjson_get_str(yyjson_obj_get(program, "program_id"));
    size_t session_index;
    TrainlogStatus status = program_insert_header(database, program, digest, now, revision);

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    for (session_index = 0U; session_index < yyjson_arr_size(sessions); ++session_index) {
        status = program_insert_session(
            database, program_id, yyjson_arr_get(sessions, session_index), session_index);
        if (status != TRAINLOG_STATUS_OK) {
            return status;
        }
    }
    return TRAINLOG_STATUS_OK;
}

typedef enum ExistingProgramResult {
    EXISTING_PROGRAM_NONE = 0,
    EXISTING_PROGRAM_IDENTICAL,
    EXISTING_PROGRAM_CONFLICT,
    EXISTING_PROGRAM_ERROR
} ExistingProgramResult;

static ExistingProgramResult program_lookup_existing_import(TrainlogDatabase *database,
                                                            const char *program_id,
                                                            const char *digest) {
    sqlite3_stmt *statement = NULL;
    ExistingProgramResult outcome = EXISTING_PROGRAM_ERROR;
    int result;

    result = sqlite3_prepare_v2(database->connection,
                                "SELECT source_payload_sha256 FROM programs WHERE program_id=?1",
                                -1,
                                &statement,
                                NULL);
    if (result != SQLITE_OK) {
        return EXISTING_PROGRAM_ERROR;
    }
    result = sqlite3_bind_text(statement, 1, program_id, -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        (void)sqlite3_finalize(statement);
        return EXISTING_PROGRAM_ERROR;
    }
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE) {
        outcome = EXISTING_PROGRAM_NONE;
    } else if (result == SQLITE_ROW) {
        const char *known_digest = (const char *)sqlite3_column_text(statement, 0);
        outcome = known_digest != NULL && strcmp(known_digest, digest) == 0
                      ? EXISTING_PROGRAM_IDENTICAL
                      : EXISTING_PROGRAM_CONFLICT;
    }
    (void)sqlite3_finalize(statement);
    return outcome;
}

static TrainlogStatus program_persist_import(TrainlogDatabase *database,
                                             yyjson_val *program,
                                             const char *digest,
                                             const char *now,
                                             const char *revision) {
    TrainlogStatus status;

    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = insert_program(database, program, digest, now, revision);
    if (status == TRAINLOG_STATUS_OK &&
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) {
        return TRAINLOG_STATUS_OK;
    }
    (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
    return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
}

static TrainlogStatus program_build_import_response(yyjson_val *program,
                                                    const char *digest,
                                                    bool imported,
                                                    char **output_json,
                                                    size_t *output_size) {
    yyjson_mut_doc *response = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = response == NULL ? NULL : yyjson_mut_obj(response);
    yyjson_mut_val *warnings = response == NULL ? NULL : yyjson_mut_arr(response);
    yyjson_val *start_date = yyjson_obj_get(program, "start_date");
    yyjson_val *end_date = yyjson_obj_get(program, "end_date");

    if (response == NULL || root == NULL || warnings == NULL) {
        yyjson_mut_doc_free(response);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(response, root);
    (void)yyjson_mut_obj_add_uint(response, root, "api_version", 1U);
    (void)yyjson_mut_obj_add_strcpy(
        response, root, "program_id", yyjson_get_str(yyjson_obj_get(program, "program_id")));
    (void)yyjson_mut_obj_add_strcpy(
        response, root, "title", yyjson_get_str(yyjson_obj_get(program, "title")));
    if (yyjson_is_null(start_date)) {
        (void)yyjson_mut_obj_add_null(response, root, "start_date");
    } else {
        (void)yyjson_mut_obj_add_strcpy(response, root, "start_date", yyjson_get_str(start_date));
    }
    if (yyjson_is_null(end_date)) {
        (void)yyjson_mut_obj_add_null(response, root, "end_date");
    } else {
        (void)yyjson_mut_obj_add_strcpy(response, root, "end_date", yyjson_get_str(end_date));
    }
    (void)yyjson_mut_obj_add_strcpy(response, root, "payload_sha256", digest);
    (void)yyjson_mut_obj_add_uint(
        response, root, "session_count", yyjson_arr_size(yyjson_obj_get(program, "sessions")));
    (void)yyjson_mut_obj_add_uint(response, root, "unknown_exercise_count", 0U);
    (void)yyjson_mut_obj_add_val(response, root, "warnings", warnings);
    (void)yyjson_mut_obj_add_bool(response, root, "imported", imported);
    return write_document(response, output_json, output_size);
}

TrainlogStatus trainlog_web_programs_import_json(TrainlogDatabase *database,
                                                 const char *body,
                                                 size_t body_size,
                                                 bool commit,
                                                 char **output_json,
                                                 size_t *output_size) {
    yyjson_doc *input;
    yyjson_val *root;
    yyjson_val *program = NULL;
    char digest[65];
    char now[32];
    char revision[80];
    const char *program_id;
    ExistingProgramResult existing;
    TrainlogStatus status = TRAINLOG_STATUS_OK;
    bool imported = false;
    if (database == NULL || body == NULL || body_size == 0U ||
        body_size > TRAINLOG_WEB_PROGRAM_BYTES_MAX || output_json == NULL || output_size == NULL ||
        !digest_hex(body, body_size, digest) || !timestamp_now(now) ||
        trainlog_id_generate("pgr", revision, sizeof(revision)) != TRAINLOG_STATUS_OK) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    input = yyjson_read(body, body_size, YYJSON_READ_NOFLAG);
    root = input == NULL ? NULL : yyjson_doc_get_root(input);
    if (!validate_program(database, root, &program)) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    program_id = yyjson_get_str(yyjson_obj_get(program, "program_id"));
    existing = program_lookup_existing_import(database, program_id, digest);
    if (existing == EXISTING_PROGRAM_ERROR) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    } else if (existing == EXISTING_PROGRAM_CONFLICT) {
        status = TRAINLOG_STATUS_CONFLICT;
    } else if (commit && existing == EXISTING_PROGRAM_NONE) {
        status = program_persist_import(database, program, digest, now, revision);
        imported = status == TRAINLOG_STATUS_OK;
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = program_build_import_response(program, digest, imported, output_json, output_size);
    }
    yyjson_doc_free(input);
    return status;
}

static TrainlogStatus program_replay_archive(TrainlogDatabase *database,
                                             const char *request_id,
                                             char **output_json,
                                             size_t *output_size,
                                             bool *found) {
    sqlite3_stmt *statement = NULL;
    int result;

    *found = false;
    result = sqlite3_prepare_v2(database->connection,
                                "SELECT response_json FROM program_requests WHERE request_id=?1",
                                -1,
                                &statement,
                                NULL);
    if (result != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    result = sqlite3_bind_text(statement, 1, request_id, -1, SQLITE_TRANSIENT);
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result == SQLITE_ROW) {
        const char *saved = (const char *)sqlite3_column_text(statement, 0);
        *output_size = saved == NULL ? 0U : strlen(saved);
        *output_json = saved == NULL ? NULL : strdup(saved);
        *found = true;
        if (*output_json == NULL) {
            result = SQLITE_NOMEM;
        } else {
            result = SQLITE_OK;
        }
    } else if (result == SQLITE_DONE) {
        result = SQLITE_OK;
    }
    (void)sqlite3_finalize(statement);
    return result == SQLITE_OK ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus program_apply_archive(TrainlogDatabase *database,
                                            const char *program_id,
                                            const char *expected_revision,
                                            const char *revision,
                                            const char *now) {
    sqlite3_stmt *statement = NULL;
    int result;

    result = sqlite3_prepare_v2(database->connection,
                                "UPDATE programs SET state='archived',updated_at=?1,revision_id=?2 "
                                "WHERE program_id=?3 AND revision_id=?4 AND state='active'",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, now, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, revision, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, program_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, expected_revision, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    }
    if (result == SQLITE_OK && sqlite3_changes(database->connection) != 1) {
        result = SQLITE_CONSTRAINT;
    }
    (void)sqlite3_finalize(statement);
    return result == SQLITE_OK ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_CONFLICT;
}

static TrainlogStatus program_build_archive_response(const char *program_id,
                                                     const char *revision,
                                                     char **output_json,
                                                     size_t *output_size) {
    yyjson_mut_doc *response = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = response == NULL ? NULL : yyjson_mut_obj(response);
    if (response == NULL || root == NULL) {
        yyjson_mut_doc_free(response);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(response, root);
    (void)yyjson_mut_obj_add_uint(response, root, "api_version", 1U);
    (void)yyjson_mut_obj_add_strcpy(response, root, "program_id", program_id);
    (void)yyjson_mut_obj_add_strcpy(response, root, "state", "archived");
    (void)yyjson_mut_obj_add_strcpy(response, root, "revision_id", revision);
    return write_document(response, output_json, output_size);
}

static TrainlogStatus program_store_archive_replay(TrainlogDatabase *database,
                                                   const char *request_id,
                                                   const char *program_id,
                                                   const char *response_json,
                                                   const char *now) {
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database->connection,
                                    "INSERT INTO program_requests VALUES(?1,'archive',?2,?3,?4)",
                                    -1,
                                    &statement,
                                    NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, request_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, program_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, response_json, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, now, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    }
    (void)sqlite3_finalize(statement);
    return result == SQLITE_OK ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_web_programs_archive_json(TrainlogDatabase *database,
                                                  const char *program_id,
                                                  const char *expected_revision,
                                                  const char *request_id,
                                                  char **output_json,
                                                  size_t *output_size) {
    char revision[80];
    char now[32];
    TrainlogStatus status;
    bool replayed = false;
    if (database == NULL || program_id == NULL || expected_revision == NULL || request_id == NULL ||
        request_id[0] == '\0' || output_json == NULL || output_size == NULL ||
        !timestamp_now(now) ||
        trainlog_id_generate("pgr", revision, sizeof(revision)) != TRAINLOG_STATUS_OK) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = program_replay_archive(database, request_id, output_json, output_size, &replayed);
    if (status == TRAINLOG_STATUS_OK && replayed) {
        (void)sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL);
        return TRAINLOG_STATUS_OK;
    }
    if (status != TRAINLOG_STATUS_OK) {
        (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        return status;
    }
    status = program_apply_archive(database, program_id, expected_revision, revision, now);
    if (status == TRAINLOG_STATUS_OK) {
        status = program_build_archive_response(program_id, revision, output_json, output_size);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = program_store_archive_replay(database, request_id, program_id, *output_json, now);
    }
    if (status != TRAINLOG_STATUS_OK ||
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        free(*output_json);
        *output_json = NULL;
        *output_size = 0U;
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
    }
    return TRAINLOG_STATUS_OK;
}

static yyjson_val *program_find_session(yyjson_val *root, const char *program_session_id) {
    yyjson_val *sessions = yyjson_obj_get(root, "sessions");
    size_t index;

    if (!yyjson_is_arr(sessions)) {
        return NULL;
    }
    for (index = 0U; index < yyjson_arr_size(sessions); ++index) {
        yyjson_val *candidate = yyjson_arr_get(sessions, index);
        if (yyjson_equals_str(yyjson_obj_get(candidate, "program_session_id"),
                              program_session_id)) {
            return candidate;
        }
    }
    return NULL;
}

static yyjson_mut_val *program_copy_preparation_occurrences(yyjson_mut_doc *document,
                                                            yyjson_val *session) {
    static const char *const KEYS[] = {"exercise_id",
                                       "equipment_id",
                                       "load_mode",
                                       "rest_seconds",
                                       "target_sets",
                                       "target_reps",
                                       "target_duration_seconds",
                                       "target_weight_kg",
                                       "notes"};
    yyjson_val *source = yyjson_obj_get(session, "occurrences");
    yyjson_mut_val *destination = yyjson_mut_arr(document);
    size_t occurrence_index;

    if (!yyjson_is_arr(source) || destination == NULL) {
        return NULL;
    }
    for (occurrence_index = 0U; occurrence_index < yyjson_arr_size(source); ++occurrence_index) {
        yyjson_val *source_occurrence = yyjson_arr_get(source, occurrence_index);
        yyjson_mut_val *destination_occurrence = yyjson_mut_obj(document);
        size_t key_index;

        if (destination_occurrence == NULL) {
            return NULL;
        }
        for (key_index = 0U; key_index < sizeof(KEYS) / sizeof(KEYS[0]); ++key_index) {
            yyjson_mut_val *value =
                yyjson_val_mut_copy(document, yyjson_obj_get(source_occurrence, KEYS[key_index]));
            if (value == NULL ||
                !yyjson_mut_obj_add_val(document, destination_occurrence, KEYS[key_index], value)) {
                return NULL;
            }
        }
        if (!yyjson_mut_arr_add_val(destination, destination_occurrence)) {
            return NULL;
        }
    }
    return destination;
}

static char *program_build_preparation_body(yyjson_val *session,
                                            const char *program_id,
                                            const char *program_session_id,
                                            size_t *body_size) {
    yyjson_mut_doc *command = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = command == NULL ? NULL : yyjson_mut_obj(command);
    yyjson_mut_val *occurrences =
        command == NULL ? NULL : program_copy_preparation_occurrences(command, session);
    char *body;

    if (command == NULL || root == NULL || occurrences == NULL) {
        yyjson_mut_doc_free(command);
        return NULL;
    }
    yyjson_mut_doc_set_root(command, root);
    (void)yyjson_mut_obj_add_strcpy(
        command, root, "title", yyjson_get_str(yyjson_obj_get(session, "title")));
    (void)yyjson_mut_obj_add_strcpy(
        command, root, "session_type", yyjson_get_str(yyjson_obj_get(session, "session_type")));
    (void)yyjson_mut_obj_add_val(
        command,
        root,
        "planned_for",
        yyjson_val_mut_copy(command, yyjson_obj_get(session, "planned_for")));
    (void)yyjson_mut_obj_add_val(
        command, root, "notes", yyjson_val_mut_copy(command, yyjson_obj_get(session, "note")));
    (void)yyjson_mut_obj_add_strcpy(command, root, "editing_state", "draft");
    (void)yyjson_mut_obj_add_val(command, root, "occurrences", occurrences);
    (void)yyjson_mut_obj_add_strcpy(command, root, "source_program_id", program_id);
    (void)yyjson_mut_obj_add_strcpy(command, root, "source_program_session_id", program_session_id);
    body = yyjson_mut_write(command, YYJSON_WRITE_NOFLAG, body_size);
    yyjson_mut_doc_free(command);
    return body;
}

TrainlogStatus trainlog_web_programs_prepare_json(TrainlogDatabase *database,
                                                  const char *program_id,
                                                  const char *program_session_id,
                                                  const char *request_id,
                                                  char **output_json,
                                                  size_t *output_size) {
    char *detail = NULL;
    size_t detail_size = 0U;
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *session;
    char *body = NULL;
    size_t body_size = 0U;
    TrainlogStatus status;

    status = trainlog_web_programs_detail_json(database, program_id, &detail, &detail_size);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    document = yyjson_read(detail, detail_size, YYJSON_READ_NOFLAG);
    free(detail);
    root = document == NULL ? NULL : yyjson_doc_get_root(document);
    session = root == NULL ? NULL : program_find_session(root, program_session_id);
    if (session == NULL) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    body = program_build_preparation_body(session, program_id, program_session_id, &body_size);
    yyjson_doc_free(document);
    if (body == NULL) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    status = trainlog_web_sessions_save_json(
        database, NULL, "", request_id, body, body_size, output_json, output_size);
    free(body);
    return status;
}
