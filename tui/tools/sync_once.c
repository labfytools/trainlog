/**
 * @file sync_once.c
 * @brief Non-interactive entry point for the shared Trainlog sync engine.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/sync.h"

static bool parse_trigger(
    const char *text,
    TrainlogSyncTrigger *output
)
{
    if (
        text == NULL ||
        output == NULL
    ) {
        return false;
    }

    if (
        strcmp(
            text,
            "tui"
        ) == 0
    ) {
        *output =
            TRAINLOG_SYNC_TRIGGER_TUI;

        return true;
    }

    if (
        strcmp(
            text,
            "android"
        ) == 0
    ) {
        *output =
            TRAINLOG_SYNC_TRIGGER_ANDROID;

        return true;
    }

    if (
        strcmp(
            text,
            "daemon"
        ) == 0
    ) {
        *output =
            TRAINLOG_SYNC_TRIGGER_DAEMON;

        return true;
    }

    return false;
}

int main(
    int argc,
    char **argv
)
{
    bool request_only = false;

    TrainlogSyncTrigger trigger =
        TRAINLOG_SYNC_TRIGGER_DAEMON;

    TrainlogSyncReport report;
    TrainlogStatus status;
    int index;

    for (
        index = 1;
        index < argc;
        ++index
    ) {
        if (
            strcmp(
                argv[index],
                "--request-only"
            ) == 0
        ) {
            request_only = true;
            continue;
        }

        if (
            strcmp(
                argv[index],
                "--trigger"
            ) == 0 &&
            index + 1 < argc
        ) {
            ++index;

            if (
                !parse_trigger(
                    argv[index],
                    &trigger
                )
            ) {
                (void)fprintf(
                    stderr,
                    "invalid trigger\n"
                );

                return 64;
            }

            continue;
        }

        (void)fprintf(
            stderr,
            "usage: %s [--request-only] [--trigger tui|android|daemon]\n",
            argv[0]
        );

        return 64;
    }

    status =
        trainlog_sync_run(
            trigger,
            request_only,
            TRAINLOG_SYNC_BIDIRECTIONAL,
            &report
        );

    if (
        request_only &&
        status ==
            TRAINLOG_STATUS_NOT_FOUND
    ) {
        (void)printf(
            "SYNC_REQUEST=NONE\n"
        );

        return 3;
    }

    if (
        status ==
            TRAINLOG_STATUS_OK &&
        report.success
    ) {
        (void)printf(
            "SYNC_RUN=PASS\n"
            "sync_id=%s\n"
            "request_id=%s\n"
            "summary=%s\n",
            report.sync_id,
            report.request_id,
            report.summary
        );

        return 0;
    }

    (void)fprintf(
        stderr,
        "SYNC_RUN=FAIL\n"
        "status=%d\n"
        "error=%s\n",
        (int)status,
        report.error[0] != '\0'
            ? report.error
            : "Erreur interne sans diagnostic."
    );

    return 2;
}
