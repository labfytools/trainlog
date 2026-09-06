# Changelog

All notable changes to Trainlog will be documented in this file.

## Unreleased

### Added

- Frozen Trainlog JSON v1 contract.
- SQLite persistence foundation.
- Direct workout entry.
- Exercise catalog.
- Body tracking.
- Colored ncursesw dashboard.
- Body-weight graph.
- Arrow-key and F1-F4 navigation.
- Highlighted list selections.
- Navigable session history.

### Changed

- Exercise selection now uses interactive keyboard navigation.
- Body tracking is now a persistent history screen instead of entry-only.
- TUI polish is prioritized before Android development.

<!-- TRAINLOG_TUI_V02_CHANGELOG -->
### TUI v0.2 checkpoint

Added:

- bordered colorful dashboard;
- arrow-key and F-key navigation;
- weight history visualization;
- flat-series weight graph handling;
- navigable session history;
- complete read-only workout details.

Planned next:

- human duration input such as `1:30`, `1m30`, `2m`, and `45s` (implemented);
- automatic minute/second display formatting (implemented);
- history graphs for all body measurements (implemented);
- left/right asymmetry summaries (implemented).
<!-- TRAINLOG_TUI_V02_CHANGELOG _END -->

<!-- TRAINLOG_GLOBAL_BODY_GRAPH_CHANGELOG -->
### Current TUI checkpoint

Implemented:
- human duration input;
- human minute/second display;
- graphs for every persisted body metric;
- recent values for every body metric;
- left/right asymmetry summaries.

Next:
- normalized global body overlay graph (implemented);
- metric legend using both colors and symbols;
- percentage evolution summary;
- richer dashboard weight graph (implemented);
- dashboard previous-weight delta and min/max (implemented);
- compact body/asymmetry dashboard summary (implemented).
<!-- TRAINLOG_GLOBAL_BODY_GRAPH_CHANGELOG _END -->

### Dashboard graph-only refinement

- removed redundant weight/min/max/asymmetry summary lines;
- dashboard now uses a global body evolution graph;
- dates are shown on X;
- Y is percentage evolution from each metric baseline;
- legend shows metric name, unit, and latest percentage change.

### Dashboard rolling 12 months

- added a fixed 12-month rolling X axis;
- empty months remain visible and empty;
- no zero fill or interpolation across missing months;
- multiple readings in one month use the last monthly value.

### Exercise performance history

- added Enter-to-open exercise performance detail;
- added mode-aware representative best-set history;
- added terminal performance graph;
- assistance explicitly treats lower assistance as better;
- best recorded set remains distinct from measured max.

### Session type schema v2

- added local `training` / `max_test` session classification;
- added transactional SQLite schema migration v1 -> v2;
- preserved migrated sessions as `training`;
- kept Trainlog JSON v1 frozen and unchanged.

### Transaction-safe session editing foundation

- added exact bounded loading of persisted exercise/set values;
- added atomic replacement of recorded session exercise/set rows;
- preserved the parent session row and linked body observations;
- added rollback coverage for failed replacements.

### Body observation history and correction

- redesigned `F4 Corps` around recorded measurement dates;
- added a framed trend graph above the newest-first record list;
- added PageUp/PageDown and a visual ncurses scrollbar;
- added two-page detail views with `e Modifier` on every page;
- added correction of existing observations without changing their timestamp or
  session link;
- added `-` to clear one erroneous measurement and Escape to cancel the edit.

<!-- TRAINLOG_EDITABILITY_NAV_CHANGELOG -->
### TUI editability and navigation checkpoint

- added local `training` / `max_test` selection to the session workflow;
- added safe persisted-session editing while preserving parent session identity;
- added newest-first body-observation history, detail pages, and editing;
- added immediate Escape cancellation to text/numeric entry paths;
- added a shared top navigation bar with `0 Accueil`, `1-4`, and `F1-F4`;
- added Tab/Shift+Tab focus on multi-zone screens;
- focused frames use a yellow border/title without recoloring content;
- added consistent ASCII banners and ncurses frames to secondary detail views;
- added direct exercise creation from the in-session exercise chooser;
- kept the final rolling 12-month `MM/YY` dashboard label inside its frame.
<!-- TRAINLOG_EDITABILITY_NAV_CHANGELOG _END -->

<!-- TRAINLOG_MTP_TRANSPORT_CHANGELOG -->
### Direct Android USB/MTP transport foundation

- added `libudev` discovery of physical MTP devices;
- filtered MTP USB interface children to avoid duplicate phones;
- added exact bus/device matching into `libmtp`;
- added MTP storage enumeration;
- added root-folder discovery/creation;
- added direct MTP text-file upload;
- added direct folder-child listing;
- added direct MTP file download;
- split cached and uncached libmtp open paths where required;
- validated a real Android internal-storage write/list/read roundtrip;
- kept Android access mount-free: no GVFS/FUSE mount lifecycle is required.
<!-- TRAINLOG_MTP_TRANSPORT_CHANGELOG _END -->

