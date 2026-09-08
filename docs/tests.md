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
mobile V2 synchronization has a separate, stricter safe-reconciliation policy
covered below; it does not modify the frozen V1 expectation.

## 4. Desktop Meson suite

Current registered set (Meson executes tests in parallel, so displayed order is
not contractual):

```text
database
catalog
equipment_catalog
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
schema_v7_migration
mobile_import_variable_sets
mobile_import_multi_occurrence
equipment_associations_exchange
equipment_definitions_exchange
exercise_reconciliation
sync_direction
sync_history
sync_screen_action
measured_max
body_analytics
terminal_input_event_type_policy
```

Validated current suite:

```text
32/32 Meson tests PASS
```

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
- repetition shorthand/list/pyramid parsing;
- direct v4 -> v7 database migration and v7 -> v8 custom-equipment migration;
- heterogeneous mobile-set import;
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
  catalog, mobile V2, and association V2 artifacts: the fixture includes
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

Current physical baseline:

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
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew assembleDebug
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

Validated current normal suite:

```text
32/32 Meson tests PASS
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

Validated current normal suite:

```text
32/32 Meson tests PASS
```

## 13. Android session draft v1

Android schema v4 introduced one durable active draft; the current additive
chain reaches schema v8 without clearing completed history or the draft. The
current `testDebugUnitTest` suite and `assembleDebug` pass. Host coverage
includes exercise
shapes and raw partial text, fresh repository restore, remove/discard, atomic
finalization and repeated-finalize rejection, rollback, catalog reconciliation,
missing-selection recovery, explicit DB-open failure, historical migration,
equipment selection and occurrence identity.

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
