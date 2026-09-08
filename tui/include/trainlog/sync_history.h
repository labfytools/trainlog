#ifndef TRAINLOG_SYNC_HISTORY_H
#define TRAINLOG_SYNC_HISTORY_H

/**
 * @file sync_history.h
 * @brief Local, backward-compatible synchronization history records.
 *
 * This format is local diagnostic state. It is not a synchronization
 * exchange artifact and has no effect on TRAINLOG_FORMAT_V1.
 */

#include <stdbool.h>

#include "trainlog/sync.h"

typedef struct TrainlogSyncHistoryEntry {
    char sync_id[TRAINLOG_ID_MAX + 1U];
    char timestamp[17];
    bool success;
    bool direction_known;
    TrainlogSyncDirection direction;
    char summary[TRAINLOG_SYNC_SUMMARY_MAX + 1U];
} TrainlogSyncHistoryEntry;

/** Parse one complete history line. New records require an a/p/b field. */
bool trainlog_sync_history_parse_line(
    const char *line,
    TrainlogSyncHistoryEntry *output
);

/** Return the stable one-character local-history encoding for a direction. */
const char *trainlog_sync_history_direction_code(
    TrainlogSyncDirection direction
);

/** Return the user-facing label, including unknown for legacy records. */
const char *trainlog_sync_history_direction_label(
    const TrainlogSyncHistoryEntry *entry
);

/**
 * Persist a completed local run record, structured JSON, and text detail.
 * Exposed for deterministic local-history regression coverage; no MTP work is
 * performed by this function.
 */
bool trainlog_sync_record_local_run(
    TrainlogSyncTrigger trigger,
    const TrainlogSyncReport *report
);

#endif
