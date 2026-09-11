# Tests and validation

## 1. Principle

A Trainlog feature is not complete without relevant validation.

Frozen formats, persistence migrations, synchronization semantics, and user-data
mutations require executable coverage where practical.

## 2. Frozen Trainlog JSON v1

Run:

```bash
python tools/validate_json.py
```

It validates:

- `examples/session-v1.json`;
- all positive fixtures;
- all negative fixtures.

A negative fixture passes only when Trainlog rejects it.

Semantic validation includes:

```text
stable-ID uniqueness
normalized exercise-name uniqueness
timestamp offsets
end > start
catalog/reference equality
tracking-mode consistency
load-mode/weight consistency
unknown-field rejection
non-blank notes
zero actual repetitions allowed
```

## 3. Import reconciliation contract

Run:

```bash
python tools/validate_import_contract.py
```

Coverage includes:

```text
same ID + same name/profile -> reuse
same ID + renamed display text -> controlled reuse/warning
same ID + incompatible mode -> reject
different ID + equivalent normalized name -> reject
new unique identity -> create
```

That command validates the frozen Trainlog JSON V1 importer contract. Active
mobile V3 synchronization has a separate, stricter safe-reconciliation policy
covered below; it does not modify the frozen V1 expectation.

## 4. Desktop Meson suite

Current registered set (Meson executes tests in parallel, so displayed order is
not contractual):

```text
database
catalog
equipment_catalog
body_zones
training_knowledge
session_generation
session_generation_policy_validation
training_context
custom_equipment
session_detail
duration
body_metrics
bodyviz
exercise_performance
session_type_schema
session_edit
body_observation_edit
usb
mtp
exercise_profile_schema
continuous_session
continuous_detail
reps
variable_sets
schema_v5_migration
schema_v9_migration
schema_v7_migration
timestamp_validation
mobile_import_variable_sets
mobile_import_multi_occurrence
session_exchange_v3
equipment_associations_exchange
equipment_definitions_exchange
exercise_reconciliation
body_zone_sync
body_zone_catalog_validation
sync_direction
sync_history
sync_screen_action
measured_max
max_results
max_sync
body_analytics
terminal_input_event_type_policy
tui_workflows
app_shell
```

The current desktop suite, including APP_SHELL_V1 production-transition
coverage, is 48/48. The following generator-specific checkpoint counts remain
historical evidence:

```text
45/45 Meson tests PASS
```

`SESSION_GENERATOR_V1` validation covered policy shape and shared fixtures,
full-history exposure/recency boundaries, selection and observed-load anchors,
Android v10 -> v11 planning migration, V3 round trips and malformed-V3
priority, and ordinary draft/editor acceptance. The checkpoint also passed 4/4
selected ASan/UBSan tests and Android 73 tests with zero failures/errors; one
known historical real-v9 fixture skipped while the structural v10 migration
test executed and passed. No hardware MTP or real app-upgrade/install validation
is claimed for this checkpoint.

The completed post-repair matrix passed 45/45 Meson tests, named ASan/UBSan
4/4, and Android 75 tests with zero failures/errors and one unavailable
external Android-v9 fixture skip. The structural Android v10 planning migration
executed and passed. Validators, strict C17 headers, deterministic policy/
fixture/knowledge regeneration, and APK asset byte comparisons passed. The
earlier sanitizer invocation that selected no tests is not used as evidence.

The desktop executable is additionally smoke-checked in isolated tmux PTYs at
100x30, the exact 72x20 minimum, and the 60x15 fallback; a resize down/up must
recover before a clean keyboard quit. Notcurses is verified as the executable's
direct terminal dependency with `readelf -d`.

Notable regression coverage:

- transactional persisted-session replacement;
- exercise removal from a session;
- body-observation stable-identity editing;
- profile-aware exercise constraints;
- continuous activity without fake sets;
- table-only desktop SETS workflow: planning does not create actual rows,
  explicit add requires an actual metric, and normal zero-row completion is
  rejected; Android legacy compact-draft decoding remains separately covered;
- direct v4 -> current database migration and v7 -> v8 custom-equipment migration;
- bounded v8 -> v9 explicit-max migration, including ambiguous-attempt preservation;
- lossless v9 -> v10 `performed_sets` rebuild: historic NULL and positive
  weights/IDs/owners/positions/metrics survive, explicit zero is accepted, and
  injected failure rolls back with integrity, foreign-key and enforcement
  checks;
