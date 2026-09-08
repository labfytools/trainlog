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
     desktop SQLite       directional PC artifacts
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
- stable-ID exercise rename/editing;
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

The Notcurses layer owns interaction and rendering. It is confined to the
desktop executable; persistence, synchronization, and core services have no
terminal-library dependency.

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

Desktop SQLite schema v8 is canonical long-term history. `session_exercises`
stores a stable occurrence `entry_id`; a catalogue `exercise_id` can therefore
occur more than once in one session without identity fusion.

Main tables:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
body_observations
custom_equipment
```

### Android

Android has an independent local SQLite schema, currently v8.

It mirrors domain concepts needed for capture, but its schema version is not
coupled to the desktop schema.

Synchronization exchanges domain artifacts rather than database files.

`TrainlogRepository` owns a singleton active-session draft, its ordered exercise
and actual-value children, and raw form text. Compose sends meaningful mutations
to that repository; lifecycle callbacks are not the sole persistence boundary.
Normal navigation never deletes the draft. Home restores the resume affordance
from SQLite after process recreation.

Drafts use separate tables from completed sessions and are never export sources.
Finalization inserts the completed session and deletes the draft in one
transaction; failures retain the draft. Catalog row references preserve draft
identity through existing PC-catalog reconciliation. Missing editing-selection
recovery preserves the raw fields and added exercises with a specific warning.

`TrainlogRepository.editExercise()` owns all Android exercise edits. It changes
the display name and normalized form in the existing catalog row identified by
`exercise_id`; foreign-key ownership consequently preserves completed history
and active drafts. A profile edit is admitted only before that row is referenced
by either completed or active-draft data. Same-ID reconciliation applies name
metadata in place. A different-ID normalized-name collision may merge only for
equal recording/tracking modes, compatible represented invariants, and
`data_fields` masks comparable by inclusion. Desktop ownership wins on both
sides; the richer mask is retained and every occurrence/draft reference moves
transactionally. Otherwise synchronization reports a conflict.

Compose presentation has one `TrainlogScreen` header component for every page.
It uses the TUI's compact accent `◆ TRAINLOG ◆` plaque and muted context line;
screen navigation and data ownership remain independent from the header.

## 5. Compatibility boundaries

### Frozen Trainlog JSON v1

`TRAINLOG_FORMAT_V1` is frozen and remains a compatibility boundary for its
existing set-based session contract.

### Synchronization artifacts

Synchronization uses separate formats:

```text
trainlog-mobile-export v1
trainlog-mobile-export v2
trainlog-pc-catalog v1
trainlog-equipment-associations v2
trainlog-equipment-definitions v1
trainlog-sync-request v1
trainlog-sync-receipt v1
```

A new domain requirement must not be forced into frozen v1 by using notes,
synthetic sets, or data loss.

The active occurrence-aware session exchange remains
`trainlog-mobile-export` v2. The separate directional
`trainlog-equipment-definitions` v1 artifacts carry user-created equipment
definitions: `trainlog-mobile-equipment-definitions-v1.json` travels from
Android to PC and `trainlog-pc-equipment-definitions-v1.json` travels from PC
to Android. The filename identifies direction; the JSON format and version do
not change. V1 historical artifacts remain readable under their frozen
contracts.

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

The same engine owns all supported directions. Its explicit modes are:

```text
a = Android -> PC only
p = PC -> Android only
b = Android -> PC, then PC -> Android
```

The Android-to-PC direction receives the mobile equipment-definitions v1
artifact, mobile-export v2, and equipment-associations v2. The PC-to-Android
direction publishes PC equipment-definitions v1 before dependent artifacts,
then publishes the PC catalog v1, PC mobile-export v2 (including completed
sessions and body observations), and equipment-associations v2.

The engine performs the applicable direction steps:

```text
1. direct-MTP device/storage discovery
2. exchange-folder resolution
3. definition artifact transfer and reconciliation before dependent V2 data
4. strict transactional Android -> PC import when selected
5. PC catalog, mobile/body, and association export when selected
6. direct-MTP publication of the selected PC -> Android artifacts
7. optional request receipt publication
8. structured run-history recording
```

Synchronization is additive and reconciliatory: artifact absence never implies
deletion of local content. Equal same-ID definitions are idempotent; divergent
same-ID definitions, unknown references, and association conflicts are reported
explicitly. The affected persisted content is preserved on conflict; the engine
does not silently overwrite it.

There is no exercise, session, body-observation, or equipment-definition
tombstone in the current formats. Omitting one of those objects from a later
snapshot is not a deletion request. The only explicit removal signal is
equipment-association V2 `state: cleared`, scoped to one
`(session_id, entry_id)` occurrence.

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
Notcurses / Compose rendering
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

Persistence and MTP code do not depend on Notcurses rendering.

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

## 13. Audited evolution constraints

Each mutating importer has strict validation and a SQLite transaction. The V2
association companion is a strict corroboration of the equipment already
carried by the mobile snapshot and does not rewrite divergent state. However,
one definitions/mobile/associations publication has no common generation
manifest and is not one cross-artifact database transaction. A late association
conflict can therefore follow a successfully committed definitions or mobile
import; replay remains idempotent and existing conflicting content is not
overwritten. A future batch protocol must be separately versioned rather than
retrofitted into frozen formats.

Android and desktop also have different stored name-normalization behavior:
Android removes diacritics, while the frozen desktop/Python rule preserves them
through NFC plus Unicode case folding. Correcting existing Android keys requires
an explicit migration and collision policy. Finally, Android retains supplied
exercise/equipment relationship tables, but its current picker searches the
complete definition catalog; applying those relations as recommendations is
future gym-catalog behavior.

Custom-equipment reconciliation is deliberately ID-based. Different custom
IDs are not merged by equal or similar display names because their load
semantics and existing occurrence references may differ. The current PC
catalog V1 Android reader also validates its required semantic fields without
the exact-key rejection used by newer V2/companion readers; stricter V1 parsing
needs an explicit compatibility review.

The desktop model/API currently permits bounded supplemental field masks on a
`SETS` profile, while Android creation and V2 inbox validation require those
masks to be zero for `SETS`. The shipped catalog uses supplemental speed and
distance only with `CONTINUOUS`; defining cross-platform behavior for a future
set-based supplemental field is a model-contract decision, not part of this
reconciliation.
