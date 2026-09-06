# Architecture

## 1. Purpose

Trainlog separates capture from analysis.

The Android application is optimized for fast data entry during training.

The TUI is optimized for durable storage, inspection, statistics, and visualization.

## 2. Components

### Android client

Responsibilities:

- start a session;
- record the session start timestamp;
- select an existing exercise;
- create a new exercise;
- record workout targets;
- record actual performed sets;
- record rest duration;
- record body data;
- record the session end timestamp;
- export one valid Trainlog JSON document.

Non-responsibilities:

- long-term analytics;
- canonical history;
- complex graphing;
- cloud synchronization.

### Exchange format

The exchange format is the compatibility boundary between Android and the TUI.

It is:

- JSON;
- UTF-8;
- versioned;
- self-contained enough to import newly created exercises;
- designed for idempotent import.

### TUI

Responsibilities:

- import Trainlog JSON;
- reject malformed or incompatible input cleanly;
- deduplicate sessions;
- maintain the canonical exercise catalog;
- create workouts directly from the terminal;
- maintain SQLite history;
- calculate progress metrics;
- render graphs and summaries;
- export data when needed.

### SQLite store

SQLite is the canonical local history.

The database must use:

- foreign keys;
- uniqueness constraints;
- schema versioning;
- explicit migration rules.

## 3. Data flow

```text
Android
  |
  | export
  v
Trainlog JSON
  |
  | import + validation
  v
TUI application
  |
  | persistence
  v
SQLite
```

## 4. Identity rules

Exercises have:

- a stable machine identifier: `exercise_id`;
- a mutable display name: `name`.

The display name is not the identity.

Sessions have:

- a globally unique `session_id`.

A second import of the same `session_id` must not duplicate the session.

## 5. Separation rules for the TUI

The C17 TUI will be split into layers:

```text
ncursesw rendering
       |
       v
TUI state / navigation
       |
       v
application services
       |
       +---- exchange-format parser
       |
       +---- analytics
       |
       v
SQLite persistence
```

The rendering layer must not own business rules.

The persistence layer must not depend on ncurses.

## 6. Error philosophy

Trainlog must prefer explicit failure over silent corruption.

Examples:

- malformed JSON: reject import with a precise error;
- unsupported format version: reject import;
- duplicate session: report already imported, do not duplicate;
- unknown exercise: import it when valid catalog data is present;
- incomplete active session: preserve it explicitly rather than silently inventing an end time.

<!-- TRAINLOG_DIRECT_MTP_ARCHITECTURE -->
## 7. Direct Android USB/MTP transport

The Linux side does not require the Android device to be mounted as a normal
filesystem.

Transport layering is:

```text
Android USB file-transfer mode
        |
        v
libudev physical-device discovery
        |
        | bus number + device number
        v
libmtp exact raw-device open
        |
        v
Android internal MTP storage
```

This avoids GVFS/FUSE mount state and manual mount/unmount lifecycle management.

`libudev` owns physical-device discovery. `libmtp` owns storage and object
operations. The JSON exchange layer remains above both and stays independent
from USB/MTP backend details.

Current transport foundation supports folder creation, file upload, folder
listing, file download, and verified byte-for-byte roundtrip.
<!-- TRAINLOG_DIRECT_MTP_ARCHITECTURE _END -->

<!-- TRAINLOG_PROFILE_AWARE_ARCHITECTURE -->
## Profile-aware activity architecture

Trainlog has two distinct actual-work persistence paths:

```text
SET-based exercise
    session_exercises
        |
        +--> performed_sets [0..N]

CONTINUOUS exercise
    session_exercises
        |
        +--> continuous_activity [exactly 1]
```

The two paths must remain semantically distinct.

Catalog metadata determines future entry forms.

Session-exercise snapshot metadata determines historical rendering/editing.

The Android client must consume the same catalog profile metadata rather than
maintaining an independent exercise-type system.
<!-- TRAINLOG_PROFILE_AWARE_ARCHITECTURE _END -->
