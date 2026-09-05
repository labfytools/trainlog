# Trainlog Exchange Format v1

## 1. Status

This document defines the initial Trainlog v1 exchange contract.

Until explicitly marked `FROZEN`, incompatible changes are allowed during early development.

Once frozen, incompatible changes require a new format version.

## 2. Encoding

A Trainlog exchange document is:

- JSON;
- UTF-8;
- one top-level JSON object.

## 3. Required top-level fields

```json
{
  "format": "trainlog",
  "version": 1,
  "exercises": [],
  "session": {}
}
```

### `format`

Must equal:

```text
trainlog
```

### `version`

Integer schema version.

For this document:

```text
1
```

## 4. Exercise catalog entries

Each exercise entry contains:

```json
{
  "exercise_id": "leg_press",
  "name": "Presse à cuisses"
}
```

### `exercise_id`

Stable identifier.

Rules:

- non-empty;
- unique within the document;
- treated as identity;
- must not change merely because the display name changes.

### `name`

Human-readable display name.

The TUI may update the local display name later without changing `exercise_id`.

## 5. Session

Required fields:

- `session_id`;
- `started_at`;
- `ended_at`;
- `exercises`.

Optional body fields may include:

- `body_weight_kg`;
- `measurements`.

## 6. Session identifier

Example:

```text
20260905-183412-a84c
```

The exact generation algorithm is implementation-defined in v1.

The invariant is uniqueness.

The TUI must enforce uniqueness at import.

## 7. Timestamps

Timestamps use ISO 8601 with an explicit UTC offset.

Example:

```text
2026-09-05T18:34:12+02:00
```

The timezone offset is part of the serialized value.

The TUI must not silently reinterpret a timestamp as local time without using the encoded offset.

## 8. Workout exercise entry

Example:

```json
{
  "exercise_id": "leg_press",
  "rest_seconds": 60,
  "target": {
    "sets": 4,
    "reps": 5,
    "weight_kg": 80
  },
  "sets": [
    { "reps": 5, "weight_kg": 80 },
    { "reps": 5, "weight_kg": 80 },
    { "reps": 5, "weight_kg": 80 },
    { "reps": 3, "weight_kg": 80 }
  ]
}
```

The `target` object describes the intended work.

The `sets` array describes what was actually performed.

These two concepts must remain distinct.

## 9. Timed exercises

Timed exercises such as planks use `duration_seconds`.

Example:

```json
{
  "exercise_id": "plank",
  "rest_seconds": 60,
  "target": {
    "sets": 3,
    "duration_seconds": 45
  },
  "sets": [
    { "duration_seconds": 45 },
    { "duration_seconds": 45 },
    { "duration_seconds": 38 }
  ]
}
```

A set may represent repetitions or duration.

The schema forbids an empty set object.

## 10. Body measurements

The initial v1 measurement object supports named measurements in centimeters.

Example:

```json
{
  "measurements": {
    "waist_cm": 91.0,
    "chest_cm": 104.0,
    "left_arm_cm": 35.0,
    "right_arm_cm": 35.0,
    "left_thigh_cm": 58.0,
    "right_thigh_cm": 57.0
  }
}
```

The initial schema intentionally uses explicit field names rather than arbitrary free-form keys.

Additional measurements may be added before v1 is frozen.

## 11. Idempotent import

The TUI must treat `session_id` as a uniqueness key.

If a session has already been imported:

- do not create another session;
- do not duplicate its sets;
- report that the session already exists.

## 12. New exercises

When Android exports a session containing an exercise unknown to the TUI:

1. the exercise must exist in the top-level `exercises` array;
2. its `exercise_id` must be valid;
3. its `name` must be non-empty;
4. the TUI imports the catalog entry;
5. the session may then reference that exercise.

## 13. Unknown fields

Before v1 is frozen, implementations may reject unknown fields during development to catch mistakes early.

The final forward-compatibility policy will be frozen explicitly before release.
