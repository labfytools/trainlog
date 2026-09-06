/**
 * @file id.c
 * @brief Trainlog UUIDv4 identifier generation.
 */

#include "trainlog/id.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <uuid/uuid.h>

static bool prefix_is_supported(const char *prefix)
{
    /*
     * Keep official creator prefixes deliberately small and explicit.
     * The exchange parser remains more permissive because imported v1 IDs are
     * opaque values governed by the frozen wire-schema syntax.
     */
    return strcmp(prefix, "ex") == 0 ||
           strcmp(prefix, "se") == 0 ||
           strcmp(prefix, "bo") == 0 ||
           strcmp(prefix, "sy") == 0;
}

TrainlogStatus trainlog_id_generate(
    const char *prefix,
    char *output,
    size_t output_size
)
{
    uuid_t value;
    char uuid_text[TRAINLOG_UUID_TEXT_LENGTH + 1U];
    int written;

    if (prefix == NULL || output == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    if (!prefix_is_supported(prefix) ||
        output_size < TRAINLOG_GENERATED_ID_CAPACITY) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    /*
     * libuuid's uuid_generate_random() creates a random RFC 4122 UUID with the
     * version and variant bits set appropriately for UUID version 4.
     */
    uuid_generate_random(value);
    uuid_unparse_lower(value, uuid_text);

    written = snprintf(
        output,
        output_size,
        "%s_%s",
        prefix,
        uuid_text
    );

    if (written < 0 ||
        (size_t)written >= output_size) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    return TRAINLOG_STATUS_OK;
}
