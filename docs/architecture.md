# Architecture

## 1. System boundary

Trainlog separates capture, durable history, transport, and presentation.

```text
Android capture client
        |
        | local SQLite
        |
        +-- automatic mobile snapshot
                    |
                    v
             Android shared storage
             Download/Trainlog
                    |
                    | direct MTP
                    v
            shared desktop sync engine
               /               \
              /                 \
     desktop SQLite       PC catalog artifact
          |                      |
          v                      v
      desktop TUI             Android
```

The desktop SQLite database is the canonical long-term history.

Android local SQLite is a capture store, not a synchronization format.

## 2. Components

### Android application

Responsibilities:

- exercise catalog entry;
- workout-session recording;
- performed set entry;
- continuous-activity entry;
- body measurement entry;
- local history/detail;
- automatic mobile snapshot generation;
- PC catalog application;
- synchronization request creation;
- synchronization receipt display.

Android is not responsible for canonical long-term analytics.

### Desktop core

The C17 core owns:

- desktop SQLite persistence;
- exercise/catalog rules;
- profile-aware session data;
- body data;
- ID and time helpers;
- USB discovery;
- direct MTP operations;
- the shared bidirectional synchronization engine.

### TUI

The ncursesw layer owns interaction and rendering.

It consumes core services for:

- session entry/editing;
- history;
- exercise performance;
- body tracking;
- graphs;
- manual synchronization;
- synchronization log/detail display.

### `trainlog-syncd`

`trainlog-syncd` is a small user-session agent.

It polls for a new Android request and invokes the same shared C synchronization
engine used by the TUI.

It does not implement a second synchronization algorithm.

## 3. Exercise model

Trainlog is metadata-driven:

```text
recording_mode = SETS | CONTINUOUS
tracking_mode  = REPS | DURATION
data_fields    = SPEED_KMH | DISTANCE_KM
```

Valid model-v1 combinations:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

Load mode is session-specific:

```text
none
external
assistance
```

Continuous work is persisted separately from performed sets.

## 4. Persistence ownership

### Desktop

Desktop SQLite schema v5 is canonical long-term history.

Main tables:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
body_observations
```

### Android

Android has an independent local SQLite schema.

It mirrors domain concepts needed for capture, but its schema version is not
coupled to the desktop schema.

Synchronization exchanges domain artifacts rather than database files.

## 5. Compatibility boundaries

### Frozen Trainlog JSON v1

`TRAINLOG_FORMAT_V1` is frozen and remains a compatibility boundary for its
existing set-based session contract.

### Synchronization artifacts

Synchronization uses separate formats:

```text
trainlog-mobile-export v1
trainlog-pc-catalog v1
trainlog-sync-request v1
trainlog-sync-receipt v1
```

A new domain requirement must not be forced into frozen v1 by using notes,
synthetic sets, or data loss.

## 6. Direct MTP transport

Linux transport:

```text
physical Android USB device
        |
        v
libudev discovery
        |
        | bus + device number
        v
libmtp exact raw-device access
        |
        v
Android internal storage
```

No GVFS/FUSE mount is required.

Canonical exchange directory:

```text
Download/Trainlog
```

## 7. Shared synchronization engine

Both user-trigger paths call:

```text
trainlog_sync_run()
```

Manual path:

```text
TUI -> trainlog_sync_run()
```

Android-triggered path:

```text
Android request
    -> trainlog-syncd
    -> trainlog_sync_run()
    -> receipt
```

The engine performs:

```text
1. direct-MTP device/storage discovery
2. exchange-folder resolution
3. mobile snapshot download
4. strict transactional Android -> PC import
5. PC catalog export
6. direct-MTP PC catalog publication
7. optional request receipt publication
8. structured run-history recording
```

## 8. Synchronization concurrency

The shared engine serializes synchronization with:

```text
$XDG_DATA_HOME/trainlog/sync.lock
```

The TUI waits for an active transaction.

Daemon polling uses non-blocking acquisition and retries later.

A request ID already successfully consumed is not processed as a new request.

## 9. Synchronization history

Every actual run gets a stable:

```text
sy_<uuid-v4>
```

Structured history is stored under:

```text
$XDG_DATA_HOME/trainlog/sync_runs/
```

The TUI exposes list/detail semantics comparable to:

```text
git log
git show
```

## 10. Error philosophy

Trainlog prefers explicit failure over silent corruption.

Hard validation or persistence failure aborts the relevant transaction.

The synchronization layer records useful failure detail rather than masking
known errors with generic placeholders.

## 11. Layering rule

```text
ncurses / Compose rendering
          |
          v
application workflow
          |
          +-- domain model
          +-- synchronization
          +-- validation
          |
          v
persistence / MTP transport
```

Rendering does not own persistence rules.

Persistence and MTP code do not depend on ncurses rendering.

## 12. Body analytics boundary

Body analytics belong to the desktop analysis layer.

```text
Android
    real measurements only
        |
        v
desktop body_observations
        |
        v
pure derived analytics
        |
        v
TUI display
```

The analytics layer does not modify canonical observations.

A desktop-only configuration file stores the estimation profile:

```text
$XDG_CONFIG_HOME/trainlog/body_analytics.conf
```

with `~/.config` fallback.

This profile is not synchronized to Android and does not require a SQLite
schema change.
