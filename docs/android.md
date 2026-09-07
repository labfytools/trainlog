# Android application

## 1. Purpose

The Android application is Trainlog's low-friction capture client.

It is a native Kotlin/Jetpack Compose application with local SQLite persistence.

The desktop remains the canonical long-term history and analytics store.

## 2. Implemented navigation

```text
Accueil
├── Reprendre la séance en cours (si un brouillon existe)
├── Enregistrer une séance
├── Enregistrer un exercice
├── Enregistrer des mensurations
├── Historique des séances
└── Synchroniser avec le PC
```

## 3. Local persistence

Android local database version:

```text
4
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

Schema v4 adds `active_session_draft`, `draft_session_exercises`,
`draft_performed_sets` and `draft_continuous_activity`. The additive v3 -> v4
migration preserves catalog, completed sessions/actuals and body observations.
Exactly one active draft is supported; it is separate from completed history.

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

### Editing an exercise

Every existing catalog item exposes **Modifier**. Editing a name trims its
input, recomputes `normalized_name`, and rejects a normalized-name collision.
The row retains its existing `exercise_id`; naming is presentation metadata,
not identity. Completed session rows and the active draft retain their catalog
row relationship and immediately resolve the renamed display text after reopen.

Profile fields (`recording_mode`, `tracking_mode`, `data_fields`) are editable
only while an exercise has no completed-session or active-draft reference. Once
referenced, Android displays the lock and returns an explicit incompatible
profile result rather than silently reinterpreting work or creating another
exercise. Renaming remains available independently.

The shared Compose `TrainlogScreen` header is used by Accueil, Séance,
Exercice, Mensurations, Historique, Détail séance and Sync. Its compact
`◆ TRAINLOG ◆` accent plaque and muted subtitle intentionally mirror the
Notcurses TUI identity in a flat mobile layout.

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

The repository durably saves every meaningful mutation, including session type,
exercise selection/addition/removal, actual values and raw form edits. Partial
text such as `4,5,6,` is retained without normalization. A failed write displays
a specific error and does not claim the latest change was saved.

Home shows **Reprendre la séance en cours** and an exercise-count/type summary.
The ordinary new-session action opens an existing draft without overwriting it.
Back returns Home and preserves the draft. Backgrounding, switching apps,
Activity/configuration recreation, background process death and force-stop with
relaunch preserve the draft; these paths were validated on the Samsung SM_G990B.

**Retirer <exercice>** removes only that draft exercise and its actual values.
It does not change the catalog or completed history. Removal survives restart.
**Supprimer la séance en cours** requires deliberate confirmation; cancellation
preserves the draft. Confirmed deletion leaves no completed session or stale
resume action after relaunch.

Final save validates the durable draft, inserts the completed session and actual
values, and removes the draft in one SQLite transaction. Failure rolls back and
retains the draft for retry; repeated completion does not create duplicates.
The existing completed-session save-time timestamp behavior is unchanged.

PC catalog reconciliation preserves draft references through catalog row
ownership. If an editing selection no longer resolves, only the selection is
cleared; added exercises and raw text remain, with a specific diagnostic.
When a received or exported catalog entry has the same `exercise_id`, a changed
display name is reconciled in that same row. A different-ID normalized-name
collision is rejected, so a rename cannot become a duplicate exercise.

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

An active draft is never included in completed history, session detail or this
snapshot. Synchronization continues to exchange completed data while the draft
stays local; no draft fields were added to the frozen mobile artifact.

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

Host regression suite: 8 tests. Device instrumentation: 5 tests (2 repository,
3 production-screen UI tests using an isolated database and no shared export).
The real device matrix additionally exercised production `MainActivity`,
including verified process exit with `am kill`, force-stop, configuration
relaunch, raw-form recovery, removal, discard and unchanged user data. Final-save
UI checks use isolated data so fictitious workouts do not enter user history.
See [tests](tests.md) for commands and the precise validation boundary.

## 14. Non-goals

Android is not intended to own:

- canonical long-term analytics;
- complex body/performance graphs;
- cloud accounts;
- direct SQLite-file synchronization;
- exercise-name heuristics;
- a mounted-filesystem dependency.

## 15. Test-max sessions

Android session entry exposes:

```text
Entraînement
Test max
```

The selection is persisted in the existing Android `sessions.session_type`
column and exported in the mobile snapshot as:

```text
training
max_test
```

History and detail visibly identify max-test sessions.

Selecting `Test max` is explicit metadata; Trainlog does not infer max tests
from large repetition or duration values.

Android's current session form still records the exercise data fields it
supports. Measured-max classification on the desktop uses only actual values
that were truly captured and synchronized.
