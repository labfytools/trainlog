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

DESKTOP_SCHEMA_V11=PASS
ANDROID_LOCAL_DATABASE_V10=PASS
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
BODY_ZONES_V1=PASS
BODY_ZONE_SYNC_V1=PASS
BODY_ZONES_DESKTOP_REAL_MIGRATION=PASS
BODY_ZONES_TUI_REAL_VALIDATION=PASS
BODY_ZONES_ANDROID_DEVICE_VALIDATION=PASS

TRAINING_KNOWLEDGE_V1=PASS

DESKTOP_TESTS=42/42 PASS (recorded validation checkpoint)
ANDROID_BUILD=PASS
HARDWARE_SYNC_VALIDATION=HISTORICAL_PASS
```

## Desktop

Implemented:

- C17/Notcurses true-color TUI (72x20 minimum, UTF-8 prompts, resize fallback);
- UTF-8 cell-aware scrolling training-knowledge screen, tested at the 72x20
  minimum terminal;
- SQLite schema v11, with stable ordered `session_exercises.entry_id`,
  occurrence-level equipment identity, and desktop-local custom-equipment
  definitions, plus occurrence-owned `max_results`; its v9 -> v10 migration
  rebuilds only `performed_sets` to permit explicit zero actual loads while
  preserving historic NULL and positive rows;
- canonical generated body-zone taxonomy, direct primary/secondary exercise
  relations, stable-ID-only initial migration and descendant-aware filtering;
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
- local SQLite database v10, with non-destructive v3 -> v10 migration;
- one durable active-session draft, Home resume and raw-form restoration;
- explicit confirmed discard and atomic completed-save/draft-clear;
- exercise creation;
- stable-ID exercise rename/editing with referenced-profile protection;
- primary/secondary body-zone selection, display, hierarchy filtering and
  prefix-search composition, including visible unclassified exercises;
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

## Training knowledge V1

The implemented read-only training-knowledge layer loads six versioned JSON
catalogs as the sole authored scientific source, generates the immutable C
catalog representation, and loads the same assets on Android. It has no
database migration, no auto-seeding, and no synchronization artifact. The
desktop database remains schema v11 and Android remains schema v10.

Desktop `training_knowledge.h` and Android `TrainingKnowledgeCatalog` expose
source-linked science lookups and resolved-candidate filters. Desktop
`training_context.h` and Android `TrainlogRepository` compose one real runtime
exercise with its persisted zones, compatible equipment, latest explicit MAX,
and bounded chronological occurrence/set history. Persisted zones remain
separate from scientific mappings; missing scientific knowledge is valid.

The scientific review passed and the reviewed catalog bytes remain unchanged.
The independent temporal review returned `TEMPORAL_DELTA_REVIEW=PASS`, with no
temporal defects or repairs. It reviewed the grammar, calendar and offset
bounds, fraction precision, bytewise ties, cursor aliasing and exclusivity,
source capacity, snapshots, and Android/Python parity; it ran the targeted
Meson and four Python temporal tests. The initial full-tranche engineering
audit initially failed with stale temporal-defect documentation (BLOCKER),
Android loader parity gaps (BLOCKER), omitted Meson generator inputs (BLOCKER),
and a C role-only query mismatch (HIGH). One bounded repair chain resolved all
four findings; independent repair verification returned
`FINAL_REVIEW_REPAIR_VERIFICATION=PASS` and
`TRAINING_KNOWLEDGE_V1_ENGINEERING_REVIEW=PASS`. Fresh final validation passed:
strict build, 42 Meson tests, eight knowledge and four temporal Python tests,
knowledge/JSON/import validators, three C17 headers, affected C knowledge and
context tests under ASan/UBSan plus Python timestamp validation, normal and
sanitized 12-form temporal probes, and Android 56 tests with zero failures or
errors and one known missing-real-v9-fixture skip; Java 17 `assembleDebug` also
passed. Generated C is byte-identical with SHA-256
`e8c099f67eb111d61621b5d76592c049823af5508d43e73ec646f22e4c377fca`; all six
Android assets are byte-identical. Preservation before and after repair confirms
schema v11/v10, unchanged catalog/science/temporal bytes, and unchanged real
database logical SHA-256 `26139cafeffbde3ec08f6ef23c5069e75afb9cd69ffffb40be5c099006fedc4d`,
counts, integrity, and foreign keys. `TRAINING_KNOWLEDGE_V1=PASS`. The
[temporal contract](reviews/training_knowledge_v1_temporal_contract.md)
defines the settled reader behavior. No manual Android install, manual TUI
visual validation or manual MTP validation is claimed for this tranche.

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
    trainlog-exercise-body-zones-v1.json

PC -> Android
    trainlog-pc-equipment-definitions-v1.json
    trainlog-pc-catalog-v1.json
    trainlog-pc-mobile-export-v2.json
    trainlog-equipment-associations-v2.json
    trainlog-exercise-body-zones-v1.json

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

Body-zone relations have one direction-neutral companion and are keyed only by
canonical zone IDs and `exercise_id`. Identical state is an idempotent skip; a
successful publication records that exact state as the publisher baseline, so
a later peer-only edit of a new custom exercise is accepted. Simultaneous
divergence is an explicit conflict and secondary lists are never unioned. The
unclassified state has no relations; orphan secondary rows are invalid. Parent
groups are derived from the manifest and never serialized as exercise
relations.

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
39/39 Meson tests PASS for the current desktop schema v11 baseline
JSON valid/invalid checks PASS
import-contract validator 6/6 PASS
ASan/UBSan 39/39 Meson tests PASS with leak detection
standalone public-header C17 syntax PASS
real Notcurses binary zone workflows PASS in Kitty
git diff --check PASS
```

