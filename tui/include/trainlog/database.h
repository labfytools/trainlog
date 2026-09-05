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


/* TRAINLOG_SESSION_DETAILS_API */

#define TRAINLOG_SET_SUMMARY_MAX 1024U

typedef struct TrainlogPersistedExerciseDetail {
    char name[TRAINLOG_NAME_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
    TrainlogLoadMode load_mode;
    int rest_seconds;
    int target_sets;
    int target_reps;
    int target_duration_seconds;
    int has_target_weight;
    double target_weight_kg;
    size_t actual_set_count;
    char actual_summary[TRAINLOG_SET_SUMMARY_MAX + 1U];
} TrainlogPersistedExerciseDetail;

/**
 * @brief Load one session header plus ordered exercise details.
 *
 * The function is read-only and allocates nothing.
 */
TrainlogStatus trainlog_database_get_session_details(
    TrainlogDatabase *database,
    const char *session_id,
    TrainlogSessionSummary *output_session,
    TrainlogPersistedExerciseDetail *output_exercises,
    size_t exercise_capacity,
    size_t *output_exercise_count
);


/* TRAINLOG_BODY_METRIC_HISTORY_API */

typedef enum TrainlogBodyMetric {
    TRAINLOG_BODY_METRIC_WEIGHT = 0,
    TRAINLOG_BODY_METRIC_NECK,
    TRAINLOG_BODY_METRIC_SHOULDERS,
    TRAINLOG_BODY_METRIC_CHEST,
    TRAINLOG_BODY_METRIC_WAIST,
    TRAINLOG_BODY_METRIC_HIPS,
    TRAINLOG_BODY_METRIC_LEFT_ARM,
    TRAINLOG_BODY_METRIC_RIGHT_ARM,
    TRAINLOG_BODY_METRIC_LEFT_FOREARM,
    TRAINLOG_BODY_METRIC_RIGHT_FOREARM,
    TRAINLOG_BODY_METRIC_LEFT_THIGH,
    TRAINLOG_BODY_METRIC_RIGHT_THIGH,
    TRAINLOG_BODY_METRIC_LEFT_CALF,
    TRAINLOG_BODY_METRIC_RIGHT_CALF,
    TRAINLOG_BODY_METRIC_COUNT
} TrainlogBodyMetric;

typedef struct TrainlogBodyMetricPoint {
    char observed_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    double value;
} TrainlogBodyMetricPoint;

typedef struct TrainlogBodyPairPoint {
    bool found;
    char observed_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    double left_value;
    double right_value;
} TrainlogBodyPairPoint;

TrainlogStatus trainlog_database_list_body_metric_points(
    TrainlogDatabase *database,
    TrainlogBodyMetric metric,
    TrainlogBodyMetricPoint *output,
    size_t capacity,
    size_t *output_count
);

TrainlogStatus trainlog_database_latest_body_pair(
    TrainlogDatabase *database,
    TrainlogBodyMetric left_metric,
    TrainlogBodyMetric right_metric,
    TrainlogBodyPairPoint *output
);

#endif