- additive desktop v10 -> v11 and Android v9 -> v10 body-zone migrations:
  exact stable row identities and history remain unchanged while only proven
  manifest mappings are seeded; the Android fixture also preserves an active
  draft, per-set loads, explicit MAX, a body observation and a custom equipment
  definition;
- canonical body-zone validation rejects duplicate IDs/order, missing parents,
  cycles, group/leaf disagreement, a taxonomy other than the exact V1 manifest,
  invalid `full_body`, boolean versions, malformed stable IDs, and direct group
  assignment;
- one primary/multiple secondary persistence, duplicate/unknown/group/orphan-
  secondary rejection, primary/secondary exclusivity, edit, unclassified and
  reopen;
- exact and parent-descendant zone filters, lower-body leaf coverage,
  primary-only participation, normalized-prefix composition and custom rows;
- body-zone companion export/import, empty mapping, identical replay,
  one-sided update, simultaneous conflict rollback and source-V2-proven
  exercise-ID reconciliation without name-only inference; custom-exercise
  publication establishes both peer baselines before a reverse one-sided edit;
  malformed `ex_<uuid-v4>` identities and timestamps without offsets are
  rejected;
- heterogeneous mobile-set import;
- per-set load persistence and correction: ordered rows retain mixed actual
  repetitions, nullable loads, positions and assistance semantics through
  desktop replacement and detail retrieval;
- V2 mobile round-trip and idempotent replay preserve each ordered set's own
  nullable `weight_kg`, without collapsing it to an occurrence target;
- V2 explicit-max Android -> desktop -> Android replay and resumed same-session update;
- Notcurses input lifecycle translation: PRESS/REPEAT are actionable while a
  RELEASE event is consumed without creating a second navigation action.
- targetless mobile SETS persistence;
- mobile-import idempotence.
- V2 mobile import with repeated exercise occurrences and stable `entry_id`;
- companion equipment import after session import, including idempotent
  reimport and independent associations for repeated occurrences;
- rejection of the historical equipment-import invocation without its required
  `--database` target, followed by the corrected complete V2 export chain.
- stable-ID mobile-to-desktop rename reconciliation without duplicate catalog
  rows or historical-reference replacement.
- supplied catalogue search/detail, local custom-equipment creation and
  selection, resolution of supplied/local occurrence links, and explicit
  visibility of an unknown historic equipment reference;
- strict custom-equipment definitions V1 import/export: reserved supplied IDs,
  additive omission, equal reimport, and divergent same-ID conflict;
- three-mode shared-engine plans and their directional receive/publish bounds;
- Sync-page action dispatch: direct `a`/`p`/`b` confirmation, one `Enter` run,
  `Esc` cancellation without a run, refresh-only `r`, inert retired `s`, and
  equivalent full/compact footer direction labels;
- definition-first ordering before V2 artifacts, so associations can resolve
  custom IDs without changing V2 shapes.
- exercise reconciliation with identical profiles, compatible subset/superset
  masks, explicit incomparable-profile rejection, current Marche identities,
  preservation of two historic occurrences and their `entry_id`/continuous
  values, ordered custom-equipment definitions/catalog/mobile/association
  replay, complete definitions/mobile/associations/body import, outbound
  publication, and stable second replay;
- a production PC-exporter to Android-importer regression over definitions,
  catalog, mobile V3, and association V2 artifacts: the fixture includes
  Marche, Leg press, two Marche occurrences, per-set loads, body data, a
  durable draft, and custom equipment; after the first import, both the second
  and third imports report zero additions and an exact snapshot of every
  Android business table remains unchanged;
- synchronization summary reporting where a reconciliation-only result keeps
  `+0 exercice(s)` and only an actual inserted row produces `+1`;
- all fourteen body metrics through the same complete reconciliation import;
- desktop-created occurrence identities use `sxe_<uuid-v4>`, not the `sy_`
  synchronization-run prefix.

## 5. Build

```bash
meson setup --reconfigure build
meson compile -C build
meson test -C build --print-errorlogs
```

