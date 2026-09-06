#ifndef TRAINLOG_SYNC_H
#define TRAINLOG_SYNC_H

/**
 * @file sync.h
 * @brief Shared bidirectional Android/desktop synchronization engine.
 */

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/model.h"
#include "trainlog/mtp.h"
#include "trainlog/status.h"
#include "trainlog/usb.h"

#define TRAINLOG_SYNC_SUMMARY_MAX 255U
#define TRAINLOG_SYNC_ERROR_MAX 511U

typedef enum TrainlogSyncTrigger {
    TRAINLOG_SYNC_TRIGGER_TUI = 0,
    TRAINLOG_SYNC_TRIGGER_ANDROID,
    TRAINLOG_SYNC_TRIGGER_DAEMON
} TrainlogSyncTrigger;

typedef struct TrainlogSyncDeviceInfo {
    bool connected;
    bool storage_ready;
    TrainlogUsbDevice device;
    TrainlogMtpStorage storage;
} TrainlogSyncDeviceInfo;

typedef struct TrainlogSyncReport {
    bool success;
    bool request_present;

    char sync_id[
        TRAINLOG_ID_MAX + 1U
    ];

    char request_id[
        TRAINLOG_ID_MAX + 1U
    ];

    char started_at[
        TRAINLOG_TIMESTAMP_MAX + 1U
    ];

    size_t exercises_imported;
    size_t exercises_reconciled;
    size_t exercises_skipped;

    size_t sessions_imported;
    size_t sessions_skipped;

    size_t body_imported;
    size_t body_skipped;

    size_t catalog_published;

    char summary[
        TRAINLOG_SYNC_SUMMARY_MAX + 1U
    ];

    char error[
        TRAINLOG_SYNC_ERROR_MAX + 1U
    ];
} TrainlogSyncReport;

/**
 * @brief Probe one currently connected MTP device without mutating it.
 */
TrainlogStatus trainlog_sync_probe(
    TrainlogSyncDeviceInfo *output
);

/**
 * @brief Run one complete bidirectional synchronization transaction.
 *
 * When require_request is true, the transaction runs only if Android has
 * published a new trainlog-sync-request-v1 request ID.
 *
 * The engine is shared by the ncurses TUI and trainlog-syncd.
 */
TrainlogStatus trainlog_sync_run(
    TrainlogSyncTrigger trigger,
    bool require_request,
    TrainlogSyncReport *output
);

#endif
