#ifndef TRAINLOG_TIMEUTIL_H
#define TRAINLOG_TIMEUTIL_H

/**
 * @file timeutil.h
 * @brief RFC3339 local timestamp helpers.
 */

#include <stddef.h>
#include <stdbool.h>

#include "trainlog/status.h"

/**
 * @brief Write the current local instant with an explicit numeric UTC offset.
 *
 * Example: `2026-09-05T19:42:01+02:00`.
 */
TrainlogStatus trainlog_time_now_rfc3339(
    char *output,
    size_t output_size
);

/* CONTRACT: exercise observations before end use "Pendant la séance" and a
 * missing anchor uses "Ressenti"; follow-ups use H+? in both cases. No caller
 * can receive a negative H+ label. */
TrainlogStatus trainlog_feedback_relative_label(
    const char *ended_at, const char *observed_at, bool exercise_feedback,
    char *output, size_t output_size);

#endif
