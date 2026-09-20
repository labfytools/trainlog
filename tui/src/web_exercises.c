#include "trainlog/web_exercises.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/evp.h>
#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"
#include "trainlog/body_zone_catalog.h"
#include "trainlog/catalog.h"
#include "trainlog/exercise_name_catalog.h"
#include "trainlog/id.h"

#define EXERCISE_REVISION_CAPACITY 72U
#define CAUSAL_OPERATION_MAX 4096
#define CAUSAL_TEXT_BUDGET (3 * 1024 * 1024)

typedef struct ExerciseInput {
    char name[TRAINLOG_NAME_MAX + 1U];
    TrainlogRecordingMode recording_mode;
    TrainlogTrackingMode tracking_mode;
    TrainlogExerciseDataFields data_fields;
    char primary_zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
    char secondary_zone_ids[16][TRAINLOG_ZONE_ID_MAX + 1U];
    const char *secondary_zone_pointers[16];
    size_t secondary_count;
} ExerciseInput;

static const char *recording_text(TrainlogRecordingMode mode) {
    return mode == TRAINLOG_RECORDING_CONTINUOUS ? "continuous" : "sets";
}

static const char *tracking_text(TrainlogTrackingMode mode) {
    return mode == TRAINLOG_TRACKING_DURATION ? "duration" : "reps";
}

