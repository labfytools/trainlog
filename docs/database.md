# Desktop database

## 1. Status

```text
TRAINLOG_DATABASE_SCHEMA_VERSION=8
DATABASE_SCHEMA_V8=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

The desktop SQLite database is the canonical long-term Trainlog history.

Its schema evolves independently from all JSON exchange-format versions.

## 2. Versioning

Schema version uses:

```sql
PRAGMA user_version;
```

Current value:

```text
8
```

Supported historical databases are migrated explicitly through the implemented
migration chain. A database newer than the running binary understands is
rejected.

Version 7 assigns `session_exercises.entry_id` to each stable occurrence.
`exercise_id` remains only the catalogue identity and may occur more than once
in a session. The v6 → v7 migration rebuilds the obsolete uniqueness
constraint while retaining rows, sets, continuous activities, weights and
equipment associations.

Version 8 adds local `custom_equipment` definitions. The v7 → v8 migration is
additive: it retains all historic occurrences and their `equipment_id` values.
Supplied definitions continue to be generated from `catalog/equipment-v1.json`;
custom definitions exist only in the desktop database.

A schema fixture must represent the real historical structure. Rewriting only
`user_version` is not an acceptable migration test.

## 3. Connection invariants

Every connection enables:

```sql
PRAGMA foreign_keys = ON;
```

A bounded SQLite busy timeout is configured by the core.

## 4. Tables

### `exercises`

Canonical desktop exercise catalog.

```text
id
exercise_id          UNIQUE stable identity
name
normalized_name      UNIQUE normalized display form
tracking_mode        reps | duration
recording_mode       sets | continuous
data_fields          bounded bit mask
```

Rules include:

- continuous implies duration tracking;
- unknown supplemental field bits are rejected;
- normalized names remain unique.

### `sessions`

```text
id
session_id           UNIQUE stable identity
started_at
ended_at             nullable
session_type         training | max_test
notes                nullable
```

### `session_exercises`

Ordered exercise occurrence inside one session.

```text
session_row_id
exercise_row_id
entry_id             UNIQUE stable occurrence identity
recording_mode
data_fields
position
load_mode
rest_seconds
target_sets
target_reps
target_duration_seconds
target_weight_kg
equipment_id         nullable equipment identity
notes
```

Current desktop history snapshots `recording_mode` and `data_fields` in the
session row. `tracking_mode` remains associated with the referenced exercise
catalog identity.

An occurrence `equipment_id` resolves to either a supplied manifest definition
or a desktop-local `custom_equipment` definition. An ID that cannot be
resolved is retained as historic data and is explicitly visible to the user;
it is never silently converted to `NULL`.

### `custom_equipment`

Desktop-local custom equipment definitions. They supplement, but do not alter
or replace, the supplied catalogue generated from `catalog/equipment-v1.json`.
They synchronize through the separate `trainlog-equipment-definitions` v1
artifact; session/mobile and association V2 artifacts retain their existing
shapes. Definition reconciliation is additive: omission does not delete, an
equal same-ID row is idempotent, a divergent same-ID row conflicts, and IDs in
the supplied manifest are reserved.

`SETS` rows support two target shapes in schema v5:

```text
explicit planned target
    target_sets + exactly one target metric

actual-only mobile observation
    target_sets = NULL
    target_reps = NULL
    target_duration_seconds = NULL
