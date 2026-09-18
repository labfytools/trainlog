/* Trainlog Web prepared-item V1 read model. */
#ifndef TRAINLOG_WEB_PREPARED_ITEMS_H
#define TRAINLOG_WEB_PREPARED_ITEMS_H

#include <stddef.h>

#include "trainlog/database.h"

#define TRAINLOG_WEB_PREPARED_ITEM_CAPACITY 32U
#define TRAINLOG_WEB_PREPARED_ITEMS_JSON_CAPACITY (64U * 1024U)

typedef enum TrainlogWebPreparedItemKind {
    TRAINLOG_WEB_PREPARED_AI_PROPOSAL = 0,
    TRAINLOG_WEB_PREPARED_EXECUTION_DRAFT = 1,
    TRAINLOG_WEB_PREPARED_MANUAL_PREPARATION = 2
} TrainlogWebPreparedItemKind;

typedef struct TrainlogWebPreparedItem {
    TrainlogWebPreparedItemKind kind;
    char identity[TRAINLOG_ID_MAX + 1U];
    char state[32];
    char title[TRAINLOG_NAME_MAX + 1U];
    char planned_for[11];
    char sort_timestamp[TRAINLOG_TIMESTAMP_MAX + 1U];
    size_t occurrence_count;
    char provenance[32];
} TrainlogWebPreparedItem;

typedef struct TrainlogWebPreparedItems {
    char generated_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogWebPreparedItem items[TRAINLOG_WEB_PREPARED_ITEM_CAPACITY];
    size_t item_count;
    bool partial;
} TrainlogWebPreparedItems;

/* WHY: sync status is one run's evidence, not a durable inventory.
 * CONTRACT: database/output are required and borrowed for the call. The
 * projection is bounded, deterministic, read-only, and keeps AI proposals
 * separate from executable drafts and completed sessions.
 * INVARIANT: targets never become performed occurrences through this API. */
TrainlogStatus trainlog_web_prepared_items_load(
    TrainlogDatabase *database, TrainlogWebPreparedItems *output);

/* Caller owns output. Success guarantees valid NUL-terminated JSON. */
TrainlogStatus trainlog_web_prepared_items_serialize(
    const TrainlogWebPreparedItems *items,
    char *output,
    size_t capacity,
    size_t *output_size);

#endif