Strict warning flags remain active. Do not weaken warnings to make a change pass.

## APP_SHELL_V1 production-transition coverage

The active desktop suite has 46 tests. `app_shell` verifies layout thresholds,
route/history bounds, overlays and focus restoration, bounded UTF-8 search and
form input, stable-ID list selection, shared action ordering, and leave guards.
`tui_workflows` covers the production single event-loop routes and controller
handoffs: sessions/generator, exercises, equipment, statistics/body/MAX,
settings, and confirmed synchronization. `custom_equipment` covers the
bounded deterministic read-only equipment page reader, including pagination,
invalid arguments, offsets and corrupt values. This coverage replaces former
nested-screen-loop workflow claims.

The Mensurations workflow regression separates the selected observation's
profile from its metric history: singleton circumference bars without a trend,
independent kg/cm rendering, missing-field omission, a real two-date Unicode
series, historical selection updates, availability-filtered metric navigation,
and bounded 120x35, 100x30, 80x24 and 72x20 geometry without ASCII chart glyphs.

The APP_SHELL PTY validation exercises six TUI sizes—72x20, 80x24, 100x25,
100x30, 120x31 and 120x35—plus help, search clear/close, F6/F7, compact focus,
resize/overlay restoration, clean exit, and navigation with no temporary
database write. Normal and ASan/UBSan Meson suites each passed 48/48; the
current normal and sanitizer real-PTY runs each passed 100/100 checks. The
sanitizer run used the upstream-prescribed Notcurses compatibility setting
`ASAN_OPTIONS=use_sigaltstack=0:detect_leaks=1:halt_on_error=1` and reported
no ASan/UBSan diagnostics.

Android unit/assembly evidence records 84 tests, 0 failures, 0 errors and one
external `TRAINLOG_ANDROID_V9_FIXTURE` skip. Required human checks remain:
the six TUI sizes above; Android widths 320, 360, 393 and 412 dp; large system
font; IME forms; durable-draft leave guards; and TalkBack. No emulator was
available; the daily installed application was not installed over or exercised
by instrumentation, so these are not marked visually passed.

## 6. Android build

When Android code changes:

```bash
cd android

printf 'sdk.dir=%s\n' "$HOME/Android/Sdk" > local.properties

JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew testDebugUnitTest assembleDebug
```

Install to the connected device when hardware behavior changes:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Android repository host tests additionally cover exercise editing:

- trimmed rename preserves `exercise_id` and recalculates `normalized_name`;
- duplicate and invalid names are rejected;
- completed history and active-draft references resolve the renamed catalog row;
- a referenced profile change is explicitly rejected;
- same-ID PC-catalog rename reconciles in place without a duplicate.
- canonical body-zone taxonomy, create/edit/reopen, parent filters,
  search+filter, unclassified rows, companion replay/update/conflict and
  zone-safe exercise-identity merging.

## 7. Hardware MTP validation

Hardware probes and real synchronization are separate from the normal automated
suite because a test runner cannot assume an unlocked MTP phone.

Available probe binaries include:

```text
trainlog-usb-probe
trainlog-mtp-probe
trainlog-mtp-exchange-probe
trainlog-mtp-roundtrip-probe
trainlog-mtp-mobile-export-probe
```

Previously established physical baseline:

```text
USB_MTP_DETECTION=PASS
MTP_STORAGE_ACCESS=PASS
MTP_WRITE=PASS
MTP_LIST_FOLDER=PASS
MTP_READ=PASS
MTP_ROUNDTRIP=PASS
```

The automated `sync_direction` regression also covers selection of the newest
Android V2 export, equipment-definitions V1 artifact, and equipment-association
V2 artifact when scoped storage has retained collision-suffixed sibling files.
It rejects non-versioned/
non-numeric names and uses deterministic ties; JSON validation remains part of
the normal import path. `mobile_import_multi_occurrence` exercises the real
definition importer before the V2 session importer, including idempotent
reimport and rejection of a missing custom definition.

## 8. Bidirectional synchronization validation

Validated workflow:

```text
Android
-> Synchroniser maintenant
-> unique sr_ request
-> trainlog-syncd
-> shared engine
-> Android -> PC import
-> PC -> Android catalog
-> sy_ structured run
-> matching receipt
-> Android final status
```

