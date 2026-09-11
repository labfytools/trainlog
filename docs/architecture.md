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
- canonical body-zone selection, display and descendant filtering;
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
- generated body-zone taxonomy and relation APIs;
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
- exercise body-zone creation/edit/detail and filters;
- body tracking;
- graphs;
- manual synchronization;
- synchronization log/detail display.

APP_SHELL_V1 places these existing capabilities below seven platform-neutral
sections: Accueil, Séances, Exercices, Équipements, Statistiques,
Synchronisation and Paramètres. The controller owns route history, focus,
overlays and transient leave guards; rendering owns no persistence, SQLite,
transport, or synchronization decisions. Navigation and redraw alone never
write a database or run synchronization. Session drafts, generator previews,
and transient forms use explicit keep/discard guards; discard of transient TUI
state performs no database write. Sync operations retain their existing
direction confirmation and diagnostic ownership.

The desktop controller has one event loop and consumes Trainlog input semantics
before screen code. Overlay input precedes route aliases, local editors precede
global aliases, and overlay close restores saved focus/stable selection. Its
layout is compact from 72x20, has a sidebar from 100x26, and expands it from
120x32. Its bounded UTF-8 adapter, stable-ID list state, shared action registry
and run-scoped terminal planes keep Notcurses infrastructure separate from
product semantics.

Android's `AppNavigationController` is the corresponding route owner. Its
Material 3 drawer exposes the same seven roots and derives drawer selection
from the route. It uses local vector resources, a sans-serif hierarchy and
minimum 48 dp actions; it has no runtime icon/parser dependency and accepts a
text fallback where an optional icon font is unavailable. Route transitions
preserve durable drafts and require keep/discard resolution for non-durable
forms or generator previews. The Android shell maps the same semantic color
roles to Material 3 tokens; neither platform treats color as the only state
carrier.

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

Body-zone semantics are a second independent metadata axis:

```text
exercise -> exercise_body_zones -> canonical body-zone manifest
role = primary | secondary
```

The repository-level `catalog/body-zones-v1.json` is the sole taxonomy source.
Android reads it as an asset and the desktop generates a bounded C
representation at build time. Stable `zone_id` values cross persistence and
synchronization boundaries; translated display names do not. `upper_body` and
`lower_body` are hierarchy groups whose descendant membership is derived at
query time, never persisted redundantly. `full_body` and `core` are autonomous.
An unclassified exercise has no relation at all: a secondary-only state is
invalid at repository, database and exchange boundaries rather than being
silently rendered as unclassified.

## 4. Persistence ownership

### Desktop

Desktop SQLite schema v12 is canonical long-term history. Its v9 -> v10
migration losslessly rebuilds only `performed_sets` so actual `weight_kg` may
be finite `>= 0`; the column already existed and targets/max results retain
their strictly-positive contracts. `session_exercises`
stores a stable occurrence `entry_id`; a catalogue `exercise_id` can therefore
occur more than once in one session without identity fusion.

The additive v10 -> v11 migration creates direct exercise/body-zone relations
and a private synchronization baseline, then seeds only stable-ID mappings
whose decision evidence is recorded in the manifest. It never rewrites an
exercise, occurrence or history row.

The additive v11 -> v12 migration adds only persistent exercise aliases. An
explicit source-to-canonical merge validates the complete recording profile and
primary BODY ZONE, unions compatible secondary zones, repoints occurrence
foreign keys, and then retires the source catalogue row. Entry IDs, sessions,
sets, continuous activity, MAX, equipment and targets remain unchanged.

