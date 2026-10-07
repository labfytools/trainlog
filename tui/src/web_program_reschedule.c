#include "trainlog/web_programs.h"

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

typedef struct PlanningRow {
    const char *id;
    const char *original;
    const char *current;
    const char *execution;
    const char *planning;
    const char *assigned;
    bool cede;
} PlanningRow;

static TrainlogStatus
planning_error(const char *reason, TrainlogStatus status, char **output, size_t *size) {
    size_t length = strlen(reason) + 16U;
    *output = malloc(length);
    if (*output == NULL) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    *size = (size_t)snprintf(*output, length, "{\"error\":\"%s\"}", reason);
    return status;
}

static bool planning_digest(
    const char *first, size_t first_size, const char *second, size_t second_size, char output[65]) {
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    unsigned char digest[32];
    unsigned int length = 0U;
    bool okay = context != NULL && EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1 &&
                EVP_DigestUpdate(context, first, first_size) == 1 &&
                EVP_DigestUpdate(context, second, second_size) == 1 &&
                EVP_DigestFinal_ex(context, digest, &length) == 1 && length == 32U;
    EVP_MD_CTX_free(context);
    if (!okay) {
        return false;
    }
    for (size_t index = 0U; index < 32U; ++index) {
        (void)snprintf(output + index * 2U, 3U, "%02x", digest[index]);
    }
    output[64] = '\0';
    return true;
}

static bool valid_civil_date(const char *value) {
    unsigned year, month, day;
    int days;
    if (value == NULL || strlen(value) != 10U || value[4] != '-' || value[7] != '-' ||
        sscanf(value, "%4u-%2u-%2u", &year, &month, &day) != 3 || year < 1900U || year > 9999U ||
        month < 1U || month > 12U) {
        return false;
    }
    for (size_t index = 0U; index < 10U; ++index) {
        if (index != 4U && index != 7U && (value[index] < '0' || value[index] > '9')) {
            return false;
        }
    }
    static const int MONTH_DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    days = MONTH_DAYS[month - 1U];
    if (month == 2U && (year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U))) {
        days = 29;
    }
    return day >= 1U && day <= (unsigned)days;
}

static bool
add_nullable(yyjson_mut_doc *document, yyjson_mut_val *object, const char *key, const char *value) {
    return value == NULL ? yyjson_mut_obj_add_null(document, object, key)
                         : yyjson_mut_obj_add_strcpy(document, object, key, value);
}

static bool request_valid(yyjson_val *root, bool commit) {
    yyjson_obj_iter iterator;
    yyjson_val *key;
    size_t count = 0U;
    if (!yyjson_is_obj(root) || yyjson_obj_size(root) != (commit ? 6U : 5U)) {
        return false;
    }
    iterator = yyjson_obj_iter_with(root);
    while ((key = yyjson_obj_iter_next(&iterator)) != NULL) {
        const char *name = yyjson_get_str(key);
        if (strcmp(name, "start_session_id") != 0 && strcmp(name, "through_session_id") != 0 &&
            strcmp(name, "start_date") != 0 && strcmp(name, "ceded_session_ids") != 0 &&
            strcmp(name, "expected_revision") != 0 &&
            !(commit && strcmp(name, "preview_sha256") == 0)) {
            return false;
        }
        ++count;
    }
    return count == (commit ? 6U : 5U) && yyjson_is_str(yyjson_obj_get(root, "start_session_id")) &&
           yyjson_is_str(yyjson_obj_get(root, "through_session_id")) &&
           yyjson_is_str(yyjson_obj_get(root, "expected_revision")) &&
           yyjson_get_len(yyjson_obj_get(root, "start_session_id")) <= 128U &&
           yyjson_get_len(yyjson_obj_get(root, "through_session_id")) <= 128U &&
           yyjson_get_len(yyjson_obj_get(root, "expected_revision")) <= 128U &&
           valid_civil_date(yyjson_get_str(yyjson_obj_get(root, "start_date"))) &&
           yyjson_is_arr(yyjson_obj_get(root, "ceded_session_ids")) &&
           yyjson_arr_size(yyjson_obj_get(root, "ceded_session_ids")) <= 64U &&
           (!commit || (yyjson_is_str(yyjson_obj_get(root, "preview_sha256")) &&
                        yyjson_get_len(yyjson_obj_get(root, "preview_sha256")) == 64U));
}

