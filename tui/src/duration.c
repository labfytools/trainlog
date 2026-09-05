/**
 * @file duration.c
 * @brief Human duration parser/formatter implementation.
 */

#include "trainlog/duration.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool ascii_space(char value)
{
    return value == ' ' ||
           value == '\t' ||
           value == '\n' ||
           value == '\r' ||
           value == '\f' ||
           value == '\v';
}

static bool ascii_digit(char value)
{
    return value >= '0' && value <= '9';
}

static bool parse_decimal_range(
    const char *begin,
    const char *end,
    int *output
)
{
    long long value = 0;
    const char *cursor;

    if (begin == NULL ||
        end == NULL ||
        output == NULL ||
        begin >= end) {
        return false;
    }

    for (cursor = begin; cursor < end; ++cursor) {
        int digit;

        if (!ascii_digit(*cursor)) {
            return false;
        }

        digit = *cursor - '0';

        if (value > ((long long)INT_MAX - digit) / 10LL) {
            return false;
        }

        value = (value * 10LL) + digit;
    }

    *output = (int)value;
    return true;
}

static bool checked_minutes_seconds(
    int minutes,
    int seconds,
    int *output
)
{
    long long total;

    if (minutes < 0 ||
        seconds < 0 ||
        seconds >= 60 ||
        output == NULL) {
        return false;
    }

    total = ((long long)minutes * 60LL) + seconds;

    if (total > (long long)INT_MAX) {
        return false;
    }

    *output = (int)total;
    return true;
}

TrainlogStatus trainlog_duration_parse(
    const char *text,
    int *output_seconds
)
{
    const char *begin;
    const char *end;
    const char *cursor;
    const char *colon = NULL;
    const char *minute_marker = NULL;
    int value;

    if (text == NULL || output_seconds == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    begin = text;
    while (*begin != '\0' && ascii_space(*begin)) {
        ++begin;
    }

    end = text + strlen(text);
    while (end > begin && ascii_space(end[-1])) {
        --end;
    }

    if (begin == end ||
        (size_t)(end - begin) > 63U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    for (cursor = begin; cursor < end; ++cursor) {
        if (ascii_space(*cursor)) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        if (*cursor == ':') {
            if (colon != NULL) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
            colon = cursor;
        }

        if (*cursor == 'm' || *cursor == 'M') {
            if (minute_marker != NULL) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
            minute_marker = cursor;
        }
    }

    if (colon != NULL) {
        int minutes;
        int seconds;

        if (minute_marker != NULL ||
            !parse_decimal_range(begin, colon, &minutes) ||
            !parse_decimal_range(colon + 1, end, &seconds) ||
            !checked_minutes_seconds(
                minutes,
                seconds,
                output_seconds
            )) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        return TRAINLOG_STATUS_OK;
    }

    if (minute_marker != NULL) {
        int minutes;
        int seconds = 0;
        const char *seconds_begin = minute_marker + 1;
        const char *seconds_end = end;

        if (!parse_decimal_range(
                begin,
                minute_marker,
                &minutes
            )) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        if (seconds_begin < end &&
            (end[-1] == 's' || end[-1] == 'S')) {
            seconds_end = end - 1;
        }

        if (seconds_begin < seconds_end) {
            if (!parse_decimal_range(
                    seconds_begin,
                    seconds_end,
                    &seconds
                )) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
        } else if (seconds_begin < end) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        if (!checked_minutes_seconds(
                minutes,
                seconds,
                output_seconds
            )) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        return TRAINLOG_STATUS_OK;
    }

    if (end[-1] == 's' || end[-1] == 'S') {
        if (!parse_decimal_range(
                begin,
                end - 1,
                &value
            )) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        *output_seconds = value;
        return TRAINLOG_STATUS_OK;
    }

    if (!parse_decimal_range(begin, end, &value)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_seconds = value;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_duration_format(
    int seconds,
    char *output,
    size_t output_size
)
{
    int minutes;
    int remainder;
    int written;

    if (seconds < 0 ||
        output == NULL ||
        output_size == 0U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    minutes = seconds / 60;
    remainder = seconds % 60;

    if (minutes == 0) {
        written = snprintf(
            output,
            output_size,
            "%d s",
            remainder
        );
    } else if (remainder == 0) {
        written = snprintf(
            output,
            output_size,
            "%d min",
            minutes
        );
    } else {
        written = snprintf(
            output,
            output_size,
            "%d min %d s",
            minutes,
            remainder
        );
    }

    if (written < 0 ||
        (size_t)written >= output_size) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    return TRAINLOG_STATUS_OK;
}
