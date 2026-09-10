#ifndef TRAINLOG_SESSION_GENERATION_H
#define TRAINLOG_SESSION_GENERATION_H

/**
 * @file session_generation.h
 * @brief Deterministic SESSION_GENERATOR_V1 policy engine and history services.
 *
 * Requests and history rows are borrowed only for a call. Results contain no
 * borrowed pointers and need no release operation. Fixed capacities are part
 * of the public ABI: exceeding one fails explicitly rather than truncating a
 * result that could be mistaken for a complete scientific analysis.
 */

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/model.h"
#include "trainlog/status.h"

#define TRAINLOG_GENERATOR_MAX_CANDIDATES 64U
#define TRAINLOG_GENERATOR_MAX_EQUIPMENT 8U
#define TRAINLOG_GENERATOR_MAX_PATTERNS 16U
#define TRAINLOG_GENERATOR_MAX_ZONES 16U
#define TRAINLOG_GENERATOR_MAX_SELECTED 6U
#define TRAINLOG_GENERATOR_MAX_SOURCE_REFS 16U

typedef enum TrainlogGenerationWarningLevel {
    TRAINLOG_GENERATION_WARNING_NONE = 0,
    TRAINLOG_GENERATION_WARNING_NOTICE,
    TRAINLOG_GENERATION_WARNING_WARNING
} TrainlogGenerationWarningLevel;

typedef struct TrainlogGenerationHistoryRow {
    const char *session_id;
    const char *occurrence_id;
    const char *exercise_id;
    const char *started_at;
    const char *equipment_id; /* NULL records absent context. */
    TrainlogRecordingMode recording_mode;
    TrainlogTrackingMode tracking_mode;
    TrainlogLoadMode load_mode;
    int rest_seconds;
    bool has_target_sets, has_target_reps, has_target_duration, has_target_weight;
    bool has_actual_set;
    bool has_explicit_max;
    size_t set_position;
    int repetitions; /* meaningful only for an actual SETS+REPS row */
    bool has_weight;
    double weight_kg;
} TrainlogGenerationHistoryRow;

typedef struct TrainlogGenerationCandidate {
    const char *exercise_id;
    const char *equipment_id;
    const char *primary_zone_id;
    const char *const *secondary_zone_ids;
    size_t secondary_zone_count;
    const char *const *pattern_ids;
    size_t pattern_count;
    const char *const *source_ref_ids;
    size_t source_ref_count;
    const char *confidence; /* exactly "high" or "moderate" */
    const char *equipment_load_semantics; /* external, assistance, bodyweight */
} TrainlogGenerationCandidate;

typedef struct TrainlogGenerationRequest {
    const char *zone_id;
    const char *goal_id;
    int duration_minutes;
    const char *reference_time;
    const TrainlogGenerationCandidate *candidates;
    size_t candidate_count;
    const char *const *preferred_exercise_ids;
    size_t preferred_count;
    const char *const *excluded_exercise_ids;
    size_t excluded_exercise_count;
    const char *const *excluded_pattern_ids;
    size_t excluded_pattern_count;
} TrainlogGenerationRequest;

typedef struct TrainlogGenerationDatabaseRequest {
    const char *zone_id;
    const char *goal_id;
    int duration_minutes;
    const char *reference_time;
    /* NULL means all scientifically compatible supplied equipment. A non-NULL
     * array with count zero means explicitly no equipment is available. */
    const char *const *available_equipment_ids;
    size_t available_equipment_count;
    const char *const *preferred_exercise_ids;
    size_t preferred_count;
    const char *const *excluded_exercise_ids;
    size_t excluded_exercise_count;
    const char *const *excluded_pattern_ids;
    size_t excluded_pattern_count;
} TrainlogGenerationDatabaseRequest;

typedef struct TrainlogExposureWindowSummary {
    size_t primary_set_count, secondary_set_count, session_count;
    char pattern_ids[TRAINLOG_GENERATOR_MAX_PATTERNS][TRAINLOG_ID_MAX + 1U];
    size_t pattern_count;
} TrainlogExposureWindowSummary;

typedef struct TrainlogBodyZoneRecentExposure {
    TrainlogExposureWindowSummary within_24h, within_72h;
    bool recent_exposure, repeated_exposure;
    TrainlogGenerationWarningLevel warning_level;
    bool has_latest;
    char latest_started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char latest_session_id[TRAINLOG_ID_MAX + 1U];
    char latest_occurrence_id[TRAINLOG_ID_MAX + 1U];
    char latest_pattern_ids[TRAINLOG_GENERATOR_MAX_PATTERNS][TRAINLOG_ID_MAX + 1U];
    size_t latest_pattern_count;
    size_t unclassified_actual_set_count;
} TrainlogBodyZoneRecentExposure;

