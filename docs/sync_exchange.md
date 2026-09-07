# Synchronization exchange

## 1. Status

```text
DIRECT_MTP_TRANSPORT=PASS
ANDROID_TO_PC_IMPORT=PASS
PC_TO_ANDROID_CATALOG=PASS
COMMON_SYNC_ENGINE=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
TUI_SYNC_LOG_SHOW=PASS
BIDIRECTIONAL_SYNC_V1=PASS
MULTI_OCCURRENCE_SESSION_V2=PASS
EQUIPMENT_ASSOCIATIONS_V2=PASS

TRAINLOG_FORMAT_V1=FROZEN_UNCHANGED
```

Synchronization artifacts are separate from the frozen Trainlog session JSON
v1 format.

## 2. Exchange directory

Canonical Android shared-storage directory:

```text
Download/Trainlog
```

Desktop accesses this directory through direct MTP.

Android accesses PC-created artifacts through a persistent Storage Access
Framework folder grant.

## 3. Artifact table

| Direction | File | Format |
| --- | --- | --- |
| Android -> PC | `trainlog-mobile-export-v1.json` | `trainlog-mobile-export` v1 |
| Android -> PC | `trainlog-mobile-export-v2.json` | `trainlog-mobile-export` v2 (active) |
| Android -> PC | `trainlog-equipment-associations-v2.json` | `trainlog-equipment-associations` v2 |
| PC -> Android | `trainlog-pc-catalog-v1.json` | `trainlog-pc-catalog` v1 |
| PC -> Android | `trainlog-pc-mobile-export-v2.json` | `trainlog-mobile-export` v2 |
| PC -> Android | `trainlog-equipment-associations-v2.json` | `trainlog-equipment-associations` v2 |
| Android -> PC agent | `trainlog-sync-request-v1.json` | `trainlog-sync-request` v1 |
| PC agent -> Android | `trainlog-sync-receipt-v1.json` | `trainlog-sync-receipt` v1 |

No SQLite file is transferred.

V2 is a separate format: every session entry has an `entry_id`, `position`,
metrics, actual loads and optional equipment identity. This permits two
occurrences of the same exercise without fusion. V1 remains readable with its
frozen contract. A V1 historical session is reconciled with V2 only when the
exercise/order correspondence is unambiguous; otherwise the importer reports
a conflict rather than silently overwriting data.

## 4. Android -> PC mobile snapshot

Header:

```json
{
  "format": "trainlog-mobile-export",
  "version": 2
}
```

The artifact is a complete idempotent mobile snapshot containing:

```text
exercises
sessions
body_observations
```

V2 session entries additionally carry:

```text
entry_id             stable occurrence identity
position             stable order within session
equipment_id         optional canonical equipment identity
weight_kg            optional actual value on each set
```

The desktop imports sessions first, preserving `entry_id`, then applies the
equipment companion only after all referenced entries exist. Reimporting either
artifact reconciles stable identities; it neither duplicates sessions nor
regenerates occurrence IDs.

Exercise profile fields:

```text
exercise_id
name
recording_mode
tracking_mode
data_fields
```

Set-based session exercise actuals use ordered:

```text
sets[]
```

with either:

```text
reps
```

or:

```text
duration_seconds
```

Heterogeneous repetition values are valid.

Continuous actuals use:

```text
continuous
    duration_seconds
    speed_kmh optional
    distance_km optional
```

No synthetic set is created for continuous work.

Android-local active-session drafts are excluded from this snapshot and remain
local during synchronization. Only successful atomic finalization makes a draft
a completed exportable session. The v1 artifact has no draft fields or tables;
catalog reconciliation preserves active draft references.
Same-ID catalog entries may update display-name metadata in their existing
catalog row; a rename never creates a second exercise identity.

## 5. Desktop mobile importer

Reference importer:

```text
tools/import_mobile_export.py
```

Properties:

```text
strict full-snapshot validation
transactional import
stable-ID idempotence
catalog reconciliation
profile conflict rejection
heterogeneous performed-set preservation
targetless schema-v5 import when no true target exists
continuous activity kept separate
```

The importer never invents a uniform target merely to fit desktop persistence.

## 6. PC -> Android catalog

