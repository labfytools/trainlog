#ifndef TRAINLOG_SYNC_SCREEN_ACTION_H
#define TRAINLOG_SYNC_SCREEN_ACTION_H

/**
 * @file sync_screen_action.h
 * @brief TUI-owned sync-screen keyboard actions and confirmation state.
 */

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/status.h"
#include "trainlog/sync.h"

typedef enum TrainlogSyncScreenEffect {
    TRAINLOG_SYNC_SCREEN_NONE = 0,
    TRAINLOG_SYNC_SCREEN_CONFIRM,
    TRAINLOG_SYNC_SCREEN_RUN,
    TRAINLOG_SYNC_SCREEN_CANCEL,
    TRAINLOG_SYNC_SCREEN_REFRESH,
    TRAINLOG_SYNC_SCREEN_EXIT
} TrainlogSyncScreenEffect;

typedef struct TrainlogSyncScreenState {
    bool confirming;
    TrainlogSyncDirection direction;
} TrainlogSyncScreenState;

typedef struct TrainlogSyncScreenAction {
    TrainlogSyncScreenEffect effect;
    TrainlogSyncDirection direction;
} TrainlogSyncScreenAction;

typedef TrainlogStatus (*TrainlogSyncScreenRunCallback)(
    TrainlogSyncDirection direction,
    TrainlogSyncReport *report,
    void *context
);

void trainlog_sync_screen_state_init(
    TrainlogSyncScreenState *state
);

TrainlogSyncScreenAction trainlog_sync_screen_dispatch(
    TrainlogSyncScreenState *state,
    int key
);

TrainlogStatus trainlog_sync_screen_execute(
    const TrainlogSyncScreenAction *action,
    TrainlogSyncScreenRunCallback callback,
    void *context,
    TrainlogSyncReport *report
);

const char *trainlog_sync_screen_direction_label(
    TrainlogSyncDirection direction
);

const char *trainlog_sync_screen_confirmation(
    TrainlogSyncDirection direction
);

const char *trainlog_sync_screen_footer(
    int terminal_columns
);

#endif
