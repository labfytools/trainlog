# Trainlog Exchange Format v1

## 1. Status

```text
GATE_0=PASS
GATE_1_REVIEW_01=PASS
GATE_1_REVIEW_02=PASS
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

This document defines the proposed final Trainlog v1 exchange contract.

The format remains `DRAFT` until Gate 1 validation and mirror review complete.

## 2. Design goal

Trainlog v1 must represent the training patterns required by the initial applications without turning the Android recorder into a complex training platform.

The format supports:

- repetition-based exercises;
- time-based exercises;
- bodyweight work;
- free-weight and machine load;
- assistance load;
- planned versus performed work;
- planned rest;
- body weight;
- body measurements;
- active or interrupted sessions;
- optional notes.

Distance, velocity, heart rate, per-set measured rest, supersets, and arbitrary custom metrics are outside v1.

## 3. Encoding and strictness

A Trainlog v1 document is:

- JSON;
- UTF-8;
- one top-level object;
- structurally strict.

Unknown fields are rejected.

This is intentional. A misspelled field must fail validation instead of being silently discarded.

## 4. Top-level object

Required fields:

```json
{
  "format": "trainlog",
  "version": 1,
  "exercises": [],
  "session": {}
}
```

`format` must equal `trainlog`.

`version` must equal integer `1`.

The top-level `exercises` array contains metadata for exactly the exercises referenced by the session.

It is not a full catalog synchronization document.

## 5. Exercise identity

Each catalog entry contains:

```json
{
  "exercise_id": "leg_press",
  "name": "Presse à cuisses",
  "tracking_mode": "reps"
}
```

### 5.1 `exercise_id`

`exercise_id` is the permanent machine identity.

Rules:

- 1 to 128 characters;
- ASCII lowercase;
- first character: `a-z` or `0-9`;
- remaining characters: `a-z`, `0-9`, `_`, `-`;
- unique inside the document;
- unchanged when the visible name changes.

### 5.2 `name`

`name` is the human-readable display name.

The display name is not the identity.

Two exercises in one document must not have equivalent normalized names.

Normalization for comparison is:

1. Unicode NFC normalization;
2. trim leading and trailing Unicode whitespace;
3. collapse each internal whitespace run to one ASCII space;
4. Unicode case folding.

The serialized name is never rewritten by this normalization rule.

The future C implementation must use a Unicode implementation capable of reproducing this contract exactly; `utf8proc` or an equivalent tested implementation is acceptable.

### 5.3 `tracking_mode`

Every exercise has one stable tracking mode:

```text
reps
duration
```

`reps` is used for repetition-counted exercises.

`duration` is used for time-counted exercises such as planks.

The tracking mode determines the Android input control and the interpretation of all sets for that exercise.

Changing the fundamental tracking mode of an existing exercise should normally create a new exercise identity rather than silently changing historical semantics.

## 6. Session identity

A session contains a unique opaque `session_id`.

Example:

```text
20260905-183412-a84c
```

Rules:

- 1 to 128 characters;
- starts with an ASCII alphanumeric character;
- remaining characters are ASCII alphanumeric, `_`, or `-`.

The generation algorithm remains implementation-defined.

The database uniqueness constraint is the final anti-duplication barrier.

Importing an already-known `session_id` is idempotent.

## 7. Session timestamps

`started_at` is required.

`ended_at` is optional.

Both use RFC 3339 / ISO 8601 date-time syntax with an explicit UTC offset.

Examples:

```text
2026-09-05T18:34:12+02:00
2026-09-05T16:34:12Z
```

Offset-less timestamps are invalid.

If `ended_at` exists, it must represent an instant strictly later than `started_at`.

An absent `ended_at` means the session is still active, interrupted, or otherwise not formally completed.

Trainlog must not invent an end time.

## 8. Session exercise order

The order of `session.exercises` is meaningful.

It records the exercise order entered by the user.

The order of `sets` is also meaningful and defines performed set order.

No separate set number is serialized.

## 9. Planned work and performed work

Each session exercise contains:

```json
{
  "exercise_id": "leg_press",
  "load_mode": "external",
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

They must remain separate.

`target.sets` is the intended number of sets.

The length of `sets` is the actual number of recorded sets and may be:

- smaller than the target;
- equal to the target;
- greater than the target;
- zero.

An empty actual-set array is valid for a planned exercise that was not performed.

## 10. Repetition mode

For a `tracking_mode` of `reps`:

- `target.reps` is required;
- `target.duration_seconds` is forbidden;
- each actual set contains `reps`;
- each actual set forbids `duration_seconds`.

Target repetitions must be at least 1.

Actual repetitions may be 0.

A zero-repetition set represents a real attempted set with no completed repetition.

A skipped set should normally be omitted instead.

## 11. Duration mode

For a `tracking_mode` of `duration`:

- `target.duration_seconds` is required;
- `target.reps` is forbidden;
- each actual set contains `duration_seconds`;
- each actual set forbids `reps`.

Duration values are positive integer seconds.

## 12. Load model

Each session exercise has exactly one `load_mode`:

```text
none
external
assistance
```

### 12.1 `none`

Use for exercises where no separate load value is recorded.

Examples:

- bodyweight squat;
- unweighted plank;
- push-up;
- pull-up without added or assisted load.

When `load_mode` is `none`, `weight_kg` is forbidden in the target and actual sets.

### 12.2 `external`

Use for a positive externally applied or machine-displayed load.

Examples:

- barbell;
- dumbbell;
- cable machine;
- leg press;
- weighted pull-up.

When `load_mode` is `external`:

- `target.weight_kg` is required;
- every actual set requires its own `weight_kg`.

Actual set load is stored per set so load changes remain representable.

### 12.3 `assistance`

Use when the numeric load represents assistance that reduces the effective difficulty of a bodyweight movement.

Example:

```text
Assisted pull-up: 20 kg assistance
```

When `load_mode` is `assistance`:

- `target.weight_kg` is required;
- every actual set requires its own `weight_kg`.

Assistance remains a positive value.

Analytics must not treat increasing assistance as increasing strength.

### 12.4 Unit and physical meaning

All serialized loads use kilograms.

For a machine, `weight_kg` records the load value displayed or declared by the machine/user.

Trainlog does not claim that this value equals exact mechanical force at the body.

This distinction matters when comparing different machines.

## 13. Rest

`rest_seconds` is required for every session exercise.

It stores the planned rest interval in integer seconds.

Example:

```text
60
```

means one minute.

Zero is valid when no planned rest exists.

v1 does not record measured rest between individual sets.

## 14. Body weight

`body_weight_kg` is optional.

It is a positive kilogram value associated with the session timestamp.

The TUI database may also support standalone body-weight observations; those records are outside this session-exchange document.

## 15. Body measurements

`measurements` is optional.

If present, it contains at least one measurement.

All measurements are circumferences in centimeters unless the field name itself defines another interpretation.

Frozen v1 measurement names proposed by Gate 1 review #1:

```text
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
```

`shoulders_cm` means shoulder-girdle circumference, not straight-line shoulder width.

Left/right fields are intentionally separate so asymmetry can be followed over time.

## 16. Notes

Optional UTF-8 notes are supported at:

- session level;
- session-exercise level.

A present note must not be empty or whitespace-only.

Session notes are limited to 4000 characters.

Exercise notes are limited to 1000 characters.

Notes are user content and must be preserved exactly after validation.

## 17. Catalog completeness

The top-level exercise catalog must contain exactly the exercise identities referenced by `session.exercises`.

Therefore:

- every session exercise has one matching catalog entry;
- no catalog entry is unreferenced;
- an empty session exercise array requires an empty top-level exercise catalog.

This keeps each session export self-contained without silently importing unrelated Android catalog entries.

## 18. One workout entry per exercise

A `session_id` may contain a given `exercise_id` at most once.

All performed sets for that exercise belong to its single session-exercise entry.

This makes editing and analytics deterministic.

## 19. Existing local exercise with different display name

Identity wins over display text.

If the TUI already knows an `exercise_id` and an imported session carries a different display name for that same identifier:

- the session may still import;
- the existing canonical TUI identity is used;
- the import must surface a non-fatal metadata warning;
- the session import must not silently rename the canonical local exercise.

Catalog synchronization and deliberate renaming are separate operations outside the v1 session-import transaction.

## 20. Structural and semantic validation

A valid v1 document passes both:

1. JSON Schema validation;
2. Trainlog semantic validation.

Semantic rules include:

- unique exercise identifiers;
- normalized display-name uniqueness;
- explicit timestamp offsets;
- timestamp chronology;
- exact catalog/reference set equality;
- one workout entry per exercise;
- catalog `tracking_mode` matching target and actual sets;
- `load_mode` matching weight presence;
- non-blank notes.

## 21. Explicit non-goals for v1

The following are deliberately not represented by v1:

- distance;
- speed;
- velocity;
- heart rate;
- calories;
- measured per-set rest;
- supersets/circuits as first-class objects;
- arbitrary custom set metrics;
- muscle-group classification;
- machine seat/settings metadata;
- photos;
- cloud synchronization.

Those features can be introduced later without corrupting the simple initial recorder.

## 22. Freeze criteria

Gate 1 may freeze v1 only after:

- canonical valid fixtures pass;
- canonical invalid fixtures fail for the intended reason;
- Android requirements are aligned;
- TUI requirements are aligned;
- the schema and semantic contract contain no known ambiguity;
- the reviewed commit is pushed and mirrored.

Gate 1 validation and mirrored review completed successfully.

The exchange format is now frozen:

```text
TRAINLOG_FORMAT_V1=FROZEN
```

Any incompatible semantic or structural change requires a new exchange-format version.
## 23. Generated identifier policy

The wire format treats `exercise_id` and `session_id` as opaque identifiers satisfying their defined syntax.

Official Trainlog implementations that create new identifiers must generate random UUID version 4 values.

Generated exercise identifiers use:

```text
ex_<uuid-v4>
```

Generated session identifiers use:

```text
se_<uuid-v4>
```

Example:

```text
ex_550e8400-e29b-41d4-a716-446655440000
se_550e8400-e29b-41d4-a716-446655440000
```

Importers accept any identifier valid under the v1 schema; they must not require that legacy or externally created IDs follow the UUID generation convention.

The generation rule exists to prevent collisions when Android and TUI can both create exercises independently.

## 24. Local catalog reconciliation

Document validity and local importability are separate concepts.

For each incoming exercise, the TUI reconciles against the local canonical catalog before the import transaction commits.

### Same ID, same tracking mode

Reuse the existing exercise.

If the normalized display name differs, update `name` and `normalized_name` in
the existing catalog row. This is a stable-ID rename: completed session
references remain attached to the same row and no second exercise is created.
If another identity already owns the incoming normalized name, reject the
import as an identity conflict.

### Same ID, different tracking mode

Reject the entire session import.

A repetition identity and duration identity are semantically incompatible.

### Different ID, equivalent normalized name

Reject the entire session import as an identity conflict.

Trainlog must not silently:

- create a duplicate;
- merge identities;
- rewrite historical identifiers.

The user must explicitly reconcile the conflict.

### New ID, unique normalized name

Create the exercise as part of the same database transaction as the session import.

### Atomicity

Any hard catalog conflict aborts the entire import.

No partial session or exercise data may remain.

## 25. Gate 1 executable import contract

Before the production importer exists, catalog reconciliation is specified by:

```text
tests/contract/catalog-import-cases.json
tools/validate_import_contract.py
```

Canonical validation requires both:

```bash
python tools/validate_json.py
python tools/validate_import_contract.py
```
