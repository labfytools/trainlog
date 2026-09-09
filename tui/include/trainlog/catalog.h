#ifndef TRAINLOG_CATALOG_H
#define TRAINLOG_CATALOG_H

/**
 * @file catalog.h
 * @brief Exercise-name normalization and canonical catalog creation.
 */

#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

/**
 * @brief Normalize an exercise display name according to frozen Trainlog v1.
 *
 * Rules:
 * - Unicode NFC composition;
 * - trim leading/trailing Unicode whitespace;
 * - collapse whitespace runs to one ASCII space;
 * - Unicode case folding.
 *
 * @param input UTF-8 display name.
 * @param output Caller-owned UTF-8 buffer.
 * @param output_size Size of @p output.
 */
TrainlogStatus trainlog_catalog_normalize_name(
    const char *input,
    char *output,
    size_t output_size
);

/**
 * @brief Create one new canonical exercise with a generated UUIDv4 identity.
 */
TrainlogStatus trainlog_catalog_create_exercise(
    TrainlogDatabase *database,
    const char *name,
    TrainlogTrackingMode tracking_mode,
    TrainlogExercise *output_exercise
);

TrainlogStatus trainlog_catalog_create_exercise_profiled(
    TrainlogDatabase *database,
    const char *name,
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields,
    TrainlogExercise *output_exercise
);

/* CONTRACT: creation and its zone relations commit as one user operation.
 * A NULL primary is reserved for explicit unclassified/historic workflows;
 * interactive strength creation supplies one assignable manifest zone. */
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
);

#endif
