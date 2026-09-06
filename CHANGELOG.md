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
