# Android application

## 1. Purpose

The Android application is Trainlog's low-friction capture client.

It is a native Kotlin/Jetpack Compose application with local SQLite persistence.

The desktop remains the canonical long-term history and analytics store.

## Session exchange V2

Completed session occurrences persist an `entry_id`; it is never regenerated
for exchange. Android publishes `trainlog-mobile-export-v2.json` as the active
desktop snapshot and imports `trainlog-pc-mobile-export-v2.json` after the PC
catalogue. The artifact preserves occurrence order, continuous metrics, set
weights and equipment. The legacy V1 contract remains separate and readable.

## 2. Implemented navigation

```text
Accueil
├── Reprendre la séance en cours (si un brouillon existe)
├── Enregistrer une séance
├── Enregistrer un exercice
├── Enregistrer des mensurations
├── Historique des séances
└── Synchroniser avec le PC
```

## 3. Local persistence

Android local database version:

```text
10
```

Domain tables cover:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
body_observations
exercise_body_zones
exercise_body_zone_sync
```

This database is Android-local. It is not copied to the PC.

Schema v4 introduced `active_session_draft`, `draft_session_exercises`,
`draft_performed_sets` and `draft_continuous_activity`. The implemented
additive v4 -> v10 chain preserves catalog, completed sessions/actuals, body
observations and the draft while adding the shared equipment catalogue,
occurrence-level equipment links and stable completed/draft `entry_id` values.
Exactly one active draft is supported; it is separate from completed history.
Schema v9 adds explicit completed/draft MAX results. Schema v10 additively
stores direct primary/secondary body-zone relations and their private sync
baseline; the taxonomy itself remains the shared manifest asset.

## 4. Exercise catalog

Exercise creation records:

```text
name
recording_mode
tracking_mode
data_fields
primary_zone_id       nullable only for explicit unclassified/history cases
secondary_zone_ids   zero or more distinct canonical IDs
```

Stable identity:

```text
ex_<uuid-v4>
```

The UI rejects invalid profile combinations and local normalized-name
collisions.

An exercise may be created standalone or inline while building a session.

The form groups translated values from `catalog/body-zones-v1.json` under
**Membres supérieurs** and **Membres inférieurs**, with autonomous **Corps
entier** and **Abdominaux / tronc** entries. Group nodes organize and filter;
they cannot be assigned directly. A new set-based exercise requires one
primary zone. Selecting it removes/disables that same ID among secondaries.
Historical unclassified exercises remain visible as **Zone : Non renseignée**
and may be classified later.

The existing exercise list combines SQL-backed normalized prefix search with a
zone filter. Parent filters include descendants, **Non renseignés** selects
only exercises with no relations, and **Toutes les zones** never hides those
rows. Each row displays primary, secondaries and the primary's derived group.

### Editing an exercise

Every existing catalog item exposes **Modifier**. Editing a name trims its
input, recomputes `normalized_name`, and rejects a normalized-name collision.
The row retains its existing `exercise_id`; naming is presentation metadata,
not identity. Completed session rows and the active draft retain their catalog
row relationship and immediately resolve the renamed display text after reopen.

Profile fields (`recording_mode`, `tracking_mode`, `data_fields`) are editable
only while an exercise has no completed-session or active-draft reference. Once
referenced, Android displays the lock and returns an explicit incompatible
profile result rather than silently reinterpreting work or creating another
exercise. Renaming remains available independently.

The shared Compose `TrainlogScreen` header is used by Accueil, Séance,
Exercice, Mensurations, Historique, Détail séance and Sync. Its compact
`◆ TRAINLOG ◆` accent plaque and muted subtitle intentionally mirror the
Notcurses TUI identity in a flat mobile layout.

## 5. Session recording

Stable session identity:

```text
se_<uuid-v4>
```

Session entry is profile-aware.

### Sets + repetitions

Actual set values may be heterogeneous. The `SETS + REPS` editor presents
ordered rows, each with its own repetitions and optional load. **Ajouter une
série** appends one blank row and **Supprimer la série** removes only the chosen
row; editing or removing a row does not alter the remaining row values.

The load heading is **Charge (kg)** for external resistance and
**Assistance (kg)** for assistance equipment. A blank load is no recorded load,
not `0`; an entered load is finite and non-negative, and French decimal commas
are accepted. The durable raw form preserves
partial row input (including a blank row or a fragment such as `32,`) across
draft save and restore, and a failed validation or draft write presents a
specific error without claiming the row was saved.

When an older compact raw draft is reopened, its repetition text can be
expanded into the row editor from:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

### Sets + duration

Each performed set stores its own duration.

### Continuous + duration

The form asks for duration and only the configured supplemental fields such as
speed or distance.

Continuous work does not create fake sets.

### Occurrences, equipment and actual loads

The same catalogue `exercise_id` may be added more than once to a session.
Every occurrence receives its own stable `entry_id`, position, actual sets and
optional equipment selection. Editing one occurrence replaces only that entry;
it does not merge or alter another passage of the same exercise.

`Machine / équipement (optionnel)` searches the shared manifest by display
name, physical-machine label and aliases. A selected equipment identity is
stored on that occurrence in both the active draft and completed session.
For `SETS + REPS`, the form is a row editor: every set owns an independently
editable repetitions field and optional load field, and rows can be added or
deleted without changing their neighbours. French decimal commas are accepted.
`Assistance (kg)` is an explicit alternative load semantic, not an external
charge. Empty load and an entered zero remain distinct. The completed-session
detail renders the persisted rows in order with the matching Charge or
Assistance heading, including an explicit empty-load marker.

## 6. Session draft editing

The repository durably saves every meaningful mutation, including session type,
exercise selection/addition/removal, actual values and raw per-set form edits.
Partial row text such as `32,` is retained without normalization. A failed write
displays a specific error and does not claim the latest change was saved.

Home shows **Reprendre la séance en cours** and an exercise-count/type summary.
The ordinary new-session action opens an existing draft without overwriting it.
Back returns Home and preserves the draft. Backgrounding, switching apps,
Activity/configuration recreation, background process death and force-stop with
relaunch preserve the draft; these paths were validated on the Samsung SM_G990B.

**Retirer <exercice>** removes only that draft exercise and its actual values.
It does not change the catalog or completed history. Removal survives restart.
**Supprimer la séance en cours** requires deliberate confirmation; cancellation
preserves the draft. Confirmed deletion leaves no completed session or stale
resume action after relaunch.

Final save validates the durable draft, inserts the completed session and actual
values, and removes the draft in one SQLite transaction. Failure rolls back and
retains the draft for retry; repeated completion does not create duplicates.
The existing completed-session save-time timestamp behavior is unchanged.

PC catalog reconciliation preserves draft references through catalog row
ownership. If an editing selection no longer resolves, only the selection is
cleared; added exercises and raw text remain, with a specific diagnostic.
When a received catalog entry has the same `exercise_id`, a changed display
name is reconciled in that same row. When the incoming PC identity differs but
the normalized name matches, Android rekeys or merges only if recording and
tracking modes match, the bounded `data_fields` masks are comparable by
inclusion, and overlapping equipment metadata has the same load semantics.
The incoming PC identity is canonical and the bit-mask union retains the richer
profile. Completed and draft occurrence row IDs, `entry_id`, positions, values,
equipment references and the active selection are moved transactionally. Any
incompatible condition rejects the catalog transaction; name equality alone
never authorizes a merge.
Zone relations follow the same row-identity move only when one side is empty or
both states are equal. Two different non-empty states are an explicit conflict;
the repository never invents an automatic union of secondary zones.

## 7. Session history

Android exposes persisted local session history and profile-aware detail.

Set-based history renders ordered performed sets.

Continuous history renders its one activity record with configured supplemental
values.

## 8. Body measurements

Supported metrics:

```text
weight
neck
shoulders
chest
waist
hips
left/right arm
left/right forearm
left/right thigh
left/right calf
```

Rules:

```text
empty field = not measured
at least one positive metric required
comma or dot accepted for decimal entry
```

Stable identity:

```text
bo_<uuid-v4>
```

## 9. Automatic mobile snapshot

Android maintains:

```text
Download/Trainlog/trainlog-mobile-export-v2.json
```

The V2 snapshot is refreshed after relevant local changes, including exercise,
session, body-observation, equipment association and PC-catalog updates. It
preserves `entry_id`, occurrence position, optional equipment and actual
per-set weights. Android also publishes the V2 companion
`trainlog-equipment-associations-v2.json`; its `set` and `cleared` states are
targeted by `(session_id, entry_id)`.

Before either V2 artifact, Android publishes its user-created equipment
definitions as `trainlog-mobile-equipment-definitions-v1.json`. The strict
`trainlog-equipment-definitions` v1 format uses stable IDs and the fields
`equipment_id`, `display_name`, `label_name`, `equipment_type`, and
`load_semantics`. Absence never deletes a definition; equal same-ID definitions
are idempotent and divergent same-ID definitions conflict. Supplied-manifest
IDs are reserved.

Android also writes `trainlog-exercise-body-zones-v1.json`, the sole zone
companion used in both directions. It contains stable exercise/zone IDs only.
After the file write succeeds, an identical local replay establishes/refreshes
the publisher's shared baseline. A one-sided change is applied transactionally,
and simultaneous divergence returns an explicit conflict without changing
local relations. The unclassified state contains neither a primary nor orphan
secondaries.

The user does not need a separate manual export step before synchronization.

An active draft is never included in completed history, session detail or this
snapshot. Synchronization continues to exchange completed data while the draft
stays local; no draft fields were added to the frozen mobile artifact.

## 10. PC catalog access

PC-created files are accessed through a persistent Storage Access Framework
grant.

The selected folder must be:

```text
Download/Trainlog
```

The Sync screen always permits changing the stored folder selection.

No application-data reset is required to fix a wrong folder choice.

## 11. Android-triggered synchronization

The Sync screen exposes:

```text
Synchroniser maintenant
```

Android writes:

```text
trainlog-sync-request-v1.json
```

If MediaStore cannot reopen an older MTP-created canonical object, it may
publish the exact collision sibling `trainlog-sync-request-v1 (N).json`. The
desktop engine selects the newest canonical-or-suffixed request
deterministically, while the stable `request_id` remains the replay boundary.

and waits for a matching:

```text
trainlog-sync-receipt-v1.json
```

The receipt is matched by `request_id`.

On success Android then applies the latest PC catalog and displays the final
result.

Before applying the PC catalog or its V2 artifacts, Android applies
`trainlog-pc-equipment-definitions-v1.json`. Thus custom definitions are known
before a received V2 association references them. Android schema v10 provides
the non-destructive v7 -> v8 migration required for `load_semantics = none`.
After catalog/session/equipment reconciliation, Android applies the same
body-zone companion so custom exercises receive their classifications.

A receipt belonging to another request is ignored as pending rather than
misreported as the current result.

## 12. Synchronization ownership

Android does not initiate raw MTP operations itself.

MTP is host-initiated:

```text
Android request
    -> PC trainlog-syncd
    -> shared desktop sync engine
    -> receipt
