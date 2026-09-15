/*
 * Trainlog statistics interface.
 *
 * Declares the module boundary and ownership contract; implementation and persistence remain in their owning modules.
 */
#ifndef TRAINLOG_STATISTICS_H
#define TRAINLOG_STATISTICS_H

/**
 * @file statistics.h
 * @brief Auditable read models for the desktop statistics centre.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/database.h"

#define TRAINLOG_STATISTICS_BUCKET_MAX 128U
#define TRAINLOG_STATISTICS_ZONE_MAX 64U
#define TRAINLOG_STATISTICS_SERIES_MAX 256U

typedef enum TrainlogStatisticsWindow {
    TRAINLOG_STATISTICS_7_DAYS = 0,
    TRAINLOG_STATISTICS_30_DAYS,
    TRAINLOG_STATISTICS_ALL
} TrainlogStatisticsWindow;

typedef enum TrainlogStatisticsBucketKind {
    TRAINLOG_STATISTICS_BUCKET_7_DAYS = 0,
    TRAINLOG_STATISTICS_BUCKET_CALENDAR_MONTH
} TrainlogStatisticsBucketKind;

typedef struct TrainlogStatisticsBucket {
    int64_t timestamp;
    int64_t end_timestamp;
    char label[24];
    size_t sessions;
    size_t sets;
    uint64_t repetitions;
    double loaded_volume_kg;
    size_t immediate_feedback_count;
} TrainlogStatisticsBucket;

typedef struct TrainlogStatisticsSummary {
    size_t sessions;
    size_t distinct_exercises;
    size_t occurrences;
    size_t sets;
    uint64_t repetitions;
    uint64_t activity_duration_seconds;
    uint64_t session_duration_seconds;
    size_t sessions_with_duration;
    double loaded_volume_kg;
    size_t active_days;
    size_t active_weeks;
    int64_t latest_session_timestamp;
    size_t days_since_last_session;
    size_t immediate_feedback_count;
    size_t followup_count;
    char top_feedback_exercise[TRAINLOG_NAME_MAX + 1U];
    size_t top_feedback_count;
    size_t planned_actual_occurrences;
    uint64_t planned_sets;
    uint64_t actual_sets_for_plans;
    uint64_t planned_repetitions;
    uint64_t actual_repetitions_for_plans;
    TrainlogStatisticsBucket buckets[TRAINLOG_STATISTICS_BUCKET_MAX];
    size_t bucket_count;
    bool buckets_partial;
    TrainlogStatisticsBucketKind bucket_kind;
} TrainlogStatisticsSummary;

typedef struct TrainlogZoneStatistics {
    char zone_id[TRAINLOG_ID_MAX + 1U];
    char label[TRAINLOG_NAME_MAX + 1U];
    size_t sessions_any;
    size_t primary_7;
    size_t secondary_7;
    size_t primary_30;
    size_t secondary_30;
    size_t distinct_exercises;
    char latest_primary[TRAINLOG_TIMESTAMP_MAX + 1U];
    char latest_secondary[TRAINLOG_TIMESTAMP_MAX + 1U];
} TrainlogZoneStatistics;

typedef enum TrainlogExerciseSeriesKind {
    TRAINLOG_EXERCISE_SERIES_LOAD = 0,
    TRAINLOG_EXERCISE_SERIES_VOLUME,
    TRAINLOG_EXERCISE_SERIES_REPETITIONS,
    TRAINLOG_EXERCISE_SERIES_SETS,
    TRAINLOG_EXERCISE_SERIES_MAX,
    TRAINLOG_EXERCISE_SERIES_COUNT
} TrainlogExerciseSeriesKind;

typedef struct TrainlogStatisticPoint {
    int64_t timestamp;
    char timestamp_label[TRAINLOG_TIMESTAMP_MAX + 1U];
    double value;
} TrainlogStatisticPoint;

typedef struct TrainlogMaxListItem {
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    double latest_max_kg;
    char latest_at[TRAINLOG_TIMESTAMP_MAX + 1U];
} TrainlogMaxListItem;

typedef struct TrainlogExerciseListStatistic {
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    size_t occurrences;
    char latest_at[TRAINLOG_TIMESTAMP_MAX + 1U];
} TrainlogExerciseListStatistic;

typedef struct TrainlogExerciseStatistics {
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char name[TRAINLOG_NAME_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
    TrainlogRecordingMode recording_mode;
    char load_semantics[32];
    size_t occurrences;
    size_t sessions;
    size_t sets;
    uint64_t repetitions;
    uint64_t duration_seconds;
    double loaded_volume_kg;
    double highest_load_kg;
    bool has_highest_load;
    int highest_repetitions;
    double highest_repetitions_load_kg;
    bool has_repetitions_at_load;
    char first_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char last_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    size_t frequency_7;
    size_t frequency_30;
    size_t max_count;
    double latest_max_kg;
    double previous_max_kg;
    double best_max_kg;
    char latest_max_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    bool has_latest_max;
    bool has_previous_max;
    size_t immediate_feedback_count;
    uint64_t planned_sets;
    uint64_t actual_sets_for_plans;
    uint64_t planned_repetitions;
    uint64_t actual_repetitions_for_plans;
    double latest_planned_weight_kg;
    double latest_actual_weight_kg;
    bool has_latest_planned_actual_weight;
    TrainlogStatisticPoint series[TRAINLOG_EXERCISE_SERIES_COUNT]
                                  [TRAINLOG_STATISTICS_SERIES_MAX];
    size_t series_count[TRAINLOG_EXERCISE_SERIES_COUNT];
    bool series_partial[TRAINLOG_EXERCISE_SERIES_COUNT];
} TrainlogExerciseStatistics;

/**
 * Load selected-period global statistics and bounded comparison buckets.
 *
 * CONTRACT: now_utc is the inclusive observation cutoff. Summary totals cover
 * the current selected calendar period, while graph buckets compare consecutive
 * calendar periods. Seven-day buckets are Monday-Sunday weeks and 30-day mode
 * uses Gregorian calendar months; all-history aggregation is adaptive.
 * Only the graph series can be bounded/partial. The
 * database and output are borrowed and no SQLite resource escapes this call.
 */
TrainlogStatus trainlog_statistics_load_summary(
    TrainlogDatabase *database, TrainlogStatisticsWindow window,
    int64_t now_utc, TrainlogStatisticsSummary *output);

/* BODY ZONES are counted only from persisted primary/secondary relations. */
TrainlogStatus trainlog_statistics_load_zones(
    TrainlogDatabase *database, int64_t now_utc,
    TrainlogZoneStatistics *output, size_t capacity, size_t *output_count,
    bool *output_partial);

/* Exact canonical exercise_id lookup; display names never select history. */
TrainlogStatus trainlog_statistics_load_exercise(
    TrainlogDatabase *database, const char *exercise_id, int64_t now_utc,
    TrainlogExerciseStatistics *output);

/* One bounded query for the default MAX catalogue surface; newest explicit
 * result wins per stable exercise_id and exercises without MAX are absent. */
TrainlogStatus trainlog_statistics_list_maxima(TrainlogDatabase *database,
    TrainlogMaxListItem *output, size_t capacity, size_t *output_count,
    bool *output_partial);
TrainlogStatus trainlog_statistics_list_exercises(TrainlogDatabase *database,
    TrainlogExerciseListStatistic *output, size_t capacity,
    size_t *output_count, bool *output_partial);

#endif
