# Android local workflows checkpoint

## Status

```text
ANDROID_SCAFFOLD=PASS
ANDROID_THEME_PARITY=PASS
ANDROID_SYSTEM_BARS=PASS
ANDROID_EXERCISE_CREATE=PASS
ANDROID_INLINE_EXERCISE_CREATE=PASS
ANDROID_SESSION_RECORDING=PASS
ANDROID_SESSION_HISTORY=PASS
ANDROID_SESSION_DETAIL=PASS
ANDROID_BODY_RECORDING=PASS
ANDROID_LOCAL_WORKFLOWS=PASS

ANDROID_MTP_SYNC=NEXT
```

## Visual contract

Android and TUI share the same Trainlog visual language:

```text
dark background
monospace typography
cyan/teal accent
yellow active/focus frame
green success
red error
blue muted/navigation
magenta graph role
```

The Android launcher icon is intentionally only:

```text
T
```

using Trainlog theme colors.

## Home structure

```text
ENREGISTREMENT
├── Enregistrer une séance
├── Enregistrer un exercice
└── Enregistrer des mensurations

CONSULTATION
└── Historique des séances
```

## Exercise creation

Android uses the same exercise-profile model as desktop:

```text
recording_mode
tracking_mode
data_fields
```

Supported profile combinations:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

Known supplemental fields:

```text
SPEED_KMH
DISTANCE_KM
```

Behavior is never inferred from exercise names.

Exercise creation is available:

```text
standalone
inline from session recording
```

Inline creation returns directly to the session workflow.

## Session recording

The local Android session flow is:

```text
choose catalog exercise
→ profile-aware form
→ add to session draft
→ repeat
→ save session
```

Persistence is split semantically:

```text
SET-based exercise
    performed_sets

CONTINUOUS exercise
    continuous_activity
```

Continuous activities never create fake performed sets.

## Session history

Persisted sessions are visible in Android history.

Detail rendering remains profile-aware:

```text
SETS + REPS
    per-set reps

SETS + DURATION
    per-set durations

CONTINUOUS
    duration
    configured speed
    configured distance
```

## Body measurements

Android supports the same body measurement fields as the TUI:

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
empty = not measured
comma or dot accepted
at least one positive metric required
```

Recent observations are displayed in the body screen.

## Android local database

Current Android-local schema version:

```text
3
```

Current local tables include:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
body_observations
```

The Android SQLite database is intentionally independent from the desktop
SQLite database.

Synchronization must exchange versioned Trainlog domain data.

Do not synchronize or copy SQLite database files.

## Manual validation

Validated on a real Samsung device through ADB:

```text
APK build/install/launch
exercise creation
catalog persistence across restart
session recording
performed-set persistence
session history/detail
body observation persistence
```

Example validated session:

```text
Pompe
SETS + REPS
5 sets × 10 reps
```

## Next cursor

```text
ANDROID_MTP_SYNC=NEXT
```

The next slice should connect the already-validated direct MTP transport design
to Android/desktop exchange without weakening the frozen Trainlog JSON v1
contract.
