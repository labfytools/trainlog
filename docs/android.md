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

<!-- TRAINLOG_ANDROID_PROFILE_CURSOR -->
## Android implementation cursor

Android is the next implementation area.

The application must share Trainlog's visual language with the TUI.

Theme direction:

```text
dark background
cyan/teal Trainlog accent
yellow active/focus role
green success
red error
```

Launcher icon:

```text
T
```

Only the letter `T`, using Trainlog theme colors.

Required recording sections:

```text
Séance
Exercice
Mensurations
```

Session recording must allow creating a new exercise inline without leaving the
session flow.

Android forms are driven by the same profile metadata as desktop:

```text
recording_mode
tracking_mode
data_fields
```

Examples:

```text
SETS + REPS
    sets / reps / load / rest

SETS + DURATION
    sets / duration / optional load / rest

CONTINUOUS + DURATION + SPEED_KMH
    duration / speed
```

The app must never infer an input form from an exercise display name.

Initial development uses fictitious records. Test/development data is removed
before normal production use starts.
<!-- TRAINLOG_ANDROID_PROFILE_CURSOR _END -->

<!-- TRAINLOG_ANDROID_SCAFFOLD_IMPLEMENTED -->
## Android scaffold implementation

The Android project now lives in:

```text
android/
```

It is a native Kotlin + Jetpack Compose application with a custom Trainlog
visual layer rather than default Material presentation.

Implemented scaffold navigation:

```text
Accueil
├── Enregistrer une séance
│   ├── Ajouter depuis le catalogue
│   └── Créer un nouvel exercice
│       └── returns to session flow
├── Enregistrer un exercice
└── Enregistrer des mensurations
```

The launcher icon is a minimal themed `T`.

The UI reuses the Trainlog visual roles from the TUI:

```text
background
accent/cyan
success/green
warning/yellow
error/red
muted/blue
graph/magenta
```

Android forms will be driven by:

```text
recording_mode
tracking_mode
data_fields
```

The scaffold intentionally does not implement persistence or MTP yet.

Next:

```text
ANDROID_LOCAL_MODEL_AND_PERSISTENCE=NEXT
ANDROID_SESSION_FORM=AFTER
ANDROID_MTP_SYNC=AFTER_LOCAL_WORKFLOW
```
<!-- TRAINLOG_ANDROID_SCAFFOLD_IMPLEMENTED _END -->

<!-- TRAINLOG_ANDROID_LOCAL_CATALOG -->
## Android local catalog checkpoint

The Android application now has a persistent local exercise catalog.

Implemented:

```text
Android SQLite exercise database
profile-aware exercise model
standalone exercise creation
inline exercise creation from session flow
catalog survives application restart
session screen refreshes after inline creation
```

Android uses the same semantic axes as desktop:

```text
recording_mode
tracking_mode
data_fields
```

Known supplemental fields remain:

```text
SPEED_KMH
DISTANCE_KM
```

Continuous creation forces duration tracking. Set-based creation keeps
supplemental continuous fields disabled.

The local Android schema is intentionally independent from the desktop SQLite
schema. Synchronization later exchanges versioned domain data rather than
copying SQLite database files.

Next:

```text
ANDROID_SESSION_RECORDING=NEXT
ANDROID_BODY_PERSISTENCE=AFTER
MTP_SYNC=AFTER_LOCAL_WORKFLOWS
```
<!-- TRAINLOG_ANDROID_LOCAL_CATALOG _END -->

<!-- TRAINLOG_ANDROID_SESSION_RECORDING -->
## Android session recording checkpoint

Android can now build and persist real local sessions.

Flow:

```text
Session
→ choose catalog exercise
→ profile-aware entry form
→ add exercise to session draft
→ repeat for additional exercises
→ save session
```

Profile-aware forms:

```text
SETS + REPS
    set count
    repetitions per set

SETS + DURATION
    set count
    duration per set

CONTINUOUS + DURATION
    duration minutes
    configured speed/distance fields
```

Persistence mirrors the domain split:

```text
sessions
session_exercises
performed_sets
continuous_activity
```

Continuous exercises do not create fake performed sets.

The Android local database version is now 2.

Next:

```text
ANDROID_SESSION_HISTORY=NEXT
ANDROID_BODY_PERSISTENCE=AFTER
MTP_SYNC=AFTER_LOCAL_WORKFLOWS
```
<!-- TRAINLOG_ANDROID_SESSION_RECORDING _END -->