Multiple distinct Android request IDs were processed successfully without
reprocessing one request as a new one.

The TUI also invokes the same engine manually and exposes structured run
details.

## 9. Sanitizers

For meaningful C checkpoints:

```bash
CC=clang meson setup build-asan \
  -Db_sanitize=address,undefined \
  -Db_lundef=false

meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

## 10. Pre-push checklist

```bash
meson compile -C build
meson test -C build --print-errorlogs

python tools/validate_json.py
python tools/validate_import_contract.py

git diff --check
git status --short
```

When Android changed, add:

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew testDebugUnitTest assembleDebug
```

Documentation must describe the resulting state, not retain contradictory old
`NEXT` checkpoints.

## 11. Measured-max regression

The normal Meson suite contains:

```text
measured_max
```

Coverage proves:

- stronger ordinary training is ignored by measured-max classification;
- only `max_test` sessions participate;
- zero-repetition failed attempts are not promoted;
- newest successful explicit test is the current measurement;
- historical external-load record can remain older than current;
- lower assistance is better;
- no-load max tests compare actual reps/duration;
- external working loads round to the configured increment;
- working-load percentages reject assistance.
- `%MAX` target calculators use the exact unrounded formula, enforce 1..100,
  exact equipment identity and external resistance, and reject assistance;
- Android manual target-plan tests prove that calculation is read-only, actual
  set weights remain unchanged, existing target dose/rest survive direct-kg
  edits, and none removes the plan target;
- the real TUI `/` path resolves through the registered action, focuses search,
  filters live, then follows clear-before-close Escape semantics;
- action stable identifiers are unique, preventing footer/F7 duplication.
- Statistics-dashboard rendering covers global summary/event facts, primary-zone
  and unclassified active-catalogue projection without merged-alias double
  counts, zero/one-point without-graph and multi-point Unicode detail-chart
  states; readable selected-period frequency with non-duplicating MAX markers;
  daily/multi-day period projection and independent catalogue bars with
  right-side counts; exact-dose per-set parity, equal-instant
  working/MAX non-events, malformed-timestamp availability, distinct wide versus
  stacked compact geometry, and Unicode blocks used only as chart geometry;
  and footer F6/F7 single-occurrence output at 120x35, 100x30, 80x24, and
  72x20.

Historical validation checkpoint (the current desktop suite is 48/48):

```text
39/39 Meson tests PASS
```

## 12. Body analytics regression

The Meson suite adds:

```text
body_analytics
```

Coverage includes:

- male circumference-formula branch;
- female circumference-formula branch;
- estimated fat and lean mass from real body weight;
- waist/hip, shoulder/waist, and chest/waist ratios;
- left/right asymmetry;
- profile-independent analytics without a configured estimation profile;
- missing required circumference handling;
- invalid estimation-profile rejection.

Historical validation checkpoint (the current desktop suite is 48/48):

```text
39/39 Meson tests PASS
```

## 13. Android session draft v1

Android schema v4 introduced one durable active draft; the current additive
chain reaches schema v12 without clearing completed history or the draft. The
explicit v10 -> v11 migration adds optional planning metadata while preserving
existing rows with `load_mode=none`, zero rest and NULL targets. The
v11 -> v12 migration adds the durable flattened exercise-alias table. Its exact
physical-v11 fixture compares every pre-existing table cell before and after
migration, requires the new alias table to be empty, and checks foreign keys.
Host coverage includes exercise shapes and raw partial text, fresh repository
restore, remove/discard, atomic
finalization and repeated-finalize rejection, rollback, catalog reconciliation,
missing-selection recovery, explicit DB-open failure, historical migration,
equipment selection and occurrence identity.

Schema-v9 host coverage additionally proves French raw max-text persistence,
explicit max creation/edit/finalization without sets, distinct movement values
on the same equipment, latest-per-exercise history, V2 replay, stable-ID resume
and bounded conversion that leaves multiple legacy attempts untouched.

Schema-v10 host coverage adds the canonical body-zone asset, stable-ID mapping,
relation constraints, filters, transactional editing and companion conflict
policy without changing explicit MAX or per-set tables.

