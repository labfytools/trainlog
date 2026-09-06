# Bidirectional synchronization checkpoint

## Validated status

```text
ANDROID_LOCAL_WORKFLOWS=PASS

ANDROID_TO_PC_MTP=PASS
DESKTOP_MOBILE_IMPORT_V1=PASS
DESKTOP_MOBILE_IMPORT_IDEMPOTENT=PASS

PC_CATALOG_EXPORT_V1=PASS
PC_TO_ANDROID_MTP_PUBLISH=PASS

ANDROID_SAF_FOLDER_SELECTION=PASS
ANDROID_SAF_FOLDER_CHANGE=PASS
```

This checkpoint represents the validated synchronization foundation on the
physical Samsung device.

## Android → PC

Android produces:

```text
Download/Trainlog/trainlog-mobile-export-v1.json
```

The desktop retrieves it through direct libmtp.

No GVFS/FUSE mount is used.

The desktop importer:

```text
tools/import_mobile_export.py
```

is:

```text
strict
transactional
idempotent by stable IDs
profile-aware
```

Validated behavior:

```text
first import:
    new exercise/session/body observation imported

second import:
    no duplicates
    existing stable IDs skipped
```

## PC → Android

The desktop produces:

```text
/tmp/trainlog-pc-catalog-v1.json
```

with:

```text
format  = trainlog-pc-catalog
version = 1
```

The catalog is published by direct MTP to:

```text
Download/Trainlog/trainlog-pc-catalog-v1.json
```

Physical-device publication has been validated.

The Android application accesses the PC-created file through one persistent
Storage Access Framework grant.

The selected folder must be:

```text
Download/Trainlog
```

The Sync screen must always allow changing this folder because a wrong
persisted SAF grant must be recoverable without clearing application data.

## Important semantics

The synchronization layer exchanges versioned Trainlog domain artifacts.

It does not synchronize SQLite files.

The frozen legacy session format remains:

```text
TRAINLOG_FORMAT_V1=FROZEN
```

Profile-aware synchronization uses separate explicit synchronization artifacts.

## Current TUI state

The existing desktop Sync action can:

```text
Android → PC import
PC → Android catalog publication
```

The current status/count-oriented Sync presentation is temporary.

A snapshot file remaining on Android is not a pending queue item. Therefore
labels such as:

```text
1 JSON candidate
```

must not be considered a final synchronization UX.

## Next synchronization UX

The TUI Sync page must become a persistent synchronization history, similar to:

```text
git log
```

Example:

```text
06/09/2026 16:00  ✓ Android +1 session · PC catalog 3 exercises
06/09/2026 15:42  ✓ no changes
06/09/2026 15:31  ! device disconnected
```

A synchronization entry must be selectable.

`Enter` opens a detailed view analogous to:

```text
git show
```

The detail must include:

```text
sync ID
trigger
start/end local time
status

Android → PC
    exercises imported/reconciled/skipped
    sessions imported/skipped
    body observations imported/skipped

PC → Android
    catalog exercises published
    Android catalog apply result when available

transport/artifact diagnostics
```

## Android-triggered synchronization

The final Android Sync UX must not require:

```text
Prepare export
then use the PC manually
```

Target behavior:

```text
Android data change
    -> mobile snapshot maintained automatically

Android "Synchronize now"
    -> synchronization request

PC trainlog-syncd
    -> detects request over direct MTP
    -> runs the same bidirectional sync engine
    -> writes synchronization receipt

Android
    -> reads receipt
    -> applies PC catalog
    -> displays final synchronization result
```

MTP remains host-initiated. Therefore Android-triggered synchronization needs
a small PC-side agent; it cannot directly command libmtp operations on the PC.

## Display normalization

User-facing session history must use local presentation:

```text
DD/MM/YYYY HH:MM
```

Canonical RFC3339 timestamps remain unchanged in persistence and exchange.

## Next cursor

```text
SYNC_HISTORY_GIT_LIKE=NEXT
COMMON_SYNC_ENGINE=NEXT
TRAINLOG_SYNCD=NEXT
ANDROID_SYNC_REQUEST_RECEIPT=AFTER
ANDROID_AUTO_OUTBOX=AFTER_AGENT_FOUNDATION
```