static bool has_preparation(TrainlogDatabase *database, const char *session_id) {
    sqlite3_stmt *statement = NULL;
    bool present = true;
    if (sqlite3_prepare_v2(database->connection,
                           "SELECT 1 FROM session_preparations WHERE "
                           "source_program_session_id=? AND withdrawn_at IS NULL LIMIT 1",
                           -1,
                           &statement,
                           NULL) == SQLITE_OK &&
        sqlite3_bind_text(statement, 1, session_id, -1, SQLITE_TRANSIENT) == SQLITE_OK) {
        present = sqlite3_step(statement) != SQLITE_DONE;
    }
    (void)sqlite3_finalize(statement);
    return present;
}

static TrainlogStatus peer_planning_fresh(TrainlogDatabase *database) {
    static const char SQL[] =
        "SELECT EXISTS(SELECT 1 FROM "
        "(SELECT DISTINCT consumer_peer_id FROM sync_generations "
        "WHERE producer_kind='desktop') peer WHERE "
        "NOT EXISTS(SELECT 1 FROM sync_generations generation "
        "JOIN sync_generation_artifacts artifact "
        "ON artifact.generation_id=generation.generation_id "
        "WHERE generation.generation_id=(SELECT latest.generation_id "
        "FROM sync_generations latest WHERE latest.consumer_peer_id=peer.consumer_peer_id "
        "AND latest.producer_kind='desktop' "
        "ORDER BY julianday(latest.generated_at) DESC,latest.generation_id DESC LIMIT 1) "
        "AND generation.status='acknowledged' AND artifact.logical_name='programs-v2' "
        "AND julianday(generation.acknowledged_at)>=julianday('now','-5 minutes')))";
    sqlite3_stmt *statement = NULL;
    TrainlogStatus status = TRAINLOG_STATUS_DATABASE_ERROR;
    if (sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        status =
            sqlite3_column_int(statement, 0) == 0 ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_CONFLICT;
    }
    (void)sqlite3_finalize(statement);
    return status;
}

