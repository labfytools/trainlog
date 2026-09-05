# Gate 2 Review #1 — Persistence Foundation

## Status

```text
GATE_2_REVIEW_01=IMPLEMENTED
GATE_2=IN_PROGRESS
DATABASE_SCHEMA_V1=DRAFT
TRAINLOG_FORMAT_V1=FROZEN
```

## Scope

This review introduces the first production C17 code in Trainlog.

It deliberately stops below the JSON import and ncurses layers.

## Decisions

### G2-R1-01 — Meson and strict C17

The TUI core is built with Meson using C17.

Warnings are errors.

Additional warning flags include:

```text
-Wconversion
-Wformat=2
-Wshadow
```

### G2-R1-02 — SQLite is isolated behind a C API

Application and future ncurses code do not call SQLite directly.

The first persistence API owns:

- connection lifecycle;
- schema bootstrap;
- schema version query;
- foreign-key state query;
- explicit transactions;
- exercise insertion;
- exercise count query.

### G2-R1-03 — Schema version is independent from JSON format version

Trainlog JSON v1 is frozen.

SQLite schema v1 is an internal implementation contract and may later migrate independently.

SQLite `PRAGMA user_version` is the canonical database schema number.

### G2-R1-04 — New database creation is atomic

Schema creation uses one explicit transaction.

`PRAGMA user_version = 1` is written before that transaction commits.

A schema bootstrap failure triggers best-effort rollback.

### G2-R1-05 — Newer schemas fail closed

A database with a `user_version` newer than the running binary supports is rejected.

Trainlog must not guess how to interpret newer persistent data.

### G2-R1-06 — Foreign keys are mandatory

Every connection enables:

```sql
PRAGMA foreign_keys = ON;
```

Tests verify the state.

### G2-R1-07 — UUIDv4 generation is implemented once in core

The C core uses libuuid for official:

```text
ex_
se_
bo_
```

identifier creation.

Tests verify prefix, length, version nibble, RFC variant, and non-equality of two generated IDs.

### G2-R1-08 — Normalized exercise name has a database uniqueness barrier

`normalized_name` is unique.

Review #1 intentionally accepts an already-normalized name as an API parameter.

Frozen Unicode normalization itself is implemented in review #2, so the persistence layer does not duplicate Unicode policy.

### G2-R1-09 — Body history is independent from workout history

The schema includes `body_observations`.

An observation may optionally point to exactly one session.

This allows weight and measurements to be recorded on days without a workout.

### G2-R1-10 — No ncurses yet

The TUI visual layer starts only after the persistence core is stable.

Gate 3 remains responsible for ncurses and color.

## Required validation

```bash
python tools/validate_json.py
python tools/validate_import_contract.py

CC=clang meson setup build
meson compile -C build
meson test -C build --print-errorlogs

CC=clang meson setup build-asan \
  -Db_sanitize=address,undefined \
  -Db_lundef=false
meson compile -C build-asan
meson test -C build-asan --print-errorlogs

git diff --check
```

## Review result

This commit is not Gate 2 PASS.

After review #1 passes, Gate 2 continues with:

```text
Unicode normalization
catalog reconciliation
JSON v1 import transaction
idempotency tests
```
