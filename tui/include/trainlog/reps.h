#ifndef TRAINLOG_REPS_H
#define TRAINLOG_REPS_H

#include <stddef.h>

#include "trainlog/status.h"

/**
 * @brief Parse one bounded repetition sequence.
 *
 * Accepted forms:
 *
 * - 5x10        -> 10,10,10,10,10
 * - 4,5,6,7    -> explicit ordered sets
 * - 4..10..4   -> ascending then descending pyramid
 *
 * Repetitions are bounded to 0..10000 and output is never truncated.
 */
TrainlogStatus trainlog_reps_parse_sequence(
    const char *text,
    int *output,
    size_t capacity,
    size_t *output_count
);

#endif
