# Trainlog synchronization exchange

## Status

```text
MOBILE_EXPORT_V1=FROZEN_FOR_IMPLEMENTATION
TRAINLOG_FORMAT_V1=FROZEN_UNCHANGED
```

This document defines a synchronization artifact. It is not the frozen
Trainlog session JSON v1 format.

## Android → PC artifact

Shared-storage path:

```text
Download/Trainlog/trainlog-mobile-export-v1.json
```

Format header:

```json
{
  "format": "trainlog-mobile-export",
  "version": 1
}
```

The artifact is a complete idempotent mobile snapshot containing:

```text
exercises
sessions
body_observations
```

### Exercises

Each exercise contains:

```text
exercise_id
name
recording_mode
tracking_mode
data_fields
```

### Sessions

Each session contains:

```text
session_id
started_at
session_type
exercises
```

Each session exercise snapshots:

```text
exercise_id
name
recording_mode
tracking_mode
data_fields
load_mode
rest_seconds
```

For `SETS`:

```text
sets[]
    reps
or
    duration_seconds
```

For `CONTINUOUS`:

```text
continuous
    duration_seconds
    speed_kmh optional
    distance_km optional
```

A continuous exercise has no synthetic set.

### Body observations

Each body observation contains:

```text
observation_id
observed_at
only the metrics actually measured
```

## Transport

Android writes its own export into shared Downloads storage.

Desktop reads the artifact through direct libmtp transport.

No filesystem mount is required.

No SQLite database file is transferred.

## PC → Android

A separate canonical catalog artifact will be defined and implemented after
Android → PC export is validated on physical hardware.

The PC → Android path must not overload frozen Trainlog JSON v1.

<!-- TRAINLOG_DESKTOP_MOBILE_IMPORT_V1 -->
## Desktop import of mobile export v1

The desktop importer is:

```text
tools/import_mobile_export.py
```

It validates the complete mobile snapshot before opening a write transaction.

Properties:

```text
transactional
idempotent by stable IDs
exercise reconciliation by normalized name
profile conflicts rejected
unknown JSON fields rejected
no SQLite file copying
```

For mobile `SETS` v1, the Android form records one uniform set metric. The
desktop importer derives:

```text
target_sets = number of logged sets
target_reps or target_duration = uniform logged value
```

and preserves all performed sets separately.

A v1 mobile session with heterogeneous set metrics or `0 reps` is rejected
rather than inventing a desktop target.

Continuous activities remain target-less and are imported only into
`continuous_activity`.

Recommended validation sequence:

```bash
python tools/import_mobile_export.py   /tmp/trainlog-mobile-export-v1.json   --dry-run

python tools/import_mobile_export.py   /tmp/trainlog-mobile-export-v1.json
```

Running the real import a second time must import nothing new and report the
existing IDs as skipped.
<!-- TRAINLOG_DESKTOP_MOBILE_IMPORT_V1 _END -->

<!-- TRAINLOG_BIDIRECTIONAL_SYNC_V1 -->
## Bidirectional synchronization v1

One desktop Sync action now performs both directions:

```text
Android → PC
    direct-MTP download
    strict transactional import

PC → Android
    canonical PC exercise catalog export
    direct-MTP publication
```

The Android app obtains one persistent Storage Access Framework grant for:

```text
Download/Trainlog
```

After this one-time grant, Android can import the PC-created catalog without
broad storage permissions.

The Sync page displays persistent synchronization history instead of remote
snapshot counts. A snapshot remaining present is not a pending queue item and
must not be shown as a "candidate".

User-facing session history timestamps are displayed as:

```text
DD/MM/YYYY HH:MM
```

Canonical RFC3339 storage remains unchanged.
<!-- TRAINLOG_BIDIRECTIONAL_SYNC_V1 _END -->

<!-- TRAINLOG_ANDROID_AUTO_OUTBOX_REQUEST -->
## Automatic Android outbox and sync request

Android no longer requires a manual export action.

The mobile snapshot is refreshed automatically on:

