#ifndef TRAINLOG_SESSION_GENERATION_POLICY_INTERNAL_H
#define TRAINLOG_SESSION_GENERATION_POLICY_INTERNAL_H

#include <stddef.h>

typedef struct TrainlogSessionGenerationGoalPolicy {
    const char *id;
    int sets, repetitions, rest_seconds;
    int sets_min, sets_max, repetitions_min, repetitions_max, rest_min, rest_max;
} TrainlogSessionGenerationGoalPolicy;

typedef struct TrainlogSessionGenerationScores {
    int requested_primary, requested_secondary, new_primary, new_pattern;
    int load_history, preferred, recent_exercise, recent_pattern;
    int exposure_primary_24h, exposure_secondary_24h;
    int exposure_primary_72h, exposure_secondary_72h, secondary_zone_exposure;
} TrainlogSessionGenerationScores;

typedef struct TrainlogSessionGenerationZoneExpansion {
    const char *zone_id;
    const char *expanded_zone_ids;
} TrainlogSessionGenerationZoneExpansion;

typedef struct TrainlogSessionGenerationPolicy {
    int load_lookback_seconds, short_window_seconds, long_window_seconds;
    int exercise_recency_seconds, pattern_recency_seconds;
    int max_exercises, min_minutes, max_minutes;
    const int *duration_presets_minutes;
    size_t duration_preset_count;
    int preparation_seconds, setup_seconds, repetition_seconds;
    int primary_24h, secondary_24h, primary_72h, secondary_72h;
    TrainlogSessionGenerationScores scores;
    const char *upper_push_patterns, *upper_pull_patterns;
    const char *lower_extension_patterns, *lower_flexion_patterns;
    const TrainlogSessionGenerationZoneExpansion *zone_expansions;
    size_t zone_expansion_count;
    const TrainlogSessionGenerationGoalPolicy *goals;
    size_t goal_count;
} TrainlogSessionGenerationPolicy;

extern const TrainlogSessionGenerationPolicy trainlog_session_generation_policy_v1;

#endif
