# Android Application

## 1. Purpose

The Android application is a lightweight training-session recorder.

Its design priority is low-friction data entry during a workout.

It is not the canonical history or analytics application.

## 2. Session flow

```text
Start session
    |
    v
record started_at
    |
    v
select/create exercise
    |
    v
enter target + planned rest
    |
    v
record actual sets
    |
    v
optional body data / notes
    |
    v
Finish session
    |
    v
record ended_at
    |
    v
validate + export Trainlog JSON
```

## 3. Exercise catalog

The Android application keeps a local exercise catalog so names are not retyped every session.

Creating an exercise requires:

- display name;
- tracking mode: repetitions or duration.

The application generates a stable `exercise_id`.

A new exercise used in an exported session is included in the top-level session export metadata and is therefore importable by the TUI.

The Android application must prevent accidental duplicate normalized names according to the Trainlog v1 contract.

## 4. Fast exercise form

For a repetition exercise, the basic form is conceptually:

```text
Exercise          Presse à cuisses
Load mode         External
Load              80 kg
Sets              4
Repetitions       5
Rest              60 s
```

For a timed exercise:

```text
Exercise          Gainage ventral
Load mode         None
Sets              3
Duration          45 s
Rest              60 s
```

The application should remember practical defaults from the previous use of an exercise when that reduces typing, but remembered UI defaults are not part of the exchange-format contract.

## 5. Load modes

The user chooses only when relevant:

- none;
- external;
- assistance.

`external` covers free weights and machine-displayed load.

`assistance` stores a positive assistance value.

The UI should label assistance explicitly so it cannot be confused with added resistance.

## 6. Actual work

The application should pre-populate actual sets from the target.

The user edits only what differs.

Example target:

```text
5 / 5 / 5 / 5
```

Actual:

```text
5 / 5 / 5 / 3
```

The export preserves both target and actual values.

Zero actual repetitions are valid for a real failed attempt.

A planned exercise may also have zero actual sets if it was never started.

## 7. Rest

`rest_seconds` is the planned rest duration for the exercise.

v1 does not require a running rest timer and does not serialize measured per-set rest.

A timer can be added later as UI behavior without changing the v1 format.

## 8. Timestamps

`started_at` is recorded automatically when the session starts.

`ended_at` is recorded automatically when the user finishes the session.

An active/interrupted local session may exist without `ended_at`.

The application must never invent an end timestamp merely to make export validation pass.

## 9. Body data

Optional session-associated data:

- body weight;
- neck;
- shoulders;
- chest;
- waist;
- hips;
- left/right arm;
- left/right forearm;
- left/right thigh;
- left/right calf.

The Android UI does not need to force these fields during every workout.

## 10. Notes

Session and exercise notes are optional.

The initial Android UI may omit note controls without violating v1, because the fields are optional.

## 11. Export

Before export, Android must enforce both:

- JSON structural validity;
- Trainlog v1 semantic validity.

A malformed or semantically inconsistent file must not be exported as a completed Trainlog document.

## 12. Non-goals

Initial Android versions do not need:

- analytics;
- complex graphs;
- cloud accounts;
- remote databases;
- social features;
- muscle classification;
- distance/cardio metrics;
- per-set rest measurement.
