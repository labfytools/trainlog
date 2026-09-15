# Trainlog

Trainlog is a local-first workout and body-data system. Its native Android app
is the lightweight field companion for fast capture and glanceable summaries;
its C17/Notcurses desktop TUI is the detailed surface for correction,
catalogue management, analytics, graphs, and long-term follow-up.

Android and desktop each own a local SQLite database. The desktop database is
the canonical long-term history. Trainlog synchronizes versioned JSON artifacts
over direct MTP; it never copies SQLite database files between devices.

## Product split

| Surface | Primary responsibility |
|---|---|
| Android | Capture sets, repetitions, loads, durations, continuous activities, feedback, J+1 follow-ups, body measurements, active drafts, and AI proposals; trigger sync; show quick summaries. |
| Desktop TUI | Consult and correct canonical history; manage exercises and equipment; analyze body data, MAX results, training statistics, body zones, calendar periods, and longitudinal graphs. |

In short: **Android captures and summarizes. The TUI analyzes and tracks over
time.** Android intentionally remains lightweight for analytics.

## Release links

- [Download the latest release](https://github.com/labfytools/trainlog/releases/latest)
- [Documentation](docs/README.md)
- [Changelog](CHANGELOG.md)

## Installation

### Download a prebuilt release

Stable releases publish an Android APK, an x86-64 Linux TUI executable, and a
`SHA256SUMS` file on [GitHub Releases](https://github.com/labfytools/trainlog/releases).
Verify the downloaded binary against `SHA256SUMS` before installing it.

The Linux TUI artifact is architecture-specific and dynamically linked. It is
not a universally portable Linux binary: the host must provide compatible
runtime libraries listed under [Dependencies](#dependencies).

```bash
chmod +x trainlog-tui-linux-x86_64-v0.1.0
./trainlog-tui-linux-x86_64-v0.1.0
```

The executable can remain in the download directory. Moving it into a directory
on `PATH`, such as a user-managed `~/.local/bin`, is optional.

For Android, download the release APK, allow installation from the browser or
file manager when Android requests it, and install the package. Android 8.0
(API 26) or later is required. Grant Trainlog all-files access only when direct
`Documents/Trainlog` synchronization is needed. Desktop synchronization also
requires the Linux side and its MTP dependencies to be installed and configured.

Android and the TUI in one GitHub Release share the same Trainlog product
version. Database schemas and JSON protocol versions are independent.

### Build from source

Desktop:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Android:

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew test
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew assembleRelease
```

`assembleRelease` produces a distributable stable APK only when release signing
is configured by the release operator. The repository does not contain signing
credentials and does not silently generate them.

## Dependencies

### Linux runtime dependencies

The published x86-64 TUI is dynamically linked. The current build directly
requires compatible versions of:

- glibc and the GCC support runtime;
- SQLite 3;
- libuuid;
- utf8proc;
- libudev;
- libmtp;
- Notcurses Core;
- the standard math library.

Distribution packages may pull additional transitive libraries, including
libusb, ncursesw, unistring, gpm, libgcrypt, libgpg-error, and libdeflate.
Package names and ABI versions vary by distribution; inspect the release's
`ldd` evidence and install packages from the target distribution rather than
assuming one universal package command.

### Optional runtime and integration dependencies

- Android is optional when using the desktop TUI alone. USB/MTP synchronization
  requires the Android companion, libudev, libmtp, and an unlocked connected
  device.
- `trainlog-syncd` installation expects a systemd user session. Manual TUI
  synchronization does not require the user service.
- `rclone` is required only for automatic AI history upload and Drive
  inbox/archive workflows. Core capture, local history, analytics, and direct
  MTP synchronization work without `rclone`.

### Build dependencies

Desktop builds require:

- Meson and Ninja;
- a C17 compiler toolchain;
- `pkg-config`;
- Notcurses Core development headers;
- SQLite, libuuid, utf8proc, libudev, and libmtp development headers;
- Python 3 for generated sources, validators, import/export helpers, and tests.

Android builds require JDK 17, an Android SDK supporting the configured API
levels, and the Gradle wrapper committed in this repository. People installing
the APK do not need Java, Gradle, or the Android SDK.

## Documentation

| Topic | Document | Purpose |
|---|---|---|
| Documentation map | [docs/README.md](docs/README.md) | Canonical index and ownership map. |
| Current state | [docs/current_state.md](docs/current_state.md) | Current implemented schemas, capabilities, validation, and limitations. |
| Architecture | [docs/architecture.md](docs/architecture.md) | Component, storage, process, and ownership boundaries. |
| Android | [docs/android.md](docs/android.md) | Field-companion behavior and local persistence. |
| Desktop TUI | [docs/tui.md](docs/tui.md) | Notcurses workflows, correction, and analytics. |
| Database | [docs/database.md](docs/database.md) | Desktop persistence schema and migration contracts. |
| Exercise model | [docs/exercise_data_model.md](docs/exercise_data_model.md) | Exercise, occurrence, load, and MAX semantics. |
| Synchronization | [docs/sync_exchange.md](docs/sync_exchange.md) | Active MTP flow and companion artifacts. |
| Exchange formats | [docs/exchange_format.md](docs/exchange_format.md) | Frozen and separately versioned JSON contracts. |
| Training feedback | [docs/training_feedback.md](docs/training_feedback.md) | Immediate feedback, revisions, and J+1 follow-ups. |
| Tests | [docs/tests.md](docs/tests.md) | Durable validation commands and coverage strategy. |
| Roadmap | [docs/roadmap.md](docs/roadmap.md) | Current cursor and future work only. |
| Development contract | [AGENTS.md](AGENTS.md) | Repository invariants and contribution rules. |
| Change history | [CHANGELOG.md](CHANGELOG.md) | Chronological implementation history. |

## Architecture overview

```text
Android local SQLite
        |
        | versioned JSON snapshots and requests
        v
Documents/Trainlog on Android storage
        |
        | direct MTP / libmtp
        v
trainlog_sync_run() <---- trainlog-syncd or desktop TUI
        |
        +---- import Android snapshot into desktop SQLite
        +---- publish desktop catalog companions to Android
        +---- write a synchronization receipt

desktop SQLite = canonical long-term history
```

The optional desktop AI flow exports read-only history and exchanges session
proposals through Google Drive via the external `rclone` process. Android does
not contain cloud credentials and does not run `rclone`.

## Current status

As of 2026-09-15:

- `TRAINLOG_FORMAT_V1=PASS/FROZEN`;
- desktop SQLite schema v18 and Android SQLite schema v17;
- Notcurses is the only active desktop terminal backend;
- direct storage is `/storage/emulated/0/Documents/Trainlog` under Android's
  all-files access setting;
- mobile export V3 is active; V1/V2 remain readable legacy inputs;
- `STATS_V1=IMPLEMENTED`, `TRAINING_KNOWLEDGE_V1=PASS`, and
  `SESSION_GENERATOR_V1=PASS` (the generator is hidden pending V2);
- `APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2`;
- `TRAINLOG_AI_SESSION_DRAFT_V1=VALIDATION_PENDING` pending a real Drive plus
  Android-triggered bidirectional smoke test.

The latest executable result belongs in
[current state](docs/current_state.md), not in multiple README narratives.

## Desktop development quick start

Requirements include Meson, Ninja, SQLite3, utf8proc, libuuid, libudev,
libmtp, and Notcurses.

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
./build/tui/trainlog
```

The database path is `$XDG_DATA_HOME/trainlog/trainlog.db`, or
`~/.local/share/trainlog/trainlog.db` when `XDG_DATA_HOME` is unset.

## Android development quick start

Use Java 17 and provide the Android SDK through `ANDROID_HOME` or the untracked
`android/local.properties` file.

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew test
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew assembleDebug
```

Instrumented tests belong on an emulator. Do not run
`connectedDebugAndroidTest` on the primary personal phone.

## Synchronization quick start

```bash
meson compile -C build
bash tools/install_syncd_user.sh
systemctl --user is-active trainlog-syncd.service
```

On Android, grant all-files access for the fixed `Documents/Trainlog`
directory, then use **Synchronisation → Synchroniser maintenant**. Hardware MTP
validation requires an unlocked connected device and remains an explicit manual
check.

## Development principles

- Preserve frozen, versioned compatibility boundaries.
- Use stable IDs; display names are not identities.
- Keep performed work separate from plans and continuous activity separate
  from fake sets.
- Keep Android capture, desktop analytics, persistence, transport, and
  rendering responsibilities distinct.
- Prefer explicit failure to silent data loss or corruption.
- Treat migrations, idempotence, tests, and documentation as part of feature
  completion.
- Keep scientific catalogs separate from runtime user data and preserve stated
  uncertainty.

## License and project notes

See [LICENSE](LICENSE). Forgejo is the primary repository; GitHub is a mirror.
Repository layout, detailed contracts, and historical review records are linked
from [docs/README.md](docs/README.md).
