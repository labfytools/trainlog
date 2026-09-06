# Database

## 1. Status

```text
GATE_2=IN_PROGRESS
DATABASE_SCHEMA_V2=IMPLEMENTED
SESSION_TYPE_PERSISTENCE=IMPLEMENTED
SESSION_EDIT_PERSISTENCE=IMPLEMENTED
BODY_OBSERVATION_EDIT=IMPLEMENTED
TRAINLOG_FORMAT_V1=FROZEN
```

Gate 2 review #1 establishes the persistence foundation.

The Trainlog exchange format v1 is already frozen and is not modified by this gate.

## 2. Purpose

SQLite is the canonical long-term store used by the TUI.

The SQLite database is an internal persistence format and is versioned independently from the Trainlog JSON exchange format.

## 3. Schema versioning

Trainlog database schema version uses SQLite:

```sql
PRAGMA user_version;
```

Current schema:

```text
DATABASE_SCHEMA_V2=2
```

A new database starts with `user_version = 0` and is initialized atomically to
the current schema.

The implemented historical path is:

```text
0 -> 2  fresh initialization
1 -> 2  transactional migration
```

Schema v2 adds local session classification while leaving the frozen Trainlog
JSON v1 exchange contract unchanged.

A database newer than the running binary understands is rejected.

Every future schema change requires an explicit migration and dedicated
coverage; metadata-only version rewriting is not an accepted migration.

## 4. Connection rules

Every Trainlog SQLite connection must enable:

```sql
PRAGMA foreign_keys = ON;
```

The core also configures a bounded SQLite busy timeout.

Foreign-key activation is verified by tests.

## 5. Tables

### 5.1 `exercises`

Canonical exercise catalog.

Fields:

```text
id                 internal INTEGER primary key
exercise_id        stable Trainlog identity, UNIQUE
name               display name
normalized_name    v1 comparison form, UNIQUE
tracking_mode      reps | duration
```

The database does not compute Unicode normalization in review #1.

The application/catalog layer will compute the frozen v1 normalized name in Gate 2 review #2.

SQLite owns the final uniqueness barrier.

### 5.2 `sessions`

Canonical workout session header.

Fields:

```text
id
session_id         UNIQUE
started_at
ended_at            nullable
session_type       training | max_test
notes               nullable
```

`session_type` is a local SQLite concern in schema v2. Existing schema-v1 rows
migrate to `training`; no historical workout is retroactively inferred to be a
max test.

Body data is stored separately so standalone body observations can use the same representation.

### 5.3 `session_exercises`

One ordered exercise within a session.

Fields include:

```text
session_row_id
exercise_row_id
position
load_mode
rest_seconds
target_sets
target_reps
target_duration_seconds
target_weight_kg
notes
```

Constraints enforce:

- one exercise identity at most once per session;
- one row at each session position;
- exactly one target metric: repetitions or duration;
- target load presence consistent with `load_mode`.

### 5.4 `performed_sets`

Ordered actual sets.

Fields:

```text
session_exercise_row_id
position
reps
duration_seconds
weight_kg
```

Exactly one of repetitions or duration is present.

Cross-table rules such as actual-set load consistency with the owning session exercise remain application/import invariants and will be tested at the import layer.

### 5.5 `body_observations`

Body history is a first-class database concept and may exist with or without a workout session.

Fields include:

```text
observation_id
observed_at
session_row_id       optional and UNIQUE
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

Imported session-associated body data will create one linked observation.

Standalone TUI measurements use the same table without `session_row_id`.

## 6. UUID generation

Official Trainlog creators generate UUID version 4 identifiers.

The C core provides generated IDs for:

```text
ex_<uuid-v4>
se_<uuid-v4>
bo_<uuid-v4>
```

This is creation policy.

The frozen exchange parser remains able to accept other schema-valid opaque v1 identifiers.

## 7. Transactions

Multi-row operations are atomic.

Gate 2 provides explicit:

```text
BEGIN IMMEDIATE
COMMIT
ROLLBACK
```

primitives.

The JSON import service must perform catalog reconciliation and all session inserts inside one transaction.

Persisted session correction also uses an explicit transaction. Editing a
session replaces only its `session_exercises` / `performed_sets` children and
preserves the parent session row, stable `session_id`, timestamps,
`session_type`, session notes, and any linked body observation.

Body-observation correction preserves observation identity, timestamp, and
optional session link.

A hard conflict or validation failure leaves the database unchanged.

## 8. Units

Canonical persistent units remain:

- weight/load: kilograms;
- body circumference: centimeters;
- duration/rest: seconds.

## 9. Current Gate 2 persistence boundary

Implemented persistence includes:

- SQLite schema v2;
- transactional schema migration v1 -> v2;
- Unicode-aware canonical exercise catalog support;
- complete session insertion;
- session detail loading;
- exact bounded editable-session loading;
- transactional replacement of session exercise/set children;
- body-observation creation, listing, exact lookup, and update;
- stable local identifiers for exercises, sessions, and body observations.

The database remains independent from ncurses rendering.

The frozen Trainlog JSON v1 format remains a separate compatibility boundary
and is not version-coupled to SQLite schema v2.

## 10. Validation

Normal build:

```bash
CC=clang meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Sanitizer build:

```bash
CC=clang meson setup build-asan \
  -Db_sanitize=address,undefined \
  -Db_lundef=false

meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

Repository-level format validators remain mandatory:

```bash
python tools/validate_json.py
python tools/validate_import_contract.py
git diff --check
```
