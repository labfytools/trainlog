#ifndef TRAINLOG_DATABASE_H
#define TRAINLOG_DATABASE_H

/**
 * @file database.h
 * @brief SQLite persistence API for the Trainlog TUI core.
 */

#include <stddef.h>

#include "trainlog/model.h"
#include "trainlog/status.h"

#define TRAINLOG_DATABASE_SCHEMA_VERSION 15

typedef struct TrainlogDatabase TrainlogDatabase;

#define TRAINLOG_FEEDBACK_TEXT_MAX 8192U
#define TRAINLOG_FEEDBACK_VIEW_MAX 4096U
typedef struct TrainlogFeedbackView {
    char stable_id[TRAINLOG_ID_MAX + 1U];
    char entry_id[TRAINLOG_ID_MAX + 1U]; /* empty for session follow-up */
    char observed_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char raw_text[TRAINLOG_FEEDBACK_TEXT_MAX + 1U];
} TrainlogFeedbackView;

/* CONTRACT: read-only chronological consultation; Android remains the sole
 * V1 creator. Ties use the stable ID byte order. No SQLite resources escape. */
TrainlogStatus trainlog_database_list_training_feedback(
    TrainlogDatabase *database, const char *session_id,
    TrainlogFeedbackView *exercise_feedback, size_t exercise_capacity,
    size_t *exercise_count, TrainlogFeedbackView *followups,
    size_t followup_capacity, size_t *followup_count);

/* Nestable read savepoints let composition services observe one database state
 * without exposing SQLite or issuing writes to application data. */
TrainlogStatus trainlog_database_read_snapshot_begin(TrainlogDatabase *database);
TrainlogStatus trainlog_database_read_snapshot_end(TrainlogDatabase *database, bool commit_snapshot);

/* WHY: session occurrences retain an equipment ID, while custom definitions
 * need durable presentation metadata. A reference alone is never a definition. */
typedef struct TrainlogCustomEquipment {
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    char display_name[TRAINLOG_NAME_MAX + 1U];
    char label_name[TRAINLOG_NAME_MAX + 1U];
    char equipment_type[TRAINLOG_NAME_MAX + 1U];
    char load_semantics[32];
} TrainlogCustomEquipment;

#define TRAINLOG_CUSTOM_EQUIPMENT_PAGE_MAX 128U

typedef enum TrainlogEquipmentOrigin {
    TRAINLOG_EQUIPMENT_SUPPLIED = 0,
    TRAINLOG_EQUIPMENT_CUSTOM,
    TRAINLOG_EQUIPMENT_UNKNOWN
} TrainlogEquipmentOrigin;

typedef struct TrainlogResolvedEquipment {
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    char display_name[TRAINLOG_NAME_MAX + 1U];
    char label_name[TRAINLOG_NAME_MAX + 1U];
    char equipment_type[TRAINLOG_NAME_MAX + 1U];
    char load_semantics[32];
    TrainlogEquipmentOrigin origin;
} TrainlogResolvedEquipment;

TrainlogStatus trainlog_database_create_custom_equipment(
    TrainlogDatabase *database,
    const TrainlogCustomEquipment *equipment
);
TrainlogStatus trainlog_database_list_custom_equipment(
    TrainlogDatabase *database,
    TrainlogCustomEquipment *output,
    size_t capacity,
    size_t *output_count
);
/* CONTRACT: reads at most capacity + 1 ordered definitions, so callers can
 * filter pages before imposing a presentation cap. database and output are
 * borrowed for the call; the caller owns copied strings. This read-only API
 * retains no resources and writes no persistence. capacity is 1..128 and all
 * pointer arguments are required. output_count is at most capacity; output_more
 * is true exactly when one additional ordered row exists. CONTRACT: output,
 * output_count, and output_more are valid only when this returns
 * TRAINLOG_STATUS_OK; on every other status callers must disregard all output,
 * including any storage copied before the failure. offset is valid only while
 * data is unchanged; refresh after writes. It is neither a durable cursor nor
 * an idempotency mechanism. */
