# Trainlog

Trainlog is a local-first workout and body-tracking system with two user
interfaces:

- a native Android application optimized for fast data entry during training;
- a C17/ncursesw TUI used for durable history, editing, visualization,
  statistics, and synchronization.

The desktop SQLite database is the canonical long-term history. Android keeps
its own local SQLite database so recording remains usable independently from the
desktop.

## Current status

```text
TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V5=PASS
ANDROID_LOCAL_WORKFLOWS=PASS

VARIABLE_REPETITION_SETS=PASS
CONTINUOUS_ACTIVITY_TRACKING=PASS

DIRECT_MTP_TRANSPORT=PASS
COMMON_SYNC_ENGINE=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
BIDIRECTIONAL_SYNC_V1=PASS

DESKTOP_TESTS=21/21 PASS
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

Actual repetition sets are stored independently. Compact input supports:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

## Repository layout

```text
android/        native Kotlin/Compose Android client
tui/            C17 ncursesw desktop application and core
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

Explicit `max_test` sessions are the only source of measured maxima.

Ordinary training best sets remain ordinary performance even when they exceed a
previous max-test result.

The desktop exercise catalog exposes a separate measured-max view with current
result, same-mode record, test history, a dedicated graph, and 60/70/80/90%
working loads for external resistance. Working loads are rounded to a selectable
practical increment and are not calculated for assistance.

Android can explicitly save a session as `Entraînement` or `Test max`.

```text
MEASURED_MAX_V1=PASS
WORKING_LOAD_PERCENTAGES=PASS
ANDROID_MAX_TEST_SESSION=PASS
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
DESKTOP_TESTS=21/21 PASS
```
