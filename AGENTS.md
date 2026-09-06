# Trainlog Development Contract

## 1. Scope

Trainlog consists of:

- a native Android application for fast workout and body-data capture;
- a Unix/Linux C17 ncursesw TUI for durable history, correction, analysis,
  visualization, and manual synchronization;
- a small user-session PC agent, `trainlog-syncd`, for Android-triggered
  synchronization;
- versioned JSON synchronization artifacts exchanged over direct MTP.

The desktop SQLite database is the canonical long-term history.

Android has an independent local SQLite store for offline capture. SQLite
database files are never synchronized directly.

## 2. Frozen compatibility boundary

`TRAINLOG_FORMAT_V1` is frozen.

A published format version must never receive an incompatible semantic change.

New synchronization or domain needs use separate, explicitly versioned
artifacts. Do not overload frozen v1 through notes, fake sets, or silent data
loss.

## 3. Exercise model

Exercise behavior is metadata-driven:

```text
recording_mode = SETS | CONTINUOUS
tracking_mode  = REPS | DURATION
data_fields    = bounded supplemental field mask
```

Valid model-v1 combinations are:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

`CONTINUOUS + REPS` is invalid.

Continuous activity must not be represented as a fake performed set.

Actual set values are independent records. Heterogeneous repetitions are valid.

## 4. Identity

Stable identities use UUIDv4-based creator IDs:

```text
ex_<uuid-v4>   exercise
se_<uuid-v4>   session
bo_<uuid-v4>   body observation
sy_<uuid-v4>   synchronization run
```

Display names are not identities.

Import and synchronization paths must remain idempotent by stable IDs.

## 5. Desktop implementation

The desktop core is C17.

Current primary dependencies:

- ncursesw;
- SQLite3;
- utf8proc;
- libuuid;
- libudev;
- libmtp;
- Meson;
- Ninja.

Business logic, persistence, transport, and rendering remain separated.

SQLite operations must not be scattered through rendering code.

Important business rules must not depend directly on ncurses.

Strict warning policy must not be weakened to make a change compile.

## 6. Android implementation

Android is a Kotlin/Jetpack Compose capture client.

It owns local data entry and local persistence for:

- exercise catalog entries;
- sessions;
- performed sets;
- continuous activities;
- body observations.

It is not the canonical analytics store.

The Android UI is driven by exercise metadata, never by exercise-name
heuristics.

## 7. Synchronization architecture

Desktop access to Android uses physical-device discovery with `libudev` and
direct object access with `libmtp`.

Do not introduce a mandatory GVFS/FUSE mount.

Canonical exchange folder:

```text
Download/Trainlog
```

The shared desktop synchronization engine is:

```text
trainlog_sync_run()
```

Both the TUI and `trainlog-syncd` use this engine.

Android-triggered synchronization uses:

```text
trainlog-sync-request-v1.json
trainlog-sync-receipt-v1.json
```

Android -> PC data uses:

```text
trainlog-mobile-export-v1.json
```

PC -> Android catalog data uses:

```text
trainlog-pc-catalog-v1.json
```

## 8. Persistence

Desktop SQLite schema is versioned with:

```sql
PRAGMA user_version;
```

The current desktop schema is v5.

Every incompatible schema evolution requires an explicit migration and
regression coverage.

Foreign keys must be enabled.

Multi-row mutations that represent one user operation must be transactional.

## 9. Error handling

Trainlog prefers explicit failure over silent corruption.

Examples:

- malformed exchange JSON -> reject;
- unsupported version -> reject;
- duplicate stable ID -> idempotent skip or explicit conflict as defined;
- incompatible exercise profile -> reject;
- incomplete session -> preserve explicitly;
- failed persisted-session replacement -> rollback;
- synchronization failure -> record a meaningful diagnostic.

User-facing code must not intentionally return placeholders such as
`error=unknown` when a specific failure can be reported.

## 10. Documentation

Canonical documents:

- `README.md`;
- `docs/current_state.md`;
- `docs/architecture.md`;
- `docs/coding_style.md`;
- `docs/exchange_format.md`;
- `docs/exercise_data_model.md`;
- `docs/database.md`;
- `docs/tui.md`;
- `docs/android.md`;
- `docs/sync_exchange.md`;
- `docs/tests.md`;
- `docs/roadmap.md`;
- `CHANGELOG.md`.

Checkpoint history belongs in Git history and `docs/reviews`; canonical
documents describe the current state rather than accumulating obsolete
`NEXT` sections.

## 11. Validation

Before every meaningful push:

```bash
meson compile -C build
meson test -C build --print-errorlogs

python tools/validate_json.py
python tools/validate_import_contract.py

git diff --check
git status --short
```

When Android code changes:

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew assembleDebug
```

Run ASan/UBSan at meaningful C implementation checkpoints.

Hardware-dependent MTP tests remain explicit manual validations and are not
required to run in CI without a connected unlocked Android device.

## 12. Git workflow

Forgejo is primary:

```text
ssh://git@git.labfytools.com:2223/fy59/trainlog.git
```

GitHub is a mirror:

```text
git@github.com:labfytools/trainlog.git
```

Do not develop directly against the GitHub mirror.

## 13. Definition of Done

A task is complete only when:

- behavior is implemented;
- the affected code builds without accepted warnings;
- relevant tests pass;
- new error paths are explicit;
- persistent-format changes have migrations;
- synchronization remains idempotent where applicable;
- documentation describes the resulting state;
- no known regression is intentionally left behind.
