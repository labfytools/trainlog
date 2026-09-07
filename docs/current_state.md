# Current implementation state

Canonical snapshot: 2026-09-07.

This document is the compact source of truth for the implemented Trainlog
baseline. Detailed behavior belongs in the topic-specific documents.

## Status

```text
GATE_0_PROJECT_CONTRACT=PASS
GATE_1_TRAINLOG_FORMAT_V1=PASS/FROZEN
GATE_2_PERSISTENCE_AND_USABLE_TUI=PASS

TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V7=PASS
ANDROID_LOCAL_DATABASE_V7=PASS
ANDROID_SESSION_DRAFT_V1=PASS
ANDROID_DRAFT_DURABLE=PASS
ANDROID_DRAFT_BACKGROUND_SURVIVAL=PASS
ANDROID_DRAFT_PROCESS_DEATH_SURVIVAL=PASS
ANDROID_DRAFT_FORCE_STOP_SURVIVAL=PASS
ANDROID_SESSION_RESUME=PASS
ANDROID_DRAFT_FORM_RESTORE=PASS
ANDROID_DRAFT_EXERCISE_REMOVE=PASS
ANDROID_DRAFT_DISCARD=PASS
ANDROID_DRAFT_FINALIZE_ATOMIC=PASS
ANDROID_DRAFT_NOT_EXPORTED_AS_SESSION=PASS
EXERCISE_EDIT_V1=PASS
EXERCISE_RENAME_STABLE_ID=PASS
ANDROID_BANNER_PARITY_V1=PASS
ANDROID_INSTALL_ADB=PASS
ANDROID_USER_DATA_PRESERVED=PASS

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
MULTI_OCCURRENCE_SESSION_V2=PASS
EQUIPMENT_ASSOCIATIONS_V2=PASS

DESKTOP_TESTS=25/25 PASS
ANDROID_BUILD=PASS
HARDWARE_SYNC_VALIDATION=PASS
```

## Desktop

Implemented:

- C17/Notcurses true-color TUI (72x20 minimum, UTF-8 prompts, resize fallback);
- SQLite schema v7, with stable ordered `session_exercises.entry_id` and
  occurrence-level equipment identity;
- direct session entry;
- persisted session detail and editing;
- exercise removal from a session through transactional child replacement;
- exercise catalog;
- profile-aware set and continuous activities;
- heterogeneous repetition sets;
- multiple occurrences of one catalogue exercise in a session;
- per-set actual loads with distinct external/assistance semantics;
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
- local SQLite database v7, with non-destructive v3 -> v7 migration;
- one durable active-session draft, Home resume and raw-form restoration;
- explicit confirmed discard and atomic completed-save/draft-clear;
- exercise creation;
- stable-ID exercise rename/editing with referenced-profile protection;
- inline exercise creation during session entry;
- profile-aware session recording;
- heterogeneous repetition-set entry;
- exercise removal from the current session draft;
- continuous activity recording;
- shared equipment selection, local custom equipment creation and occurrence
  equipment persistence;
- local session history/detail;
- body measurements;
- automatic mobile snapshot maintenance;
- PC catalog application through a persistent SAF folder grant;
- Android-triggered synchronization request;
- synchronization receipt handling.

The Android catalog exposes **Modifier** for every existing exercise. A rename
updates `name` and `normalized_name` in the original row, never creates an ID,
and remains valid for completed session and active-draft references. A profile
change is only accepted while the row has neither completed-session nor draft
references. Same-ID catalog reconciliation updates display metadata in place in
both Android and desktop import directions.

All Android screens use the shared compact `◆ TRAINLOG ◆` header: the
Notcurses accent, muted context line, and flat touch layout reproduce the TUI
plaque without literal terminal box drawing.

## Synchronization

Canonical exchange directory:

```text
Download/Trainlog
```

Artifacts:

```text
Android -> PC
    trainlog-mobile-export-v2.json
    trainlog-equipment-associations-v2.json

PC -> Android
    trainlog-pc-catalog-v1.json
    trainlog-pc-mobile-export-v2.json
    trainlog-equipment-associations-v2.json

Android -> PC agent
    trainlog-sync-request-v1.json

PC agent -> Android
    trainlog-sync-receipt-v1.json
```

The desktop TUI and `trainlog-syncd` share `trainlog_sync_run()`.

V2 resolves equipment by `(session_id, entry_id)`, never by display name or
catalogue identity alone. V1 files remain legacy-compatible and do not gain
multi-occurrence semantics retroactively.

No SQLite file is copied.

No mounted Android filesystem is required.

## Validation checkpoint

Desktop:

```text
25/25 Meson tests PASS
frozen JSON validator PASS
import-contract validator PASS
git diff --check PASS
```

Android:

```text
assembleDebug and Android unit tests are run for every Android delivery.
The prior device baseline below is hardware evidence, not a claim that every
new implementation detail was re-exercised on the device in this document.
device instrumentation 5/5 PASS
real Samsung background/process-death/force-stop/resume matrix PASS
real migration and original user-data preservation PASS
real Samsung request -> daemon -> bidirectional sync -> receipt PASS
multiple distinct request IDs consumed once each PASS
```

## Current implementation cursor

The Android draft correction is implemented, device-validated and reviewed.
No product-roadmap ordering changes were made.

```text
MEASURED_MAX_V1=PASS
WORKING_LOAD_PERCENTAGES=PASS
ANDROID_MAX_TEST_SESSION=PASS
NEXT_FEATURE=UNFROZEN
```

Future work must start from this validated baseline rather than from obsolete
historical `NEXT` notes.

## Measured max v1

```text
MEASURED_MAX_V1=PASS
MEASURED_MAX_ONLY_FROM_MAX_TEST=PASS
WORKING_LOAD_PERCENTAGES=PASS
ASSISTANCE_DIRECTION_AWARE=PASS
ANDROID_MAX_TEST_SESSION=PASS
DESKTOP_TESTS=25/25 PASS
```

A measured maximum is derived only from explicit `max_test` sessions. Ordinary
training is never promoted implicitly.

The current measured result is the newest successful max test. The historical
record compares max tests using the same load mode.

External-load working percentages are pure calculations from the current
measured load; they are not persisted and no estimated 1RM is introduced.

## Body analytics v1

```text
BODY_ANALYTICS_V1=PASS
BODY_ANALYTICS_TUI_ONLY=PASS
BODY_COMPOSITION_ESTIMATE=PASS
BODY_PROPORTION_RATIOS=PASS
BODY_SYMMETRY_ANALYTICS=PASS
NO_ESTIMATE_PERSISTENCE=PASS
DESKTOP_TESTS=25/25 PASS
```

Android remains capture-only for this feature.

The TUI derives analytics from canonical body observations. A local
desktop-only profile provides height and the circumference-formula branch
needed for the optional body-fat estimate.

No estimated body-fat, fat-mass, lean-mass, ratio, or asymmetry value is stored
as if it were a real measurement.
