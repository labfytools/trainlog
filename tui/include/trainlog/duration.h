#ifndef TRAINLOG_DURATION_H
#define TRAINLOG_DURATION_H

/**
 * @file duration.h
 * @brief Human duration parsing and formatting for Trainlog.
 */

#include <stddef.h>

#include "trainlog/status.h"

TrainlogStatus trainlog_duration_parse(
    const char *text,
    int *output_seconds
);

TrainlogStatus trainlog_duration_format(
    int seconds,
    char *output,
    size_t output_size
);

#endif