Main tables:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
max_results
body_observations
custom_equipment
exercise_body_zones
exercise_body_zone_sync
```

### Android

Android has an independent local SQLite schema, currently v12. Completed and
draft MAX values use one-to-one `max_results` and `draft_max_results` rows;
resuming a completed Test max records its stable source session in the one
durable draft.

Its v9 -> v10 migration adds equivalent exercise/body-zone relations and seeds
the same manifest mappings. User creation and editing replace name/profile and
zone relations in one repository transaction.

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

Compose presentation has one fixed `AndroidAppShell` Material 3 `Scaffold`
with a `TopAppBar` above its page-content host. Root routes show `TRAINLOG`;
non-root routes show their route title. Screen navigation and data ownership
remain independent from this shell.

## 5. Compatibility boundaries

### Session generator V1

`SESSION_GENERATOR_V1=PASS`. `catalog/session-generation-policy-v1.json` is its sole authored
policy source; generated C data and Android asset loading consume that same
policy. The generator composes read-only runtime/knowledge context, complete
history, deterministic selection and explicit uncertainty into an in-memory
proposal. Preview writes nothing. Android acceptance creates the normal
singleton draft atomically; TUI acceptance enters the normal editor. There is
no generated-session persistence silo.

Exposure counts actual positive-repetition completed SETS+REPS rows only.
The exclusive thresholds are primary/secondary 1/3 at 24 hours and 6/12 at 72
hours; primary produces `warning`, secondary-only produces `notice`, and `none`
is available only after successful analysis. Invalid stored time fails analysis;
these signals do not estimate recovery. Selection is bounded to six exercises,
uses requested-zone/group coverage, diversity, compatible equipment, exclusions,
preferences, recency and deterministic ID ties, and returns shortages rather
than invented candidates. Numeric weight is only an unchanged minimum observed
qualifying actual external-load dose for the exact exercise/equipment within
28 days; MAX, assistance and unknown context do not prescribe a number.

### Frozen Trainlog JSON v1

`TRAINLOG_FORMAT_V1` is frozen and remains a compatibility boundary for its
existing set-based session contract.

### Synchronization artifacts

Synchronization uses separate formats:

```text
trainlog-mobile-export v1
trainlog-mobile-export v2
trainlog-mobile-export v3
trainlog-pc-catalog v1
trainlog-equipment-associations v2
trainlog-equipment-definitions v1
trainlog-exercise-body-zones v1
trainlog-sync-request v1
trainlog-sync-receipt v1
```

A new domain requirement must not be forced into frozen v1 by using notes,
synthetic sets, or data loss.

The active occurrence-aware session exchange is
`trainlog-mobile-export` v3. V2 remains a readable historical artifact. The separate directional
`trainlog-equipment-definitions` v1 artifacts carry user-created equipment
definitions: `trainlog-mobile-equipment-definitions-v1.json` travels from
Android to PC and `trainlog-pc-equipment-definitions-v1.json` travels from PC
to Android. The filename identifies direction; the JSON format and version do
not change. V1 historical artifacts remain readable under their frozen
contracts.

`trainlog-exercise-body-zones-v1.json` is the one direction-neutral zone
companion in both directions. It carries only `exercise_id`, a nullable
`primary_zone_id` and `secondary_zone_ids`; taxonomy definitions stay in the
canonical manifest.

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
artifact, mobile-export v3, and equipment-associations v2. The PC-to-Android
direction publishes PC equipment-definitions v1 before dependent artifacts,
then publishes the PC catalog v1, PC mobile-export v3 (including completed
sessions and body observations), and equipment-associations v2.
Both directions also transfer the same body-zone companion after exercise
definitions are established and before completion of the direction.

The engine performs the applicable direction steps:

```text
1. direct-MTP device/storage discovery
2. exchange-folder resolution
3. definition artifact transfer and reconciliation before dependent V2 data
4. strict transactional Android -> PC import and body-zone reconciliation when selected
5. PC catalog, body-zone, mobile/body, and association export when selected
6. direct-MTP publication of the selected PC -> Android artifacts
7. optional request receipt publication
8. structured run-history recording
```

Synchronization is additive and reconciliatory: artifact absence never implies
deletion of local content. Equal same-ID definitions are idempotent; divergent
same-ID definitions, unknown references, and association conflicts are reported
explicitly. The affected persisted content is preserved on conflict; the engine
does not silently overwrite it.

There is no exercise, session, body-observation, body-zone-relation, or equipment-definition
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
one definitions/mobile/body-zones/associations publication has no common generation
manifest and is not one cross-artifact database transaction. A late association
conflict can therefore follow a successfully committed definitions or mobile
import; replay remains idempotent and existing conflicting content is not
overwritten. A future batch protocol must be separately versioned rather than
retrofitted into frozen formats.

Body-zone concurrency uses an internal canonical-state baseline. Equal states
are idempotent; the sole changed side wins; if both local and incoming states
diverge from the baseline, the companion reports a conflict and rolls back.
Secondary lists are never merged by union. A safe exercise-identity merge may
carry the only non-empty zone state, but rejects two different non-empty states
and clears the baseline because identity reconciliation is not acknowledgement.
After a companion has actually been published, the publisher records that
exact snapshot as its own baseline too; a failed file/MTP publication never
acknowledges data that the peer could not have observed.

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

## 14. Training knowledge V1 boundary

`TRAINING_KNOWLEDGE_V1` is a read-only composition layer. Six versioned JSON
catalogs under `catalog/` are the only authored scientific source; generated C
data and Android asset loading derive from them. They contain cited anatomy,
movement, exercise and equipment knowledge, not user history. The generated
knowledge audit is evidence output, not an editable source.

The desktop `training_knowledge.h` API exposes immutable catalog records and
stable-ID queries. `training_context.h` combines one exact persisted exercise
with its stored BODY ZONE relations, optional science, compatible equipment,
latest explicit maximum, and bounded occurrence/set history under one read
snapshot. Android provides the corresponding catalog and repository context.
This composition neither writes SQLite nor seeds catalog mappings. An unknown
runtime ID and a missing scientific record remain valid states.

Scientific BODY ZONE projections and persisted BODY ZONE relations have
different ownership and are never substituted for one another. Labels and
generic equipment descriptions are not runtime identities; an equipment
capability does not create an `ex_<uuid-v4>` exercise or historical association.
The feature is `TRAINING_KNOWLEDGE_V1=PASS`. The temporal contract has
independent PASS evidence:
the readers parse the admitted source forms into exact instants, compare exact
fractions, then use bytewise session and entry ID ties; emitted exclusive
cursors preserve the original timestamp text and IDs. Selection and hydration
share a read snapshot, while separate page calls retain current-data semantics.
Malformed caller cursors and malformed matching stored timestamps fail
explicitly; a selected timestamp beyond C's 40-character output field also
fails explicitly. The initial full-tranche audit's stale temporal-documentation,
Android loader, Meson input, and C role-only query findings were resolved by one
bounded repair chain and independently verified. Its full contract, uncertainty boundary and
future-only planning architecture are in [Training knowledge system V1](domain/knowledge_system.md).
