/**
 * @file catalog.c
 * @brief Frozen Trainlog v1 exercise-name normalization.
 */

#include "trainlog/catalog.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utf8proc.h>

#include "trainlog/id.h"

static bool codepoint_is_whitespace(utf8proc_int32_t codepoint)
{
    utf8proc_category_t category = utf8proc_category(codepoint);

    if (codepoint == '\t' ||
        codepoint == '\n' ||
        codepoint == '\v' ||
        codepoint == '\f' ||
        codepoint == '\r') {
        return true;
    }

    return category == UTF8PROC_CATEGORY_ZS ||
           category == UTF8PROC_CATEGORY_ZL ||
           category == UTF8PROC_CATEGORY_ZP;
}

TrainlogStatus trainlog_catalog_normalize_name(
    const char *input,
    char *output,
    size_t output_size
)
{
    utf8proc_uint8_t *mapped = NULL;
    utf8proc_ssize_t mapped_length;
    utf8proc_ssize_t offset = 0;
    size_t output_used = 0U;
    bool pending_space = false;
    bool wrote_content = false;

    if (input == NULL ||
        output == NULL ||
        output_size == 0U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    output[0] = '\0';

    mapped_length = utf8proc_map(
        (const utf8proc_uint8_t *)input,
        0,
        &mapped,
        UTF8PROC_STABLE |
            UTF8PROC_COMPOSE |
            UTF8PROC_CASEFOLD |
            UTF8PROC_NULLTERM
    );

    if (mapped_length < 0 || mapped == NULL) {
        free(mapped);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    while (offset < mapped_length) {
        utf8proc_int32_t codepoint = 0;
        utf8proc_ssize_t consumed;
        utf8proc_uint8_t encoded[4];
        utf8proc_ssize_t encoded_length;

        consumed = utf8proc_iterate(
            mapped + offset,
            mapped_length - offset,
            &codepoint
        );
        if (consumed <= 0) {
            free(mapped);
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        offset += consumed;

        if (codepoint_is_whitespace(codepoint)) {
            if (wrote_content) {
                pending_space = true;
            }
            continue;
        }

        if (pending_space) {
            if (output_used + 1U >= output_size) {
                free(mapped);
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
            output[output_used++] = ' ';
            pending_space = false;
        }

        encoded_length = utf8proc_encode_char(codepoint, encoded);
        if (encoded_length <= 0) {
            free(mapped);
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        if (output_used + (size_t)encoded_length >= output_size) {
            free(mapped);
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        (void)memcpy(
            output + output_used,
            encoded,
            (size_t)encoded_length
        );
        output_used += (size_t)encoded_length;
        wrote_content = true;
    }

    free(mapped);

    if (!wrote_content) {
        output[0] = '\0';
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    output[output_used] = '\0';
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_catalog_create_exercise_profiled(
    TrainlogDatabase *database,
    const char *name,
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields,
    TrainlogExercise *output_exercise
)
{
    char normalized[(TRAINLOG_NAME_MAX * 4U) + 1U];
    char exercise_id[TRAINLOG_GENERATED_ID_CAPACITY];
    TrainlogStatus status;

    if (database == NULL ||
        name == NULL ||
        output_exercise == NULL ||
        strlen(name) > TRAINLOG_NAME_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status = trainlog_catalog_normalize_name(
        name,
        normalized,
        sizeof(normalized)
    );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status = trainlog_id_generate(
        "ex",
        exercise_id,
        sizeof(exercise_id)
    );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status = trainlog_database_insert_exercise_profiled(
        database,
        exercise_id,
        name,
        normalized,
        tracking_mode,
        recording_mode,
        data_fields
    );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    (void)memset(
        output_exercise,
        0,
        sizeof(*output_exercise)
    );

    (void)snprintf(
        output_exercise->exercise_id,
        sizeof(output_exercise->exercise_id),
        "%s",
        exercise_id
    );

    (void)snprintf(
        output_exercise->name,
        sizeof(output_exercise->name),
        "%s",
        name
    );

    output_exercise->tracking_mode = tracking_mode;
    output_exercise->recording_mode = recording_mode;
    output_exercise->data_fields = data_fields;

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_catalog_create_exercise_profiled_with_zones(
    TrainlogDatabase *database,
    const char *name,
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields,
    const char *primary_zone_id,
    const char *const *secondary_zone_ids,
    size_t secondary_count,
    TrainlogExercise *output_exercise
)
{
    char normalized[(TRAINLOG_NAME_MAX * 4U) + 1U];
    char exercise_id[TRAINLOG_GENERATED_ID_CAPACITY];
    TrainlogStatus status;
    if (database == NULL || name == NULL || output_exercise == NULL ||
        strlen(name) > TRAINLOG_NAME_MAX) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    status = trainlog_catalog_normalize_name(name, normalized, sizeof(normalized));
    if (status != TRAINLOG_STATUS_OK) return status;
    status = trainlog_id_generate("ex", exercise_id, sizeof(exercise_id));
    if (status != TRAINLOG_STATUS_OK) return status;
    status = trainlog_database_begin(database);
    if (status != TRAINLOG_STATUS_OK) return status;
    status = trainlog_database_insert_exercise_profiled(database, exercise_id,
        name, normalized, tracking_mode, recording_mode, data_fields);
    if (status == TRAINLOG_STATUS_OK) {
        status = trainlog_database_replace_exercise_body_zones(database,
            exercise_id, primary_zone_id, secondary_zone_ids, secondary_count);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = trainlog_database_commit(database);
        if (status != TRAINLOG_STATUS_OK) {
            (void)trainlog_database_rollback(database);
        }
    } else {
        (void)trainlog_database_rollback(database);
    }
    if (status != TRAINLOG_STATUS_OK) return status;
    (void)memset(output_exercise, 0, sizeof(*output_exercise));
    (void)snprintf(output_exercise->exercise_id,
        sizeof(output_exercise->exercise_id), "%s", exercise_id);
    (void)snprintf(output_exercise->name,
        sizeof(output_exercise->name), "%s", name);
    output_exercise->tracking_mode = tracking_mode;
    output_exercise->recording_mode = recording_mode;
    output_exercise->data_fields = data_fields;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_catalog_create_exercise(
    TrainlogDatabase *database,
    const char *name,
    TrainlogTrackingMode tracking_mode,
    TrainlogExercise *output_exercise
)
{
    return trainlog_catalog_create_exercise_profiled(
        database,
        name,
        tracking_mode,
        TRAINLOG_RECORDING_SETS,
        0U,
        output_exercise
    );
}
