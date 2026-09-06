# Profile-aware continuous exercise checkpoint

## Status

```text
EXERCISE_DATA_MODEL_V1=IMPLEMENTED
DATABASE_SCHEMA_V4=IMPLEMENTED
PROFILED_CATALOG_API=IMPLEMENTED
PROFILE_AWARE_EXERCISE_CREATION=IMPLEMENTED
CONTINUOUS_ACTIVITY_PERSISTENCE=IMPLEMENTED
CONTINUOUS_ACTIVITY_DETAIL_DISPLAY=IMPLEMENTED
CONTINUOUS_DURATION_MINUTES_UI=IMPLEMENTED
TRAINLOG_FORMAT_V1=FROZEN
ANDROID_APP=NEXT
```

The implementation now distinguishes set-based exercises from continuous
activities.

This checkpoint must be treated as the current architecture by future agents.

## Core model

Exercise catalog metadata:

```text
recording_mode = SETS | CONTINUOUS
tracking_mode  = REPS | DURATION
data_fields    = bounded supplemental-field bit mask
```

Known supplemental fields:

```text
SPEED_KMH
DISTANCE_KM
```

Initial valid combinations:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

`CONTINUOUS + REPS` is invalid.

Behavior is driven by catalog metadata, never by exercise-name heuristics.

## Examples

```text
Presse à cuisses
    SETS + REPS

Gainage
    SETS + DURATION

Marche
    CONTINUOUS + DURATION
    SPEED_KMH

Vélo
    CONTINUOUS + DURATION
    SPEED_KMH | DISTANCE_KM
```

## Historical stability

Catalog metadata controls future entry.

Each `session_exercises` row stores a snapshot of:

```text
recording_mode
data_fields
```

Historical sessions must use their snapshot rather than current catalog
metadata.

Existing rows migrated from older schemas remain `SETS` unless explicitly
changed for future entries.

No migration guesses behavior from names such as `Marche`.

## SQLite schema v4

Current schema version:

```text
PRAGMA user_version = 4
```

Relevant tables:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
body_observations
```

`continuous_activity` is one-to-one with a continuous session exercise and
contains:

```text
duration_seconds
speed_kmh     nullable
distance_km   nullable
```

A continuous activity has:

```text
target_sets              = NULL
target_reps              = NULL
target_duration_seconds  = NULL
load_mode                = none
rest_seconds             = 0
performed_sets rows      = 0
```

Trainlog must not create a fake one-set representation for a continuous
activity.

## TUI behavior

Set-based exercise:

```text
Charge
Séries prévues
Répétitions or duration target
Repos
Séries réellement faites
```

Continuous exercise:

```text
Durée (minutes)
optional configured supplemental fields
```

For `Marche + SPEED_KMH`:

```text
Durée (minutes)
Vitesse km/h
```

A bare duration value in this continuous form is minutes.

Example:

```text
15 -> 15 minutes -> 900 seconds persisted
```

SQLite still stores durations in seconds.

## Session detail behavior

Set-based history uses the existing sets presentation.

Continuous history displays its actual continuous values.

Example:

```text
Exercice 1/1 — Marche

Mode : continu

Durée : 15 min
Vitesse : 7.0 km/h

Réalisé : activité continue
```

It must never display:

```text
0 séries
0 série × 0 s
aucune série réalisée
```

for a valid continuous activity.

## Catalog creation

The lower-level catalog supports explicit profile creation.

Current API:

```text
trainlog_catalog_create_exercise_profiled(...)
```

Legacy exercise creation remains available and means:

```text
recording_mode = SETS
data_fields    = 0
```

TUI creation from both:

```text
Exercices page
session inline creation
```

supports selecting continuous mode and supplemental fields.

## Frozen exchange boundary

Trainlog session JSON v1 remains frozen.

Continuous metrics are not forced into v1.

Trainlog must never:

- hide speed/distance in notes;
- synthesize a fake set to make continuous data fit v1;
- silently drop continuous metrics.

A future versioned exchange contract will carry profile-aware session data.

## Android implementation cursor

The next implementation area is the Android client.

Android must use the same model:

```text
recording_mode
tracking_mode
data_fields
```

Required top-level recording sections:

```text
Séance
Exercice
Mensurations
```

Inside session recording, the user must be able to create a new exercise
without leaving the session flow.

Android visual identity must match the TUI:

```text
dark background
Trainlog cyan/teal accent
yellow active/focused border role
green success
red error
```

The Android launcher icon is intentionally minimal:

```text
T
```

using the Trainlog theme colors.

Initial Android development uses fictitious data. The development database is
purged before normal use begins.

## Validation checkpoint

Normal build/test checkpoint after continuous activity work:

```text
15 tests expected
git diff --check clean
```

The important functional manual checks are:

```text
create continuous Marche + speed
record duration in minutes
persist duration as seconds
persist speed in continuous_activity
no performed_sets rows for continuous activity
reopen session detail
display duration + speed as continuous activity
```
