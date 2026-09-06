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
## 13. Identifier generation

When Android creates a new exercise, it generates:

```text
ex_<random UUID v4>
```

When Android creates a new session, it generates:

```text
se_<random UUID v4>
```

Display-name slugs must not be used as persistent identifiers.

The visible exercise name remains independent from identity.

## 14. Catalog conflict behavior

Android must prevent duplicate normalized names inside its own local catalog.

A valid Android export can still conflict with an independently edited TUI catalog.

The TUI owns final reconciliation.

Android must not assume that a matching display name means two different IDs may be silently merged.

<!-- TRAINLOG_ANDROID_MTP_TRANSPORT -->
## 15. USB file-transfer transport

The initial Android/Linux integration uses standard Android **file transfer
(MTP)** mode.

The Linux TUI/core accesses the device directly with `libudev` + `libmtp`.
Trainlog does not require a mounted Android filesystem.

Validated Linux-side capabilities:

```text
USB_MTP_DETECTION=PASS
MTP_STORAGE_ACCESS=PASS
MTP_WRITE=PASS
MTP_READ=PASS
MTP_ROUNDTRIP=PASS
```

The transport foundation currently uses a root `Trainlog` folder for physical
validation.

The final JSON exchange subdirectory/naming convention is defined in the next
slice before the Android recorder depends on it.

The Android application itself will only need to produce a valid frozen
Trainlog JSON v1 document and place it in the agreed exchange area.
<!-- TRAINLOG_ANDROID_MTP_TRANSPORT _END -->

<!-- TRAINLOG_ANDROID_NEXT_SLICE -->
## 16. Current Android implementation cursor

The Linux transport and Sync TUI foundations are complete enough to begin the
Android client.

Initial Android development uses fictitious data.

First Android slice:

```text
1. application scaffold
2. local exercise catalog
3. create/select exercise
4. session form
5. actual set entry
6. optional body measurements
7. fictitious completed session
```

Transport/export is added only after the local recorder workflow is comfortable.

The Android client must preserve stable exercise IDs so a catalog snapshot from
the PC can normalize exercise selection on both sides.
<!-- TRAINLOG_ANDROID_NEXT_SLICE _END -->

<!-- TRAINLOG_ANDROID_PROFILE_AWARE_ENTRY -->
## Profile-aware Android forms

Android uses the same exercise metadata as the TUI:

```text
recording_mode
tracking_mode
data_fields
```

A continuous `Marche` configured with speed displays only:

```text
Durée
Vitesse
```

and no set count.

Creating an exercise directly inside session entry must configure this metadata
before adding it to the catalog/session.
<!-- TRAINLOG_ANDROID_PROFILE_AWARE_ENTRY _END -->
