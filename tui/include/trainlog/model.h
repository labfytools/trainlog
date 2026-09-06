#ifndef TRAINLOG_MODEL_H
#define TRAINLOG_MODEL_H

/**
 * @file model.h
 * @brief Bounded data structures shared by Trainlog TUI core layers.
 */

#include <stdbool.h>
#include <stddef.h>

#define TRAINLOG_ID_MAX 128U
#define TRAINLOG_NAME_MAX 200U
#define TRAINLOG_TIMESTAMP_MAX 40U
#define TRAINLOG_NOTE_MAX 4000U

typedef enum TrainlogTrackingMode {
    TRAINLOG_TRACKING_REPS = 0,
    TRAINLOG_TRACKING_DURATION
} TrainlogTrackingMode;

typedef enum TrainlogLoadMode {
    TRAINLOG_LOAD_NONE = 0,
    TRAINLOG_LOAD_EXTERNAL,
    TRAINLOG_LOAD_ASSISTANCE
} TrainlogLoadMode;

typedef enum TrainlogSessionType {
    TRAINLOG_SESSION_TRAINING = 0,
    TRAINLOG_SESSION_MAX_TEST
} TrainlogSessionType;

typedef struct TrainlogExercise {
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char name[TRAINLOG_NAME_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
} TrainlogExercise;

typedef struct TrainlogSetInput {
    int reps;
    int duration_seconds;
    bool has_weight;
    double weight_kg;
} TrainlogSetInput;

typedef struct TrainlogSessionExerciseInput {
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    TrainlogLoadMode load_mode;
    int rest_seconds;
    int target_sets;
    int target_reps;
    int target_duration_seconds;
    bool target_has_weight;
    double target_weight_kg;
    const char *notes;
    const TrainlogSetInput *sets;
    size_t set_count;
} TrainlogSessionExerciseInput;

typedef struct TrainlogSessionInput {
    char session_id[TRAINLOG_ID_MAX + 1U];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogSessionType session_type;
    const char *notes;
    const TrainlogSessionExerciseInput *exercises;
    size_t exercise_count;
} TrainlogSessionInput;

typedef struct TrainlogSessionSummary {
    char session_id[TRAINLOG_ID_MAX + 1U];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogSessionType session_type;
    size_t exercise_count;
} TrainlogSessionSummary;

/**
 * Missing body values are represented by has_* flags rather than sentinel
 * numbers so zero and NaN never acquire accidental persistence semantics.
 */
typedef struct TrainlogBodyObservationInput {
    char observation_id[TRAINLOG_ID_MAX + 1U];
    char observed_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    const char *session_id;
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
    const char *notes;
} TrainlogBodyObservationInput;

typedef struct TrainlogWeightPoint {
    char observed_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    double body_weight_kg;
} TrainlogWeightPoint;

#endif
