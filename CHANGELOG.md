# Changelog

All notable changes to Trainlog are documented here.

Detailed implementation chronology remains available in Git history and
`docs/reviews`. This file summarizes the current unreleased product baseline.

## Unreleased

### Added

- desktop equipment v8: a supplied catalogue generated from
  `catalog/equipment-v1.json`, browse/search/detail views, and local custom
  equipment creation/selection; occurrence links resolve supplied or local
  definitions and show unknown historic references explicitly;

- multi-occurrence session V2: stable per-occurrence `entry_id`, repeated
  catalogue exercises in one session, per-set actual weights, and occurrence
  equipment associations across Android, desktop, import and export;
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
- `ANDROID_BANNER_PARITY_V1`: one Android `◆ TRAINLOG ◆` header component
  matching the compact Notcurses banner's accent and muted context rhythm;

- one durable Android active-session draft with Home resume, raw form restore,
  confirmed discard and draft-only exercise removal;

- native Kotlin/Compose Android capture client;
- Android local exercise, session, continuous-activity, and body persistence;
- C17/Notcurses desktop TUI with direct session entry and durable SQLite history;
- profile-aware exercise model using recording mode, tracking mode, and
  supplemental fields;
- continuous activity persistence without synthetic sets;
- variable repetition-set input including `5x10`, explicit lists, and pyramid
  shorthand such as `4..10..4`;
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

- desktop and Android schema v8 preserve existing equipment references while
  permitting custom definitions with `load_semantics = none`; Android's v7 ->
  v8 migration is non-destructive;
- custom definitions reconcile before V2 mobile/association artifacts in each
  direction. The V2 mobile and association shapes are unchanged;

- desktop SQLite schema v7 and Android SQLite schema v7 preserve historic rows
  while adding durable occurrence identities and occurrence-level equipment;
- the active Android↔PC completed-session exchange is V2; frozen V1 artifacts
  remain readable as historical formats and are not redefined for repeated
  occurrences;
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

DESKTOP_SCHEMA_V8=PASS
DESKTOP_TESTS=32/32 PASS

ANDROID_BUILD=PASS
ANDROID_LOCAL_WORKFLOWS=PASS
ANDROID_LOCAL_DATABASE_V8=PASS
ANDROID_TEST_DEBUG_UNIT=PASS
ANDROID_SESSION_DRAFT_V1=PASS

USB_MTP_DETECTION=PASS
MTP_HARDWARE_ROUNDTRIP=HISTORICAL_PASS
CURRENT_SANDBOX_LIBMTP_OPEN=BLOCKED
REAL_ANDROID_ARTIFACT_COPY_ROUNDTRIP=PASS
REAL_DATABASE_APPLICATION=BLOCKED_BY_SANDBOX_WRITE_BOUNDARY

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
```

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
