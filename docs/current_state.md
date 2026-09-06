# Current implementation state

Canonical snapshot: 2026-09-06.

This document is the compact source of truth for the implemented Trainlog
baseline. Detailed behavior belongs in the topic-specific documents.

## Status

```text
GATE_0_PROJECT_CONTRACT=PASS
GATE_1_TRAINLOG_FORMAT_V1=PASS/FROZEN
GATE_2_PERSISTENCE_AND_USABLE_TUI=PASS

TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V5=PASS
ANDROID_LOCAL_DATABASE_V3=PASS

PROFILE_AWARE_EXERCISES=PASS
CONTINUOUS_ACTIVITY=PASS
VARIABLE_REPETITION_SETS=PASS

DIRECT_MTP_TRANSPORT=PASS
ANDROID_TO_PC_IMPORT=PASS
PC_TO_ANDROID_CATALOG=PASS
COMMON_SYNC_ENGINE=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
TUI_SYNC_LOG_SHOW=PASS
BIDIRECTIONAL_SYNC_V1=PASS

DESKTOP_TESTS=19/19 PASS
ANDROID_BUILD=PASS
HARDWARE_SYNC_VALIDATION=PASS
```

## Desktop

Implemented:

- C17/ncursesw TUI;
- SQLite schema v5;
- direct session entry;
- persisted session detail and editing;
- exercise removal from a session through transactional child replacement;
- exercise catalog;
- profile-aware set and continuous activities;
- heterogeneous repetition sets;
- body-observation creation/history/editing;
- body graphs and normalized overlays;
- exercise performance history;
- direct USB/MTP device access;
- manual bidirectional synchronization;
- structured synchronization history and detail.

Primary navigation:

```text
0 Accueil
1 Séance
2 Historique
3 Exercices
4 Corps
5 Sync
```

## Android

Implemented:

- native Kotlin/Compose application;
- local SQLite database v3;
- exercise creation;
- inline exercise creation during session entry;
- profile-aware session recording;
- heterogeneous repetition-set entry;
- exercise removal from the current session draft;
- continuous activity recording;
- local session history/detail;
- body measurements;
- automatic mobile snapshot maintenance;
- PC catalog application through a persistent SAF folder grant;
- Android-triggered synchronization request;
- synchronization receipt handling.

## Synchronization

Canonical exchange directory:

```text
Download/Trainlog
```

Artifacts:

```text
Android -> PC
    trainlog-mobile-export-v1.json

PC -> Android
    trainlog-pc-catalog-v1.json

Android -> PC agent
    trainlog-sync-request-v1.json

PC agent -> Android
    trainlog-sync-receipt-v1.json
```

The desktop TUI and `trainlog-syncd` share `trainlog_sync_run()`.

No SQLite file is copied.

No mounted Android filesystem is required.

## Validation checkpoint

Desktop:

```text
19/19 Meson tests PASS
frozen JSON validator PASS
import-contract validator PASS
git diff --check PASS
```

Android:

```text
assembleDebug PASS
real Samsung request -> daemon -> bidirectional sync -> receipt PASS
multiple distinct request IDs consumed once each PASS
```

## Current implementation cursor

No new feature is frozen by this documentation cleanup.

```text
NEXT_FEATURE=UNFROZEN
```

Future work must start from this validated baseline rather than from obsolete
historical `NEXT` notes.
