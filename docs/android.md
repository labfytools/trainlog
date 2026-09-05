# Android Application

## 1. Purpose

The Android application is a lightweight training-session recorder.

Its design priority is low-friction data entry during a workout.

## 2. Session flow

Expected flow:

```text
Start session
    |
    v
record started_at
    |
    v
add exercises and targets
    |
    v
record actual sets
    |
    v
optional body data
    |
    v
Finish session
    |
    v
record ended_at
    |
    v
export Trainlog JSON
```

## 3. Exercise catalog

The application maintains a local exercise catalog for selection.

The user must not need to retype the same exercise every session.

When a new exercise is created:

- generate a stable `exercise_id`;
- store the display name;
- use that same identifier in future sessions;
- include the exercise catalog entry in exported files as required.

## 4. Planned work

The basic exercise form supports:

- number of sets;
- repetitions or timed duration;
- load when relevant;
- rest duration.

Example:

```text
4 sets
5 repetitions
80 kg
60 seconds rest
```

## 5. Actual work

The UI should pre-populate performed sets from the target when convenient.

The user only needs to edit differences.

Example target:

```text
5 / 5 / 5 / 5
```

Actual:

```text
5 / 5 / 5 / 3
```

The exported file must preserve both target and actual values.

## 6. Timestamps

`started_at` is recorded automatically when the session starts.

`ended_at` is recorded automatically when the session ends.

The UI may later allow explicit correction for forgotten starts or stops, but such correction must be visible to the user.

## 7. Export

The application exports valid Trainlog JSON.

It must not silently export malformed or incomplete data.

The application must validate required fields before final export.

## 8. Non-goals

Initial Android versions do not need:

- advanced analytics;
- complex charts;
- a cloud account;
- a remote database;
- social features.
