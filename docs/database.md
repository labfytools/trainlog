# Database

## 1. Purpose

SQLite is the canonical long-term data store used by the TUI.

The database is not the Android exchange format.

## 2. Initial entities

The initial data model is expected to contain:

- schema metadata;
- exercises;
- sessions;
- session exercises;
- performed sets;
- body measurements;
- optional maximum-performance records.

The exact SQL schema will be frozen before implementation.

## 3. Exercise identity

The database must preserve a stable external exercise identifier.

Conceptually:

```text
exercises
  id              internal SQLite primary key
  exercise_id     stable Trainlog identifier
  name            mutable display name
```

`exercise_id` must be unique.

## 4. Session identity

`session_id` must be unique.

This is the primary anti-duplication barrier for imported sessions.

## 5. Foreign keys

SQLite foreign-key enforcement must be enabled explicitly for every connection:

```sql
PRAGMA foreign_keys = ON;
```

Tests must verify that the expected constraints are actually active.

## 6. Schema versioning

The database must store an explicit schema version.

Schema changes must be classified as:

- additive and compatible;
- migration required;
- destructive and therefore forbidden without explicit migration logic.

## 7. Transactions

Multi-table imports must use transactions.

A failed import must not leave a partially inserted session.

Expected behavior:

```text
BEGIN
  validate
  insert missing exercises
  insert session
  insert workout rows
  insert performed sets
COMMIT
```

On failure:

```text
ROLLBACK
```

## 8. Units

Canonical storage units:

- body weight: kilograms;
- load: kilograms;
- body measurements: centimeters;
- duration: seconds.

The UI may format values differently later, but persistent units remain explicit and stable.

## 9. Migration policy

A schema migration must:

- be deterministic;
- preserve user data;
- be testable from the previous supported version;
- update the stored schema version only after success.