<!-- TRAINLOG_SYNC_FINAL_CHANGELOG -->
### Sync TUI page

- added `5 Sync / F5` to primary navigation;
- added dedicated Sync ASCII banner and framed page;
- added live direct-MTP device and storage status;
- suppressed libmtp terminal output during ncurses rendering;
- added focused-frame navigation with Tab/Shift+Tab;
- added clean list navigation and scrollbar behavior;
- exposed Android -> PC session/exercise/body synchronization directions;
- exposed PC -> Android canonical exercise-catalog direction;
- kept frozen Trainlog session JSON v1 unchanged.
<!-- TRAINLOG_SYNC_FINAL_CHANGELOG _END -->

<!-- TRAINLOG_PROFILE_AWARE_CHANGELOG -->
### Profile-aware and continuous exercise tracking

- added set-based versus continuous recording organization;
- added speed and distance supplemental-field metadata;
- added profiled catalog creation API;
- added profile-aware exercise creation in the TUI;
- added inline profiled exercise creation while recording a session;
- migrated SQLite persistence to schema v4;
- added one-to-one continuous activity persistence;
- kept continuous activities out of `performed_sets`;
- added profile-aware session detail loading;
- added continuous duration/speed/distance history display;
- made continuous TUI duration entry explicitly minute-based;
- preserved frozen session JSON v1 unchanged.
<!-- TRAINLOG_PROFILE_AWARE_CHANGELOG _END -->

<!-- TRAINLOG_ANDROID_LOCAL_CHANGELOG -->
### Android local client

- added native Kotlin/Jetpack Compose Android client;
- matched Trainlog TUI visual language;
- added minimal themed `T` launcher icon;
- added exercise catalog and profile-aware exercise creation;
- added inline exercise creation from session recording;
- added persistent local session recording;
- preserved SETS versus CONTINUOUS persistence semantics;
- added local session history and detail views;
- added persistent body measurement recording;
- validated the application on a real Samsung device through ADB.
<!-- TRAINLOG_ANDROID_LOCAL_CHANGELOG _END -->

<!-- TRAINLOG_SYNC_FOUNDATION_CHANGELOG -->
### Bidirectional synchronization foundation

- validated Android-to-PC domain snapshot transfer through direct MTP;
- added strict transactional and idempotent desktop mobile import;
- added canonical PC exercise-catalog export;
- validated PC-to-Android catalog publication through direct MTP;
- added Android Storage Access Framework access for PC-created catalog files;
- made the Android synchronization folder selection recoverable/changeable;
- kept synchronization artifacts separate from frozen Trainlog session JSON v1;
- documented the next synchronization architecture: structured history,
  detailed sync inspection, common sync engine and PC-side `trainlog-syncd`.
<!-- TRAINLOG_SYNC_FOUNDATION_CHANGELOG _END -->

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

<!-- TRAINLOG_SHARED_SYNC_ENGINE_V1 -->
## Shared bidirectional synchronization v1

Validated architecture:

```text
Android local write
    -> automatic mobile snapshot

Android "Synchroniser maintenant"
    -> trainlog-sync-request-v1.json

trainlog-syncd
    -> shared C synchronization engine
    -> Android → PC mobile import
    -> PC → Android catalog publish
    -> trainlog-sync-receipt-v1.json

Android
    -> receipt matched by request_id
    -> PC catalog applied locally
    -> final result displayed
```

The ncurses TUI and `trainlog-syncd` call the same
`trainlog_sync_run()` implementation.

Direct libmtp remains mandatory. No filesystem mount and no SQLite-file
synchronization are introduced.

### Concurrency

The shared engine owns:

```text
$XDG_DATA_HOME/trainlog/sync.lock
```

A TUI-triggered transaction waits for the lock. Daemon request polling is
non-blocking and retries later.

### Sync history

Every actual synchronization transaction creates:

```text
$XDG_DATA_HOME/trainlog/sync_runs/sy_*.json
$XDG_DATA_HOME/trainlog/sync_runs/sy_*.txt
```

and appends a compact entry to:

```text
$XDG_DATA_HOME/trainlog/sync_history.log
```

The TUI behaves like:

```text
git log
    ↑/↓ select synchronization

git show
    Enter opens structured detail
```

Legacy three-field history entries remain readable but have no structured
detail file.

### Android request and receipt

Request:

```text
format  = trainlog-sync-request
version = 1
```

Receipt:

```text
format  = trainlog-sync-receipt
version = 1
```

The receipt carries the originating `request_id`, a generated `sync_id`,
status, summary and synchronization counts. Android ignores a receipt for a
different request ID.

### User service

Install/refresh the user service with:

```text
bash tools/install_syncd_user.sh
```

No root privilege is required.

### Status

```text
COMMON_SYNC_ENGINE=PASS
TUI_SYNC_LOG_SHOW=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
BIDIRECTIONAL_SYNC_V1=PASS
```

Frozen `TRAINLOG_FORMAT_V1` remains unchanged.
<!-- TRAINLOG_SHARED_SYNC_ENGINE_V1 _END -->
