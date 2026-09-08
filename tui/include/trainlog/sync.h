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

typedef enum TrainlogSyncDirection {
    TRAINLOG_SYNC_ANDROID_TO_PC = 0,
    TRAINLOG_SYNC_PC_TO_ANDROID,
    TRAINLOG_SYNC_BIDIRECTIONAL
} TrainlogSyncDirection;

typedef struct TrainlogSyncDirectionPlan {
    bool receive_android;
    bool publish_android;
} TrainlogSyncDirectionPlan;

typedef struct TrainlogSyncDeviceInfo {
    bool connected;
    bool storage_ready;
    TrainlogUsbDevice device;
    TrainlogMtpStorage storage;
} TrainlogSyncDeviceInfo;

typedef struct TrainlogSyncReport {
    bool success;
    bool request_present;
    TrainlogSyncDirection direction;

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

    size_t equipment_definitions_imported;
    size_t equipment_definitions_skipped;

    size_t catalog_published;

    char summary[
        TRAINLOG_SYNC_SUMMARY_MAX + 1U
    ];

    char error[
        TRAINLOG_SYNC_ERROR_MAX + 1U
    ];
} TrainlogSyncReport;

TrainlogSyncDirectionPlan trainlog_sync_direction_plan(
    TrainlogSyncDirection direction
);

/**
 * @brief Build the user-visible mutation summary for an initialized report.
 *
 * CONTRACT: the `+N exercice(s)` value counts only newly inserted persistent
 * exercise rows. Reconciliation and idempotent lookup remain available in
 * their dedicated report fields and must never be presented as additions.
 * The function borrows `report`, mutates only its `summary` array, performs no
 * allocation or persistence, and is deterministic for equal report fields.
 * A NULL report is accepted as a no-op; output is always bounded and NUL
 * terminated by the fixed Trainlog report storage.
 */
void trainlog_sync_build_summary(
    TrainlogSyncReport *report
);

/**
 * @brief Select the current Android mobile-export V2 entry from one folder.
 *
 * Android's scoped-storage provider can retain an existing immutable object
 * and create a collision-suffixed sibling such as
 * `trainlog-mobile-export-v2 (2).json`.  Both that form and the canonical
 * filename are valid V2 publications.  The newest usable MTP modification
 * time wins; deterministic suffix and item-ID tie breaks prevent arbitrary
 * selection when a device reports equal or unavailable times.  This routine
 * only selects a candidate.  The caller must still validate its JSON before
 * import and must never fall back to an older candidate on validation error.
 */
bool trainlog_sync_select_mobile_export(
    const TrainlogMtpEntry *entries,
    size_t count,
    size_t *output_index
);

/**
 * @brief Select the latest Android artifact for one canonical JSON filename.
 *
 * The canonical name and Android's scoped-storage collision form
 * `<stem> (N).json` are accepted.  @p canonical_name is borrowed and must be
 * a nonempty `.json` filename.  False reports invalid arguments or no valid
 * candidate.  This selection is deterministic, but callers must validate the
 * selected artifact before making any persistent import decision.
 */
bool trainlog_sync_select_android_artifact(
    const TrainlogMtpEntry *entries,
    size_t count,
    const char *canonical_name,
    size_t *output_index
);

/**
 * @brief Probe one currently connected MTP device without mutating it.
 */
TrainlogStatus trainlog_sync_probe(
    TrainlogSyncDeviceInfo *output
);

/**
 * @brief Run one synchronization transaction in the selected direction.
 *
 * When require_request is true, the transaction runs only if Android has
 * published a new trainlog-sync-request-v1 request ID.
 *
 * The engine is shared by the Notcurses TUI and trainlog-syncd.
 */
TrainlogStatus trainlog_sync_run(
    TrainlogSyncTrigger trigger,
    bool require_request,
    TrainlogSyncDirection direction,
    TrainlogSyncReport *output
);

#endif
