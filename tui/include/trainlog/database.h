#ifndef TRAINLOG_DATABASE_H
#define TRAINLOG_DATABASE_H

/**
 * @file database.h
 * @brief SQLite persistence API for the Trainlog TUI core.
 */

#include <stddef.h>

#include "trainlog/model.h"
#include "trainlog/status.h"

#define TRAINLOG_DATABASE_SCHEMA_VERSION 7

typedef struct TrainlogDatabase TrainlogDatabase;

TrainlogStatus trainlog_database_open(
    const char *path,
    TrainlogDatabase **output_database
);

/**
 * @brief Open a database and report the backend failure that prevented it.
 *
 * CONTRACT: @p output_diagnostic is optional. When supplied with a non-zero
 * capacity, it receives a NUL-terminated explanation of the failed SQLite
 * operation; callers still use the returned TrainlogStatus for control flow.
 * This preserves the stable application status surface without hiding the
 * path-specific reason needed to repair a user's durable database safely.
 */
TrainlogStatus trainlog_database_open_with_diagnostic(
    const char *path,
    TrainlogDatabase **output_database,
    char *output_diagnostic,
    size_t output_diagnostic_capacity
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

TrainlogStatus trainlog_database_insert_exercise_profiled(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *name,
    const char *normalized_name,
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields
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
    /* Stable occurrence identity; exercise_id is catalogue identity only. */
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char name[TRAINLOG_NAME_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
    TrainlogRecordingMode recording_mode;
    TrainlogExerciseDataFields data_fields;
    TrainlogLoadMode load_mode;
    int rest_seconds;
    int target_sets;
    int target_reps;
    int target_duration_seconds;
    int has_target_weight;
    double target_weight_kg;

    int continuous_duration_seconds;
    int has_continuous_speed;
    double continuous_speed_kmh;
    int has_continuous_distance;
    double continuous_distance_km;

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


/* TRAINLOG_EXERCISE_PERFORMANCE_API */

typedef struct TrainlogExercisePerformancePoint {
    char session_id[TRAINLOG_ID_MAX + 1U];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogSessionType session_type;
    TrainlogTrackingMode tracking_mode;
    TrainlogLoadMode load_mode;
    size_t actual_set_count;
    int has_performance;
    int metric_value;
    int has_weight;
    double weight_kg;
} TrainlogExercisePerformancePoint;

/**
 * @brief Read newest-first per-session representative performance.
 *
 * Representative-set semantics:
 *
 * - no load: greatest successful reps/duration;
 * - external load: greatest load, then greatest reps/duration;
 * - assistance: lowest assistance, then greatest reps/duration.
 *
 * A zero-repetition failed attempt is never promoted to representative
 * performance. This API does not create or infer a measured maximum.
 */
TrainlogStatus trainlog_database_list_exercise_performance(
    TrainlogDatabase *database,
    const char *exercise_id,
    TrainlogExercisePerformancePoint *output,
    size_t capacity,
    size_t *output_count
);


/* TRAINLOG_SESSION_EDIT_API */

typedef struct TrainlogEditableExerciseRecord {
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    char name[TRAINLOG_NAME_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
    TrainlogLoadMode load_mode;
    int rest_seconds;
    int target_sets;
    int target_reps;
    int target_duration_seconds;
    int has_target_weight;
    double target_weight_kg;
    char notes[TRAINLOG_NOTE_MAX + 1U];
    size_t set_offset;
    size_t set_count;
} TrainlogEditableExerciseRecord;

/**
 * @brief Load a complete persisted session into caller-owned editable buffers.
 *
 * The function never truncates a session silently. If either caller capacity is
 * too small it returns TRAINLOG_STATUS_INVALID_ARGUMENT and the outputs must not
 * be used.
 */
TrainlogStatus trainlog_database_load_session_editable(
    TrainlogDatabase *database,
    const char *session_id,
    TrainlogSessionSummary *output_session,
    TrainlogEditableExerciseRecord *output_exercises,
    size_t exercise_capacity,
    size_t *output_exercise_count,
    TrainlogSetInput *output_sets,
    size_t set_capacity,
    size_t *output_set_count
);

/**
 * @brief Replace only the exercise/set contents of an existing session.
 *
 * The session row itself is preserved, so session_id, timestamps, session type,
 * session notes and body_observations.session_row_id remain attached to the
 * same row. The replacement is atomic: any error rolls the whole operation
 * back.
 */
TrainlogStatus trainlog_database_replace_session_exercises(
    TrainlogDatabase *database,
    const char *session_id,
    const TrainlogSessionExerciseInput *exercises,
    size_t exercise_count
);


/* TRAINLOG_BODY_OBSERVATION_RECORD_API */

typedef struct TrainlogBodyObservationRecord {
    char observation_id[TRAINLOG_ID_MAX + 1U];
    char observed_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char session_id[TRAINLOG_ID_MAX + 1U];

    bool has_body_weight;
    double body_weight_kg;
    bool has_neck;
    double neck_cm;
    bool has_shoulders;
    double shoulders_cm;
    bool has_chest;
    double chest_cm;
    bool has_waist;
    double waist_cm;
    bool has_hips;
    double hips_cm;
    bool has_left_arm;
    double left_arm_cm;
    bool has_right_arm;
    double right_arm_cm;
    bool has_left_forearm;
    double left_forearm_cm;
    bool has_right_forearm;
    double right_forearm_cm;
    bool has_left_thigh;
    double left_thigh_cm;
    bool has_right_thigh;
    double right_thigh_cm;
    bool has_left_calf;
    double left_calf_cm;
    bool has_right_calf;
    double right_calf_cm;

    char notes[TRAINLOG_NOTE_MAX + 1U];
} TrainlogBodyObservationRecord;

TrainlogStatus trainlog_database_list_body_observations(
    TrainlogDatabase *database,
    TrainlogBodyObservationRecord *output,
    size_t capacity,
    size_t *output_count
);

TrainlogStatus trainlog_database_get_body_observation(
    TrainlogDatabase *database,
    const char *observation_id,
    TrainlogBodyObservationRecord *output
);

/**
 * @brief Correct metric values of one existing observation.
 *
 * observation_id identifies the stable row. observed_at and session linkage
 * remain unchanged. At least one body metric must remain present.
 */
TrainlogStatus trainlog_database_update_body_observation(
    TrainlogDatabase *database,
    const TrainlogBodyObservationInput *observation
);

#endif
