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
- normalized global body overlay graph;
- metric legend using both colors and symbols;
- percentage evolution summary;
- richer dashboard weight graph;
- dashboard previous-weight delta and min/max;
- compact body/asymmetry dashboard summary.
<!-- TRAINLOG_GLOBAL_BODY_GRAPH_CHANGELOG _END -->
