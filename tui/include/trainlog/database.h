#ifndef TRAINLOG_DATABASE_H
#define TRAINLOG_DATABASE_H

/**
 * @file database.h
 * @brief SQLite persistence API for the Trainlog TUI core.
 */

#include <stddef.h>

#include "trainlog/model.h"
#include "trainlog/status.h"

#define TRAINLOG_DATABASE_SCHEMA_VERSION 1

typedef struct TrainlogDatabase TrainlogDatabase;

TrainlogStatus trainlog_database_open(
    const char *path,
    TrainlogDatabase **output_database
);

void trainlog_database_close(TrainlogDatabase *database);

TrainlogStatus trainlog_database_schema_version(
    TrainlogDatabase *database,
    int *output_version
);

TrainlogStatus trainlog_database_foreign_keys_enabled(
    TrainlogDatabase *database,
    int *output_enabled
);

TrainlogStatus trainlog_database_begin(TrainlogDatabase *database);
TrainlogStatus trainlog_database_commit(TrainlogDatabase *database);
TrainlogStatus trainlog_database_rollback(TrainlogDatabase *database);

/**
 * @brief Insert one already-normalized canonical exercise row.
 *
 * Unicode normalization belongs to catalog.c. This lower-level API owns the
 * final SQLite uniqueness barrier.
 */
TrainlogStatus trainlog_database_insert_exercise(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *name,
    const char *normalized_name,
    TrainlogTrackingMode tracking_mode
);

TrainlogStatus trainlog_database_exercise_count(
    TrainlogDatabase *database,
    size_t *output_count
);

TrainlogStatus trainlog_database_list_exercises(
    TrainlogDatabase *database,
    TrainlogExercise *output,
    size_t capacity,
    size_t *output_count
);

TrainlogStatus trainlog_database_insert_session(
    TrainlogDatabase *database,
    const TrainlogSessionInput *session
);

TrainlogStatus trainlog_database_session_count(
    TrainlogDatabase *database,
    size_t *output_count
);

TrainlogStatus trainlog_database_list_sessions(
    TrainlogDatabase *database,
    TrainlogSessionSummary *output,
    size_t capacity,
    size_t *output_count
);

TrainlogStatus trainlog_database_insert_body_observation(
    TrainlogDatabase *database,
    const TrainlogBodyObservationInput *observation
);

TrainlogStatus trainlog_database_list_weight_points(
    TrainlogDatabase *database,
    TrainlogWeightPoint *output,
    size_t capacity,
    size_t *output_count
);

#endif
