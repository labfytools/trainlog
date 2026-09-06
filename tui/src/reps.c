/**
 * @file reps.c
 * @brief Bounded parser for Trainlog repetition-set shorthand.
 */

#include "trainlog/reps.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRAINLOG_REPS_TEXT_MAX 511U
#define TRAINLOG_REPS_VALUE_MAX 10000

static bool parse_token(
    const char *start,
    size_t length,
    int *output
)
{
    char buffer[32];
    char *end = NULL;
    long value;

    if (
        start == NULL ||
        output == NULL ||
        length == 0U ||
        length >= sizeof(buffer)
    ) {
        return false;
    }

    (void)memcpy(
        buffer,
        start,
        length
    );

    buffer[length] = '\0';

    value =
        strtol(
            buffer,
            &end,
            10
        );

    if (
        end == buffer ||
        *end != '\0' ||
        value < 0L ||
        value > TRAINLOG_REPS_VALUE_MAX
    ) {
        return false;
    }

    *output = (int)value;
    return true;
}

static bool compact_text(
    const char *text,
    char output[TRAINLOG_REPS_TEXT_MAX + 1U]
)
{
    size_t read_index;
    size_t write_index = 0U;

    if (
        text == NULL ||
        output == NULL
    ) {
        return false;
    }

    for (
        read_index = 0U;
        text[read_index] != '\0';
        ++read_index
    ) {
        unsigned char value =
            (unsigned char)text[read_index];

        if (isspace(value) != 0) {
            continue;
        }

        if (
            write_index >=
            TRAINLOG_REPS_TEXT_MAX
        ) {
            return false;
        }

        output[write_index] =
            (char)value;

        ++write_index;
    }

    output[write_index] = '\0';

    return write_index > 0U;
}

static TrainlogStatus parse_repeat(
    char *text,
    int *output,
    size_t capacity,
    size_t *output_count
)
{
    char *separator =
        strchr(
            text,
            'x'
        );

    int count;
    int reps;
    size_t index;

    if (separator == NULL) {
        separator =
            strchr(
                text,
                'X'
            );
    }

    if (separator == NULL) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    if (
        strchr(
            separator + 1,
            'x'
        ) != NULL ||
        strchr(
            separator + 1,
            'X'
        ) != NULL
    ) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *separator = '\0';

    if (
        !parse_token(
            text,
            strlen(text),
            &count
        ) ||
        !parse_token(
            separator + 1,
            strlen(separator + 1),
            &reps
        ) ||
        count < 1 ||
        (size_t)count > capacity
    ) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    for (
        index = 0U;
        index < (size_t)count;
        ++index
    ) {
        output[index] = reps;
    }

    *output_count = (size_t)count;

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus parse_pyramid(
    char *text,
    int *output,
    size_t capacity,
    size_t *output_count
)
{
    char *first =
        strstr(
            text,
            ".."
        );

    char *second;
    int start;
    int peak;
    int end;
    size_t count = 0U;
    int value;

    if (first == NULL) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    second =
        strstr(
            first + 2,
            ".."
        );

    if (
        second == NULL ||
        strstr(
            second + 2,
            ".."
        ) != NULL
    ) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *first = '\0';
    *second = '\0';

    if (
        !parse_token(
            text,
            strlen(text),
            &start
        ) ||
        !parse_token(
            first + 2,
            strlen(first + 2),
            &peak
        ) ||
        !parse_token(
            second + 2,
            strlen(second + 2),
            &end
        ) ||
        start > peak ||
        end > peak
    ) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    for (
        value = start;
        value <= peak;
        ++value
    ) {
        if (count >= capacity) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        output[count] = value;
        ++count;
    }

    if (peak > end) {
        for (
            value = peak - 1;
            value >= end;
            --value
        ) {
            if (count >= capacity) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }

            output[count] = value;
            ++count;
        }
    }

    if (count == 0U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = count;

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus parse_explicit(
    const char *text,
    int *output,
    size_t capacity,
    size_t *output_count
)
{
    const char *cursor = text;
    const char *token_start = text;
    size_t count = 0U;

    for (;;) {
        if (
            *cursor == ',' ||
            *cursor == ';' ||
            *cursor == '\0'
        ) {
            int reps;

            if (
                count >= capacity ||
                !parse_token(
                    token_start,
                    (size_t)(
                        cursor -
                        token_start
                    ),
                    &reps
                )
            ) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }

            output[count] = reps;
            ++count;

            if (*cursor == '\0') {
                break;
            }

            token_start =
                cursor + 1;
        }

        ++cursor;
    }

    if (count == 0U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = count;

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_reps_parse_sequence(
    const char *text,
    int *output,
    size_t capacity,
    size_t *output_count
)
{
    char compact[
        TRAINLOG_REPS_TEXT_MAX + 1U
    ];

    char working[
        TRAINLOG_REPS_TEXT_MAX + 1U
    ];

    TrainlogStatus status;

    if (
        text == NULL ||
        output_count == NULL ||
        capacity == 0U ||
        output == NULL
    ) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = 0U;

    if (
        !compact_text(
            text,
            compact
        )
    ) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)snprintf(
        working,
        sizeof(working),
        "%s",
        compact
    );

    status =
        parse_repeat(
            working,
            output,
            capacity,
            output_count
        );

    if (
        status !=
        TRAINLOG_STATUS_NOT_FOUND
    ) {
        return status;
    }

    (void)snprintf(
        working,
        sizeof(working),
        "%s",
        compact
    );

    status =
        parse_pyramid(
            working,
            output,
            capacity,
            output_count
        );

    if (
        status !=
        TRAINLOG_STATUS_NOT_FOUND
    ) {
        return status;
    }

    return
        parse_explicit(
            compact,
            output,
            capacity,
            output_count
        );
}
