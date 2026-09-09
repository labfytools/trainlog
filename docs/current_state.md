# Current implementation state

Canonical snapshot: 2026-09-09.

This document is the compact source of truth for the implemented Trainlog
baseline. Detailed behavior belongs in the topic-specific documents.

## Status

```text
GATE_0_PROJECT_CONTRACT=PASS
GATE_1_TRAINLOG_FORMAT_V1=PASS/FROZEN
GATE_2_PERSISTENCE_AND_USABLE_TUI=PASS

TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V10=PASS
ANDROID_LOCAL_DATABASE_V9=PASS
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
ANDROID_MAX_V9_INSTALL_ADB=PASS
ANDROID_MAX_V9_REAL_DATA_MIGRATION=PASS
DESKTOP_MAX_V9_REAL_DATA_MIGRATION=PASS
REAL_DATABASE_APPLICATION=PASS

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
EQUIPMENT_DEFINITIONS_V1=PASS
EXERCISE_RECONCILIATION_V2=PASS
EXPLICIT_MAX_RESULTS_V1=PASS
MAX_TEST_RESUME_STABLE_ID=PASS

DESKTOP_TESTS=36/36 PASS
ANDROID_BUILD=PASS
HARDWARE_SYNC_VALIDATION=PASS
```

## Desktop

Implemented:

- C17/Notcurses true-color TUI (72x20 minimum, UTF-8 prompts, resize fallback);
- SQLite schema v10, with stable ordered `session_exercises.entry_id`,
  occurrence-level equipment identity, and desktop-local custom-equipment
  definitions, plus occurrence-owned `max_results`; its v9 -> v10 migration
  rebuilds only `performed_sets` to permit explicit zero actual loads while
  preserving historic NULL and positive rows;
- direct session entry;
- normal desktop SETS planning followed by the table-only explicit actual-row
  editor; zero-row completion is rejected while MAX and continuous entries keep
  their separate no-set contracts;
- persisted session detail and editing;
- exercise removal from a session through transactional child replacement;
- exercise catalog;
- supplied-equipment browsing/search/detail and desktop-local custom-equipment
  creation/selection;
- profile-aware set and continuous activities;
- heterogeneous repetition sets;
- multiple occurrences of one catalogue exercise in a session;
- per-set actual loads with distinct external/assistance semantics;
- body-observation creation/history/editing;
- body graphs and normalized overlays;
- exercise performance history;
- direct USB/MTP device access;
- manual Android -> PC, PC -> Android, and bidirectional synchronization;
- structured synchronization history and detail.

Primary navigation:

```text
0 Accueil
1 Séance
2 Historique
3 Exercices
4 Équipements
5 Corps
6 Sync
```

## Android

Implemented:

- native Kotlin/Compose application;
- local SQLite database v9, with non-destructive v3 -> v9 migration;
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
references. Same-ID catalog reconciliation updates display metadata in place.
A different-ID normalized-name collision is reconciled only when recording and
tracking modes match, other known invariants are compatible, and one
`data_fields` mask contains the other. PC identity is canonical on Android;
the existing desktop identity is canonical on desktop. The richer bit-mask
union is retained without rewriting historical occurrence snapshots.

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
    trainlog-mobile-equipment-definitions-v1.json
    trainlog-mobile-export-v2.json
    trainlog-equipment-associations-v2.json

PC -> Android
    trainlog-pc-equipment-definitions-v1.json
    trainlog-pc-catalog-v1.json
    trainlog-pc-mobile-export-v2.json
    trainlog-equipment-associations-v2.json

Android -> PC agent
    trainlog-sync-request-v1.json

PC agent -> Android
    trainlog-sync-receipt-v1.json