The desktop publishes:

```text
trainlog-pc-catalog-v1.json
```

It is a canonical exercise catalog snapshot containing stable profile metadata.

Android reconciles the received catalog into its local exercise catalog.

This direction does not overload frozen Trainlog session JSON v1.

## 7. Android sync request

Android writes:

```text
trainlog-sync-request-v1.json
```

Header:

```json
{
  "format": "trainlog-sync-request",
  "version": 1
}
```

Required synchronization identity:

```text
request_id = sr_<uuid-v4>
```

The artifact also carries the request timestamp.

A new `request_id` represents a new synchronization request.

## 8. PC sync receipt

After processing an Android request, the PC publishes:

```text
trainlog-sync-receipt-v1.json
```

Header:

```json
{
  "format": "trainlog-sync-receipt",
  "version": 1
}
```

The receipt contains:

```text
request_id
sync_id
status
summary
Android -> PC counts
PC -> Android catalog count
```

Android accepts a receipt only when its `request_id` matches the pending
request.

## 9. Shared desktop engine

Canonical implementation:

```text
trainlog_sync_run()
```

TUI path:

```text
TUI
-> shared engine
```

Android-triggered path:

```text
Android request
-> trainlog-syncd
-> shared engine
-> receipt
```

One synchronization transaction performs:

```text
mobile snapshot download
-> mobile import
-> equipment companion import by (session_id, entry_id)
-> PC catalog export
-> PC mobile V2 export
-> PC equipment companion V2 export
-> PC catalog MTP publication
-> optional receipt publication
-> structured run history
```

## 10. Concurrency and request consumption

Synchronization owns:

```text
$XDG_DATA_HOME/trainlog/sync.lock
```

The daemon uses non-blocking acquisition while polling.

The TUI manual action waits for the active synchronization lock.

After a request is completed and its receipt is published, the request ID is
recorded locally so the same request is not processed as a new request again.

## 11. Structured sync history

Every real run has:

```text
sy_<uuid-v4>
```

Artifacts:

```text
$XDG_DATA_HOME/trainlog/sync_runs/sy_*.json
$XDG_DATA_HOME/trainlog/sync_runs/sy_*.txt
$XDG_DATA_HOME/trainlog/sync_history.log
```

The TUI presents newest runs in a selectable list and opens the detail file with
`Enter`.

Legacy history rows without a `sync_id` remain readable as list entries but
cannot have structured detail.

## 12. PC user service

Install or refresh:

```bash
bash tools/install_syncd_user.sh
```

Check:

```bash
systemctl --user is-active trainlog-syncd.service
systemctl --user --no-pager --full status trainlog-syncd.service
```

Daemon log:

```bash
tail -f ~/.local/state/trainlog/syncd.log
```

No root privilege is required.

## 13. Transport invariants

Do not regress to:

```text
SQLite database copying
mandatory GVFS/FUSE mounts
exercise-name identity heuristics
fake sets for continuous activity
fake uniform targets for heterogeneous actual sets
overloading frozen Trainlog JSON v1
```

## 14. Hardware validation

## 15. Equipment associations V2 and legacy V1

`TRAINLOG_FORMAT_V1` remains frozen. The active companion is
`trainlog-equipment-associations-v2.json`, format
`trainlog-equipment-associations`, version `2`. Each row is identified by
`(session_id, entry_id)` and contains `exercise_id` as consistency metadata,
then either `state: set` with a canonical `equipment_id`, or `state: cleared`
for an intentional removal. A missing companion conveys no equipment
information and cannot clear a previously known choice. Unknown canonical IDs,
unknown entries and ambiguous identities reject the companion transaction
explicitly; an unknown equipment reference is never silently changed to null.

The historical V1 companion remains readable only where its
`(session_id, exercise_id)` targeting is unambiguous. It cannot represent two
occurrences of the same exercise in one session and is not redefined to do so.

Validated on the physical Android device:

```text
MTP device discovery PASS
storage access PASS
read/write/list/delete PASS
Android mobile snapshot download PASS
desktop idempotent import PASS
PC catalog publication PASS
Android request detection PASS
trainlog-syncd processing PASS
receipt publication/readback PASS
multiple distinct Android request IDs PASS
```