typedef struct TrainlogTrainingRecencyWarning {
    bool recent_same_exercise;
    bool recent_same_pattern;
} TrainlogTrainingRecencyWarning;

typedef struct TrainlogGeneratedExercise {
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    char primary_zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
    char secondary_zone_ids[TRAINLOG_GENERATOR_MAX_ZONES][TRAINLOG_ZONE_ID_MAX + 1U];
    size_t secondary_zone_count;
    char pattern_ids[TRAINLOG_GENERATOR_MAX_PATTERNS][TRAINLOG_ID_MAX + 1U];
    size_t pattern_count;
    int target_sets, target_repetitions, rest_seconds;
    bool has_target_weight;
    double target_weight_kg;
    TrainlogLoadMode planned_load_mode;
    char equipment_load_semantics[32];
    char confidence[16];
    TrainlogTrainingRecencyWarning recency;
    TrainlogGenerationWarningLevel exposure_warning_level;
    int estimated_seconds;
    char load_source_session_id[TRAINLOG_ID_MAX + 1U];
    char load_source_occurrence_id[TRAINLOG_ID_MAX + 1U];
    char load_source_started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char rationale_codes[8][64];
    size_t rationale_count;
    char source_ref_ids[TRAINLOG_GENERATOR_MAX_SOURCE_REFS][TRAINLOG_ID_MAX + 1U];
    size_t source_ref_count;
} TrainlogGeneratedExercise;

typedef struct TrainlogGeneratedSession {
    TrainlogGeneratedExercise exercises[TRAINLOG_GENERATOR_MAX_SELECTED];
    size_t exercise_count;
    int estimated_duration_seconds;
    bool insufficient_resolved_candidates;
    char shortage_codes[16][64];
    size_t shortage_count;
    TrainlogBodyZoneRecentExposure exposure;
} TrainlogGeneratedSession;

typedef struct TrainlogSessionGenerationAnalyzer TrainlogSessionGenerationAnalyzer;
typedef struct TrainlogDatabase TrainlogDatabase;
typedef TrainlogStatus (*TrainlogGenerationHistoryVisitor)(
    void *context, const TrainlogGenerationHistoryRow *row);

/* Runs visitor over the complete completed-session occurrence history in one
 * nested-safe read snapshot. Rows are grouped as required by analyzer_accept;
 * no preview/page limit or SQL date predicate is applied. */
TrainlogStatus trainlog_database_scan_generation_history(
    TrainlogDatabase *database,
    TrainlogGenerationHistoryVisitor visitor,
    void *context
);

/* Production convenience path. Candidate assembly, runtime-profile checking,
 * exact equipment compatibility, and the complete history scan share one
 * outer read snapshot. Sparse knowledge is an OK result with insufficiency. */
TrainlogStatus trainlog_session_generate_from_database(
    TrainlogDatabase *database,
    const TrainlogGenerationDatabaseRequest *request,
    TrainlogGeneratedSession *output
);

/* Streaming boundary: memory is bounded by candidate/catalog caps, not history
 * length. Rows must be grouped by session_id then occurrence_id then position;
 * duplicate set identity is a DATABASE_ERROR. Every stored occurrence must be
 * offered, including empty/MAX/continuous rows, so malformed time cannot hide. */
TrainlogStatus trainlog_session_generation_analyzer_create(
    const TrainlogGenerationRequest *request,
    TrainlogSessionGenerationAnalyzer **output
);
TrainlogStatus trainlog_session_generation_analyzer_accept(
    TrainlogSessionGenerationAnalyzer *analyzer,
    const TrainlogGenerationHistoryRow *row
);
TrainlogStatus trainlog_session_generation_analyzer_finish(
    TrainlogSessionGenerationAnalyzer *analyzer,
    TrainlogGeneratedSession *output
);
void trainlog_session_generation_analyzer_destroy(TrainlogSessionGenerationAnalyzer *analyzer);

/* Pure dose-edit helper: changing sets/repetitions always re-runs anchor
 * qualification; it never retains a load qualified for a smaller dose. */
TrainlogStatus trainlog_session_generation_requalify_dose(
    const TrainlogGenerationRequest *request,
    size_t selected_index,
    int target_sets,
    int target_repetitions,
    TrainlogSessionGenerationAnalyzer **output_analyzer
);

/* Checked duration helpers use the policy-authored formula and return
 * INVALID_ARGUMENT on invalid dose or integer overflow. */
TrainlogStatus trainlog_session_generation_exercise_duration(
    int target_sets, int target_repetitions, int rest_seconds, int *output_seconds);

#endif