static TrainlogStatus build_preview(TrainlogDatabase *database,
                                    const char *program_id,
                                    yyjson_val *request,
                                    char **output,
                                    size_t *output_size) {
    char *detail = NULL;
    size_t detail_size = 0U;
    yyjson_doc *source = NULL;
    yyjson_mut_doc *response = NULL;
    yyjson_val *program;
    yyjson_val *sessions;
    yyjson_val *ceded;
    yyjson_mut_val *root;
    yyjson_mut_val *moves;
    PlanningRow rows[64] = {0};
    const char *slots[64] = {0};
    size_t first = 64U;
    size_t last = 64U;
    size_t count;
    size_t slot_count = 0U;
    size_t next_slot = 0U;
    char fingerprint[65];
    char *choices = NULL;
    size_t choices_size = 0U;
    TrainlogStatus status =
        trainlog_web_programs_detail_json(database, program_id, &detail, &detail_size);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    source = yyjson_read(detail, detail_size, YYJSON_READ_NOFLAG);
    program = source == NULL ? NULL : yyjson_doc_get_root(source);
    sessions = program == NULL ? NULL : yyjson_obj_get(program, "sessions");
    ceded = yyjson_obj_get(request, "ceded_session_ids");
    count = yyjson_arr_size(sessions);
    if (program == NULL || count > 64U ||
        !yyjson_equals_str(yyjson_obj_get(program, "state"), "active") ||
        !yyjson_equals_str(yyjson_obj_get(program, "revision_id"),
                           yyjson_get_str(yyjson_obj_get(request, "expected_revision")))) {
        status = planning_error(
            "stale_or_ineligible_program", TRAINLOG_STATUS_CONFLICT, output, output_size);
        goto done;
    }
    for (size_t index = 0U; index < count; ++index) {
        yyjson_val *session = yyjson_arr_get(sessions, index);
        rows[index].id = yyjson_get_str(yyjson_obj_get(session, "program_session_id"));
        rows[index].original = yyjson_get_str(yyjson_obj_get(session, "planned_for"));
        rows[index].current = yyjson_get_str(yyjson_obj_get(session, "current_for"));
        rows[index].execution = yyjson_get_str(yyjson_obj_get(session, "execution_state"));
        rows[index].planning = yyjson_get_str(yyjson_obj_get(session, "planning_state"));
        if (strcmp(rows[index].id, yyjson_get_str(yyjson_obj_get(request, "start_session_id"))) ==
            0) {
            first = index;
        }
        if (strcmp(rows[index].id, yyjson_get_str(yyjson_obj_get(request, "through_session_id"))) ==
            0) {
            last = index;
        }
    }
    if (first >= count || last >= count || first > last ||
        (first != last && (!yyjson_is_str(yyjson_obj_get(program, "end_date")) ||
                           strcmp(yyjson_get_str(yyjson_obj_get(request, "start_date")),
                                  yyjson_get_str(yyjson_obj_get(program, "end_date"))) > 0))) {
        status = planning_error(
            "invalid_selection", TRAINLOG_STATUS_INVALID_ARGUMENT, output, output_size);
        goto done;
    }
    for (size_t index = 0U; index < yyjson_arr_size(ceded); ++index) {
        yyjson_val *id = yyjson_arr_get(ceded, index);
        bool matched = false;
        if (!yyjson_is_str(id) || yyjson_get_len(id) > 128U) {
            status = planning_error(
                "invalid_ceded_identity", TRAINLOG_STATUS_INVALID_ARGUMENT, output, output_size);
            goto done;
        }
        for (size_t candidate = first; candidate <= last; ++candidate) {
            if (strcmp(rows[candidate].id, yyjson_get_str(id)) == 0 && !rows[candidate].cede) {
                rows[candidate].cede = true;
                matched = true;
                break;
            }
        }
        if (!matched) {
            status = planning_error(
                "invalid_ceded_identity", TRAINLOG_STATUS_INVALID_ARGUMENT, output, output_size);
            goto done;
        }
    }
    /* WHY: calendar moves target one identity, including a formerly ceded
     * identity. CONTRACT: its requested civil day is the new current date,
     * even when another session already occupies that day. INVARIANT: no
     * other planning row is included in the resulting mutation. */
    if (first == last && yyjson_arr_size(ceded) == 0U) {
        PlanningRow *row = &rows[first];
        if (strcmp(row->execution, "todo") != 0 || has_preparation(database, row->id)) {
            status = planning_error(
                "prepared_or_executed_session", TRAINLOG_STATUS_CONFLICT, output, output_size);
            goto done;
        }
        row->assigned = yyjson_get_str(yyjson_obj_get(request, "start_date"));
        goto assigned;
    }
    for (size_t index = first; index <= last; ++index) {
        if (strcmp(rows[index].execution, "todo") != 0 ||
            has_preparation(database, rows[index].id) || rows[index].original == NULL ||
            !valid_civil_date(rows[index].original)) {
            status = planning_error(
                "prepared_or_executed_session", TRAINLOG_STATUS_CONFLICT, output, output_size);
            goto done;
        }
        if (strcmp(rows[index].planning, "ceded") == 0 && !rows[index].cede) {
            status = planning_error(
                "ceded_session_requires_selection", TRAINLOG_STATUS_CONFLICT, output, output_size);
            goto done;
        }
        // WHY: a second explicit decision starts from the durable current
        // schedule. A ceded identity has no current date, so its original slot
        // is available only when the user selects that identity again.
        const char *slot = rows[index].current == NULL ? rows[index].original : rows[index].current;
        if (strcmp(slot, yyjson_get_str(yyjson_obj_get(request, "start_date"))) >= 0) {
            slots[slot_count++] = slot;
        }
    }
    // Selected Program slots define permitted days; no rest day is synthesized.
    // Position order may differ from dates, so sort candidate dates explicitly.
    for (size_t outer = 0U; outer < slot_count; ++outer) {
        for (size_t inner = outer + 1U; inner < slot_count; ++inner) {
            if (strcmp(slots[inner], slots[outer]) < 0) {
                const char *swap = slots[outer];
                slots[outer] = slots[inner];
                slots[inner] = swap;
            }
        }
    }
    // A previously ceded original day may now be occupied by an active
    // session. It remains one calendar slot for this operation.
    size_t unique_slots = 0U;
    for (size_t index = 0U; index < slot_count; ++index) {
        if (unique_slots == 0U || strcmp(slots[index], slots[unique_slots - 1U]) != 0) {
            slots[unique_slots++] = slots[index];
        }
    }
    slot_count = unique_slots;
    for (size_t index = first; index <= last; ++index) {
        if (rows[index].cede) {
            continue;
        }
        if (next_slot >= slot_count ||
            strcmp(slots[next_slot], yyjson_get_str(yyjson_obj_get(program, "end_date"))) > 0) {
            status = planning_error("insufficient_or_duplicate_slots",
                                    TRAINLOG_STATUS_INVALID_ARGUMENT,
                                    output,
                                    output_size);
            goto done;
        }
        rows[index].assigned = slots[next_slot++];
    }
assigned:
    // CONTRACT: the choice digest includes stable identities in their supplied
    // order. The fact digest includes the complete current Program projection.
    {
        yyjson_mut_doc *choice_document = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *choice_root =
            choice_document == NULL ? NULL : yyjson_mut_obj(choice_document);
        if (choice_document == NULL || choice_root == NULL) {
            yyjson_mut_doc_free(choice_document);
            status = TRAINLOG_STATUS_SYSTEM_ERROR;
            goto done;
        }
        yyjson_mut_doc_set_root(choice_document, choice_root);
        (void)yyjson_mut_obj_add_strcpy(
            choice_document,
            choice_root,
            "start_session_id",
            yyjson_get_str(yyjson_obj_get(request, "start_session_id")));
        (void)yyjson_mut_obj_add_strcpy(
            choice_document,
            choice_root,
            "through_session_id",
            yyjson_get_str(yyjson_obj_get(request, "through_session_id")));
        (void)yyjson_mut_obj_add_strcpy(choice_document,
                                        choice_root,
                                        "start_date",
                                        yyjson_get_str(yyjson_obj_get(request, "start_date")));
        yyjson_mut_val *ids = yyjson_mut_arr(choice_document);
        for (size_t index = 0U; index < yyjson_arr_size(ceded); ++index) {
            (void)yyjson_mut_arr_add_strcpy(
                choice_document, ids, yyjson_get_str(yyjson_arr_get(ceded, index)));
        }
        (void)yyjson_mut_obj_add_val(choice_document, choice_root, "ceded_session_ids", ids);
        choices = yyjson_mut_write(choice_document, YYJSON_WRITE_NOFLAG, &choices_size);
        yyjson_mut_doc_free(choice_document);
    }
    if (choices == NULL ||
        !planning_digest(detail, detail_size, choices, choices_size, fingerprint)) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto done;
    }
    response = yyjson_mut_doc_new(NULL);
    root = response == NULL ? NULL : yyjson_mut_obj(response);
    moves = response == NULL ? NULL : yyjson_mut_arr(response);
    if (response == NULL || root == NULL || moves == NULL) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto done;
    }
    yyjson_mut_doc_set_root(response, root);
    (void)yyjson_mut_obj_add_uint(response, root, "api_version", 1U);
    (void)yyjson_mut_obj_add_strcpy(response, root, "program_id", program_id);
    (void)yyjson_mut_obj_add_strcpy(
        response, root, "revision_id", yyjson_get_str(yyjson_obj_get(program, "revision_id")));
    (void)yyjson_mut_obj_add_strcpy(response, root, "preview_sha256", fingerprint);
    (void)yyjson_mut_obj_add_val(response, root, "moves", moves);
    for (size_t index = first; index <= last; ++index) {
        yyjson_mut_val *move = yyjson_mut_obj(response);
        (void)yyjson_mut_obj_add_strcpy(response, move, "program_session_id", rows[index].id);
        (void)add_nullable(response, move, "original_for", rows[index].original);
        (void)add_nullable(response, move, "old_for", rows[index].current);
        (void)add_nullable(response, move, "new_for", rows[index].assigned);
        const char *change =
            rows[index].cede ? "ceded"
            : rows[index].current != NULL && strcmp(rows[index].current, rows[index].assigned) == 0
                ? "unchanged"
                : "rescheduled";
        (void)yyjson_mut_obj_add_strcpy(response, move, "change", change);
        (void)yyjson_mut_arr_add_val(moves, move);
    }
    *output = yyjson_mut_write(response, YYJSON_WRITE_NOFLAG, output_size);
    status = *output == NULL ? TRAINLOG_STATUS_SYSTEM_ERROR : TRAINLOG_STATUS_OK;