The current set-row-editor coverage additionally proves that row edits,
deletion and append preserve neighbouring rows; French-comma loads, blank
loads and explicit zero loads remain distinct; reopening preserves aligned raw
row fields; and completed history keeps the ordered per-set values. The
instrumentation source exercises row edit, deletion, append and Activity
recreation, but this document does not claim that instrumentation was executed
for the current documentation checkpoint.

For the settled implementation, the validation inventory is:

```text
Android JVM:             ./gradlew testDebugUnitTest
Android compilation:     ./gradlew assembleDebug
Android instrumentation: adb shell am instrument ... (execution is explicit;
                         no device execution is asserted here)
Desktop:                 meson compile -C build
                         meson test -C build --print-errorlogs
Frozen JSON/import:      python tools/validate_json.py
                         python tools/validate_import_contract.py
V2 regression:           mobile_import_variable_sets and the desktop/Android
                         V2 round-trip/idempotent-replay coverage
Sanitizers:              clang ASan/UBSan Meson build and test invocation
```

Executed Body Zones V1 evidence is recorded after each closeout run: Android
`testDebugUnitTest` 44/44 with the retained real v9 fixture enabled and
`assembleDebug`, a historical 39/39 Meson checkpoint, valid and invalid JSON checks,
import-contract checks, and the ASan/UBSan Meson suite. Device installation and
installed Android-store migration remain explicit hardware steps and are never
inferred from host tests.

The 2026-09-09 desktop closeout additionally backed up the real v10 database,
opened it through the production Notcurses binary, and verified v11 integrity,
foreign keys, all historical row counts and bidirectional row equality against
the backup. A real Kitty terminal validation exercised the zone list, parent
filter, classified/unclassified details and edit preloading/cancellation. The
Samsung SM-G990B then passed certificate matching, `adb install -r`, the real
v9 -> v10 migration, SQLite integrity/FK checks, row equality for every
pre-existing application table, the Android zone UI matrix and two live MTP
round trips. The second run reported zero additions/reconciliations and left
the Android application tables and semantic companion exercise states equal to
the first run. A regression also covers a current
`trainlog-sync-request-v1 (N).json` beside an older canonical request.

The optional `RealAndroidV9BodyZonesMigrationTest` is enabled by setting
`TRAINLOG_ANDROID_V9_FIXTURE` to a coherent copied v9 database. It makes two
test-owned copies before opening the production repository, then compares all
16 pre-existing application tables in both directions; it never opens or
modifies the supplied fixture through the migration helper.

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew test
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew assembleDebug assembleDebugAndroidTest
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell am instrument -w -e package com.labfytools.trainlog \
  com.labfytools.trainlog.test/androidx.test.runner.AndroidJUnitRunner
