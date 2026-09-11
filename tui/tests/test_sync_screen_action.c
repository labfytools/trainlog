#include <stdio.h>
#include <string.h>

#include "trainlog/sync_screen_action.h"

#define CHECK(value) do { \
    if (!(value)) { \
        fprintf(stderr, "FAIL line %d\n", __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct RunRecorder {
    size_t calls;
    TrainlogSyncDirection direction;
} RunRecorder;

static TrainlogStatus record_run(
    TrainlogSyncDirection direction,
    TrainlogSyncReport *report,
    void *context
)
{
    RunRecorder *recorder = context;

    ++recorder->calls;
    recorder->direction = direction;
    report->success = true;
    report->direction = direction;
    return TRAINLOG_STATUS_OK;
}

int main(void)
{
    TrainlogSyncScreenState state;
    TrainlogSyncScreenAction action;
    TrainlogSyncReport report = {0};
    RunRecorder recorder = {0};

    trainlog_sync_screen_state_init(&state);
    CHECK(trainlog_sync_screen_dispatch(&state, 'a').effect ==
        TRAINLOG_SYNC_SCREEN_NONE);
    CHECK(trainlog_sync_screen_dispatch(&state, 'p').effect ==
        TRAINLOG_SYNC_SCREEN_NONE);
    CHECK(trainlog_sync_screen_dispatch(&state, 'b').effect ==
        TRAINLOG_SYNC_SCREEN_NONE);
    action = trainlog_sync_screen_dispatch(&state, 's');
    CHECK(action.effect == TRAINLOG_SYNC_SCREEN_CONFIRM);
    CHECK(action.direction == TRAINLOG_SYNC_BIDIRECTIONAL);
    CHECK(state.confirming);
    CHECK(state.direction == TRAINLOG_SYNC_BIDIRECTIONAL);
    action = trainlog_sync_screen_dispatch(&state, 27);
    CHECK(action.effect == TRAINLOG_SYNC_SCREEN_CANCEL);
    CHECK(!state.confirming);

    action = trainlog_sync_screen_dispatch(&state, 's');
    CHECK(action.effect == TRAINLOG_SYNC_SCREEN_CONFIRM);
    action = trainlog_sync_screen_dispatch(&state, '\n');
    CHECK(action.effect == TRAINLOG_SYNC_SCREEN_RUN);
    CHECK(trainlog_sync_screen_execute(
        &action, record_run, &recorder, &report
    ) == TRAINLOG_STATUS_OK);
    CHECK(recorder.calls == 1U);
    CHECK(recorder.direction == TRAINLOG_SYNC_BIDIRECTIONAL);
    action = trainlog_sync_screen_dispatch(&state, '\n');
    CHECK(action.effect == TRAINLOG_SYNC_SCREEN_NONE);
    CHECK(recorder.calls == 1U);

    action = trainlog_sync_screen_dispatch(&state, 'r');
    CHECK(action.effect == TRAINLOG_SYNC_SCREEN_REFRESH);
    CHECK(recorder.calls == 1U);

    /* Directional keys remain inert after a complete run too. */
    action = trainlog_sync_screen_dispatch(&state, 'a');
    CHECK(action.effect == TRAINLOG_SYNC_SCREEN_NONE);
    CHECK(recorder.calls == 1U);

    CHECK(strcmp(
        trainlog_sync_screen_confirmation(TRAINLOG_SYNC_ANDROID_TO_PC),
        "Importer Android vers le PC maintenant ?"
    ) == 0);
    CHECK(strcmp(
        trainlog_sync_screen_confirmation(TRAINLOG_SYNC_PC_TO_ANDROID),
        "Publier le catalogue du PC vers Android maintenant ?"
    ) == 0);
    CHECK(strcmp(
        trainlog_sync_screen_confirmation(TRAINLOG_SYNC_BIDIRECTIONAL),
        "Synchroniser Android et PC dans les deux sens maintenant ?"
    ) == 0);

    CHECK(strstr(trainlog_sync_screen_footer(120), "Synchroniser maintenant") != NULL);
    CHECK(strstr(trainlog_sync_screen_footer(80), "Synchroniser PC↔Android") != NULL);
    CHECK(strstr(trainlog_sync_screen_footer(120), "Android→PC") == NULL);
    CHECK(strstr(trainlog_sync_screen_footer(120), "PC→Android") == NULL);

    puts("PASS sync screen action dispatcher");
    return 0;
}
