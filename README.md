# Trainlog

Trainlog is a local-first workout and body-data system. Trainlog Core owns
business truth and canonical desktop persistence. Its native Android app is the
field companion, its C17/Notcurses desktop TUI is the administration and
technical surface, and its local browser Web sibling provides the implemented
Dashboard while reserving later analysis, program, session, and exercise
modules for separately contracted work.

Android and desktop each own a local SQLite database. The desktop database is
the canonical long-term history. Trainlog synchronizes versioned JSON artifacts
over direct MTP; it never copies SQLite database files between devices.

## Product split

| Surface | Primary responsibility |
|---|---|
| Android | Capture and quickly correct sets, repetitions, loads, durations, and continuous activities; reorder active and completed-session occurrences; capture feedback, J+1 follow-ups, body measurements, and AI proposals; trigger sync; show quick summaries. |
| Desktop TUI | Administer, inspect, maintain, import/export, correct canonical history, and provide technical tools. |
| Local Web (0.1.2 development) | Display the implemented local Dashboard and persist its private layout through typed Trainlog Core/API boundaries. Analyse, Programmes, Sessions, and Exercises remain placeholders. |

No interface reconstructs business truth from SQLite tables. The local Web is
a sibling adapter, not an extension of the TUI. On `main`, its loopback-only
CLI/HTTP adapter, embedded frontend, read-only Dashboard API, factual tiles,
visualizations, and private layout persistence are implemented. This does not
make the other four Web routes functional or publish version 0.1.2.

## Releases

When a Trainlog release is published, its prebuilt assets are available from
both official mirrors:

- [GitHub Releases](https://github.com/labfytools/trainlog/releases)
- [Forgejo Releases](https://git.labfytools.com/fy59/trainlog/releases)

Each published stable release provides:

- a signed Android APK;
- a Linux x86-64 TUI executable;
- SHA-256 checksums.

Both mirrors publish the same Trainlog product version and release assets. The
current development version is **0.1.2** on Android and desktop; this is one
shared Trainlog version, not separate interface versions. Version 0.1.1 is the
latest stable release.

Version 0.1.1 presents Trainlog in French by default, with English selectable
from **Settings → Language** on Android and the desktop TUI. The selection is
local to that installation and updates the visible interface immediately; it
does not translate user exercise/catalogue names or change training data,
stable IDs, databases, schemas, exchange artifacts, synchronization protocols,
AI, MAX, or feedback. Synchronization status is rendered locally from typed
status and counters; raw protocol and operational summaries are not translated
or injected into the other surface.

See the [documentation](docs/README.md) and [changelog](CHANGELOG.md) for more
details.

## Installation

### Download a prebuilt release

For a published release, download Trainlog from either official mirror:

- [GitHub](https://github.com/labfytools/trainlog/releases)
- [Forgejo](https://git.labfytools.com/fy59/trainlog/releases)

Both mirrors contain the same Android APK, Linux x86-64 TUI binary, and SHA-256
checksums for each stable Trainlog release. Verify the downloaded binaries
against `SHA256SUMS` before installing them.

The Linux TUI artifact is architecture-specific and dynamically linked. It is
not a universally portable Linux binary: the host must provide compatible
runtime libraries listed under [Dependencies](#dependencies).

```bash
chmod +x trainlog-tui-linux-x86_64-v<version>
./trainlog-tui-linux-x86_64-v<version>
```

The executable can remain in the download directory. Moving it into a directory
on `PATH`, such as a user-managed `~/.local/bin`, is optional.

For Android, download the release APK, allow installation from the browser or
file manager when Android requests it, and install the package. Android 8.0
(API 26) or later is required. Grant Trainlog all-files access only when direct
`Documents/Trainlog` synchronization is needed. Desktop synchronization also
requires the Linux side and its MTP dependencies to be installed and configured.

Android and the TUI in each stable release share the same Trainlog product
version on both mirrors. Database schemas and JSON protocol versions are
independent.

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

Release hosts provide `TRAINLOG_RELEASE_STORE_FILE`,
`TRAINLOG_RELEASE_STORE_PASSWORD`, `TRAINLOG_RELEASE_KEY_ALIAS`, and
`TRAINLOG_RELEASE_KEY_PASSWORD`, or a private
`~/.config/trainlog/release-signing.properties` file containing the equivalent
`storeFile`, `storePassword`, `keyAlias`, and `keyPassword` keys. Environment
variables take precedence. Keystores and local credential files are ignored by
Git; neither belongs in source control, build logs, or release notes.

The Android release key is the permanent update identity for the application.
The release operator must preserve an encrypted external backup of both the
keystore and its credentials. Losing either prevents future compatible Android
updates; generating a replacement key is not a normal release procedure.

## Dependencies

### Linux runtime dependencies

The published v0.1.1 x86-64 TUI is dynamically linked. Its tagged source
directly requires compatible versions of:

- glibc and the GCC support runtime;
- SQLite 3;
- libuuid;
- utf8proc;
- libudev;
- libmtp;
- Notcurses Core;
- the standard math library.

The current 0.1.2 development source additionally links GNU libmicrohttpd and
yyjson for the local Web adapter and Dashboard layout configuration. Those are
not retroactive requirements of the pre-Web v0.1.1 tagged source.

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
- SQLite, libuuid, utf8proc, libudev, libmtp, GNU libmicrohttpd, and yyjson
  development headers for the current development source;
- Python 3 for generated sources, validators, import/export helpers, and tests.

The embedded Web frontend additionally uses Node and npm at build time only.
Prepare its exact locked dependencies explicitly, then enable the release
frontend build:

```bash
cd web
npm ci
npm run typecheck
npm test
cd ..
meson setup build -Dweb=enabled
meson compile -C build
```

Meson never runs `npm install` or downloads packages. Run `npm ci` explicitly
for the exact lockfile before configuring a Web-enabled build. The default `web=auto`
embeds the frontend when Node, npm, and the prepared `web/node_modules` are
available; otherwise it preserves a desktop-only build and `trainlog -w`
reports that frontend support is absent. Release builds must use
`-Dweb=enabled`. `-Dweb=disabled` is the explicit desktop-only choice. Node,
npm, `node_modules`, and `web/dist` are not runtime dependencies.

For an existing build directory, change the option with `meson configure
build -Dweb=enabled` (or the required value) instead of running `meson setup`
as though the directory were new.

### Optional systemd user service for local Web development

The versioned `systemd/trainlog-web.service` unit can keep `trainlog -w`
available on `127.0.0.1:8080` during an interactive user session. Without
systemd user lingering, enabling it does not guarantee startup before login.
This is a development convenience, not a requirement of the final Trainlog
distribution.
It intentionally runs the canonical user entry point
`~/.local/bin/trainlog`; in a source checkout that entry point may be a symlink
to the current `build/tui/trainlog` binary, but the symlink must resolve to an
executable before the service is started.

The unit prevents privilege acquisition, gives the process a private temporary
directory, makes system paths read-only, and applies kernel/control-group and
setuid restrictions that are compatible with the local HTTP server. It does
not protect or hide the home directory because SQLite and Trainlog
configuration must remain writable under `~/.local/share/trainlog` and
`~/.config/trainlog`. Failed starts use a ten-second restart delay and are
limited to three attempts per minute, preventing a persistent port-8080
conflict from producing an uncontrolled loop.

Install the development unit as a symlink so repository updates are picked up
after `daemon-reload`:

```bash
mkdir -p ~/.config/systemd/user
ln -s "$(pwd)/systemd/trainlog-web.service" ~/.config/systemd/user/trainlog-web.service
systemctl --user daemon-reload
systemctl --user enable --now trainlog-web.service
```

Useful lifecycle and diagnostic commands are:

```bash
systemctl --user status trainlog-web
systemctl --user restart trainlog-web
systemctl --user stop trainlog-web
systemctl --user start trainlog-web
journalctl --user -u trainlog-web
```

An operator may separately configure a local nginx proxy such as
`http://trainlog.perf`. That hostname and proxy are machine-local conveniences,
not a public Trainlog domain or a runtime dependency, and Trainlog does not
configure them automatically.

Uninstalling the integration disables the unit before removing only its user
configuration symlink:

```bash
systemctl --user disable --now trainlog-web.service
rm ~/.config/systemd/user/trainlog-web.service
systemctl --user daemon-reload
```

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

As of 2026-09-17:

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
- `WEB_FRONTEND_SHELL_V1=PASS/FROZEN` and
  `WEB_DASHBOARD_V1=PASS/FROZEN`; the other four Web routes remain placeholders;
- `TRAINLOG_WEB_V1=CONTRACT_FROZEN / IMPLEMENTATION_STARTED`;
- `TRAINLOG_SYNC_GAP_CONTRACT_V1=CONTRACT_FROZEN /
  IMPLEMENTATION_NOT_STARTED` and the next cursor remains
  `TRAINLOG_SYNC_CHARACTERIZATION_V1`.

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
