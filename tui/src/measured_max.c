/**
 * @file measured_max.c
 * @brief Explicit measured-max and working-load implementation.
 */

#include "trainlog/measured_max.h"

#include <math.h>
#include <string.h>

static bool measured_point_better(
    const TrainlogExercisePerformancePoint *candidate,
    const TrainlogExercisePerformancePoint *current
)
{
    if (
        candidate == NULL ||
        candidate->has_performance == 0
    ) {
        return false;
    }

    if (
        current == NULL ||
        current->has_performance == 0
    ) {
        return true;
    }

    if (
        candidate->load_mode !=
        current->load_mode
    ) {
        return false;
    }

    switch (candidate->load_mode) {
    case TRAINLOG_LOAD_EXTERNAL:
        if (candidate->has_weight == 0) {
            return false;
        }

        if (
            candidate->weight_kg >
            current->weight_kg
        ) {
            return true;
        }

        return
            candidate->weight_kg ==
                current->weight_kg &&
            candidate->metric_value >
                current->metric_value;

    case TRAINLOG_LOAD_ASSISTANCE:
        if (candidate->has_weight == 0) {
            return false;
        }

        if (
            candidate->weight_kg <
            current->weight_kg
        ) {
            return true;
        }

        return
            candidate->weight_kg ==
                current->weight_kg &&
            candidate->metric_value >
                current->metric_value;

    case TRAINLOG_LOAD_NONE:
    default:
        return
            candidate->metric_value >
            current->metric_value;
    }
}

TrainlogStatus trainlog_measured_max_summarize(
    const TrainlogExercisePerformancePoint *points,
    size_t count,
    TrainlogMeasuredMaxSummary *output
)
{
    size_t index;

    if (
        output == NULL ||
        (
            count > 0U &&
            points == NULL
        )
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(
        output,
        0,
        sizeof(*output)
    );

    for (
        index = 0U;
        index < count;
        ++index
    ) {
        const TrainlogExercisePerformancePoint *point =
            &points[index];

        if (
            point->session_type !=
            TRAINLOG_SESSION_MAX_TEST
        ) {
            continue;
        }

        ++output->test_count;

        if (point->has_performance == 0) {
            continue;
        }

        ++output->successful_test_count;

        if (!output->found) {
            output->found = true;
            output->current = *point;
            output->has_record = true;
            output->record = *point;
            continue;
        }

        if (
            point->load_mode ==
                output->current.load_mode &&
            measured_point_better(
                point,
                &output->record
            )
        ) {
            output->record = *point;
        }
    }

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_measured_max_working_load(
    const TrainlogExercisePerformancePoint *measured,
    double percentage,
    double increment_kg,
    double *output_kg
)
{
    double raw;
    double rounded;

    if (
        measured == NULL ||
        output_kg == NULL ||
        measured->session_type !=
            TRAINLOG_SESSION_MAX_TEST ||
        measured->has_performance == 0 ||
        measured->load_mode !=
            TRAINLOG_LOAD_EXTERNAL ||
        measured->has_weight == 0 ||
        !isfinite(measured->weight_kg) ||
        measured->weight_kg <= 0.0 ||
        !isfinite(percentage) ||
        percentage <= 0.0 ||
        percentage > 100.0 ||
        !isfinite(increment_kg) ||
        increment_kg <= 0.0
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    raw =
        measured->weight_kg *
        percentage /
        100.0;

    rounded =
        floor(
            raw /
            increment_kg +
            0.5
        ) *
        increment_kg;

    if (rounded <= 0.0) {
        rounded =
            increment_kg;
    }

    *output_kg = rounded;

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_measured_max_target_load(
    const TrainlogLatestExplicitMax *latest,
    const char *equipment_id,
    TrainlogLoadMode equipment_load_semantics,
    int percent,
    double *output_kg
)
{
    double result;
    /* WHY: %MAX is an arithmetic shortcut explicitly chosen by the user, not
     * a training recommendation. CONTRACT: only the newest explicit MAX for
     * this exercise and exact equipment context may cross into the calculator;
     * assistance and missing/unknown contexts are rejected. */
    if (latest == NULL || equipment_id == NULL || output_kg == NULL ||
        !latest->found || equipment_id[0] == '\0' ||
        strcmp(latest->equipment_id, equipment_id) != 0 ||
        equipment_load_semantics != TRAINLOG_LOAD_EXTERNAL ||
        !isfinite(latest->max_weight_kg) || latest->max_weight_kg <= 0.0 ||
        percent < 1 || percent > 100)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    result = latest->max_weight_kg * (double)percent / 100.0;
    if (!isfinite(result) || result <= 0.0)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    /* INVARIANT: callers persist only this resulting target weight. The MAX
     * identity and selected percentage remain transient UI state. */
    *output_kg = result;
    return TRAINLOG_STATUS_OK;
}
