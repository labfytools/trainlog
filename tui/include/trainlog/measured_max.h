#ifndef TRAINLOG_MEASURED_MAX_H
#define TRAINLOG_MEASURED_MAX_H

/**
 * @file measured_max.h
 * @brief Explicit measured-max classification and working-load calculations.
 */

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

typedef struct TrainlogMeasuredMaxSummary {
    size_t test_count;
    size_t successful_test_count;
    bool found;
    TrainlogExercisePerformancePoint current;
    bool has_record;
    TrainlogExercisePerformancePoint record;
} TrainlogMeasuredMaxSummary;

/**
 * @brief Summarize explicit max-test sessions from newest-first performance.
 *
 * Only points whose session_type is TRAINLOG_SESSION_MAX_TEST participate.
 * Ordinary training performance is never promoted to a measured maximum.
 *
 * current is the newest successful explicit max-test result.
 * record is the best successful max-test result using the same load mode as
 * current:
 *
 * - external: greatest actual load, tie -> greatest reps/duration;
 * - assistance: lowest assistance, tie -> greatest reps/duration;
 * - no load: greatest reps/duration.
 *
 * No estimated 1RM is calculated.
 */
TrainlogStatus trainlog_measured_max_summarize(
    const TrainlogExercisePerformancePoint *points,
    size_t count,
    TrainlogMeasuredMaxSummary *output
);

/**
 * @brief Calculate a working load from the current measured external load.
 *
 * The result is percentage of the measured load rounded to the nearest
 * positive increment. This calculation is deliberately unavailable for
 * assistance and no-load performance.
 */
TrainlogStatus trainlog_measured_max_working_load(
    const TrainlogExercisePerformancePoint *measured,
    double percentage,
    double increment_kg,
    double *output_kg
);

/**
 * @brief Calculate a user-directed target from one compatible explicit MAX.
 *
 * Compatibility is exact exercise ownership (established by the caller's
 * exercise-scoped latest-max query), exact nonempty equipment ID, and external
 * resistance. percent must be the integer range 1..100. No rounding,
 * recommendation, or percentage metadata is persisted by this function.
 */
TrainlogStatus trainlog_measured_max_target_load(
    const TrainlogLatestExplicitMax *latest,
    const char *equipment_id,
    TrainlogLoadMode equipment_load_semantics,
    int percent,
    double *output_kg
);

#endif
