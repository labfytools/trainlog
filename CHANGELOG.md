# Changelog

## Unreleased — MACHINE_EXERCISE_MODEL_V1

- Add desktop and Android schema v13 for machine-specific exercise metadata.
- Preserve stable IDs for pure renames and introduce the five approved UUIDv4
  identities for Plate Loaded Leg Press, Treadmill, Pec Fly, Chin Assist, and
  Dip Assist.
- Make exact manifest splits transactional and idempotent while preserving all
  occurrence child facts and unresolved provenance.
- Remove Equipment from normal Android and TUI navigation while retaining its
  Phase 1 storage and sync compatibility roles.

- Added `PERCENT_MAX_INPUT_V1` as a transient user calculator in TUI planning
  and both generator previews: exact exercise/equipment external-load context,
  integer 1..100, `MAX × percentage / 100`, no recommendation, and only the
  resulting target kg persisted. Automatic generator policy V1 is unchanged.
- Repaired TUI `/` through the stable action registry, deduplicated actions by
  stable ID, strengthened Lavender plus textual selection states, compacted
  Android generator choices into localized chips, and made generator duration,
  shortfall, warm-up and cool-down limitations explicit.

All notable changes to Trainlog are documented here.

Detailed implementation chronology remains available in Git history and
`docs/reviews`. This file summarizes the current unreleased product baseline.

## Unreleased

### Added

- `APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2`: a shared seven-root
  application shell—Accueil, Séances, Exercices, Équipements, Statistiques,
  Synchronisation and Paramètres—on the Notcurses TUI and Android Material 3
  drawer. Séances now contains the existing generator, current/manual session
  entry and completed history; existing body and MAX views are reached from
  Statistiques. The TUI adds its run-scoped multi-plane shell, one event loop,
  stable-ID list/focus restoration, bounded UTF-8 search/forms, F6 Navigation,
  F7 shared actions, compact/sidebar thresholds, and explicit transient leave
  guards. Android adds typed controller-owned routes, local vectors, 48 dp
  actions and guarded navigation. The change adds no statistics, schema,
  synchronization artifact or exercise-domain semantics. Automated validation
  passed. A second automated repair review covered root search/action dispatch,
  F6/F7 focus/selection restoration, shell hubs/catalogues and Android compact
  presentation/latest-MAX semantics; human visual/accessibility review remains
  pending.

- `SESSION_GENERATOR_V1=PASS`: one frozen shared policy; deterministic bounded previews for
  11 BODY ZONES and four goals; explicit incomplete coverage and recorded-dose
  recency; observed exact-equipment 28-day load anchors without MAX-derived
  numeric fallback; Android normal-draft and TUI normal-editor acceptance; an
  additive Android v10 -> v11 plan migration; and separate strict V3 mobile
  exchange preserving plans with actual occurrence data. V1/V2 remain readable.

- `TRAINING_KNOWLEDGE_V1` read-only scientific knowledge infrastructure:
  six authored, versioned JSON catalogs with cited references; deterministic C
  generation; Android immutable asset loading; stable-ID catalog queries; and
  desktop/Android composition of persisted exercise zones, compatible
  equipment, explicit MAX and bounded occurrence/set history. The feature
  introduces no schema migration, database seeding, synchronization artifact,
  runtime prescription or generated UUID association. Scientific review,
  independent temporal review, final engineering review, repair verification,
  and final executable validation passed. The initial audit's four findings
  were closed by one bounded repair chain. See
  `docs/reviews/training_knowledge_v1_temporal_contract.md` and
  `docs/domain/knowledge_system.md`.

- `BODY_ZONES_V1`: the canonical `catalog/body-zones-v1.json` taxonomy with
  stable IDs, French display metadata, hierarchy, deterministic sort order and
  exact stable-exercise-ID migration evidence;
- one primary plus multiple distinct secondary body-zone relations on desktop
  and Android, with explicit unclassified history, transactional creation/edit,
  exercise detail summaries, parent/descendant filters, prefix-search
  composition, and custom-exercise support;
- one strict bidirectional `trainlog-exercise-body-zones` v1 companion carrying
  `exercise_id`, nullable primary ID and ordered secondary IDs; replay is
  idempotent, one-sided edits reconcile, simultaneous divergence conflicts,
  secondary lists are never unioned automatically, and the publishing peer
  records the exact published snapshot as its common baseline;
