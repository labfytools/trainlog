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

## 4. Desktop Meson suite

Current normal suite:

```text
 1  database
 2  catalog
 3  session_detail
 4  duration
 5  body_metrics
 6  bodyviz
 7  exercise_performance
 8  session_type_schema
 9  session_edit
10  body_observation_edit
11  mtp
12  continuous_session
13  continuous_detail
14  reps
15  exercise_profile_schema
16  usb
17  variable_sets
18  schema_v5_migration
19  mobile_import_variable_sets
```

Validated checkpoint:

```text
21/21 PASS
```

Notable regression coverage:

- transactional persisted-session replacement;
- exercise removal from a session;
- body-observation stable-identity editing;
- profile-aware exercise constraints;
- continuous activity without fake sets;
- repetition shorthand/list/pyramid parsing;
- direct v4 -> v5 database migration;
- heterogeneous mobile-set import;
- targetless mobile SETS persistence;
- mobile-import idempotence.

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
./gradlew assembleDebug
```

Install to the connected device when hardware behavior changes:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

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

Current normal baseline:

```text
21/21 PASS
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

Current normal baseline:

```text
21/21 PASS
```
