#ifndef TRAINLOG_ID_H
#define TRAINLOG_ID_H

/**
 * @file id.h
 * @brief Collision-resistant Trainlog identifier generation.
 */

#include <stddef.h>

#include "trainlog/status.h"

/** UUID text length without a terminating NUL byte. */
#define TRAINLOG_UUID_TEXT_LENGTH 36U

/**
 * Longest official v1 generated identifier:
 * two-character type prefix + '_' + UUID text + terminating NUL.
 */
#define TRAINLOG_GENERATED_ID_CAPACITY \
    (2U + 1U + TRAINLOG_UUID_TEXT_LENGTH + 1U)

/**
 * @brief Generate an official Trainlog v1 identifier using UUID version 4.
 *
 * The frozen v1 contract requires official creators to generate identifiers
 * in the form `<prefix>_<uuid-v4>`. The function accepts only the currently
 * reserved two-character prefixes:
 *
 * - `ex` for exercises;
 * - `se` for sessions;
 * - `bo` for body observations.
 *
 * Imported v1 documents may contain other schema-valid opaque identifiers;
 * this API defines creation policy, not import validation.
 *
 * @param prefix Two-character Trainlog object prefix.
 * @param output Caller-owned output buffer.
 * @param output_size Size of @p output in bytes.
 *
 * @return TRAINLOG_STATUS_OK on success.
 * @return TRAINLOG_STATUS_INVALID_ARGUMENT for invalid pointers, prefix, or a
 *         buffer smaller than TRAINLOG_GENERATED_ID_CAPACITY.
 */
TrainlogStatus trainlog_id_generate(
    const char *prefix,
    char *output,
    size_t output_size
);

#endif