- generator-ready read composition for descendant and primary-only exercise
  selection, direct zone relations, existing performance history and explicit
  MAX history without duplicating historical or MAX data;
- taxonomy, schema migration, relation constraint, filtering, identity-merge,
  reopen, companion replay/update/conflict and Android repository regressions;

- explicit MAX result mode for `max_test` sessions: exercise, optional
  equipment context and positive `max_weight_kg`, with no synthetic set or
  repetition;
- Android and TUI max-only entry/edit/detail flows, Android latest max per
  exercise, and stable-ID continuation of a completed Test max;
- V2 round-trip of explicit max results in both directions, including bounded
  reconciliation of an appended or corrected continuation of the same
  max-test session;
- desktop and Android schema v9 migrations that convert only an unambiguous
  sole `1 rep × positive load` legacy result and preserve ambiguous attempts;
- explicit-max persistence, migration, sync and same-machine/different-movement
  regression coverage;

- desktop equipment v8: a supplied catalogue generated from
  `catalog/equipment-v1.json`, browse/search/detail views, and local custom
  equipment creation/selection; occurrence links resolve supplied or local
  definitions and show unknown historic references explicitly;

- multi-occurrence session V2: stable per-occurrence `entry_id`, repeated
  catalogue exercises in one session, per-set actual weights, and occurrence
  equipment associations across Android, desktop, import and export;
- structured Android and Notcurses per-set row editing: independent actual
  repetitions and nullable Charge/Assistance values, ordered add/delete/edit
  operations, and ordered history/detail presentation without target-value
  substitution;
- shared versioned equipment catalogue, Android machine selection/search,
  Android-local custom equipment creation, and explicit rejection of unknown
  equipment identities rather than silent association loss;

- `trainlog-equipment-definitions` v1, with directional Android-to-PC and
  PC-to-Android filenames, strict stable definition fields, additive
  reconciliation, reserved supplied-manifest IDs, and explicit divergence
  conflicts;
- explicit TUI/shared-engine synchronization actions: `a` Android -> PC, `p`
  PC -> Android, and `b` inbound then outbound bidirectional synchronization;
  each opens one direction-specific confirmation, `Enter` runs it once, `Esc`
  cancels without an operation, `r` only refreshes device status, and the
  former `s` shortcut is inert;
- bounded V2 exercise reconciliation for distinct IDs with equal normalized
  names: modes and represented invariants must match, field masks must be
  comparable, desktop identity stays canonical, and the richer bit-mask union
  is retained without changing historical occurrence snapshots;
- regression coverage for identical/different-ID profiles, compatible
  subset/superset masks, incompatible collisions, the real Marche identity
  pair, full Android -> PC import and stable PC ↔ Android replay;

- Android `EXERCISE_EDIT_V1`: visible catalog editing, stable-ID name rename,
  explicit invalid/conflict/profile/database results, and profile locking once
  completed history or an active draft references the exercise;
- `ANDROID_BANNER_PARITY_V1`: the earlier Android `◆ TRAINLOG ◆` header
  component matched the compact Notcurses banner's accent and muted context
  rhythm. APP_SHELL_V1 supersedes that per-page presentation with the fixed
  Material 3 `AndroidAppShell` top bar;

- one durable Android active-session draft with Home resume, raw form restore,
  confirmed discard and draft-only exercise removal;

- native Kotlin/Compose Android capture client;
- Android local exercise, session, continuous-activity, and body persistence;
- C17/Notcurses desktop TUI with direct session entry and durable SQLite history;
- profile-aware exercise model using recording mode, tracking mode, and
  supplemental fields;
- continuous activity persistence without synthetic sets;
- explicit table-based desktop actual-set entry, with independently added rows
  and no compact performed-repetition input;
- persisted desktop session editing and exercise removal;
- Android current-session draft exercise removal;
- body-observation history, editing, graphs, and normalized overlays;
- exercise performance history;
- direct physical Android USB/MTP discovery with libudev/libmtp;
- direct MTP read/write/list/delete support without filesystem mounts;
- Android full mobile snapshot export;
- strict idempotent desktop mobile importer;
- PC canonical exercise-catalog publication to Android;
- automatic Android snapshot maintenance;
- Android `Synchroniser maintenant` request flow;
- shared C bidirectional synchronization engine;
- `trainlog-syncd` user-session synchronization agent;
- request/receipt synchronization protocol;
- stable `sy_<uuid-v4>` synchronization identities;
- structured synchronization history with selectable TUI list/detail views.