TrainlogStatus trainlog_database_list_custom_equipment_page(
    TrainlogDatabase *database,
    size_t offset,
    TrainlogCustomEquipment *output,
    size_t capacity,
    size_t *output_count,
    bool *output_more
);
/* CONTRACT: an occurrence ID always resolves to a visible value. Unknown IDs
 * are returned verbatim with TRAINLOG_EQUIPMENT_UNKNOWN, never hidden. */
TrainlogStatus trainlog_database_resolve_equipment(
    TrainlogDatabase *database,
    const char *equipment_id,
    TrainlogResolvedEquipment *output
);
TrainlogStatus trainlog_database_list_exercise_equipment(
    TrainlogDatabase *database,
    const char *exercise_id,
    TrainlogResolvedEquipment *output,
    size_t capacity,
    size_t *output_count
);

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

/**
 * @brief Resolve a current or merged exercise identity to its canonical ID.
 *
 * CONTRACT: current catalogue IDs resolve to themselves; durable legacy IDs
 * resolve through exercise_aliases. Unknown IDs return NOT_FOUND. The output
 * buffer is caller-owned and must hold TRAINLOG_ID_MAX + 1 bytes.
 */
TrainlogStatus trainlog_database_resolve_exercise_id(
    TrainlogDatabase *database,
    const char *exercise_id,
    char *output_canonical_id,
    size_t output_capacity
);

/**
 * @brief Atomically merge one current catalogue exercise into another.
 *
 * CONTRACT: source and canonical must be distinct current catalogue IDs.
 * Tracking/recording/data-field or conflicting non-empty primary-zone
 * profiles reject with CONFLICT and leave the database untouched. Compatible
 * direct zones are unioned; every occurrence is repointed without rewriting
 * its stable entry ID or any owned actual/planning data. Existing aliases to
 * source collapse directly to canonical and source becomes a durable alias.
 */
TrainlogStatus trainlog_database_merge_exercises(
    TrainlogDatabase *database,
    const char *source_exercise_id,
    const char *canonical_exercise_id
);

typedef struct TrainlogExerciseMergePreview {
    size_t occurrences;
    size_t performed_sets;
    size_t continuous_activities;
    size_t max_results;
    size_t associated_equipment;
    size_t body_zones;
} TrainlogExerciseMergePreview;

/* CONTRACT: counts describe source-owned records which the atomic merge will
 * repoint or union. Unknown IDs return NOT_FOUND and output is never partial. */
TrainlogStatus trainlog_database_preview_exercise_merge(
    TrainlogDatabase *database,
    const char *source_exercise_id,
    TrainlogExerciseMergePreview *output
);

/**
 * @brief Atomically replace one exercise's direct body-zone relations.
 *
 * @param primary_zone_id NULL or empty preserves the explicitly allowed
 *        unclassified state, which requires secondary_count == 0. Group zones
 *        cannot be assigned because their descendant relationship is derived
 *        from the canonical manifest.
 * @param secondary_zone_ids Caller-owned array borrowed for this call only.
 * @param secondary_count Bounded by the number of canonical manifest zones.
 * @return INVALID_ARGUMENT for unknown/group/duplicate zones, NOT_FOUND for
 *         an unknown exercise, or the explicit persistence status.
 */
TrainlogStatus trainlog_database_replace_exercise_body_zones(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *primary_zone_id,
    const char *const *secondary_zone_ids,
    size_t secondary_count
);

/* NOT_FOUND distinguishes an unknown exercise identity from the valid empty
 * relation set. Output is caller-owned; output_count reports required capacity
 * and no relation is truncated as a successful result. */
TrainlogStatus trainlog_database_list_exercise_body_zones(
    TrainlogDatabase *database,
    const char *exercise_id,
    TrainlogExerciseBodyZone *output,
    size_t capacity,
    size_t *output_count
);

/* CONTRACT: the caller owns output storage; output_count reports the required
 * count and INVALID_ARGUMENT is returned when capacity is insufficient.
 * Filtering combines a normalized name prefix with either one
 * direct zone, its manifest descendants, or the explicit unclassified state.
 * `primary_only` excludes secondary participation without changing storage.
 * zone_id and unclassified_only are mutually exclusive.
 * The resulting exercise IDs compose with list_exercise_body_zones() and
 * list_exercise_performance(); the latter exposes newest history and explicit
 * MAX without duplicating either datum for a future session generator. */
