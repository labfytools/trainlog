# Trainlog Exchange Format v1

## 1. Status

This document defines the Trainlog v1 exchange contract draft.

Current state:

```text
TRAINLOG_FORMAT_V1=DRAFT
GATE_0=VALIDATION_PENDING
```

Incompatible changes are allowed until the format is explicitly marked `FROZEN`.

Once frozen, incompatible changes require a new format version.

## 2. Encoding

A Trainlog exchange document is:

- JSON;
- UTF-8;
- one top-level JSON object.

Unknown fields are rejected in v1.

This strict rule is intentional: a misspelled or unsupported field must fail validation rather than be silently ignored.

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

Must equal integer `1`.

## 4. Exercise catalog

Each catalog entry contains:

```json
{
  "exercise_id": "leg_press",
  "name": "Presse à cuisses"
}
```

### 4.1 Stable identity

`exercise_id` is the permanent machine identifier.

Rules:

- ASCII lowercase identifier;
- 1 to 128 characters;
- allowed characters: `a-z`, `0-9`, `_`, `-`;
- unique within one exchange document;
- must not change merely because the visible name changes.

The visible name is not the persistent identity.

### 4.2 Display-name anti-duplication rule

Two catalog entries must not have equivalent display names.

For duplicate detection, implementations normalize names using this semantic algorithm:

1. Unicode NFC normalization;
2. remove leading and trailing whitespace;
3. collapse each internal run of whitespace to one ASCII space;
4. Unicode case folding.

Example:

```text
"Presse   à cuisses"
" presse à cuisses "
"PRESSE À CUISSES"
```

are considered the same display name.

This rule prevents accidental duplicate exercises while still allowing an exercise to be renamed without changing `exercise_id`.

JSON Schema cannot express this normalization rule. It is mandatory semantic validation.

## 5. Session

Required fields:

- `session_id`;
- `started_at`;
- `exercises`.

Optional fields:

- `ended_at`;
- `body_weight_kg`;
- `measurements`.

`ended_at` is optional because Trainlog may preserve an active or interrupted session.

A completed Android export normally includes `ended_at`.

The TUI must never invent an end timestamp for a session that does not have one.

## 6. Session identifier

Example:

```text
20260905-183412-a84c
```

Rules:

- 1 to 128 characters;
- starts with an ASCII alphanumeric character;
- remaining characters are ASCII alphanumeric, `_`, or `-`;
- treated as an opaque unique identifier.

The generation algorithm is implementation-defined in v1.

The TUI enforces uniqueness in SQLite.

Repeated import of the same `session_id` is idempotent.

## 7. Timestamps

Timestamps use RFC 3339 / ISO 8601 date-time syntax with an explicit UTC offset.

Examples:

```text
2026-09-05T18:34:12+02:00
2026-09-05T16:34:12Z
```

An offset-less timestamp is invalid.

If `ended_at` is present, it must represent an instant strictly later than `started_at`.

Chronological ordering is a semantic validation rule.

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

`target` describes intended work.

`sets` describes actual performed work.

The two concepts must remain distinct.

The number of actual sets is deliberately allowed to differ from `target.sets`.

This records failure, extra work, interrupted sessions, and manual corrections truthfully.

## 9. Repetition mode and timed mode

Each workout exercise has exactly one target mode:

- repetition mode: `reps`;
- timed mode: `duration_seconds`.

A target must not contain both.

All actual sets for that exercise must use the same mode as the target.

### Repetition example

```json
{
  "target": {
    "sets": 4,
    "reps": 5
  }
}
```

### Timed example

```json
{
  "target": {
    "sets": 3,
    "duration_seconds": 45
  }
}
```

The schema rejects a set or target containing both `reps` and `duration_seconds`.

The target/actual mode-match rule is semantic validation.

## 10. Load

`weight_kg` represents external load in kilograms.

It is optional because some exercises are bodyweight or duration-only exercises.

When supplied, actual-set load is recorded per set so a session can truthfully represent load changes between sets.

## 11. Rest

`rest_seconds` is the planned rest duration after sets for the workout exercise.

Example:

```text
60
```

means one minute.

v1 does not record measured rest duration per individual set.

That may be introduced only by an additive compatible extension before freeze or a later format version after freeze.

## 12. Body weight

`body_weight_kg` is optional and uses kilograms.

It represents body weight associated with the session.

Standalone body-weight observations outside a workout session are a TUI/database concern and do not require this session exchange object.

## 13. Body measurements

Supported v1 measurements use centimeters:

- `waist_cm`;
- `chest_cm`;
- `shoulders_cm`;
- `left_arm_cm`;
- `right_arm_cm`;
- `left_thigh_cm`;
- `right_thigh_cm`;
- `left_calf_cm`;
- `right_calf_cm`.

If `measurements` is present, it must contain at least one measurement.

Additional measurements may still be added before v1 is frozen.

## 14. Catalog references

Every `session.exercises[*].exercise_id` must reference an entry present in the top-level `exercises` catalog.

This permits Android to introduce a new exercise safely during import.

An unknown reference makes the document invalid.

JSON Schema cannot express this cross-reference rule. It is mandatory semantic validation.

## 15. Duplicate workout exercise entries

A session must not contain the same `exercise_id` more than once.

All performed sets for one exercise belong to its single workout entry.

This keeps analysis and editing deterministic.

## 16. Idempotent import

The TUI treats `session_id` as a uniqueness key.

If the session is already present:

- do not create another session;
- do not duplicate sets;
- do not partially merge the repeated document;
- report that the session already exists.

Database constraints are the final anti-duplication barrier.

## 17. Validation layers

A valid Trainlog v1 document must pass both:

1. JSON Schema validation;
2. Trainlog semantic validation.

Schema validation handles structure and primitive bounds.

Semantic validation handles rules such as:

- normalized exercise-name uniqueness;
- exercise identifier uniqueness;
- catalog-reference integrity;
- duplicate workout exercise rejection;
- target/actual mode consistency;
- timestamp chronology.

Both Android export and TUI import must eventually implement the same semantic contract.

## 18. Freeze policy

`TRAINLOG_FORMAT_V1` must not be marked `FROZEN` until:

- all v1 fields are reviewed;
- valid fixtures pass;
- invalid fixtures fail for the intended reason;
- Android and TUI requirements contain no format ambiguity;
- the semantic validator contract is stable.
