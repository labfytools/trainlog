/**
 * @file sync_screen_action.c
 * @brief Backend-independent sync-screen keyboard action dispatcher.
 */

#include "trainlog/sync_screen_action.h"

#include "trainlog/terminal.h"

static TrainlogSyncScreenAction sync_screen_action(
    TrainlogSyncScreenEffect effect,
    TrainlogSyncDirection direction
)
{
    TrainlogSyncScreenAction action;

    action.effect = effect;
    action.direction = direction;
    return action;
}

void trainlog_sync_screen_state_init(
    TrainlogSyncScreenState *state
)
{
    if (state == NULL) {
        return;
    }

    state->confirming = false;
    state->direction = TRAINLOG_SYNC_BIDIRECTIONAL;
}

TrainlogSyncScreenAction trainlog_sync_screen_dispatch(
    TrainlogSyncScreenState *state,
    int key
)
{
    TrainlogSyncDirection direction;

    if (state == NULL) {
        return sync_screen_action(
            TRAINLOG_SYNC_SCREEN_NONE,
            TRAINLOG_SYNC_BIDIRECTIONAL
        );
    }

    direction = state->direction;

    if (key == 'a' || key == 'A') {
        direction = TRAINLOG_SYNC_ANDROID_TO_PC;
    } else if (key == 'p' || key == 'P') {
        direction = TRAINLOG_SYNC_PC_TO_ANDROID;
    } else if (key == 'b' || key == 'B') {
        direction = TRAINLOG_SYNC_BIDIRECTIONAL;
    } else if (state->confirming) {
        if (key == '\n' || key == TRAINLOG_KEY_ENTER) {
            /* CONTRACT: consuming confirmation before execution guarantees
             * that one Enter event can yield at most one sync-engine call. */
            state->confirming = false;
            return sync_screen_action(
                TRAINLOG_SYNC_SCREEN_RUN,
                state->direction
            );
        }

        if (key == 27) {
            state->confirming = false;
            return sync_screen_action(
                TRAINLOG_SYNC_SCREEN_CANCEL,
                state->direction
            );
        }

        return sync_screen_action(
            TRAINLOG_SYNC_SCREEN_NONE,
            state->direction
        );
    } else if (key == 'r' || key == 'R') {
        return sync_screen_action(
            TRAINLOG_SYNC_SCREEN_REFRESH,
            state->direction
        );
    } else if (key == 27) {
        return sync_screen_action(
            TRAINLOG_SYNC_SCREEN_EXIT,
            state->direction
        );
    } else {
        /* The former s shortcut is deliberately not an action. */
        return sync_screen_action(
            TRAINLOG_SYNC_SCREEN_NONE,
            state->direction
        );
    }

    state->direction = direction;
    state->confirming = true;
    return sync_screen_action(
        TRAINLOG_SYNC_SCREEN_CONFIRM,
        direction
    );
}

TrainlogStatus trainlog_sync_screen_execute(
    const TrainlogSyncScreenAction *action,
    TrainlogSyncScreenRunCallback callback,
    void *context,
    TrainlogSyncReport *report
)
{
    if (
        action == NULL ||
        action->effect != TRAINLOG_SYNC_SCREEN_RUN ||
        callback == NULL ||
        report == NULL
    ) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    return callback(
        action->direction,
        report,
        context
    );
}

const char *trainlog_sync_screen_direction_label(
    TrainlogSyncDirection direction
)
{
    if (direction == TRAINLOG_SYNC_ANDROID_TO_PC) {
        return "Android→PC";
    }

    if (direction == TRAINLOG_SYNC_PC_TO_ANDROID) {
        return "PC→Android";
    }

    return "PC↔Android";
}

const char *trainlog_sync_screen_confirmation(
    TrainlogSyncDirection direction
)
{
    if (direction == TRAINLOG_SYNC_ANDROID_TO_PC) {
        return "Importer Android vers le PC maintenant ?";
    }

    if (direction == TRAINLOG_SYNC_PC_TO_ANDROID) {
        return "Publier le catalogue du PC vers Android maintenant ?";
    }

    return "Synchroniser Android et PC dans les deux sens maintenant ?";
}

const char *trainlog_sync_screen_footer(
    int terminal_columns
)
{
    if (terminal_columns >= 100) {
        return "a Android→PC  p PC→Android  b PC↔Android  r actualiser  Échap retour";
    }

    return "a A→PC  p PC→A  b A↔PC  r act.  Échap retour";
}