TrainlogStatus trainlog_database_list_exercises_filtered(
    TrainlogDatabase *database,
    const char *normalized_prefix,
    const char *zone_id,
    bool include_descendants,
    bool primary_only,
    bool unclassified_only,
    TrainlogExercise *output,
    size_t capacity,
    size_t *output_count
);

/* CONTRACT: name/profile/direct zones are one transaction and all string/list
 * inputs are borrowed only for the duration of the call. A profile change is
 * rejected once completed history references the exercise; a same-profile
 * rename or zone replacement preserves exercise_id and history. Zone rules
 * match replace_exercise_body_zones(). Unknown exercise IDs return NOT_FOUND;
 * invalid metadata and name collisions remain explicit errors. */
TrainlogStatus trainlog_database_update_exercise_profiled(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *name,
    const char *normalized_name,
    TrainlogTrackingMode tracking_mode,
    TrainlogRecordingMode recording_mode,
    TrainlogExerciseDataFields data_fields,
    const char *primary_zone_id,
    const char *const *secondary_zone_ids,
    size_t secondary_count
);

TrainlogStatus trainlog_database_insert_session(
    TrainlogDatabase *database,
    const TrainlogSessionInput *session
);

TrainlogStatus trainlog_database_session_count(
    TrainlogDatabase *database,
    size_t *output_count
);

/* Observable history only: a returned session owns at least one persisted
 * performed set, continuous activity, or explicit MAX result. ended_at is
 * presentation metadata and is not an actual-work gate. */
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

    int has_max_weight;
    double max_weight_kg;

    int continuous_duration_seconds;
    int has_continuous_speed;
    double continuous_speed_kmh;
    int has_continuous_distance;
    double continuous_distance_km;

    size_t actual_set_count;
    /* CONTRACT: history consumes this occurrence-owned ordered snapshot;
     * rendering must not reach into SQLite or impose a global dataset cap. */
    TrainlogSetInput *actual_sets;
    char actual_summary[TRAINLOG_SET_SUMMARY_MAX + 1U];
} TrainlogPersistedExerciseDetail;

/**
 * @brief Load one session header plus ordered exercise details.
 *
 * Callers must release a prior successful result before reusing its array.
 * On success, each copied detail owns actual_sets until released with
 * trainlog_database_free_session_details(). On failure, the function releases
 * every partial allocation and, when output_exercise_count is non-NULL, sets
 * it to zero. A nonzero capacity requires a non-NULL output_exercises array.
 */
TrainlogStatus trainlog_database_get_session_details(
    TrainlogDatabase *database,
    const char *session_id,
    TrainlogSessionSummary *output_session,
    TrainlogPersistedExerciseDetail *output_exercises,
    size_t exercise_capacity,
    size_t *output_exercise_count
);