done:
    free(choices);
    yyjson_mut_doc_free(response);
    yyjson_doc_free(source);
    free(detail);
    return status;
}

static TrainlogStatus apply_preview(TrainlogDatabase *database,
                                    const char *operation_id,
                                    const char *program_id,
                                    const char *expected_revision,
                                    const char *request_digest,
                                    const char *preview,
                                    char **output,
                                    size_t *output_size) {
    yyjson_doc *document = yyjson_read(preview, strlen(preview), YYJSON_READ_NOFLAG);
    yyjson_val *root = document == NULL ? NULL : yyjson_doc_get_root(document);
    yyjson_val *moves = root == NULL ? NULL : yyjson_obj_get(root, "moves");
    sqlite3_stmt *statement = NULL;
    char revision[80];
    char now[32];
    time_t clock_now = time(NULL);
    struct tm utc;
    TrainlogStatus status = TRAINLOG_STATUS_DATABASE_ERROR;
    if (moves == NULL || clock_now == (time_t)-1 || gmtime_r(&clock_now, &utc) == NULL ||
        strftime(now, sizeof(now), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0U ||
        trainlog_id_generate("pgr", revision, sizeof(revision)) != TRAINLOG_STATUS_OK) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        goto done;
    }
    if (sqlite3_prepare_v2(database->connection,
                           "UPDATE programs SET revision_id=?,updated_at=? WHERE program_id=? "
                           "AND revision_id=? AND deleted_at IS NULL AND state='active'",
                           -1,
                           &statement,
                           NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, program_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, expected_revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(database->connection) != 1) {
        status = TRAINLOG_STATUS_CONFLICT;
        goto done;
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    if (sqlite3_prepare_v2(database->connection,
                           "INSERT INTO program_session_planning "
                           "(program_session_id,current_for,state,operation_id,updated_at) "
                           "VALUES(?,?,?,?,?) ON CONFLICT(program_session_id) DO UPDATE SET "
                           "current_for=excluded.current_for,state=excluded.state,"
                           "operation_id=excluded.operation_id,updated_at=excluded.updated_at",
                           -1,
                           &statement,
                           NULL) != SQLITE_OK) {
        goto done;
    }
    for (size_t index = 0U; index < yyjson_arr_size(moves); ++index) {
        yyjson_val *move = yyjson_arr_get(moves, index);
        yyjson_val *new_for = yyjson_obj_get(move, "new_for");
        if (yyjson_equals_str(yyjson_obj_get(move, "change"), "unchanged")) {
            continue;
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
        if (sqlite3_bind_text(statement,
                              1,
                              yyjson_get_str(yyjson_obj_get(move, "program_session_id")),
                              -1,
                              SQLITE_TRANSIENT) != SQLITE_OK ||
            (yyjson_is_null(new_for)
                 ? sqlite3_bind_null(statement, 2)
                 : sqlite3_bind_text(
                       statement, 2, yyjson_get_str(new_for), -1, SQLITE_TRANSIENT)) != SQLITE_OK ||
            sqlite3_bind_text(statement,
                              3,
                              yyjson_equals_str(yyjson_obj_get(move, "change"), "ceded") ? "ceded"
                                                                                         : "active",
                              -1,
                              SQLITE_STATIC) != SQLITE_OK ||
            sqlite3_bind_text(statement, 4, operation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 5, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE) {
            goto done;
        }
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    {
        size_t length = strlen(preview) + strlen(revision) + 52U;
        *output = malloc(length);
        if (*output == NULL) {
            status = TRAINLOG_STATUS_SYSTEM_ERROR;
            goto done;
        }
        *output_size = (size_t)snprintf(*output,
                                        length,
                                        "%.*s,\"applied\":true,\"new_revision_id\":\"%s\"}",
                                        (int)strlen(preview) - 1,
                                        preview,
                                        revision);
    }
    if (sqlite3_prepare_v2(database->connection,
                           "INSERT INTO program_reschedule_operations VALUES(?,?,?,?,?,?)",
                           -1,
                           &statement,
                           NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, operation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, program_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, request_digest, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 4, expected_revision, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 5, *output, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 6, now, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) {
        goto done;
    }
    status = TRAINLOG_STATUS_OK;
done:
    (void)sqlite3_finalize(statement);
    yyjson_doc_free(document);
    return status;
}

TrainlogStatus trainlog_web_programs_reschedule_json(TrainlogDatabase *database,
                                                     const char *program_id,
                                                     const char *operation_id,
                                                     const char *body,
                                                     size_t body_size,
                                                     bool commit,
                                                     char **output_json,
                                                     size_t *output_size) {
    yyjson_doc *request_doc = NULL;
    yyjson_val *request;
    sqlite3_stmt *statement = NULL;
    char request_digest[65];
    char *preview = NULL;
    size_t preview_size = 0U;
    TrainlogStatus status;
    if (database == NULL || program_id == NULL || body == NULL || output_json == NULL ||
        output_size == NULL || (commit && (operation_id == NULL || operation_id[0] == '\0')) ||
        body_size > 16384U || (operation_id != NULL && strlen(operation_id) > 128U)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    request_doc = yyjson_read(body, body_size, YYJSON_READ_NOFLAG);
    request = request_doc == NULL ? NULL : yyjson_doc_get_root(request_doc);
    if (!request_valid(request, commit) ||
        !planning_digest(body, body_size, "", 0U, request_digest)) {
        yyjson_doc_free(request_doc);
        return planning_error("invalid_reschedule_request",
                              TRAINLOG_STATUS_INVALID_ARGUMENT,
                              output_json,
                              output_size);
    }
    if (sqlite3_exec(
            database->connection, commit ? "BEGIN IMMEDIATE" : "BEGIN", NULL, NULL, NULL) !=
        SQLITE_OK) {
        yyjson_doc_free(request_doc);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (commit) {
        if (sqlite3_prepare_v2(database->connection,
                               "SELECT program_id,request_sha256,response_json FROM "
                               "program_reschedule_operations WHERE operation_id=?",
                               -1,
                               &statement,
                               NULL) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, operation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            status = TRAINLOG_STATUS_DATABASE_ERROR;
            goto finish;
        }
        if (sqlite3_step(statement) == SQLITE_ROW) {
            const char *known_program = (const char *)sqlite3_column_text(statement, 0);
            const char *known_digest = (const char *)sqlite3_column_text(statement, 1);
            const char *saved = (const char *)sqlite3_column_text(statement, 2);
            if (known_program != NULL && known_digest != NULL && saved != NULL &&
                strcmp(known_program, program_id) == 0 &&
                strcmp(known_digest, request_digest) == 0) {
                *output_json = strdup(saved);
                *output_size = *output_json == NULL ? 0U : strlen(*output_json);
                status = *output_json == NULL ? TRAINLOG_STATUS_SYSTEM_ERROR : TRAINLOG_STATUS_OK;
            } else {
                status = planning_error("operation_identity_conflict",
                                        TRAINLOG_STATUS_CONFLICT,
                                        output_json,
                                        output_size);
            }
            goto finish;
        }
        (void)sqlite3_finalize(statement);
        statement = NULL;
        if (sqlite3_prepare_v2(database->connection,
                               "SELECT 1 FROM program_requests WHERE request_id=? "
                               "UNION ALL SELECT 1 FROM program_deletions WHERE request_id=?",
                               -1,
                               &statement,
                               NULL) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, operation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 2, operation_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            status = TRAINLOG_STATUS_DATABASE_ERROR;
            goto finish;
        }
        int cross_command = sqlite3_step(statement);
        (void)sqlite3_finalize(statement);
        statement = NULL;
        if (cross_command != SQLITE_DONE) {
            status = cross_command == SQLITE_ROW ? planning_error("operation_identity_conflict",
                                                                  TRAINLOG_STATUS_CONFLICT,
                                                                  output_json,
                                                                  output_size)
                                                 : TRAINLOG_STATUS_DATABASE_ERROR;
            goto finish;
        }
        // WHY: the desktop's last imported draft snapshot cannot prove that
        // Android stayed idle while offline. A recent acknowledged V2 Program
        // generation is required; Android independently rejects a changed
        // schedule if a Program draft starts after that acknowledgement.
        status = peer_planning_fresh(database);
        if (status != TRAINLOG_STATUS_OK) {
            if (status == TRAINLOG_STATUS_CONFLICT) {
                status = planning_error("program_peer_not_fresh", status, output_json, output_size);
            }
            goto finish;
        }
    }
    status = build_preview(database, program_id, request, &preview, &preview_size);
    if (status != TRAINLOG_STATUS_OK) {
        if (preview != NULL) {
            *output_json = preview;
            *output_size = preview_size;
            preview = NULL;
        }
        goto finish;
    }
    if (!commit) {
        *output_json = preview;
        *output_size = preview_size;
        preview = NULL;
        goto finish;
    }
    {
        yyjson_doc *preview_doc = yyjson_read(preview, preview_size, YYJSON_READ_NOFLAG);
        yyjson_val *preview_root = preview_doc == NULL ? NULL : yyjson_doc_get_root(preview_doc);
        const char *fingerprint = yyjson_get_str(yyjson_obj_get(preview_root, "preview_sha256"));
        if (fingerprint == NULL ||
            strcmp(fingerprint, yyjson_get_str(yyjson_obj_get(request, "preview_sha256"))) != 0) {
            status = planning_error(
                "stale_reschedule_preview", TRAINLOG_STATUS_CONFLICT, output_json, output_size);
        } else {
            status = apply_preview(database,
                                   operation_id,
                                   program_id,
                                   yyjson_get_str(yyjson_obj_get(request, "expected_revision")),
                                   request_digest,
                                   preview,
                                   output_json,
                                   output_size);
        }
        yyjson_doc_free(preview_doc);
    }
finish:
    (void)sqlite3_finalize(statement);
    free(preview);
    yyjson_doc_free(request_doc);
    if (status == TRAINLOG_STATUS_OK && commit) {
        if (sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) {
            return status;
        }
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
    if (status != TRAINLOG_STATUS_OK && *output_json == NULL) {
        return planning_error("reschedule_unavailable", status, output_json, output_size);
    }
    return status;
}