```

The following device instrumentation evidence is a prior baseline, not a
blanket device-validation claim for schema-v8 definitions or three-mode sync.
That suite has **5 tests**: two real-SQLite repository
checks and three production-screen Compose UI checks. Coverage includes exact
raw form restoration through Activity recreation, cancellation and confirmation
of discard, final save with no stale Resume after recreation, and refusal of a
second completion. All tests use isolated databases; the debug-only,
non-exported Activity renders production Home/Session screens without exporting
synthetic data. The phone must be unlocked and interactive. Do not interpret a
locked-screen `No compose hierarchies found` failure as a passing UI check.

The Samsung SM_G990B additionally passed the normal `MainActivity` matrix:

- two real catalog exercises with distinct continuous values survived Home,
  another app, background `am kill` with verified PID exit, and force-stop;
- rotation recreated the Activity and Home Resume restored both exercises;
- removal of one exercise survived process death, with the other intact;
- raw duration/speed text survived process death and force-stop exactly;
- Back and the normal new-session action preserved the existing draft;
- discard cancellation preserved the DB exactly; confirmation and relaunch
  left all draft tables empty and no Resume action;
- completed History and mobile export excluded the populated draft;
- every original domain row survived migration and the entire device matrix.

Final-save UI and repeated-finalization tests ran on-device with isolated data;
no fictitious completed session was added to the user's history. Real migration
and final state passed SQLite integrity/foreign-key checks. The pre-upgrade DB
and preferences backup is outside the repository. No uninstall, package-data
clear, desktop schema change or frozen artifact change is part of this repair.

## 14. Current real-data reconciliation validation

Before mutation, coherent desktop and Android v8 copies were created and passed
`PRAGMA integrity_check` plus `PRAGMA foreign_key_check`. The newest real
Android V2 artifacts were selected, including scoped-storage `(N)` siblings.
The production tools then ran in protocol order on the desktop copy:

```text
equipment definitions -> exercise/session/body V2 -> equipment associations
-> PC definitions/catalog/mobile/associations export -> second import
```

The result retained the desktop Marche identity with catalog
`data_fields = 3`, both older speed-only Marche occurrences, the Android
speed-and-distance occurrence, all original `entry_id`/positions, custom
equipment, set weights and body observations. The retired Android Marche ID had
no remaining desktop catalog/reference owner. The second replay changed no row
counts and both integrity checks remained clean.

This exercises real artifacts and production import/export code on copies. It
does not substitute for a final direct-MTP run when USB ownership or sandbox
permissions prevent libmtp access.

For this checkpoint, the USB probe discovered the connected Samsung interface
but libmtp failed at `libusb_open()`. The canonical desktop DB was also
read-only to the sandbox; the first attempted definitions import failed before
mutation, and a repeated dump hash plus integrity/FK checks proved it unchanged.
The Android package remained force-stopped and ADB was used only for read-only
inspection/pull. Applying the validated state to both real stores and running
the two transport directions remains an out-of-sandbox hardware validation.

Detailed retained evidence: [Android draft execution record](reviews/android_session_draft_v1_resume.md).

## 15. Training knowledge V1 validation checkpoint

The current implementation passes the training-knowledge catalog
validator, eight Python generation/catalog regressions, 42 desktop Meson tests,
and standalone C17 public-header checks for `training_knowledge.h`,
`training_context.h` and `database.h`. A targeted AddressSanitizer/Undefined-
Behavior-Sanitizer build passed the two new desktop API tests. Android
`testDebugUnitTest assembleDebug` passed with 56 tests, zero failures/errors,
and one skipped real-v9 fixture test because `TRAINLOG_ANDROID_V9_FIXTURE` was
not available. The TUI knowledge screen's UTF-8 cell-aware scrolling was tested
at the 72x20 minimum terminal.

The catalog validator, JSON validator and import-contract validator also pass.
`git diff --check` passes. Scientific-review metadata and
catalog hashes were verified separately.

Temporal regressions now cover the original `+14:30`, `+15:00` and lowercase-`t`
failure, lowercase `z`, omitted seconds, high/negative offsets, exact fractions,
stable identity ties, cursor reuse, traversal through exhaustion and malformed
stored values. C tests also cover selected/unselected values beyond its fixed
output capacity. Four Python tests cover the explicit grammar and exact
chronology, including actual document admission with missing, strict and
permissive optional JSON Schema format checkers. An independent twelve-form
production C probe passes normally and under ASan/UBSan.

Fresh final validation passed: strict build; 42 Meson tests; eight Python
knowledge tests; four Python temporal tests; knowledge, JSON, and import
validators; three strict C17 headers; affected C knowledge/context tests under
ASan/UBSan plus Python timestamp validation; normal and sanitized independent
12-form temporal probes; Android 56 tests with zero failures/errors and one
known missing real-v9 fixture skip; Java 17 debug assembly; skill validation;
and `git diff --check`. Generated C is byte-identical with SHA-256
`e8c099f67eb111d61621b5d76592c049823af5508d43e73ec646f22e4c377fca`; all six
Android assets are byte-identical.

`TRAINING_KNOWLEDGE_V1=PASS`. The independent temporal review passed with no
findings; its coverage included parser grammar and limits, exact chronology,
ties, cursor semantics, source capacity, snapshots, and Android/Python parity.
The initial full audit's stale temporal documentation, Android loader, Meson
dependency, and C role-only query findings were repaired and independently
verified. See the [temporal contract](reviews/training_knowledge_v1_temporal_contract.md)
for the established contract. No real Android install,
manual TUI visual exercise, or manual MTP hardware validation was performed
for Training Knowledge V1.

Shell/naming closeout coverage includes real-PTY shell behavior, canonical
name export/import replay on temporary databases, and preservation of sets,
MAX, equipment, BODY ZONES, aliases, and distinct unmapped exercises.