/* Safe for zero-initialized details and details returned by the loader. */
void trainlog_database_free_session_details(
    TrainlogPersistedExerciseDetail *exercises,
    size_t exercise_count
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
    /* Explicit schema-v9 max: weight is the result; metric_value is internal. */
    int has_explicit_max;
    int metric_value;
    int has_weight;
    double weight_kg;
    char equipment_id[TRAINLOG_ID_MAX + 1U];
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

/* TRAINING_KNOWLEDGE_RUNTIME_READ_V1 */
#define TRAINLOG_OCCURRENCE_PAGE_MAX 32U
#define TRAINLOG_OCCURRENCE_SET_PAGE_MAX 64U

typedef struct TrainlogExerciseOccurrenceCursor {
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char session_id[TRAINLOG_ID_MAX + 1U];
    char entry_id[TRAINLOG_ID_MAX + 1U];
} TrainlogExerciseOccurrenceCursor;

typedef struct TrainlogExerciseOccurrence {
    char session_id[TRAINLOG_ID_MAX + 1U];
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    TrainlogSessionType session_type;
    TrainlogTrackingMode tracking_mode;
    TrainlogRecordingMode recording_mode;
    TrainlogExerciseDataFields data_fields;
    TrainlogLoadMode load_mode;
    size_t set_count;
    int continuous_duration_seconds;
    bool continuous_has_speed;
    double continuous_speed_kmh;
    bool continuous_has_distance;
    double continuous_distance_km;
} TrainlogExerciseOccurrence;

typedef struct TrainlogOccurrenceSet {
    size_t position;
    bool has_reps;
    int reps;
    bool has_duration;
    int duration_seconds;
    bool has_weight;
    double weight_kg;
} TrainlogOccurrenceSet;

typedef struct TrainlogLatestExplicitMax {
    bool found;
    char session_id[TRAINLOG_ID_MAX + 1U];
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    TrainlogLoadMode load_mode;
    double max_weight_kg;
} TrainlogLatestExplicitMax;

/* Exact-ID profile read; unknown IDs return NOT_FOUND. */
TrainlogStatus trainlog_database_get_exercise_profile(
    TrainlogDatabase *database,
    const char *exercise_id,
    TrainlogExercise *output
);

/**
 * Current-data keyset page ordered by the exact started_at instant, then
 * session_id and entry_id bytewise DESC. Accepted timestamps use the frozen
 * extended RFC3339 grammar, including T/t, Z/z, numeric offsets through
 * 23:59, arbitrary fractional precision, and the Android writer's omitted
 * seconds form. Equivalent trailing-zero fractions represent one instant.
 * A non-NULL cursor is exclusive. Pages are not a cross-call snapshot under
 * concurrent edits. Cursor arrays are borrowed for this call, must be NUL
 * terminated within their declared capacities, and started_at must be a real
 * accepted instant with an explicit numeric offset or Z/z. Malformed cursors
 * are INVALID_ARGUMENT before any history query. Malformed matching persisted
 * timestamps, or a selected timestamp too long for the fixed public output,
 * are DATABASE_ERROR rather than omitted or truncated. limit must be
 * 1..TRAINLOG_OCCURRENCE_PAGE_MAX.
 */
TrainlogStatus trainlog_database_list_exercise_occurrences_page(
    TrainlogDatabase *database,
    const char *exercise_id,
    const TrainlogExerciseOccurrenceCursor *after,
    size_t limit,
    TrainlogExerciseOccurrence *output,
    size_t *output_count,
    bool *output_has_more,
    TrainlogExerciseOccurrenceCursor *output_next
);

/* Original set positions are preserved. after_position is exclusive; use -1
 * for the first page. Reps are non-negative, so zero preserves a failed
 * attempt. Nullable values remain distinct from zero. Corrupt storage types or
 * values outside the public int/size_t ranges return DATABASE_ERROR. No rows
 * exist for continuous occurrences. Output remains caller-owned. */
TrainlogStatus trainlog_database_list_occurrence_sets_page(
    TrainlogDatabase *database,
    const char *entry_id,
    int after_position,
    size_t limit,
    TrainlogOccurrenceSet *output,
    size_t *output_count,
    bool *output_has_more,
    int *output_next_position
);

/* Reads max_results joined only through max_test sessions; ordinary large sets
 * never qualify. It uses the same exact instant and bytewise-ID ordering as
 * occurrence pagination. Output keeps the exact source timestamp and
 * occurrence load/equipment context; corrupt or over-capacity selected storage
 * returns DATABASE_ERROR. */
TrainlogStatus trainlog_database_latest_explicit_max_context(
    TrainlogDatabase *database,
    const char *exercise_id,
    TrainlogLatestExplicitMax *output
);

/* Same chronological contract, additionally restricted to one exact nonempty
 * equipment ID. This is the compatibility reader for user-directed %MAX. */
TrainlogStatus trainlog_database_latest_explicit_max_equipment_context(
    TrainlogDatabase *database,
    const char *exercise_id,
    const char *equipment_id,
    TrainlogLatestExplicitMax *output
);


/* TRAINLOG_SESSION_EDIT_API */

typedef struct TrainlogEditableExerciseRecord {
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    char name[TRAINLOG_NAME_MAX + 1U];
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
    int has_max_weight;
    double max_weight_kg;
    int continuous_duration_seconds;
    int has_continuous_speed;
    double continuous_speed_kmh;
    int has_continuous_distance;
    double continuous_distance_km;
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
 * session notes, session follow-ups and body_observations.session_row_id remain
 * attached to the same row. Exercise feedback is remapped only by retained
 * entry_id within that session; removed entries cascade and new entries never
 * inherit feedback. The replacement is atomic: any error rolls the whole
 * operation back.
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
