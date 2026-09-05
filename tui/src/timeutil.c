/**
 * @file timeutil.c
 * @brief Local RFC3339 timestamp generation.
 */

#include "trainlog/timeutil.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

TrainlogStatus trainlog_time_now_rfc3339(
    char *output,
    size_t output_size
)
{
    time_t now;
    struct tm local;
    char date_part[32];
    char offset_part[8];
    char offset_with_colon[7];
    int written;

    if (output == NULL || output_size < 26U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (localtime_r(&now, &local) == NULL) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (strftime(
            date_part,
            sizeof(date_part),
            "%Y-%m-%dT%H:%M:%S",
            &local
        ) == 0U) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (strftime(
            offset_part,
            sizeof(offset_part),
            "%z",
            &local
        ) != 5U) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    offset_with_colon[0] = offset_part[0];
    offset_with_colon[1] = offset_part[1];
    offset_with_colon[2] = offset_part[2];
    offset_with_colon[3] = ':';
    offset_with_colon[4] = offset_part[3];
    offset_with_colon[5] = offset_part[4];
    offset_with_colon[6] = '\0';

    written = snprintf(
        output,
        output_size,
        "%s%s",
        date_part,
        offset_with_colon
    );

    if (written < 0 || (size_t)written >= output_size) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    return TRAINLOG_STATUS_OK;
}
