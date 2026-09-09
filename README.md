# Trainlog

Trainlog is a local-first workout and body-tracking system with two user
interfaces:

- a native Android application optimized for fast data entry during training;
- a C17/Notcurses TUI used for durable history, editing, visualization,
  statistics, and synchronization.

The desktop SQLite database is the canonical long-term history. Android keeps
its own local SQLite database so recording remains usable independently from the
desktop.

## Current status

```text
TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V10=PASS
ANDROID_LOCAL_WORKFLOWS=PASS
ANDROID_LOCAL_DATABASE_V9=PASS
ANDROID_SESSION_DRAFT_V1=PASS
EXERCISE_EDIT_V1=PASS
ANDROID_BANNER_PARITY_V1=PASS

VARIABLE_REPETITION_SETS=PASS
CONTINUOUS_ACTIVITY_TRACKING=PASS

DIRECT_MTP_TRANSPORT=PASS
COMMON_SYNC_ENGINE=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
BIDIRECTIONAL_SYNC_V1=PASS
MULTI_OCCURRENCE_SESSION_V2=PASS
EQUIPMENT_ASSOCIATIONS_V2=PASS
EQUIPMENT_DEFINITIONS_V1=PASS
EXERCISE_RECONCILIATION_V2=PASS
EXPLICIT_MAX_RESULTS_V1=PASS

DESKTOP_TESTS=36/36 PASS
ANDROID_BUILD=PASS
```

## Architecture

```text
                Android application
                local SQLite store
                       |
             automatic mobile snapshot
                       |
                       v
        Download/Trainlog on Android storage
                       |
                       | direct MTP / libmtp
                       v
                trainlog_sync_run()
                  /             \
                 /               \
        Android -> PC         PC -> Android
        snapshot import       catalog publish
                 \               /
                  \             /
                   sync receipt
                       |
                       v
                    Android

Desktop TUI --------------------+
    |                           |
    +-- same sync engine -------+
    |
    v
desktop SQLite
canonical long-term history
```

No SQLite database file is copied between devices. The desktop does not require
a GVFS/FUSE mount of the phone.

## Exercise model

Trainlog does not infer behavior from exercise names.

```text
recording_mode = SETS | CONTINUOUS
tracking_mode  = REPS | DURATION
data_fields    = SPEED_KMH | DISTANCE_KM
```

Valid model-v1 combinations are:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

Actual repetition sets are stored independently. The Notcurses desktop flow
collects planning only, then creates actual work in an ordered table: the user
explicitly adds every row and enters its actual repetitions (or duration) and
optional load. It does not accept compact performed-repetition input; normal
set sessions cannot finish with zero actual rows.

A session may contain several ordered occurrences of the same catalogue
exercise. Each occurrence has a stable `entry_id`, distinct from the stable
`exercise_id` of the catalogue item. Equipment selection belongs to that
occurrence, as do its actual per-set loads. `external` records an applied or
machine-displayed load; `assistance` records assistance and is not interpreted
as increasing strength. An actual load is either absent or finite and
non-negative, so an explicit zero remains distinct from no recorded load;
planned targets remain strictly positive and are never substituted for actuals.

In a `max_test` session, an occurrence may instead own one explicit positive
`max_weight_kg`. This result has no performed set, repetitions, or target-set
count. Its identity remains the movement's `exercise_id` plus the occurrence's
`entry_id`; optional equipment is context and never owns a shared maximum.

During V2 synchronization, a different-ID normalized-name collision is merged
only when recording/tracking modes match, every other known invariant is
compatible, and one `data_fields` mask contains the other. The desktop identity
is retained as canonical, the bit-mask union preserves the richer capability,
and historical occurrence snapshots remain unchanged; absent optional values
stay absent. Incomparable profiles remain explicit conflicts. Name equality
alone is never sufficient.

## Repository layout

```text
android/        native Kotlin/Compose Android client
tui/            C17 Notcurses desktop application and core
docs/           canonical project documentation
format/         frozen Trainlog JSON v1 schema material
examples/       valid frozen-format examples
tests/          fixtures and cross-component tests
tools/          validators, import/export helpers, sync daemon tooling
```

## Desktop build and validation

```bash
meson setup --reconfigure build
meson compile -C build
meson test -C build --print-errorlogs

python tools/validate_json.py
python tools/validate_import_contract.py

git diff --check
```

## Android build

Android keeps one durable in-progress workout in its local SQLite database.
Home offers **Reprendre la séance en cours** after navigation, app switching,
Activity recreation, process death or force-stop/relaunch. Added exercises and
raw unfinished form text are retained. Removing an exercise affects only the
draft; abandoning the draft requires confirmation. Final save atomically
creates completed history and clears the draft. Drafts never enter mobile
export or desktop synchronization as completed sessions.