### Changed

- desktop schema v11 additively introduces `exercise_body_zones` and its
  internal sync-baseline table; Android schema v10 introduces the equivalent
  relations. Both migrations seed only manifest mappings proven by stable
  `exercise_id`, leave uncertain exercises unclassified, and preserve all
  session, occurrence, set, MAX, equipment and body-observation identities;
- Android and TUI exercise catalog workflows now select translated manifest
  values, derive group labels, exclude a primary from secondary selection and
  combine hierarchy-aware zone filtering with normalized prefix search;

- desktop schema v10 losslessly rebuilds only `performed_sets` to accept an
  explicit zero actual `weight_kg`; historic NULL and positive actual loads
  remain unchanged, while planned targets and explicit MAX results stay
  strictly positive;
- desktop and Android schema v9 add one-to-one completed/draft max-result rows;
  `TRAINLOG_FORMAT_V1` remains frozen and V1 export refuses explicit MAX data
  rather than losing or fabricating it;
- desktop and Android schema v8 preserve existing equipment references while
  permitting custom definitions with `load_semantics = none`; Android's v7 ->
  v8 migration is non-destructive;
- custom definitions reconcile before V2 mobile/association artifacts in each
  direction. The V2 mobile and association shapes are unchanged;

- desktop SQLite schema v7 and Android SQLite schema v7 preserve historic rows
  while adding durable occurrence identities and occurrence-level equipment;
- the then-active Android↔PC completed-session exchange was V2; frozen V1
  artifacts remain readable as historical formats and are not redefined for
  repeated occurrences; the current active artifact is separately versioned V3;
- synchronization invokes each local helper with the explicit XDG-resolved
  desktop database path and records the concrete equipment-import failure;

- same-ID Android ↔ PC catalog reconciliation updates display-name metadata in
  place. Compatible different-ID normalized-name collisions now merge through
  the explicit V2 policy; incompatible collisions still reject atomically;

- Android local SQLite v3 -> v4 additive migration for structured active drafts;
- completed-session insertion and draft clearing are atomic; drafts remain
  excluded from completed history and frozen mobile export;

- desktop SQLite schema evolved to v5;
- schema v5 permits targetless set-session rows for actual-only mobile data;
- heterogeneous performed sets are preserved without inventing a uniform target;
- Android/desktop synchronization uses dedicated versioned artifacts instead of
  modifying frozen Trainlog JSON v1;
- PC and TUI synchronization paths now share one engine;
- Android PC-created artifact access uses a persistent SAF grant for
  `Download/Trainlog`;
- the Android data package is no longer accidentally hidden by a broad
  repository `data/` ignore rule;
- user-facing session history timestamps use `DD/MM/YYYY HH:MM` while canonical
  storage remains RFC3339.

### Fixed

- a newly published custom-exercise zone state now establishes the publisher's
  comparison baseline as well as the receiver's; a later peer-only edit no
  longer produces a false simultaneous-conflict result;
- secondary-only body-zone states are rejected consistently by C, Kotlin and
  Python boundaries instead of being persisted but hidden by exercise detail;
- touched migration/custom-equipment/body-metric/TUI C regressions now return
  a failing process status from `main()`; the repaired workflow fixture uses a
  real in-memory database instead of passing an invalid null handle;
- synchronization summaries no longer add exercise reconciliations to the
  `exercices ajoutés` value. A compatible different-ID lookup that makes no
  persistent change is reported as an idempotent skip, while real insertions
  and real reconciliation mutations retain distinct counters. A production
  PC-exporter/Android-importer regression proves zero additions and exact
  business-table stability on the second and third imports;
- equipment companion import previously omitted its required `--database`
  argument and blocked synchronization after a successful session import;
- PC catalogue/mobile export paths now accept schema v7 and preserve catalogue
  tracking metadata; Android completed-session equipment editing now targets
  the stable occurrence `entry_id`, not an ambiguous catalogue exercise ID;
- Android scoped-storage suffix selection now covers equipment associations as
  well as mobile/definition artifacts; a historic V1 snapshot cannot consume a
  neighboring V2 equipment companion;