```

The desktop TUI and `trainlog-syncd` share `trainlog_sync_run()`. Its explicit
`a`, `p`, and `b` modes perform Android -> PC only, PC -> Android only, and
inbound-then-outbound respectively. Each direction reconciles equipment
definitions V1 before V2 artifacts that reference them.

Each structured local sync-history run persists its selected `a`, `p`, or `b`
direction. `direction inconnue` is reserved for legacy history rows whose
direction was not recorded; it is never used for a current run.

On the TUI Sync page, `a`, `p`, and `b` directly open one confirmation for the
selected direction; `Enter` runs that confirmed action once, while `Esc`
cancels it without an operation (or returns when idle). `r` refreshes device
status only, and the former `s` shortcut is inert. Full and compact footers use
the same three directional actions, with compact labels only abbreviating the
endpoints.

V2 resolves equipment by `(session_id, entry_id)`, never by display name or
catalogue identity alone. V1 files remain legacy-compatible and do not gain
multi-occurrence semantics retroactively.

Supplied equipment remains generated from `catalog/equipment-v1.json`; its IDs
are reserved. User-created definitions synchronize additively through the
directional definitions V1 artifacts. Equal same-ID definitions are idempotent;
divergent definitions, unknown references, and association conflicts are
reported explicitly without silent overwrite.

Conflict summaries identify the stable session, body-observation, association,
or definition identity and the source artifact/direction. Existing persisted
content is preserved when a conflict is reported.

Synchronization addition counters describe durable inserts only. Exercise
rekeys or profile enrichments remain available through the separate
reconciliation counter; a compatible different-ID lookup that requires no
database write is an idempotent skip. This prevents a repeated Android
snapshot carrying a retired creator ID from being displayed as `+1 exercice`
when the canonical desktop catalog row and all business tables are unchanged.

The current Marche collision is safely reconciled to desktop identity
`ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d`. Its catalog profile is the compatible
union `continuous/duration/data_fields=3`; prior `data_fields=1` occurrences
retain their own snapshots and absent distance values. The Android occurrence
`sxe_f25142c8-455e-4346-9bfc-31d0989e275d` retains its duration, speed, distance
and equipment. No reference to the retired Android exercise identity remains
in the validated merged database copy.

No SQLite file is copied.

No mounted Android filesystem is required.

## Validation checkpoint

Desktop:

```text
36/36 Meson tests PASS for the current desktop schema v10 baseline
JSON valid/invalid checks PASS
import-contract validator 6/6 PASS
ASan/UBSan 14/14 Meson tests PASS postrepair
git diff --check PASS
```

Android:

```text
37 Android unit tests PASS
assembleDebug PASS
```

Current real-data/environment boundary:

```text
REAL_ANDROID_ARTIFACT_COPY_ROUNDTRIP=PASS
PC_EXPORT_ANDROID_IMPORT_THREE_PASS_COPY=PASS
ANDROID_INSTALL_R_DATABASE_HASH_PRESERVED=PASS
CURRENT_SANDBOX_LIBMTP_OPEN=BLOCKED
REAL_PC_TO_ANDROID_TWO_RUN=BLOCKED_BY_SANDBOX_LIBMTP_OPEN
```

The current sandbox discovers the connected Samsung MTP interface but
`libusb_open()` cannot acquire it. It also exposes the canonical desktop DB as
read-only. The failed first write left that DB byte-for-byte logically
unchanged and integrity-clean; Android remained force-stopped and was not
modified through ADB. Therefore this checkpoint makes no new direct-device MTP
claim and does not claim that the real stores have consumed the reconciled
artifacts.

## Current implementation cursor

Exercise reconciliation, definition-first V2 synchronization and the current
real-data importer/exporter copy validation are complete. No product-roadmap
ordering change was made by this corrective tranche.

```text
MEASURED_MAX_V1=PASS
WORKING_LOAD_PERCENTAGES=PASS
ANDROID_MAX_TEST_SESSION=PASS
EXPLICIT_MAX_RESULTS_V1=PASS
MAX_TEST_RESUME_STABLE_ID=PASS
CURRENT_OPERATIONAL_CURSOR=REAL_DATA_BASELINE_V1
NEXT_FEATURE=GYM_CATALOG_V1
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
EXPLICIT_MAX_RESULTS_V1=PASS
MAX_TEST_RESUME_STABLE_ID=PASS
DESKTOP_TESTS=36/36 PASS
```

A measured maximum belongs to an exercise occurrence in an explicit `max_test`
session. Schema v9 introduced the positive `max_weight_kg` separately from sets;
equipment is optional context, so one physical machine may carry independent
Pec Fly and Rear Delt Fly results. Ordinary training is never promoted
implicitly.

The Android and TUI max forms request no set or repetition count. Android loads
a completed Test max without changing `session_id`, existing `entry_id` values,
order, exercise identity, or equipment context; finalization replaces that
session's children atomically, permits explicit value/equipment corrections,
and may append new entries.

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
DESKTOP_TESTS=36/36 PASS
```

Android remains capture-only for this feature.

The TUI derives analytics from canonical body observations. A local
desktop-only profile provides height and the circumference-formula branch
needed for the optional body-fat estimate.

No estimated body-fat, fat-mass, lean-mass, ratio, or asymmetry value is stored
as if it were a real measurement.