Schema migrations are additive and preserve existing capture data. See
[Android behavior](docs/android.md) and [validation](docs/tests.md).

The current Android schema is v9. Its additive v4 -> v9 chain adds the shared
equipment catalogue, per-occurrence equipment links, durable occurrence
identities, custom-equipment definition support, explicit MAX results and
stable-source Test max resumption without recreating completed history or
discarding the active draft.

Exercises can be renamed in place from Android. The `ex_<uuid-v4>` identity is
unchanged; completed history, an active draft, and synchronization therefore
continue to resolve the same logical exercise. Referenced profiles are locked;
only unreferenced catalog exercises may change their recording/tracking profile.

The local Android SDK is intentionally not committed. Configure it with either
`ANDROID_HOME` or `android/local.properties`.

Example:

```bash
cd android

printf 'sdk.dir=%s\n' "$HOME/Android/Sdk" > local.properties

JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew assembleDebug
```

## Android-triggered synchronization

Launch the desktop TUI from a built checkout with:

```bash
trainlog
```

The usual user command resolves to `build/tui/trainlog` in this checkout. The
desktop database is `$XDG_DATA_HOME/trainlog/trainlog.db`, or
`~/.local/share/trainlog/trainlog.db` when `XDG_DATA_HOME` is unset.

Build the desktop first, then install the user service:

```bash
bash tools/install_syncd_user.sh
```

Check it with:

```bash
systemctl --user is-active trainlog-syncd.service
tail -f ~/.local/state/trainlog/syncd.log
```

On Android:

```text
Sync
-> Synchroniser maintenant
```

The request is consumed by `trainlog-syncd`, the shared bidirectional engine
runs, a receipt is returned to Android, and the PC catalog is applied locally.
The active completed-session exchange is V2 and preserves occurrence
`entry_id`, per-set weights and equipment associations. Frozen V1 artifacts
remain readable as legacy artifacts; they are not silently redefined as V2.

## Documentation

- `docs/current_state.md`: compact canonical implementation snapshot;
- `docs/architecture.md`: component and ownership boundaries;
- `docs/exercise_data_model.md`: exercise semantics;
- `docs/database.md`: desktop SQLite schema and migrations;
- `docs/android.md`: Android behavior;
- `docs/tui.md`: desktop TUI behavior;
- `docs/sync_exchange.md`: MTP synchronization artifacts and protocol;
- `docs/exchange_format.md`: frozen Trainlog JSON v1 contract;
- `docs/tests.md`: validation strategy;
- `docs/roadmap.md`: completed gates and future cursor;
- `AGENTS.md`: development contract.

## Development principles

- local-first;
- no mandatory cloud account;
- user-owned data;
- versioned persistent and exchange formats;
- stable identities;
- idempotent synchronization;
- no fake data representation to force incompatible models together;
- strict compiler warnings;
- documentation and tests are part of feature completion.

## Measured max

Explicit `max_test` sessions are the only source of measured maxima. Schema v9
stores each new weight result in a one-to-one `max_results` row; it no longer
encodes a maximum as a synthetic `1 × 1` set.

Ordinary training best sets remain ordinary performance even when they exceed a
previous max-test result.

The desktop exercise catalog exposes a separate measured-max view with current
result, same-mode record, test history, a dedicated graph, and 60/70/80/90%
working loads for external resistance. Working loads are rounded to a selectable
practical increment and are not calculated for assistance.

Android and the TUI expose a dedicated `Test max` form containing exercise,
optional equipment, and `Poids max (kg)`. Android can reopen an existing Test
max and atomically replace its ordered entries while preserving the original
session and occurrence identities.

```text
MEASURED_MAX_V1=PASS
WORKING_LOAD_PERCENTAGES=PASS
ANDROID_MAX_TEST_SESSION=PASS
EXPLICIT_MAX_RESULTS_V1=PASS
```

## Body analytics

Body analytics are desktop-only. Android remains a capture client.

The TUI derives descriptive ratios, left/right asymmetry, and an optional
circumference-based body-fat estimate from real body observations.

The estimate requires a local desktop-only analytics profile containing the
formula branch and height. Estimated fat mass and lean mass are calculated only
when a real body weight is present.

Estimated values are never persisted as direct measurements.

```text
BODY_ANALYTICS_V1=PASS
BODY_COMPOSITION_ESTIMATE=PASS
BODY_PROPORTION_RATIOS=PASS
BODY_SYMMETRY_ANALYTICS=PASS
DESKTOP_TESTS=36/36 PASS
```
