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
 * Longest official generated identifier:
 * three-character type prefix + '_' + UUID text + terminating NUL.
 *
 * CONTRACT: occurrence identities use the longer `sxe` prefix while the
 * frozen v1 object identities retain their existing two-character prefixes.
 */
#define TRAINLOG_GENERATED_ID_CAPACITY \
    (3U + 1U + TRAINLOG_UUID_TEXT_LENGTH + 1U)

/**
 * @brief Generate an official Trainlog identifier using UUID version 4.
 *
 * Official creators generate identifiers in the form
 * `<prefix>_<uuid-v4>`. The function accepts only the currently reserved
 * prefixes:
 *
 * - `ex` for exercises;
 * - `se` for sessions;
 * - `bo` for body observations.
 * - `sy` for synchronization runs;
 * - `sxe` for persisted session-exercise occurrences.
 *
 * Imported v1 documents may contain other schema-valid opaque identifiers;
 * this API defines creation policy, not import validation.
 *
 * @param prefix Reserved Trainlog object prefix.
 * @param output Caller-owned output buffer.
 * @param output_size Size of @p output in bytes.
 *
 * @return TRAINLOG_STATUS_OK on success.
 * @return TRAINLOG_STATUS_INVALID_ARGUMENT for invalid pointers, prefix, or a
 *         buffer smaller than that prefix's exact identifier representation.
 *         Existing two-character prefixes still require 40 bytes; `sxe`
 *         requires TRAINLOG_GENERATED_ID_CAPACITY bytes.
 */
TrainlogStatus trainlog_id_generate(
    const char *prefix,
    char *output,
    size_t output_size
);

#endif
