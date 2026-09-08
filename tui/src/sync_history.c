/**
 * @file sync_history.c
 * @brief Strict codec for local synchronization history lines.
 */

#include "trainlog/sync_history.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define SYNC_HISTORY_LINE_MAX 767U

static bool sync_history_timestamp_valid(const char *timestamp)
{
    size_t index;

    if (timestamp == NULL || strlen(timestamp) != 16U) {
        return false;
    }

    for (index = 0U; index < 16U; ++index) {
        char expected = '\0';

        if (index == 2U || index == 5U) {
            expected = '/';
        } else if (index == 10U) {
            expected = ' ';
        } else if (index == 13U) {
            expected = ':';
        }

        if (
            (expected != '\0' && timestamp[index] != expected) ||
            (expected == '\0' && !isdigit((unsigned char)timestamp[index]))
        ) {
            return false;
        }
    }

    return true;
}

const char *trainlog_sync_history_direction_code(
    TrainlogSyncDirection direction
)
{
    switch (direction) {
    case TRAINLOG_SYNC_ANDROID_TO_PC:
        return "a";
    case TRAINLOG_SYNC_PC_TO_ANDROID:
        return "p";
    case TRAINLOG_SYNC_BIDIRECTIONAL:
        return "b";
    default:
        return NULL;
    }
}

static bool sync_history_direction_parse(
    const char *text,
    TrainlogSyncDirection *output
)
{
    if (text == NULL || output == NULL || text[1] != '\0') {
        return false;
    }

    if (text[0] == 'a') {
        *output = TRAINLOG_SYNC_ANDROID_TO_PC;
    } else if (text[0] == 'p') {
        *output = TRAINLOG_SYNC_PC_TO_ANDROID;
    } else if (text[0] == 'b') {
        *output = TRAINLOG_SYNC_BIDIRECTIONAL;
    } else {
        return false;
    }

    return true;
}

const char *trainlog_sync_history_direction_label(
    const TrainlogSyncHistoryEntry *entry
)
{
    if (entry == NULL || !entry->direction_known) {
        /* Legacy records predate the direction field; summary text is not a
         * trustworthy substitute, especially for failure diagnostics. */
        return "direction inconnue";
    }

    switch (entry->direction) {
    case TRAINLOG_SYNC_ANDROID_TO_PC:
        return "Android→PC";
    case TRAINLOG_SYNC_PC_TO_ANDROID:
        return "PC→Android";
    case TRAINLOG_SYNC_BIDIRECTIONAL:
        return "PC↔Android";
    default:
        return "direction inconnue";
    }
}

bool trainlog_sync_history_parse_line(
    const char *line,
    TrainlogSyncHistoryEntry *output
)
{
    char copy[SYNC_HISTORY_LINE_MAX + 1U];
    char *fields[5];
    char *cursor;
    size_t field_count = 1U;
    size_t length;
    bool has_sync_id;
    bool has_direction;
    const char *sync_id;
    const char *timestamp;
    const char *status;
    const char *summary;

    if (line == NULL || output == NULL) {
        return false;
    }

    length = strlen(line);
    while (length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r')) {
        --length;
    }
    if (length == 0U || length > SYNC_HISTORY_LINE_MAX) {
        return false;
    }

    (void)memcpy(copy, line, length);
    copy[length] = '\0';
    fields[0] = copy;
    for (cursor = copy; *cursor != '\0'; ++cursor) {
        if (*cursor == '\t') {
            if (field_count == sizeof(fields) / sizeof(fields[0])) {
                return false;
            }
            *cursor = '\0';
            fields[field_count] = cursor + 1;
            ++field_count;
        }
    }

    has_sync_id = field_count >= 4U;
    has_direction = field_count == 5U;
    if (field_count != 3U && field_count != 4U && field_count != 5U) {
        return false;
    }

    sync_id = has_sync_id ? fields[0] : "";
    timestamp = has_sync_id ? fields[1] : fields[0];
    status = has_sync_id ? fields[2] : fields[1];
    summary = fields[field_count - 1U];

    if (
        (has_sync_id && (sync_id[0] == '\0' || strlen(sync_id) > TRAINLOG_ID_MAX)) ||
        !sync_history_timestamp_valid(timestamp) ||
        (strcmp(status, "0") != 0 && strcmp(status, "1") != 0) ||
        summary[0] == '\0' || strlen(summary) > TRAINLOG_SYNC_SUMMARY_MAX
    ) {
        return false;
    }

    (void)memset(output, 0, sizeof(*output));
    (void)snprintf(output->sync_id, sizeof(output->sync_id), "%s", sync_id);
    (void)snprintf(output->timestamp, sizeof(output->timestamp), "%s", timestamp);
    output->success = strcmp(status, "1") == 0;
    (void)snprintf(output->summary, sizeof(output->summary), "%s", summary);

    if (has_direction) {
        if (!sync_history_direction_parse(fields[3], &output->direction)) {
            return false;
        }
        output->direction_known = true;
    }

    return true;
}
