# Changelog

All notable changes to Trainlog are documented here.

Detailed implementation chronology remains available in Git history and
`docs/reviews`. This file summarizes the current unreleased product baseline.

## Unreleased

### Added

- native Kotlin/Compose Android capture client;
- Android local exercise, session, continuous-activity, and body persistence;
- C17/ncursesw desktop TUI with direct session entry and durable SQLite history;
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

- stale schema-v4 importer call after desktop schema v5 migration;
- stale schema-v4 guard in the PC catalog exporter;
- missing `sy` prefix support in the UUID creator;
- synchronization failures that previously surfaced only as `error=unknown`;
- Android folder-selection UX so a wrong SAF folder can be changed without
  clearing application data;
- libmtp terminal output leaking into ncurses rendering.

### Validation

Current validated baseline:

```text
TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V5=PASS
DESKTOP_TESTS=19/19 PASS

ANDROID_BUILD=PASS
ANDROID_LOCAL_WORKFLOWS=PASS

USB_MTP_DETECTION=PASS
MTP_ROUNDTRIP=PASS

ANDROID_TO_PC_MTP=PASS
PC_TO_ANDROID_MTP_PUBLISH=PASS
COMMON_SYNC_ENGINE=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
TUI_SYNC_LOG_SHOW=PASS
BIDIRECTIONAL_SYNC_V1=PASS
```
