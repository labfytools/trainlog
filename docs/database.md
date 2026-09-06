# Desktop database

## 1. Status

```text
TRAINLOG_DATABASE_SCHEMA_VERSION=5
DATABASE_SCHEMA_V5=PASS
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
5
```

Supported historical databases are migrated explicitly through the implemented
migration chain. A database newer than the running binary understands is
rejected.

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
recording_mode
data_fields
position
load_mode
rest_seconds
target_sets
target_reps
target_duration_seconds
target_weight_kg
notes
```

Current desktop history snapshots `recording_mode` and `data_fields` in the
session row. `tracking_mode` remains associated with the referenced exercise
catalog identity.

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
schema-v5 aware
transactional
idempotent by stable IDs
profile-aware catalog reconciliation
heterogeneous performed sets preserved
no fake target generated
continuous activity kept separate
```

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
```

The current normal suite contains 19 tests.

## 11. Measured-max derivation

Measured maxima require no desktop schema v6.

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

## 12. Body analytics persistence rule

Body analytics require no schema v6.

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
