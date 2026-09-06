# Exercise data model

## Status

```text
EXERCISE_DATA_MODEL_V1=FROZEN_FOR_IMPLEMENTATION
DATABASE_SCHEMA_V3=NEXT
TUI_PROFILE_AWARE_ENTRY=AFTER_SCHEMA_V3
ANDROID_PROFILE_AWARE_ENTRY=AFTER_TUI
TRAINLOG_FORMAT_V1=FROZEN
SESSION_EXCHANGE_V2=DESIGN_REQUIRED_LATER
```

Trainlog must not use one universal exercise form.

An exercise is defined by three independent pieces of metadata:

```text
recording_mode = SETS | CONTINUOUS
tracking_mode  = REPS | DURATION
data_fields    = supplemental field bit mask
```

Initial valid combinations:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

`CONTINUOUS + REPS` is invalid in model v1.

Initial supplemental fields:

```text
SPEED_KMH
DISTANCE_KM
```

Unknown field bits are invalid.

Examples:

```text
Presse à cuisses
    SETS + REPS

Gainage
    SETS + DURATION

Marche
    CONTINUOUS + DURATION
    SPEED_KMH

Course
    CONTINUOUS + DURATION
    SPEED_KMH

Vélo
    CONTINUOUS + DURATION
    SPEED_KMH | DISTANCE_KM

Rameur
    CONTINUOUS + DURATION
    DISTANCE_KM
```

Load semantics remain separate and session-specific:

```text
none
external
assistance
```

A continuous exercise does not ask for:

```text
number of sets
repetitions
per-set rest
```

For example:

```text
Marche

Durée      45 min
Vitesse    5.8 km/h
```

Creating/editing an exercise asks for:

```text
Name
Organization: Sets | Continuous
Primary metric: Repetitions | Duration
Supplemental fields: Speed | Distance
```

Rules:

- continuous forces duration in model v1;
- sets accepts reps or duration;
- UI fields are driven by metadata, never exercise-name heuristics;
- changing catalog metadata affects future entry only.

Historical stability:

Every session exercise stores a snapshot of:

```text
recording_mode
tracking_mode
data_fields
```

So changing `Marche` from an old set-based duration exercise to continuous
duration + speed does not reinterpret old sessions.

## SQLite schema v3 direction

Schema v3 adds to `exercises`:

```text
recording_mode
data_fields
```

and snapshots the same values in `session_exercises`.

Migration v2 -> v3 is conservative:

```text
all existing exercises         -> SETS
all existing session exercises -> SETS
data_fields                    -> 0
```

No migration guesses by exercise name.

Continuous actual activity data gets its own one-to-one record:

```text
duration_seconds
speed_kmh       nullable
distance_km     nullable
```

Continuous activities have no `performed_sets` rows and no fake one-set
representation.

## Frozen JSON v1

Trainlog JSON session v1 remains frozen.

It represents the existing set-based exchange model.

Continuous data that cannot be represented in v1 must not be:

- hidden in notes;
- converted into a fake set;
- silently discarded.

A future explicit session exchange v2 will carry profile-aware exercise data
while v1 import remains supported.

## Android/TUI parity

Both interfaces consume identical exercise metadata.

The future PC -> Android catalog snapshot must contain:

```text
exercise_id
name
recording_mode
tracking_mode
data_fields
```

## Implementation order

```text
1. SQLite schema v3 + migration tests
2. C model/API additions
3. catalog create/edit support
4. TUI profile-aware session entry
5. manually convert Marche to CONTINUOUS + SPEED_KMH
6. profile-aware detail/history
7. Android uses the same model
8. design session exchange v2
```

<!-- TRAINLOG_PROFILE_AWARE_IMPLEMENTED -->
## Implemented checkpoint

The profile-aware exercise model is now implemented in the C model, SQLite
persistence and TUI.

Current canonical rules:

```text
recording_mode = SETS | CONTINUOUS
tracking_mode  = REPS | DURATION

known data_fields:
    SPEED_KMH
    DISTANCE_KM
```

Valid model-v1 combinations:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

Continuous exercise actual data is persisted as one `continuous_activity`
record rather than a performed-set list.

A continuous activity never manufactures a one-set representation.

The TUI asks continuous duration in **minutes**, converts to seconds, and stores
seconds internally.

Example:

```text
Marche
    CONTINUOUS + DURATION + SPEED_KMH

TUI entry:
    Durée (minutes)
    Vitesse km/h
```

Historical session rows snapshot recording metadata and are not reinterpreted
when catalog metadata later changes.
<!-- TRAINLOG_PROFILE_AWARE_IMPLEMENTED _END -->
