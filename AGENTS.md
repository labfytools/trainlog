# Trainlog Development Contract

## 1. Scope

Trainlog consists of:

- a native Android application for fast workout and body-data capture;
- a Unix/Linux C17 terminal TUI for durable history, correction, analysis,
  visualization, and manual synchronization, using Notcurses as its completed
  active rendering/input backend;
- a loopback-only local Web adapter whose Dashboard is implemented on the
  0.1.2 development branch; its Analyse, Programmes, Sessions, and Exercises
  routes remain placeholders pending separate contracts;
- a small user-session PC agent, `trainlog-syncd`, for Android-triggered
  synchronization;
- versioned JSON synchronization artifacts exchanged over direct MTP.

The desktop SQLite database is the canonical long-term history.

The full-generation orchestrator and Web sync surface are implemented behind a
trusted local opt-in. Default deployed automatic exchange remains V3 until the
separate controlled-rollout decision.

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

Android exercise editing preserves `exercise_id`: a rename trims and
re-normalizes display metadata in the existing row and must not create a second
exercise. Profile changes are rejected once completed history or an active draft
references the exercise; renaming remains safe. Same-ID catalog reconciliation
updates name metadata in place and rejects different-ID normalized-name
collisions.

## 5. Desktop implementation

The desktop core is C17.

Current primary dependencies:

- SQLite3;
- utf8proc;
- libuuid;
- libudev;
- libmtp;
- Meson;
- Ninja;
- one active desktop terminal backend.

For the completed `TUI_NOTCURSES_V1` infrastructure checkpoint:

```text
legacy backend = ncursesw (historical only)
active backend = Notcurses
```

`TUI_NOTCURSES_V1=PASS`. Active desktop TUI code and build wiring use
Notcurses and do not retain ncursesw as an unused compatibility backend.

Business logic, persistence, transport, and rendering remain separated.

SQLite operations must not be scattered through rendering code.

Important business rules must not depend directly on ncurses, Notcurses, or
terminal-library-specific key constants.

Strict warning policy must not be weakened to make a change compile.

### Desktop TUI backend contract

The terminal library is infrastructure, not product semantics.

The active TUI backend must preserve:

```text
public entry point: trainlog_tui_run(TrainlogDatabase *)
minimum terminal: 72x20
small-terminal fallback
keyboard-first navigation
UTF-8 text input
resize recovery
semantic color roles
all existing screen/workflow behavior
```

Terminal-library state must not become process-global application state.

Application screen logic should consume Trainlog-owned key/input semantics
rather than raw backend-specific `KEY_*`/event constants.

The Notcurses migration may modernize rendering with true color, Unicode
borders, flat panels, and clearer focus/selection states, but must not change:

```text
database schema or SQL semantics
TRAINLOG_FORMAT_V1
exercise semantics
session semantics
measured-max semantics
body-analytics semantics
sync/MTP protocols
Android behavior
```

After the migration is validated, canonical documentation must describe
Notcurses as the active desktop TUI backend.

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

Android local SQLite schema v23 retains the exactly-one durable active-session
draft introduced in v4.
Every meaningful draft/form mutation is persisted by the repository. Back,
backgrounding and process death never delete the draft. Home offers explicit
resume; whole-draft discard requires confirmation. Final completed-session
insertion and draft deletion are one transaction. Drafts are excluded from
completed history and mobile export. Preserve raw partial form input and use
an explicit, non-destructive migration for future Android schema changes.

Android schema v11 additionally stores optional planning metadata separately
from actual occurrence data after the additive v10 -> v11 migration. Existing
rows retain `load_mode=none`, zero rest and NULL targets. Android v12 adds the
durable flattened exercise-alias mapping used to resolve retired creator IDs.
Desktop schema v12 owns the same alias compatibility boundary. Later additive
desktop migrations reach v24 and Android migrations reach v23 without changing
that identity contract. The active
completed-session exchange is the separate strict
`trainlog-mobile-export` V3; V1/V2 remain readable and `TRAINLOG_FORMAT_V1`
remains unchanged.

## 7. Synchronization architecture

Desktop access to Android uses physical-device discovery with `libudev` and
direct object access with `libmtp`.

Do not introduce a mandatory GVFS/FUSE mount.

Canonical exchange folder:

```text
Documents/Trainlog
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
trainlog-mobile-export-v3.json
```

V3 is the active completed-session snapshot. V1/V2 remain readable legacy
inputs and `TRAINLOG_FORMAT_V1` remains a separate frozen contract.

PC -> Android catalog data uses:

```text
trainlog-pc-catalog-v1.json
```

## 8. Persistence

Desktop SQLite schema is versioned with:

```sql
PRAGMA user_version;
```

The current desktop schema is v24. Schema v9 added an occurrence-owned explicit
maximum result; v10/v11 added body-zone and planning metadata; v12 added durable
flattened exercise aliases; v13-v18 add machine metadata, feedback and
revisions, occurrence tracking snapshots, causal profile state, and AI proposal
storage/publication state; v19-v21 add lifecycle, causal deletion, and staged
generation/ACK ledgers; v22-v24 add generation archival, manual preparations,
and durable preparation withdrawal. None changes `TRAINLOG_FORMAT_V1`.

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

Repository documentation and publication language is:

```text
README and canonical documentation: English
CHANGELOG and release notes: English
New or modified source comments: English
User interface: preserve its actual localization
```

Exact UI strings, user data, identifiers, commands, paths, protocol fields,
version/status markers, citations, and historical evidence are not translated
merely to satisfy the prose-language rule.

Production code, tests, fixtures expressed as source, and repository tooling
must remain readable by a human reviewer. Do not compress functions, control
flow, declarations, assertions, or cleanup onto one line to reduce line or
token count. Grouped missions may and should use small comprehensible commits.
Perform an explicit readability review before the final commit.

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

`README.md` is the concise repository entry point. `docs/README.md` is the
canonical documentation index. `docs/current_state.md` owns current implemented
status; `docs/roadmap.md` owns future work. Design and review records are
historical evidence and must not override those current owners.

Checkpoint history belongs in Git history and `docs/reviews`; canonical
documents describe the current state rather than accumulating obsolete
`NEXT` sections.

Every functional change identifies the affected documentation. When relevant,
reconcile `docs/current_state.md`, `docs/roadmap.md`, and `CHANGELOG.md`; the
completion report lists updated documents or explains why none were affected.
Release publication verifies both GitHub and Forgejo notes, their English
language, and their correspondence with the exact tag independently of later
development. A requested status in prose is never sufficient evidence for a
`PASS` or `FROZEN` marker.

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

Run Android unit and instrumented automation on the host/emulator. The primary
personal phone is reserved for manual, non-destructive install and sync smoke
tests; do not run `connectedDebugAndroidTest` on it.

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

## 14. Training knowledge

Before domain decisions involving anatomy, biomechanics, exercise targeting,
BODY ZONES, machine/exercise interpretation, substitution, MAX interpretation,
workout generation or program generation, consult the project
[`trainlog-anatomy` skill](.agents/skills/trainlog-anatomy/SKILL.md),
`docs/domain/`, the scientific catalogs in `catalog/`, and their cited references.
Scientific knowledge is separate from runtime user data; preserve explicit
uncertainty and settle scientific conclusions before encoding behavior.