```

This v5 rule is what permits heterogeneous mobile performed sets without
inventing a fake uniform target.

`CONTINUOUS` rows are targetless and require:

```text
load_mode = none
rest_seconds = 0
target_weight_kg = NULL
```

### `performed_sets`

Ordered actual set records.

```text
session_exercise_row_id
position
reps                 nullable
duration_seconds     nullable
weight_kg            nullable
```

Exactly one primary actual metric is present:

```text
reps
or
duration_seconds
```

Actual repetitions may be zero.

Each row is independent; heterogeneous repetition sequences are first-class
data.

### `continuous_activity`

One-to-one actual record for a continuous session exercise.

```text
session_exercise_row_id  UNIQUE
duration_seconds
speed_kmh                nullable
distance_km              nullable
```

Continuous activity never creates a fake performed set.

### `body_observations`

```text
observation_id       UNIQUE
observed_at
session_row_id       optional UNIQUE link
body_weight_kg
neck_cm
shoulders_cm
chest_cm
waist_cm
hips_cm
left_arm_cm
right_arm_cm
left_forearm_cm
right_forearm_cm
left_thigh_cm
right_thigh_cm
left_calf_cm
right_calf_cm
notes
```

At least one body metric must be present.

## 5. Identifier generation

Official desktop creator prefixes:

```text
ex_   exercise
se_   session
bo_   body observation
sy_   synchronization run
sxe_  session-exercise occurrence
```

All use random UUIDv4 values.

Exchange parsers may accept other schema-valid opaque identities where their
contract explicitly permits it.

## 6. Transactions

Multi-row user operations are atomic.

Persisted session correction replaces session child rows transactionally while
preserving the parent:

```text
session_id
started_at
ended_at
session_type
session notes
linked body observation
```

Removing an exercise from a persisted session is therefore a transactional
replacement of the remaining child set.

A failed replacement rolls back to the previously persisted session.

Body-observation editing preserves its stable identity, timestamp, and optional
session link.

## 7. Mobile import semantics

`tools/import_mobile_export.py` validates the complete mobile snapshot before
committing database changes.

Properties:

```text
schema-v5-through-v8 aware
transactional
idempotent by stable IDs
profile-aware catalog reconciliation
heterogeneous performed sets preserved
no fake target generated
continuous activity kept separate
```

For active V2 reconciliation, an identical normalized name never suffices by
itself. Different exercise identities may coalesce only when recording and
tracking modes match, other represented invariants remain compatible, and
their bounded `data_fields` masks are comparable by inclusion. The existing
desktop identity is deterministic canonical ownership. The mask union retains
the richer capability, while `session_exercises.data_fields` and all child
values remain unchanged. A missing historic optional value stays `NULL`.
Incomparable masks or modes reject the entire mobile-import transaction.

## 8. Units

Canonical desktop persistence:

```text
weight/load          kg
body circumference   cm
duration/rest         seconds
speed                 km/h
distance              km
```

## 9. Android database

The Android SQLite database is independent.

Current Android-local version: **8**. The explicit migration chain adds the
durable draft in v4, equipment references in v5, per-set load in v6, occurrence
identity/multi-occurrence support in v7, and the widened custom-equipment
definition graph in v8.

| Table | Ownership |
| --- | --- |
| `active_session_draft` | Single `id = 1` row, session type, selected catalog row, raw form text, update time |
| `draft_session_exercises` | Ordered draft exercises and profile snapshots |
| `draft_performed_sets` | Ordered heterogeneous repetition or duration actuals |
| `draft_continuous_activity` | Duration and configured speed/distance without synthetic sets |
| `equipment`, `equipment_aliases` | Supplied and user-created definitions used by selectors and occurrence FKs |
| `exercise_equipment`, `catalog_exercise_equipment` | Persisted manifest relationship metadata retained across migrations and identity reconciliation |

Foreign keys remain enabled. Draft deletion cascades only through draft child
tables; it cannot delete catalog entries or completed history. The repository
commits completed-session insertion and draft removal together, rolling back
both on failure. Repeating finalization after success cannot create another
completed session. Completed `started_at` semantics are unchanged by this repair.

Migration tests cover historical v4 and v7 shapes rather than changing only
`user_version`. The prior physical Samsung migration preserved every existing
domain row, with successful integrity and foreign-key checks. Current
reconciliation validation additionally uses coherent Android and desktop v8
copies whose integrity and foreign keys are checked before and after import.
SQLite files are never synchronization artifacts.

The schemas deliberately differ where ownership differs. Desktop sessions own
`ended_at`, notes, planned targets and session-specific load semantics; the
Android capture schema does not. Desktop body observations may link to a
session and carry notes; Android body observations contain capture metrics only.
Android occurrences snapshot `tracking_mode` as well as recording mode and
fields, whereas the current desktop schema resolves tracking mode through the
catalog exercise. Desktop stores occurrence `equipment_id` as stable text so
unknown historic IDs remain visible; Android stores a foreign key to its local
equipment definition.

Audit limitation: Android persists supplied exercise/equipment relationship
tables, but the current session selector searches the complete equipment list
instead of filtering or ranking it through those relations. The tables are
retained because migration and identity reconciliation already preserve them;
making them authoritative UI policy is future catalog work, not a v8 cleanup.

Desktop and Android schema versions are not required to match.

Do not synchronize SQLite database files.

## 10. Validation

```bash
meson compile -C build
meson test -C build --print-errorlogs
```

Migration-specific regression coverage includes:

```text
schema_v5_migration
schema_v7_migration
exercise_reconciliation
```

The current normal desktop suite contains 32 tests.

## 11. Measured-max derivation

Measured maxima require no schema change beyond the current desktop schema v8.

The existing `sessions.session_type = max_test` classification plus actual
`performed_sets` are sufficient.

Exercise performance points carry the originating session type so the
measured-max layer can distinguish explicit tests from ordinary training.

Rules:

```text
training session
    never becomes measured max implicitly

max_test + external
    greatest successful actual load
    tie -> greatest reps/duration

max_test + assistance
    lowest successful assistance
    tie -> greatest reps/duration

max_test + no load
    greatest successful reps/duration
```

A zero-repetition failed attempt is not a successful measurement.

The current measured result is the newest successful max-test point. The record
is the best max-test point using the same load mode.

No extra maximum row is persisted; results are derived from canonical history.

## 12. Equipment and occurrence migration

Desktop schema v6 added nullable `session_exercises.equipment_id`. Schema v7
adds the non-null stable `entry_id` and removes the obsolete
`UNIQUE(session_row_id, exercise_row_id)` constraint. The v6 -> v7 rebuild
preserves primary keys, completed sessions, ordered sets, continuous activities,
per-set weights and equipment values.

Schema v8 adds local `custom_equipment` definitions through an additive v7 ->
v8 migration. Existing occurrence links remain unchanged. They resolve through
the supplied manifest or the local custom table; unknown historic references
remain explicit rather than being discarded.

Android schema v6 added nullable `weight_kg` to completed and durable draft
set rows. Schema v7 assigns stable `entry_id` values to completed and draft
occurrences. Actual per-set weights remain independent values, so heterogeneous
sets and weights survive edit, finalization, reopen and V2 exchange.

Android schema v8 non-destructively migrates the v7 equipment reference graph
so custom definitions may use `load_semantics = none`. Historic completed
occurrences, draft occurrences, and their equipment references remain intact.

## 13. Body analytics persistence rule

Body analytics require no schema change beyond schema v8.

Canonical persistence continues to contain only measurements actually entered
by the user.

These values remain derived at display time and are not persisted:

```text
body-fat estimate
estimated fat mass
estimated lean mass
waist/hip ratio
shoulder/waist ratio
chest/waist ratio
left/right asymmetry percentages
```

The optional estimation profile is desktop configuration, not database
history.
