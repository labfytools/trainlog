#ifndef TRAINLOG_MODEL_H
#define TRAINLOG_MODEL_H

/**
 * @file model.h
 * @brief Bounded data structures shared by Trainlog TUI core layers.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRAINLOG_ID_MAX 128U
#define TRAINLOG_NAME_MAX 200U
#define TRAINLOG_TIMESTAMP_MAX 40U
#define TRAINLOG_NOTE_MAX 4000U
#define TRAINLOG_ZONE_ID_MAX 64U

typedef enum TrainlogTrackingMode {
    TRAINLOG_TRACKING_REPS = 0,
    TRAINLOG_TRACKING_DURATION
} TrainlogTrackingMode;

typedef enum TrainlogRecordingMode {
    TRAINLOG_RECORDING_SETS = 0,
    TRAINLOG_RECORDING_CONTINUOUS
} TrainlogRecordingMode;

typedef uint32_t TrainlogExerciseDataFields;

#define TRAINLOG_EXERCISE_DATA_SPEED_KMH UINT32_C(1)
#define TRAINLOG_EXERCISE_DATA_DISTANCE_KM UINT32_C(2)
#define TRAINLOG_EXERCISE_DATA_KNOWN_MASK \
    (TRAINLOG_EXERCISE_DATA_SPEED_KMH | \
     TRAINLOG_EXERCISE_DATA_DISTANCE_KM)

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
    TrainlogRecordingMode recording_mode;
    TrainlogExerciseDataFields data_fields;
} TrainlogExercise;

typedef enum TrainlogBodyZoneRole {
    TRAINLOG_BODY_ZONE_PRIMARY = 0,
    TRAINLOG_BODY_ZONE_SECONDARY
} TrainlogBodyZoneRole;

/* CONTRACT: zone_id is a stable manifest identity. Database readers copy
 * relations into caller-owned fixed-width records; display names never cross
 * this persistence API. */
typedef struct TrainlogExerciseBodyZone {
    char zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
    TrainlogBodyZoneRole role;
} TrainlogExerciseBodyZone;

typedef struct TrainlogSetInput {
    int reps;
    int duration_seconds;
    bool has_weight;
    double weight_kg;
} TrainlogSetInput;

typedef struct TrainlogSessionExerciseInput {
    /* Empty means local creation; the database allocates a stable sxe UUID. */
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    /* Optional canonical ID from equipment-v1.json, never a SQLite row ID. */
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    TrainlogRecordingMode recording_mode;
    TrainlogExerciseDataFields data_fields;
    TrainlogLoadMode load_mode;
    int rest_seconds;
    int target_sets;
    int target_reps;
    int target_duration_seconds;
    bool target_has_weight;
    double target_weight_kg;

    /*
     * CONTRACT: max weight is an occurrence-owned result for max_test only.
     * When present it is mutually exclusive with sets and continuous data.
     */
    bool has_max_weight;
    double max_weight_kg;

    int continuous_duration_seconds;
    bool continuous_has_speed;
    double continuous_speed_kmh;
    bool continuous_has_distance;
    double continuous_distance_km;

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