<!-- TRAINLOG_ANDROID_SESSION_HISTORY -->
## Android session history checkpoint

Android now exposes persisted local sessions through:

```text
Accueil
→ Consultation
→ Historique des séances
→ Détail séance
```

Detail rendering remains profile-aware:

```text
SETS + REPS
    one line per performed set with reps

SETS + DURATION
    one line per performed set with duration

CONTINUOUS
    duration
    configured speed
    configured distance
```

The history reader uses the persisted session snapshot metadata rather than
inferring behavior from exercise names.

Next:

```text
ANDROID_BODY_PERSISTENCE=NEXT
ANDROID_LOCAL_WORKFLOWS_THEN_MTP
```
<!-- TRAINLOG_ANDROID_SESSION_HISTORY _END -->

<!-- TRAINLOG_ANDROID_BODY_PERSISTENCE -->
## Android body measurement checkpoint

The Android body workflow is now persistent and uses the same measurement set
as the TUI.

Fields:

```text
weight
neck
shoulders
chest
waist
hips
left/right arm
left/right forearm
left/right thigh
left/right calf
```

Rules:

```text
empty field = measurement not taken
at least one positive metric required
comma or dot accepted for decimal entry
```

Android SQLite schema version:

```text
3
```

The body screen also shows the five most recent observations.

At this point the three primary Android recording workflows are locally
functional:

```text
session recording
exercise creation
body measurement recording
```

Next:

```text
ANDROID_LOCAL_POLISH_AND_VALIDATION=NEXT
MTP_SYNC=AFTER_LOCAL_CHECKPOINT
```
<!-- TRAINLOG_ANDROID_BODY_PERSISTENCE _END -->

<!-- TRAINLOG_ANDROID_LOCAL_WORKFLOWS_PASS -->
## Local Android workflows — validated

```text
ANDROID_SCAFFOLD=PASS
ANDROID_THEME_PARITY=PASS
ANDROID_EXERCISE_CREATE=PASS
ANDROID_INLINE_EXERCISE_CREATE=PASS
ANDROID_SESSION_RECORDING=PASS
ANDROID_SESSION_HISTORY=PASS
ANDROID_BODY_RECORDING=PASS
ANDROID_LOCAL_WORKFLOWS=PASS
```

The application is now locally usable for its three primary recording flows:

```text
session
exercise
body measurements
```

Session and history rendering are profile-aware.

The Android-local SQLite database is not a synchronization format.

Next:

```text
ANDROID_MTP_SYNC=NEXT
```
<!-- TRAINLOG_ANDROID_LOCAL_WORKFLOWS_PASS _END -->

<!-- TRAINLOG_MOBILE_EXPORT_V1 -->
## MTP mobile export v1

Android now prepares a versioned full mobile snapshot at:

```text
Download/Trainlog/trainlog-mobile-export-v1.json
```

The file contains:

```text
exercise profiles
sessions
body observations
```

It is explicitly separate from frozen `TRAINLOG_FORMAT_V1`.

Desktop direct-MTP validation is available through:

```text
./build/tui/trainlog-mtp-mobile-export-probe
```

The probe traverses:

```text
internal storage
→ Download
→ Trainlog
→ trainlog-mobile-export-v1.json
```

and downloads it directly through libmtp without a mount.

Next after hardware PASS:

```text
DESKTOP_MOBILE_EXPORT_IMPORT=NEXT
PC_TO_ANDROID_CATALOG=AFTER
```
<!-- TRAINLOG_MOBILE_EXPORT_V1 _END -->

<!-- TRAINLOG_ANDROID_SYNC_FOLDER_CHECKPOINT -->
## Android synchronization folder

PC-created synchronization artifacts are consumed through a persistent Storage
Access Framework grant.

Canonical selected folder:

```text
Download/Trainlog
```

The Sync screen always exposes the folder-selection action.

When a folder is already authorized, the action becomes:

```text
Changer le dossier Trainlog
```

This is required so a wrong persisted folder selection can be corrected without
clearing the Android application database.

Validated PC catalog publication:

```text
trainlog-pc-catalog-v1.json
```

The final Android synchronization workflow must evolve toward a single
`Synchroniser maintenant` action backed by a PC-side synchronization agent,
rather than manual export/import steps.
<!-- TRAINLOG_ANDROID_SYNC_FOLDER_CHECKPOINT _END -->
