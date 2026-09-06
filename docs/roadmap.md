# Roadmap

This file describes the current project state and the next implementation
cursor. Historical checkpoint detail belongs in Git history and `docs/reviews`.

## Gate 0 — Project contract

```text
GATE_0=PASS
```

Development rules, ownership boundaries, identity rules, validation discipline,
and Git workflow are established.

## Gate 1 — Trainlog exchange format v1

```text
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

The published Trainlog JSON v1 compatibility boundary remains unchanged.

## Gate 2 — Persistence and usable desktop TUI

```text
GATE_2=PASS
DESKTOP_SCHEMA_V5=PASS
FIRST_USABLE_TUI=PASS
```

Completed baseline includes:

- SQLite persistence and explicit migrations;
- direct session entry;
- session history/detail/editing;
- exercise catalog;
- profile-aware set and continuous work;
- body history/editing/visualization;
- exercise performance history;
- strict validation and rollback behavior.

## Android local client

```text
ANDROID_LOCAL_WORKFLOWS=PASS
ANDROID_LOCAL_DATABASE_V3=PASS
```

Completed:

- exercise creation;
- inline exercise creation;
- profile-aware session entry;
- heterogeneous repetition sets;
- continuous activity;
- session history/detail;
- body measurements;
- exercise removal from the current session draft.

## Variable set checkpoint

```text
VARIABLE_REPETITION_SETS=PASS
REPETITION_SHORTHAND_5x10=PASS
REPETITION_EXPLICIT_LIST=PASS
REPETITION_PYRAMID=PASS

DESKTOP_SCHEMA_V5=PASS
V4_TO_V5_MIGRATION_REGRESSION=PASS
MOBILE_HETEROGENEOUS_SET_IMPORT=PASS
NO_FAKE_UNIFORM_TARGET=PASS
```

## Direct MTP transport

```text
USB_MTP_DETECTION=PASS
MTP_STORAGE_ACCESS=PASS
MTP_WRITE=PASS
MTP_LIST_FOLDER=PASS
MTP_READ=PASS
MTP_ROUNDTRIP=PASS
DIRECT_MTP_TRANSPORT=PASS
```

No mounted-filesystem dependency is required.

## Bidirectional synchronization v1

```text
ANDROID_TO_PC_MTP=PASS
DESKTOP_MOBILE_IMPORT_V1=PASS
DESKTOP_MOBILE_IMPORT_IDEMPOTENT=PASS

PC_CATALOG_EXPORT_V1=PASS
PC_TO_ANDROID_MTP_PUBLISH=PASS

COMMON_SYNC_ENGINE=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
TUI_SYNC_LOG_SHOW=PASS

BIDIRECTIONAL_SYNC_V1=PASS
```

Validated architecture:

```text
Android local change
-> automatic mobile snapshot

Android "Synchroniser maintenant"
-> request

trainlog-syncd
-> shared C sync engine
-> Android -> PC import
-> PC -> Android catalog
-> receipt

Android
-> matching receipt
-> catalog apply
-> final result
```

The TUI invokes the same engine manually.

## Current quality baseline

```text
DESKTOP_TESTS=19/19 PASS
ANDROID_BUILD=PASS
HARDWARE_SYNC_VALIDATION=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

## Current implementation cursor

No next product feature is frozen by this documentation checkpoint.

```text
NEXT_FEATURE=UNFROZEN
```

Candidate future areas may include richer analytics, measured-max semantics,
additional editing workflows, synchronization hardening, or other product work,
but none is canonical until explicitly selected.

## Permanent constraints

Do not regress to:

```text
SQLite file synchronization
mandatory mounted Android filesystem
exercise-name identity heuristics
fake performed sets for continuous activity
fake uniform targets for heterogeneous actual sets
incompatible changes to Trainlog JSON v1
```