```text
application start
exercise save
session save
body observation save
PC catalog apply
```

The Android Sync screen exposes:

```text
Synchroniser maintenant
```

This writes:

```text
Download/Trainlog/trainlog-sync-request-v1.json
```

with a stable request ID and timestamp.

The next PC-agent slice consumes this request and writes a sync receipt.
<!-- TRAINLOG_ANDROID_AUTO_OUTBOX_REQUEST _END -->

<!-- TRAINLOG_BIDIRECTIONAL_VALIDATED_CHECKPOINT -->
## Validated bidirectional transport checkpoint

Validated on the physical Samsung device:

```text
ANDROID_TO_PC_MTP=PASS
DESKTOP_MOBILE_IMPORT_V1=PASS
DESKTOP_MOBILE_IMPORT_IDEMPOTENT=PASS

PC_CATALOG_EXPORT_V1=PASS
PC_TO_ANDROID_MTP_PUBLISH=PASS
```

Artifacts:

```text
Android → PC
    Download/Trainlog/trainlog-mobile-export-v1.json

PC → Android
    Download/Trainlog/trainlog-pc-catalog-v1.json
```

Both are synchronization artifacts and remain separate from frozen
`TRAINLOG_FORMAT_V1`.

The Android Storage Access Framework folder grant must target:

```text
Download/Trainlog
```

and the UI must permit changing the stored folder selection.

Remaining synchronization work:

```text
persistent structured sync history
selectable sync detail
common sync engine
trainlog-syncd
Android-triggered request/receipt workflow
automatic mobile snapshot maintenance
```
<!-- TRAINLOG_BIDIRECTIONAL_VALIDATED_CHECKPOINT _END -->

<!-- TRAINLOG_VARIABLE_SET_REPS_V1 -->
## Variable repetition sets

Trainlog preserves each performed set independently.

Accepted repetition input:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

`4..10..4` expands to:

```text
4,5,6,7,8,9,10,9,8,7,6,5,4
```

Desktop schema v5 permits targetless `SETS` rows for actual-only mobile
observations. Synchronization therefore does not invent a uniform target when
performed sets are heterogeneous.

`performed_sets` remains the source of truth for actual per-set values.

Existing planned desktop sessions may still carry explicit target sets/reps or
target durations.

`trainlog-mobile-export` v1 keeps ordered heterogeneous `sets[]`.

Frozen `TRAINLOG_FORMAT_V1` is unchanged.
<!-- TRAINLOG_VARIABLE_SET_REPS_V1 _END -->

<!-- TRAINLOG_VARIABLE_SETS_CHECKPOINT_FINAL -->
## Variable sets and session exercise removal checkpoint

Validated functionality in this checkpoint:

```text
VARIABLE_REPETITION_SETS=PASS
REPETITION_SHORTHAND_5x10=PASS
REPETITION_EXPLICIT_LIST=PASS
REPETITION_PYRAMID=PASS

DESKTOP_SCHEMA_V5=PASS
V4_TO_V5_MIGRATION_REGRESSION=PASS
MOBILE_HETEROGENEOUS_SET_IMPORT=PASS
MOBILE_IMPORT_IDEMPOTENCE=PASS
NO_FAKE_UNIFORM_TARGET=PASS

ANDROID_SESSION_DRAFT_EXERCISE_REMOVE=PASS
DESKTOP_SESSION_EXERCISE_REMOVE=PASS
```

Accepted repetition examples:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

A heterogeneous mobile session is persisted as ordered `performed_sets`.
The desktop does not invent `target_sets`, `target_reps` or
`target_duration_seconds` for actual-only mobile observations.

On Android, an exercise already added to the current session can be removed
before saving the session.

On the desktop TUI, session editing already supports:

```text
d supprimer
```

for removing the selected exercise from a current or persisted session draft.
The database replacement remains transactional.

`TRAINLOG_FORMAT_V1` remains frozen and unchanged.
<!-- TRAINLOG_VARIABLE_SETS_CHECKPOINT_FINAL _END -->