- desktop-generated session occurrences now receive `sxe_<uuid-v4>` rather
  than the synchronization-run `sy_` prefix; public generator capacity and C17
  regression coverage include the three-character occurrence prefix;
- TUI footer help now advertises every active direct function key through F5;

- in-progress Android workout loss when leaving the foreground or recreating
  the Activity/process;
- missing selected-exercise recovery preserves raw partial input and reports a
  specific warning; draft write/finalization failures return explicit errors;

- stale schema-v4 importer call after desktop schema v5 migration;
- stale schema-v4 guard in the PC catalog exporter;
- missing `sy` prefix support in the UUID creator;
- synchronization failures that previously surfaced only as `error=unknown`;
- Android folder-selection UX so a wrong SAF folder can be changed without
  clearing application data;
- libmtp terminal output leaking into terminal rendering.

### Validation

Current validated baseline:

```text
TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V11=PASS
DESKTOP_TESTS=39/39 PASS

ANDROID_BUILD=PASS
ANDROID_LOCAL_WORKFLOWS=PASS
ANDROID_LOCAL_DATABASE_V10=PASS
ANDROID_TEST_DEBUG_UNIT=44/44 PASS (real v9 fixture enabled)
ANDROID_SESSION_DRAFT_V1=PASS
ANDROID_MAX_V9_REAL_DATA_MIGRATION=PASS
ANDROID_MAX_V9_INSTALL_ADB=PASS

USB_MTP_DETECTION=PASS
MTP_HARDWARE_ROUNDTRIP=HISTORICAL_PASS
REAL_ANDROID_ARTIFACT_COPY_ROUNDTRIP=HISTORICAL_PASS
BODY_ZONES_DESKTOP_REAL_DATABASE=PASS
BODY_ZONES_ANDROID_DEVICE=PASS

ANDROID_TO_PC_MTP=HISTORICAL_PASS
PC_TO_ANDROID_MTP_PUBLISH=HISTORICAL_PASS
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
BODY_ZONES_V1=PASS
BODY_ZONE_SYNC_V1=PASS
BODY_ZONES_DESKTOP_REAL_MIGRATION=PASS
BODY_ZONES_ANDROID_DEVICE_VALIDATION=PASS
```

For Body Zones V1, the real desktop database was backed up coherently and
migrated v10 -> v11 through the production binary. All eight pre-existing
tables compare equal row-for-row with the backup; integrity/FK checks pass and
the migration adds 32 direct relations for 20 of 23 exercises. On the Samsung
SM-G990B, the matching signed APK was installed with `adb install -r`; the real
Android database migrated v9 -> v10 with every pre-existing application row
unchanged, 32 relations for the same 20 exercises and the expected three
unclassified exercises. The Android zone/detail/filter/edit-cancel matrix and
two live PC <-> Android MTP passes completed successfully. The second pass
reported no additions or reconciliations and left every Android application
table and the semantic Body Zones companion unchanged. Scoped storage had
published the requests as exact `(N).json` collision siblings; the shared
engine now selects the newest request with the same deterministic helper used
for other Android-originated artifacts.

### Measured max v1

Added:

- explicit measured-max derivation from `max_test` sessions only;
- newest successful measured result and same-mode historical record;
- dedicated TUI measured-max history and graph;
- external working-load calculations at 60/70/80/90%;
- selectable 0.5/1/2.5/5.0 kg working-load rounding;
- direction-aware assistance measured-max semantics;
- Android `Entraînement` / `Test max` session selection;
- Android history/detail max-test identification;
- measured-max regression coverage.

No estimated 1RM, schema v6, or frozen Trainlog JSON v1 change was introduced.

### Body analytics v1

Added:

- desktop-only body analytics view;
- local height/formula estimation profile;
- circumference-based body-fat estimate;
- estimated fat and lean mass when real weight is available;
- waist/hip, shoulder/waist, and chest/waist ratios;
- arm, forearm, thigh, and calf left/right asymmetry percentages;
- weight, waist, and estimated-body-fat trend deltas;
- body analytics regression test.

Estimated analytics remain derived display values and are never persisted as
direct measurements. Android remains capture-only for this feature.

- Hid `SESSION_GENERATOR_V1` from normal UI pending V2; manual `%MAX` entry is unchanged.
- Added versioned canonical exercise display names, stale-peer-name convergence,
  and the STATS_V1 query-contract preparation (no analytics implementation).