Android:

```text
Android unit tests 44/44 PASS with a real v9 copy enabled, including body-zone
v9 -> v10 migration and sync
assembleDebug PASS
APK Signature Scheme v2 verification PASS
APK certificate SHA-256 matches the local debug keystore
APK SHA-256 5355f54e83f00ef31dc0e2f1db866c43ea3e9d880875f67878b1ffe169b5dc50
```

Current real-data/environment boundary:

```text
BODY_ZONES_DESKTOP_BACKUP=PASS
BODY_ZONES_DESKTOP_REAL_V10_TO_V11=PASS
BODY_ZONES_DESKTOP_HISTORY_ROW_EQUALITY=PASS
BODY_ZONES_DESKTOP_RELATIONS=32/20_EXERCISES
BODY_ZONES_DESKTOP_UNCLASSIFIED=3
BODY_ZONES_ANDROID_INSTALL=PASS
BODY_ZONES_ANDROID_REAL_V9_COPY_TO_V10=PASS
BODY_ZONES_ANDROID_INSTALLED_DB_V10=PASS
BODY_ZONES_LIVE_MTP_ROUNDTRIP=PASS
BODY_ZONES_LIVE_MTP_SECOND_PASS_IDEMPOTENT=PASS
REAL_ANDROID_ARTIFACT_COPY_ROUNDTRIP=PASS
PC_EXPORT_ANDROID_IMPORT_THREE_PASS_COPY=PASS
ANDROID_INSTALL_R_DATABASE_HASH_PRESERVED=PASS
```

The real desktop v10 database was backed up with SQLite to
`backups/body-zones-v1-20260909T101310Z/trainlog-v10-before.db` under the Trainlog
user-data directory (SHA-256
`9d5dcacd8be941be17bd3b98bbbc1815944fec9dc1b18133111ca481f50684e9`).
The production binary migrated it to v11; integrity/FK checks pass and every
row of all eight historical tables compares equal in both directions with the
backup. The migration added 32 direct relations for 20 of 23 exercises and
left Gym échauffement, Gym/Échauffement and Marche unclassified.

The Samsung SM-G990B application data was freshly backed up before installation
under `backups/body-zones-v1-android-20260909TVFprJ8/`. The coherent v9 SQLite
copy has SHA-256
`c402b69cdbfa4912eec1553a84754af1e892fa8da0cae6c3cbd14c4f10198401`;
its integrity check passed and its foreign-key check was empty. The installed,
built and established-keystore certificate fingerprints all matched
`aa56c97f2781a0d01f007f4444c3970deb58ad8327ca3da7b0b90f37dbe2ad25`
before `adb install -r`. The normal installed migration reached v10 with every
row of the 16 existing application tables plus `android_metadata` equal in both
directions to the v9 backup. Integrity/FK checks pass, 32 direct relations cover
20 of 23 exercises, and Gym échauffement, Gym/Échauffement and Marche remain
unclassified.

The real Android UI displayed primary, secondary and derived-group metadata;
the chest, upper-body descendant, lower-body descendant and unclassified
filters; and the `Dos` + `lat` prefix intersection. Editing `Lat pull`
preloaded `Dos` and secondary `Bras`; cancellation left every application table
unchanged. Live sync runs `sy_0b00dc46-d899-4865-8718-c95085a380b2` and
`sy_fb0630a8-1938-4d62-9836-b0281955f625` both completed successfully. The
second run reported zero additions/reconciliations, both stores retained the
same 32-relation stable-ID hash, and Android application tables plus companion
exercise states compared equal to the first pass. The engine accepts the exact
MediaStore collision family `trainlog-sync-request-v1 (N).json`; request-ID
replay protection remains authoritative.

## Current implementation cursor

Body Zones V1, exercise reconciliation, definition-first V2 synchronization
and the current real-data importer/exporter copy validation are complete. The
body-zone taxonomy now supplies the read boundary required by a future
zone-driven planner; no session generator or proposed-load calculation is part
of this tranche.

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
DESKTOP_TESTS=39/39 PASS
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
DESKTOP_TESTS=39/39 PASS
```

Android remains capture-only for this feature.

The TUI derives analytics from canonical body observations. A local
desktop-only profile provides height and the circumference-formula branch
needed for the optional body-fat estimate.

No estimated body-fat, fat-mass, lean-mass, ratio, or asymmetry value is stored
as if it were a real measurement.
