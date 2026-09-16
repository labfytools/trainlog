/*
 * Trainlog Dashboard Core read model.
 *
 * This public boundary deliberately exposes neither SQLite nor TUI types.
 */
#ifndef TRAINLOG_DASHBOARD_H
#define TRAINLOG_DASHBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/database.h"

#define TRAINLOG_DASHBOARD_SESSION_CAPACITY 128U
#define TRAINLOG_DASHBOARD_FACT_CAPACITY 4096U
#define TRAINLOG_DASHBOARD_PERFORMANCE_CAPACITY 128U
#define TRAINLOG_DASHBOARD_BODY_CAPACITY 256U
#define TRAINLOG_DASHBOARD_BUCKET_CAPACITY 12U
#define TRAINLOG_DASHBOARD_ZONE_CAPACITY 17U
#define TRAINLOG_DASHBOARD_BODY_METRIC_COUNT 14U

typedef enum TrainlogDashboardPeriod {
    TRAINLOG_DASHBOARD_30_DAYS = 0,
    TRAINLOG_DASHBOARD_7_DAYS,
    TRAINLOG_DASHBOARD_90_DAYS,
    TRAINLOG_DASHBOARD_YEAR,
    TRAINLOG_DASHBOARD_ALL
} TrainlogDashboardPeriod;

typedef struct TrainlogDashboardQuery {
    TrainlogDashboardPeriod period;
    /* CONTRACT: explicit UTC Unix second used by every rolling window and by
     * the final presentation bucket. This makes one query deterministic. */
    int64_t reference_unix_second;
} TrainlogDashboardQuery;

typedef struct TrainlogDashboardBucket {
    int64_t start_day;
    int64_t span_days;
    size_t sessions;
    size_t maxima;
    size_t working_improvements;
    size_t max_improvements;
} TrainlogDashboardBucket;

typedef struct TrainlogDashboardZoneBucket {
    /* Empty means the explicit unclassified bucket. */
    char zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
    size_t count;
} TrainlogDashboardZoneBucket;

typedef struct TrainlogDashboardSnapshot {
    bool error;
    bool partial;
    bool invalid_data;
    bool has_performance;
    TrainlogExercise exercise;
    TrainlogLoadMode performance_load_mode;
    TrainlogTrackingMode performance_tracking_mode;
    int performance_dose;
    char performance_equipment_id[TRAINLOG_ID_MAX + 1U];
    char performance_equipment_label[TRAINLOG_NAME_MAX + 1U];
    TrainlogExercisePerformancePoint
        performance[TRAINLOG_DASHBOARD_PERFORMANCE_CAPACITY];
    size_t performance_count;
    bool has_explicit_max;
    TrainlogExercise max_exercise;
    double max_weight_kg;
    char max_timestamp[TRAINLOG_TIMESTAMP_MAX + 1U];
    bool has_body;
    size_t body_metric;
    TrainlogBodyMetricPoint body[TRAINLOG_DASHBOARD_BODY_CAPACITY];
    size_t body_count;
    TrainlogDashboardBucket weeks[TRAINLOG_DASHBOARD_BUCKET_CAPACITY];
    size_t week_count;
    size_t completed_session_count;
    size_t performed_set_count;
    size_t distinct_exercise_count;
    size_t explicit_max_count;
    /* CONTRACT: current canonical catalogue, primary zone only; aliases and
     * secondary zones never contribute. Missing primary zones are explicit. */
    TrainlogDashboardZoneBucket zones[TRAINLOG_DASHBOARD_ZONE_CAPACITY];
    size_t zone_count;
    TrainlogDashboardPeriod period;
} TrainlogDashboardSnapshot;

/*
 * WHY: all interfaces must consume one canonical Dashboard projection.
 * CONTRACT: database/query/snapshot are borrowed and required. The function
 * initializes the complete caller-owned snapshot and retains no pointer or
 * SQLite resource. Invalid periods return INVALID_ARGUMENT. A non-OK result
 * sets snapshot->error; partial and invalid_data remain independent quality
 * flags preserving the frozen legacy projection.
 * INVARIANT: capacities and ordering are part of
 * WEB_DASHBOARD_CHARACTERIZATION_V1 and must not change implicitly.
 */
TrainlogStatus trainlog_dashboard_load(
    TrainlogDatabase *database,
    const TrainlogDashboardQuery *query,
    TrainlogDashboardSnapshot *snapshot
);

#endif
