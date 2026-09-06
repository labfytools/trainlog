# Android application

## 1. Purpose

The Android application is Trainlog's low-friction capture client.

It is a native Kotlin/Jetpack Compose application with local SQLite persistence.

The desktop remains the canonical long-term history and analytics store.

## 2. Implemented navigation

```text
Accueil
├── Enregistrer une séance
├── Enregistrer un exercice
├── Enregistrer des mensurations
├── Historique des séances
└── Synchroniser avec le PC
```

## 3. Local persistence

Android local database version:

```text
3
```

Domain tables cover:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
body_observations
```

This database is Android-local. It is not copied to the PC.

## 4. Exercise catalog

Exercise creation records:

```text
name
recording_mode
tracking_mode
data_fields
```

Stable identity:

```text
ex_<uuid-v4>
```

The UI rejects invalid profile combinations and local normalized-name
collisions.

An exercise may be created standalone or inline while building a session.

## 5. Session recording

Stable session identity:

```text
se_<uuid-v4>
```

Session entry is profile-aware.

### Sets + repetitions

Actual set values may be heterogeneous.

Compact entry supports:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

### Sets + duration

Each performed set stores its own duration.

### Continuous + duration

The form asks for duration and only the configured supplemental fields such as
speed or distance.

Continuous work does not create fake sets.

## 6. Session draft editing

Before a session is saved, an exercise already added to the draft can be
removed.

Removing one exercise does not alter the exercise catalog entry itself.

## 7. Session history

Android exposes persisted local session history and profile-aware detail.

Set-based history renders ordered performed sets.

Continuous history renders its one activity record with configured supplemental
values.

## 8. Body measurements

Supported metrics:

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
empty field = not measured
at least one positive metric required
comma or dot accepted for decimal entry
```

Stable identity:

```text
bo_<uuid-v4>
```

## 9. Automatic mobile snapshot

Android maintains:

```text
Download/Trainlog/trainlog-mobile-export-v1.json
```

The snapshot is refreshed after relevant local changes, including exercise,
session, body-observation, and PC-catalog updates.

The user does not need a separate manual export step before synchronization.

## 10. PC catalog access

PC-created files are accessed through a persistent Storage Access Framework
grant.

The selected folder must be:

```text
Download/Trainlog
```

The Sync screen always permits changing the stored folder selection.

No application-data reset is required to fix a wrong folder choice.

## 11. Android-triggered synchronization

The Sync screen exposes:

```text
Synchroniser maintenant
```

Android writes:

```text
trainlog-sync-request-v1.json
```

and waits for a matching:

```text
trainlog-sync-receipt-v1.json
```

The receipt is matched by `request_id`.

On success Android then applies the latest PC catalog and displays the final
result.

A receipt belonging to another request is ignored as pending rather than
misreported as the current result.

## 12. Synchronization ownership

Android does not initiate raw MTP operations itself.

MTP is host-initiated:

```text
Android request
    -> PC trainlog-syncd
    -> shared desktop sync engine
    -> receipt
```

## 13. Build

Example local configuration:

```bash
cd android

printf 'sdk.dir=%s\n' "$HOME/Android/Sdk" > local.properties

JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew assembleDebug
```

Install to a connected test device:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

`local.properties` is local machine configuration and must not be committed.

## 14. Non-goals

Android is not intended to own:

- canonical long-term analytics;
- complex body/performance graphs;
- cloud accounts;
- direct SQLite-file synchronization;
- exercise-name heuristics;
- a mounted-filesystem dependency.