```

## 13. Build

Example local configuration:

```bash
cd android

printf 'sdk.dir=%s\n' "$HOME/Android/Sdk" > local.properties

JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew testDebugUnitTest assembleDebug
```

Install to a connected test device:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

`local.properties` is local machine configuration and must not be committed.

The current JVM host regression suite has 44 passing tests when the retained
real v9 fixture is enabled. The prior device
instrumentation suite had 5 tests (2 repository, 3 production-screen UI tests
using an isolated database and no shared export). That device matrix exercised
production `MainActivity`,
including verified process exit with `am kill`, force-stop, configuration
relaunch, raw-form recovery, removal, discard and unchanged user data. Final-save
UI checks used isolated data so fictitious workouts did not enter user history.
The schema-v8 definition change is recorded as targeted JVM validation, not a
blanket device-validation claim. See [tests](tests.md) for commands and the
precise validation boundary.

## 14. Non-goals

Android is not intended to own:

- canonical long-term analytics;
- complex body/performance graphs;
- cloud accounts;
- direct SQLite-file synchronization;
- exercise-name heuristics;
- a mounted-filesystem dependency.

## 15. Equipment and multi-occurrence exchange V2

During exercise entry, `Machine / équipement (optionnel)` searches the shared
catalogue by display name, physical-machine label and aliases. The selected
canonical ID belongs to that session exercise entry, is durable in the active
draft and completed session, and is visible in session detail. It may be
cleared. The active V2 exchange preserves multiple ordered occurrences of the
same exercise in one session through `entry_id`. The frozen V1 artifacts remain
readable only as legacy artifacts and keep their historical one-exercise
identity assumptions; V1 is not rewritten to claim V2 support.

For a `SETS + REPS` exercise, selecting equipment never changes that exercise
profile: the form retains per-set repetitions and exposes `Charge (kg)`. French
decimal input is accepted (`12,5`); one value applies to all sets or values may
be separated with `;`. Assisted equipment is explicitly labelled
`Assistance (kg)`. Empty load remains distinct from an entered zero.

`Nouvelle machine` in that same selector creates a persistent local custom
equipment entry with a generated stable `eq_…` ID and selects it immediately.
The shared bundled catalogue uses reserved canonical IDs. User-created IDs are
exchanged first by definitions V1, so a V2 association can resolve them on the
receiving side; conflicts remain explicit and are never converted to null.
The active-session list exposes `Modifier <exercice>`; saving replaces that
entry in place, while cancelling only discards the form and preserves it.

## 16. Test-max sessions

Android session entry exposes:

```text
Entraînement
Test max
```

The selection is persisted in the existing Android `sessions.session_type`
column and exported in the mobile snapshot as:

```text
training
max_test
```

History and detail visibly identify max-test sessions.

Selecting `Test max` is explicit metadata; Trainlog does not infer max tests
from large repetition or duration values.

Its exercise form contains only the existing exercise search, optional
machine/equipment selection, and `Poids max (kg)`. French decimal commas are
accepted; empty is distinct from zero and only a finite positive value can be
saved. Each saved line is immediately visible as `Exercice · Max : N kg`, can
be edited in place with the same `entry_id`, or cancelled without mutation.
Several movement results may be appended successively.

Schema v9 stores the result in `max_results` or `draft_max_results`, never in a
synthetic one-repetition set. Equipment remains occurrence context. History
also lists the newest explicit maximum for each `exercise_id`, with date and
equipment used.

A completed max-test detail exposes `Reprendre ce Test max`. The one durable
draft then records the source `session_id` and retains all existing occurrence
IDs, order, movement IDs and equipment. Finalization atomically replaces that
same completed session and may append new occurrences. The completed source is
left intact as the crash-safe baseline until finalization.

## 17. Audited limitations

Body Zones V1 defines no custom zone creation and no session generator. The
repository already exposes manifest lookup/hierarchy, descendant and
primary-only exercise filtering, direct exercise relations and the existing
latest-MAX/history reads needed for future composition; it calculates no
suggested load.

Android's stored `normalized_name` currently removes diacritics, while the
frozen desktop/Python normalization contract uses NFC, Unicode whitespace
collapse and case folding without accent removal. Existing Marche/Leg press
data is unaffected, but changing this safely requires an explicit Android
schema migration that recomputes every normalized key and handles newly exposed
collisions. It is not silently changed inside schema v10.

The bundled exercise/equipment relationship metadata is seeded and preserved,
including during exercise-identity reconciliation, but the current equipment
picker searches all known supplied and custom definitions. Filtering or ranking
that picker by exercise relation remains future gym-catalog policy.

Custom equipment remains identified only by `equipment_id`: Android never
coalesces two different custom IDs merely because their labels match. This can
leave conceptually duplicated definitions created independently on two devices,
but avoids guessing across potentially different load semantics and persisted
occurrence references.

The PC-catalog V1 inbox validates required IDs, modes, names, and bounded field
masks, but unlike the newer mobile V2 and equipment-companion parsers it does
not reject every unknown root or item key. Tightening this published V1 reader
requires a compatibility decision rather than an incidental schema-v10 change.

Android requires `data_fields = 0` for `SETS`, while the desktop model/API
currently accepts known supplemental bits on either recording mode. Supplied
profiles do not exercise this difference. Supporting a future set-based
supplemental field requires an explicit shared-model decision.