static bool sha256_hex(const char *input, size_t input_size, char output[65]) {
    unsigned char digest[32];
    unsigned int digest_size = 0U;
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    size_t index;
    bool success;

    if (context == NULL) {
        return false;
    }
    success = EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1 &&
              EVP_DigestUpdate(context, input, input_size) == 1 &&
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

static bool timestamp_now(char output[TRAINLOG_TIMESTAMP_MAX + 1U]) {
    time_t now = time(NULL);
    struct tm utc;

    return now != (time_t)-1 && gmtime_r(&now, &utc) != NULL &&
           strftime(output, TRAINLOG_TIMESTAMP_MAX + 1U, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0U;
}

static bool json_finish(yyjson_mut_doc **document, char **output_json, size_t *output_size) {
    *output_json = yyjson_mut_write(*document, 0U, output_size);
    yyjson_mut_doc_free(*document);
    *document = NULL;
    return *output_json != NULL;
}

static bool exercise_is_builtin(const char *exercise_id) {
    return trainlog_exercise_name_catalog_lookup(exercise_id) != NULL;
}

/* WHY: profile_state alone omits mutable name and BODY ZONES. This digest is
 * the optimistic token for exactly the complete Web-editable state. CONTRACT:
 * byte ordering is fixed and zones are queried in role/ID order. INVARIANT:
 * no timestamp or SQLite row ID participates. */
static TrainlogStatus exercise_revision(TrainlogDatabase *database,
                                        const char *exercise_id,
                                        char output[EXERCISE_REVISION_CAPACITY]) {
    sqlite3_stmt *statement = NULL;
    EVP_MD_CTX *digest = NULL;
    unsigned char raw[32];
    unsigned int raw_size = 0U;
    int result;
    int column;
    size_t index;

    result = sqlite3_prepare_v2(
        database->connection,
        "SELECT e.exercise_id,e.name,e.recording_mode,e.tracking_mode,e.data_fields,"
        "COALESCE((SELECT group_concat(zone_id||':'||role,'|') FROM (SELECT zone_id,role "
        "FROM exercise_body_zones WHERE exercise_row_id=e.id ORDER BY role,zone_id)),''),"
        "COALESCE((SELECT deleted FROM sync_causal_state WHERE target_kind='exercise' AND "
        "target_id=e.exercise_id),0) FROM exercises e WHERE e.exercise_id=?1",
        -1,
        &statement,
        NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        return result == SQLITE_DONE ? TRAINLOG_STATUS_NOT_FOUND : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    digest = EVP_MD_CTX_new();
    if (digest == NULL || EVP_DigestInit_ex(digest, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(digest);
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    for (column = 0; column < 7; ++column) {
        const void *value = sqlite3_column_text(statement, column);
        int bytes = sqlite3_column_bytes(statement, column);
        static const char separator = '\0';
        if (value == NULL || bytes < 0 || EVP_DigestUpdate(digest, value, (size_t)bytes) != 1 ||
            EVP_DigestUpdate(digest, &separator, 1U) != 1) {
            EVP_MD_CTX_free(digest);
            (void)sqlite3_finalize(statement);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    if (sqlite3_finalize(statement) != SQLITE_OK ||
        EVP_DigestFinal_ex(digest, raw, &raw_size) != 1 || raw_size != 32U) {
        EVP_MD_CTX_free(digest);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    EVP_MD_CTX_free(digest);
    (void)memcpy(output, "exrev_", 6U);
    for (index = 0U; index < 32U; ++index) {
        (void)snprintf(output + 6U + (index * 2U), 3U, "%02x", raw[index]);
    }
    output[70] = '\0';
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus append_zones(TrainlogDatabase *database,
                                   const char *exercise_id,
                                   yyjson_mut_doc *document,
                                   yyjson_mut_val *object) {
    TrainlogExerciseBodyZone relations[16];
    size_t count = 0U;
    size_t index;
    yyjson_mut_val *secondary = yyjson_mut_arr(document);
    const char *primary = NULL;
    TrainlogStatus status =
        trainlog_database_list_exercise_body_zones(database, exercise_id, relations, 16U, &count);

    if (status != TRAINLOG_STATUS_OK || count > 16U || secondary == NULL) {
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
    }
    for (index = 0U; index < count; ++index) {
        if (relations[index].role == TRAINLOG_BODY_ZONE_PRIMARY) {
            primary = relations[index].zone_id;
        } else if (!yyjson_mut_arr_add_strcpy(document, secondary, relations[index].zone_id)) {
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    }
    if (primary == NULL) {
        if (!yyjson_mut_obj_add_null(document, object, "primary_zone_id")) {
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    } else if (!yyjson_mut_obj_add_strcpy(document, object, "primary_zone_id", primary)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    return yyjson_mut_obj_add_val(document, object, "secondary_zone_ids", secondary)
               ? TRAINLOG_STATUS_OK
               : TRAINLOG_STATUS_SYSTEM_ERROR;
}

static TrainlogStatus append_equipment(TrainlogDatabase *database,
                                       const char *exercise_id,
                                       yyjson_mut_doc *document,
                                       yyjson_mut_val *object) {
    TrainlogResolvedEquipment equipment[64];
    size_t count = 0U;
    size_t index;
    yyjson_mut_val *items = yyjson_mut_arr(document);
    TrainlogStatus status =
        trainlog_database_list_exercise_equipment(database, exercise_id, equipment, 64U, &count);

    if (status != TRAINLOG_STATUS_OK || count > 64U || items == NULL) {
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
    }
    for (index = 0U; index < count; ++index) {
        yyjson_mut_val *item = yyjson_mut_obj(document);
        if (item == NULL ||
            !yyjson_mut_obj_add_strcpy(
                document, item, "equipment_id", equipment[index].equipment_id) ||
            !yyjson_mut_obj_add_strcpy(
                document, item, "display_name", equipment[index].display_name) ||
            !yyjson_mut_arr_add_val(items, item)) {
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    }
    return yyjson_mut_obj_add_val(document, object, "equipment", items)
               ? TRAINLOG_STATUS_OK
               : TRAINLOG_STATUS_SYSTEM_ERROR;
}

static TrainlogStatus append_exercise(TrainlogDatabase *database,
                                      const TrainlogExercise *exercise,
                                      bool detail,
                                      yyjson_mut_doc *document,
                                      yyjson_mut_val *object) {
    char revision[EXERCISE_REVISION_CAPACITY];
    TrainlogStatus status;

    if (!yyjson_mut_obj_add_strcpy(document, object, "exercise_id", exercise->exercise_id) ||
        !yyjson_mut_obj_add_strcpy(document, object, "name", exercise->name) ||
        !yyjson_mut_obj_add_strcpy(
            document, object, "recording_mode", recording_text(exercise->recording_mode)) ||
        !yyjson_mut_obj_add_strcpy(
            document, object, "tracking_mode", tracking_text(exercise->tracking_mode)) ||
        !yyjson_mut_obj_add_uint(document, object, "data_fields", exercise->data_fields) ||
        !yyjson_mut_obj_add_bool(
            document, object, "retireable", !exercise_is_builtin(exercise->exercise_id))) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    status = append_zones(database, exercise->exercise_id, document, object);
    if (status != TRAINLOG_STATUS_OK || !detail) {
        return status;
    }
    status = exercise_revision(database, exercise->exercise_id, revision);
    if (status != TRAINLOG_STATUS_OK ||
        !yyjson_mut_obj_add_strcpy(document, object, "revision", revision)) {
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_SYSTEM_ERROR : status;
    }
    return append_equipment(database, exercise->exercise_id, document, object);
}

TrainlogStatus trainlog_web_exercises_list_json(TrainlogDatabase *database,
                                                const TrainlogWebExercisesQuery *query,
                                                char **output_json,
                                                size_t *output_size) {
    static const char SQL[] =
        "SELECT e.exercise_id,e.name,e.tracking_mode,e.recording_mode,e.data_fields FROM "
        "exercises e WHERE NOT EXISTS(SELECT 1 FROM sync_causal_state c WHERE "
        "c.target_kind='exercise' AND c.target_id=e.exercise_id AND c.deleted=1) AND "
        "(?1='' OR e.normalized_name LIKE ?1||'%') AND (?2='' OR "
        "(e.recording_mode||'_'||e.tracking_mode)=?2) AND "
        "(?3='' OR EXISTS(SELECT 1 FROM exercise_body_zones z WHERE z.exercise_row_id=e.id AND "
        "z.zone_id=?3)) AND (?4=0 OR NOT EXISTS(SELECT 1 FROM exercise_body_zones z WHERE "
        "z.exercise_row_id=e.id)) ORDER BY e.name COLLATE NOCASE,e.exercise_id LIMIT ?5 OFFSET ?6";
    char normalized[(TRAINLOG_NAME_MAX * 4U) + 1U] = "";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_doc *document = NULL;
    yyjson_mut_val *root;
    yyjson_mut_val *items;
    size_t count = 0U;
    int result;
    TrainlogStatus status = TRAINLOG_STATUS_OK;

    if (database == NULL || query == NULL || output_json == NULL || output_size == NULL ||
        query->limit == 0U || query->limit > TRAINLOG_WEB_EXERCISES_PAGE_MAX ||
        query->offset > 1000000U || (query->zone_id != NULL && query->unclassified) ||
        (query->profile != NULL && query->profile[0] != '\0' &&
         strcmp(query->profile, "sets_reps") != 0 && strcmp(query->profile, "sets_duration") != 0 &&
         strcmp(query->profile, "continuous_duration") != 0)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    if (query->search != NULL && query->search[0] != '\0' &&
        trainlog_catalog_normalize_name(query->search, normalized, sizeof(normalized)) !=
            TRAINLOG_STATUS_OK) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    items = document == NULL ? NULL : yyjson_mut_arr(document);
    if (document == NULL || root == NULL || items == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    result = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, normalized, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(
            statement, 2, query->profile == NULL ? "" : query->profile, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(
            statement, 3, query->zone_id == NULL ? "" : query->zone_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 4, query->unclassified ? 1 : 0);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 5, (sqlite3_int64)query->limit + 1);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 6, (sqlite3_int64)query->offset);
    }
    if (result != SQLITE_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    while (status == TRAINLOG_STATUS_OK && (result = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count < query->limit) {
            TrainlogExercise exercise;
            yyjson_mut_val *item = yyjson_mut_obj(document);
            const char *tracking = (const char *)sqlite3_column_text(statement, 2);
            const char *recording = (const char *)sqlite3_column_text(statement, 3);
            (void)memset(&exercise, 0, sizeof(exercise));
            if (item == NULL || sqlite3_column_text(statement, 0) == NULL ||
                sqlite3_column_text(statement, 1) == NULL || tracking == NULL ||
                recording == NULL) {
                status = TRAINLOG_STATUS_DATABASE_ERROR;
                break;
            }
            (void)snprintf(exercise.exercise_id,
                           sizeof(exercise.exercise_id),
                           "%s",
                           sqlite3_column_text(statement, 0));
            (void)snprintf(
                exercise.name, sizeof(exercise.name), "%s", sqlite3_column_text(statement, 1));
            exercise.tracking_mode = strcmp(tracking, "duration") == 0 ? TRAINLOG_TRACKING_DURATION
                                                                       : TRAINLOG_TRACKING_REPS;
            exercise.recording_mode = strcmp(recording, "continuous") == 0
                                          ? TRAINLOG_RECORDING_CONTINUOUS
                                          : TRAINLOG_RECORDING_SETS;
            exercise.data_fields = (TrainlogExerciseDataFields)sqlite3_column_int64(statement, 4);
            status = append_exercise(database, &exercise, false, document, item);
            if (status != TRAINLOG_STATUS_OK || !yyjson_mut_arr_add_val(items, item)) {
                status = status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_SYSTEM_ERROR : status;
                break;
            }
        }
        ++count;
    }
    if (result != SQLITE_DONE && status == TRAINLOG_STATUS_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK &&
        status == TRAINLOG_STATUS_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (status == TRAINLOG_STATUS_OK &&
        (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
         !yyjson_mut_obj_add_uint(document, root, "offset", query->offset) ||
         !yyjson_mut_obj_add_bool(document, root, "more", count > query->limit) ||
         !yyjson_mut_obj_add_uint(document,
                                  root,
                                  "next_offset",
                                  query->offset + (count > query->limit ? query->limit : count)) ||
         !yyjson_mut_obj_add_val(document, root, "items", items) ||
         !json_finish(&document, output_json, output_size))) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
        document = NULL;
    }
    if (status != TRAINLOG_STATUS_OK) {
        yyjson_mut_doc_free(document);
    }
    return status;
}

TrainlogStatus trainlog_web_exercises_detail_json(TrainlogDatabase *database,
                                                  const char *exercise_id,
                                                  char **output_json,
                                                  size_t *output_size) {
    TrainlogExercise exercise;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    TrainlogStatus status;

    if (database == NULL || exercise_id == NULL || exercise_id[0] == '\0' || output_json == NULL ||
        output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    status = trainlog_database_get_exercise_profile(database, exercise_id, &exercise);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    if (document == NULL || root == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        (status = append_exercise(database, &exercise, true, document, root)) !=
            TRAINLOG_STATUS_OK ||
        !json_finish(&document, output_json, output_size)) {
        if (status == TRAINLOG_STATUS_OK) {
            status = TRAINLOG_STATUS_SYSTEM_ERROR;
        }
        yyjson_mut_doc_free(document);
        return status;
    }
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_exercises_zones_json(char **output_json, size_t *output_size) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *zones;
    size_t index;

    if (output_json == NULL || output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    zones = document == NULL ? NULL : yyjson_mut_arr(document);
    if (document == NULL || root == NULL || zones == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    for (index = 0U; index < trainlog_body_zone_catalog_count(); ++index) {
        const TrainlogBodyZone *zone = trainlog_body_zone_catalog_at(index);
        yyjson_mut_val *item;
        if (zone == NULL || zone->is_group) {
            continue;
        }
        item = yyjson_mut_obj(document);
        if (item == NULL || !yyjson_mut_obj_add_strcpy(document, item, "zone_id", zone->zone_id) ||
            !yyjson_mut_obj_add_strcpy(document, item, "name", zone->display_name) ||
            !yyjson_mut_arr_add_val(zones, item)) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    }
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_val(document, root, "zones", zones) ||
        !json_finish(&document, output_json, output_size)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

static bool input_has_exact_keys(yyjson_val *root) {
    static const char *const EXPECTED[] = {"name",
                                           "recording_mode",
                                           "tracking_mode",
                                           "data_fields",
                                           "primary_zone_id",
                                           "secondary_zone_ids"};
    bool seen[sizeof(EXPECTED) / sizeof(EXPECTED[0])] = {false};
    yyjson_obj_iter iterator;
    yyjson_val *key;

    if (!yyjson_is_obj(root) || yyjson_obj_size(root) != sizeof(EXPECTED) / sizeof(EXPECTED[0])) {
        return false;
    }
    yyjson_obj_iter_init(root, &iterator);
    while ((key = yyjson_obj_iter_next(&iterator)) != NULL) {
        const char *name = yyjson_get_str(key);
        size_t index;
        bool matched = false;

        for (index = 0U; index < sizeof(EXPECTED) / sizeof(EXPECTED[0]); ++index) {
            if (strcmp(name, EXPECTED[index]) == 0 && !seen[index]) {
                seen[index] = true;
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return true;
}

static TrainlogStatus
parse_input(const char *body, size_t body_size, bool require_sets_primary, ExerciseInput *output) {
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *name;
    yyjson_val *recording;
    yyjson_val *tracking;
    yyjson_val *fields;
    yyjson_val *primary;
    yyjson_val *secondary;
    size_t index;
    size_t maximum = trainlog_body_zone_catalog_count();

    if (body == NULL || body_size == 0U || body_size > TRAINLOG_WEB_EXERCISES_BODY_MAX ||
        output == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    document = yyjson_read_opts((char *)body, body_size, YYJSON_READ_NOFLAG, NULL, NULL);
    if (document == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    root = yyjson_doc_get_root(document);
    name = yyjson_obj_get(root, "name");
    recording = yyjson_obj_get(root, "recording_mode");
    tracking = yyjson_obj_get(root, "tracking_mode");
    fields = yyjson_obj_get(root, "data_fields");
    primary = yyjson_obj_get(root, "primary_zone_id");
    secondary = yyjson_obj_get(root, "secondary_zone_ids");
    if (!input_has_exact_keys(root) || !yyjson_is_str(name) || yyjson_get_len(name) == 0U ||
        yyjson_get_len(name) > TRAINLOG_NAME_MAX || !yyjson_is_str(recording) ||
        !yyjson_is_str(tracking) || !yyjson_is_uint(fields) ||
        yyjson_get_uint(fields) > TRAINLOG_EXERCISE_DATA_KNOWN_MASK ||
        (!yyjson_is_null(primary) && !yyjson_is_str(primary)) || !yyjson_is_arr(secondary) ||
        yyjson_arr_size(secondary) > maximum || yyjson_arr_size(secondary) > 16U) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(output, 0, sizeof(*output));
    (void)snprintf(output->name, sizeof(output->name), "%s", yyjson_get_str(name));
    if (strcmp(yyjson_get_str(recording), "sets") == 0) {
        output->recording_mode = TRAINLOG_RECORDING_SETS;
    } else if (strcmp(yyjson_get_str(recording), "continuous") == 0) {
        output->recording_mode = TRAINLOG_RECORDING_CONTINUOUS;
    } else {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (strcmp(yyjson_get_str(tracking), "reps") == 0) {
        output->tracking_mode = TRAINLOG_TRACKING_REPS;
    } else if (strcmp(yyjson_get_str(tracking), "duration") == 0) {
        output->tracking_mode = TRAINLOG_TRACKING_DURATION;
    } else {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    output->data_fields = (TrainlogExerciseDataFields)yyjson_get_uint(fields);
    if (yyjson_is_str(primary)) {
        if (yyjson_get_len(primary) > TRAINLOG_ZONE_ID_MAX) {
            yyjson_doc_free(document);
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        (void)snprintf(output->primary_zone_id,
                       sizeof(output->primary_zone_id),
                       "%s",
                       yyjson_get_str(primary));
    }
    if ((output->recording_mode == TRAINLOG_RECORDING_SETS && output->data_fields != 0U) ||
        (require_sets_primary && output->recording_mode == TRAINLOG_RECORDING_SETS &&
         output->primary_zone_id[0] == '\0')) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    output->secondary_count = yyjson_arr_size(secondary);
    for (index = 0U; index < output->secondary_count; ++index) {
        yyjson_val *value = yyjson_arr_get(secondary, index);
        if (!yyjson_is_str(value) || yyjson_get_len(value) == 0U ||
            yyjson_get_len(value) > TRAINLOG_ZONE_ID_MAX) {
            yyjson_doc_free(document);
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        (void)snprintf(output->secondary_zone_ids[index],
                       sizeof(output->secondary_zone_ids[index]),
                       "%s",
                       yyjson_get_str(value));
        output->secondary_zone_pointers[index] = output->secondary_zone_ids[index];
    }
    yyjson_doc_free(document);
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_exercises_create_json(TrainlogDatabase *database,
                                                  const char *body,
                                                  size_t body_size,
                                                  char **output_json,
                                                  size_t *output_size) {
    ExerciseInput input;
    TrainlogExercise exercise;
    TrainlogStatus status = parse_input(body, body_size, true, &input);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    status = trainlog_catalog_create_exercise_profiled_with_zones(
        database,
        input.name,
        input.tracking_mode,
        input.recording_mode,
        input.data_fields,
        input.primary_zone_id[0] == '\0' ? NULL : input.primary_zone_id,
        input.secondary_zone_pointers,
        input.secondary_count,
        &exercise);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    return trainlog_web_exercises_detail_json(
        database, exercise.exercise_id, output_json, output_size);
}

TrainlogStatus trainlog_web_exercises_update_json(TrainlogDatabase *database,
                                                  const char *exercise_id,
                                                  const char *expected_revision,
                                                  const char *body,
                                                  size_t body_size,
                                                  char **output_json,
                                                  size_t *output_size) {
    ExerciseInput input;
    char current[EXERCISE_REVISION_CAPACITY];
    char normalized[(TRAINLOG_NAME_MAX * 4U) + 1U];
    TrainlogStatus status = parse_input(body, body_size, false, &input);
    if (status != TRAINLOG_STATUS_OK || expected_revision == NULL || expected_revision[0] == '\0') {
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_INVALID_ARGUMENT : status;
    }
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = exercise_revision(database, exercise_id, current);
    if (status == TRAINLOG_STATUS_OK && strcmp(current, expected_revision) != 0) {
        status = TRAINLOG_STATUS_CONFLICT;
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = trainlog_catalog_normalize_name(input.name, normalized, sizeof(normalized));
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = trainlog_database_update_exercise_profiled(
            database,
            exercise_id,
            input.name,
            normalized,
            input.tracking_mode,
            input.recording_mode,
            input.data_fields,
            input.primary_zone_id[0] == '\0' ? NULL : input.primary_zone_id,
            input.secondary_zone_pointers,
            input.secondary_count);
    }
    if (status == TRAINLOG_STATUS_OK &&
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (status != TRAINLOG_STATUS_OK) {
        (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        return status;
    }
    return trainlog_web_exercises_detail_json(database, exercise_id, output_json, output_size);
}

static TrainlogStatus causal_capacity_available(TrainlogDatabase *database) {
    sqlite3_stmt *statement = NULL;
    int result =
        sqlite3_prepare_v2(database->connection,
                           "SELECT "
                           "COUNT(*),COALESCE(SUM(length(operation_id)+length(target_kind)+length("
                           "target_id)+length(creator_id)+length(predecessor_revision_id)+length("
                           "created_at)+length(payload_sha256)+128),0) FROM sync_causal_operations",
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
    result = sqlite3_column_int64(statement, 0) < CAUSAL_OPERATION_MAX &&
             sqlite3_column_int64(statement, 1) < CAUSAL_TEXT_BUDGET;
    (void)sqlite3_finalize(statement);
    return result ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_CONFLICT;
}

static TrainlogStatus
exercise_live_revision(TrainlogDatabase *database, const char *exercise_id, char output[68]) {
    sqlite3_stmt *statement = NULL;
    yyjson_mut_doc *document = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *array = document == NULL ? NULL : yyjson_mut_arr(document);
    char *json = NULL;
    size_t json_size = 0U;
    char digest[65];
    int result;
    if (document == NULL || array == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, array);
    result = sqlite3_prepare_v2(database->connection,
                                "SELECT exercise_id,name,recording_mode,tracking_mode,data_fields "
                                "FROM exercises WHERE exercise_id=?1",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result != SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        yyjson_mut_doc_free(document);
        return result == SQLITE_DONE ? TRAINLOG_STATUS_NOT_FOUND : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (!yyjson_mut_arr_add_strcpy(
            document, array, (const char *)sqlite3_column_text(statement, 0)) ||
        !yyjson_mut_arr_add_strcpy(
            document, array, (const char *)sqlite3_column_text(statement, 1)) ||
        !yyjson_mut_arr_add_strcpy(
            document, array, (const char *)sqlite3_column_text(statement, 2)) ||
        !yyjson_mut_arr_add_strcpy(
            document, array, (const char *)sqlite3_column_text(statement, 3)) ||
        !yyjson_mut_arr_add_int(document, array, sqlite3_column_int64(statement, 4))) {
        (void)sqlite3_finalize(statement);
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    json = yyjson_mut_write(document, 0U, &json_size);
    yyjson_mut_doc_free(document);
    if (json == NULL || !sha256_hex(json, json_size, digest)) {
        free(json);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    free(json);
    (void)snprintf(output, 68U, "lv_%s", digest);
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_exercises_retire_json(TrainlogDatabase *database,
                                                  const char *exercise_id,
                                                  const char *expected_revision,
                                                  char **output_json,
                                                  size_t *output_size) {
    static const char CREATOR[] = "peer_desktop_web";
    sqlite3_stmt *statement = NULL;
    char current[EXERCISE_REVISION_CAPACITY];
    char predecessor[68];
    char operation_id[TRAINLOG_ID_MAX + 1U];
    char created_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char payload[1024];
    char digest[65];
    int written;
    int result;
    TrainlogStatus status;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    if (database == NULL || exercise_id == NULL || expected_revision == NULL ||
        output_json == NULL || output_size == NULL || exercise_is_builtin(exercise_id)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    result = sqlite3_prepare_v2(database->connection,
                                "SELECT operation_id FROM sync_causal_state WHERE "
                                "target_kind='exercise' AND target_id=?1 AND deleted=1",
                                -1,
                                &statement,
                                NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (result == SQLITE_ROW) {
        (void)sqlite3_finalize(statement);
        (void)sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL);
        document = yyjson_mut_doc_new(NULL);
        root = document == NULL ? NULL : yyjson_mut_obj(document);
        if (document == NULL || root == NULL) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
        yyjson_mut_doc_set_root(document, root);
        (void)yyjson_mut_obj_add_uint(document, root, "api_version", 1U);
        (void)yyjson_mut_obj_add_strcpy(document, root, "exercise_id", exercise_id);
        (void)yyjson_mut_obj_add_strcpy(document, root, "state", "retired");
        return json_finish(&document, output_json, output_size) ? TRAINLOG_STATUS_OK
                                                                : TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (result != SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    (void)sqlite3_finalize(statement);
    status = exercise_revision(database, exercise_id, current);
    if (status == TRAINLOG_STATUS_OK && strcmp(current, expected_revision) != 0) {
        status = TRAINLOG_STATUS_CONFLICT;
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = causal_capacity_available(database);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = exercise_live_revision(database, exercise_id, predecessor);
    }
    if (status == TRAINLOG_STATUS_OK &&
        (trainlog_id_generate("del", operation_id, sizeof(operation_id)) != TRAINLOG_STATUS_OK ||
         !timestamp_now(created_at))) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    written = status == TRAINLOG_STATUS_OK
                  ? snprintf(payload,
                             sizeof(payload),
                             "{\"created_at\":\"%s\",\"creator_id\":\"%s\",\"operation_id\":\"%s\","
                             "\"predecessor_revision_id\":\"%s\",\"publication_context\":null,"
                             "\"target_id\":\"%s\",\"target_kind\":\"exercise\"}",
                             created_at,
                             CREATOR,
                             operation_id,
                             predecessor,
                             exercise_id)
                  : -1;
    if (status == TRAINLOG_STATUS_OK && (written < 0 || (size_t)written >= sizeof(payload) ||
                                         !sha256_hex(payload, (size_t)written, digest))) {
        status = TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (status == TRAINLOG_STATUS_OK) {
        result = sqlite3_prepare_v2(
            database->connection,
            "INSERT INTO sync_causal_operations VALUES(?1,'exercise',?2,?3,?4,?5,?6,NULL)",
            -1,
            &statement,
            NULL);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, operation_id, -1, SQLITE_TRANSIENT);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, exercise_id, -1, SQLITE_TRANSIENT);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, CREATOR, -1, SQLITE_STATIC);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, predecessor, -1, SQLITE_TRANSIENT);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 5, created_at, -1, SQLITE_TRANSIENT);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 6, digest, -1, SQLITE_TRANSIENT);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
        statement = NULL;
    }
    if (status == TRAINLOG_STATUS_OK && result != SQLITE_DONE) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (status == TRAINLOG_STATUS_OK) {
        result = sqlite3_prepare_v2(
            database->connection,
            "INSERT OR REPLACE INTO "
            "sync_causal_state(target_kind,target_id,current_revision_id,deleted,"
            "operation_id) VALUES('exercise',?1,?2,1,?2)",
            -1,
            &statement,
            NULL);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, operation_id, -1, SQLITE_TRANSIENT);
    }
    if (status == TRAINLOG_STATUS_OK && result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
        statement = NULL;
    }
    if (status == TRAINLOG_STATUS_OK && result != SQLITE_DONE) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (status == TRAINLOG_STATUS_OK &&
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (status != TRAINLOG_STATUS_OK) {
        (void)sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        return status;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    if (document == NULL || root == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_strcpy(document, root, "exercise_id", exercise_id) ||
        !yyjson_mut_obj_add_strcpy(document, root, "state", "retired") ||
        !json_finish(&document, output_json, output_size)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}
