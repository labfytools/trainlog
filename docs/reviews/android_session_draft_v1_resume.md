# Android session draft v1 — execution and validation record

Date: 2026-09-07. **ANDROID_SESSION_DRAFT_V1=PASS.** Implementation, canonical
documentation synchronization, real-device validation and final audit complete.

## Scope and baseline

User authorized full execution of `android-session-draft-prompt.md` and selected
GPT-6 Astra, MEDIUM for substantial implementation. Baseline HEAD: `444a4d2`.
Existing Notcurses changes were preserved. No staging, commit, push, reset,
restore, stash, clean, uninstall or package-data clear was performed.

## Persistence and review

Android local schema migrated additively from v3 to v4. The singleton
`active_session_draft` and its `draft_session_exercises`,
`draft_performed_sets`, `draft_continuous_activity` children persist workout
and raw form state independently of completed history. Repository mutations
own durable autosave; finalization inserts completed history and clears the
draft transactionally. Frozen exchange artifacts, desktop schema and timestamp
semantics are unchanged.

The bounded reviewer identified missing selection diagnostics/raw preservation
and unguarded DB-open/transaction-begin failures. Astra repaired these and added
regressions. Subsequent bounded review passed the exact repairs and isolated UI
harness. The missing-selection path clears only the selection, retains every
raw field and added exercise, and returns a specific warning.

## Real-device results

Device: Samsung SM_G990B, serial `RFCT10N6ZFP`.
Both APKs were installed with `adb install -r`; existing installation and
user data were preserved. The app remains installed and MainActivity was
launched after validation.

The first locked-device UI attempt could not find an interactive Compose
hierarchy. After the user unlocked the phone, the same installed UI tests
passed 3/3. Additional cancellation/recreation/idempotency assertions were
built; the final instrumentation APK then passed **5/5** tests (2 repository,
3 UI). No keyguard bypass or show-when-locked flags were used.

### Production MainActivity matrix

- Created a draft through the normal UI using the two existing catalog entries:
  `marche` = 720 seconds at 5.5 km/h; `Gym/Échauffement` = 420 seconds.
- Home and switching to Android Settings preserved both exact exercises.
- Back showed Home Resume and the two-exercise summary.
- Background `am kill` was verified by absent PID; original PID 20863 exited
  and resumed process PID 22891 displayed the exact two-exercise draft.
- Explicit force-stop and cold relaunch restored Home Resume and exact values.
- Rotation landscape/portrait caused real `wm_relaunch_resume_activity` events
  for `MainActivity` at 15:16:28 and 15:16:30 device time. Home Resume remained.
- `lifecycle-two-exercises.db` and `after-lifecycle.db` are byte-identical.
- Removed `marche` from the draft, leaving `Gym/Échauffement` at 420 seconds.
  Selected `marche` for another unfinished entry, with raw duration `39.`
  and speed `6,`.
- After verified background process exit and relaunch, Home Resume restored the
  remaining exercise and exact raw fields; the removed exercise was not added
  back. Force-stop/relaunch also restored the raw fields.
- `removed-with-raw-form.db` and `removed-after-kill-verified.db` are
  byte-identical. Both catalog entries remain present.
- The normal new-session action opened the existing draft without overwriting
  it. History still displayed no completed sessions.
- Discard confirmation -> Annuler retained Resume and exact DB bytes
  (`after-discard-cancel.db` matches the raw-form snapshot).
- Confirmed discard -> force-stop/relaunch left no Resume, and all four draft
  tables had zero rows.
- The actual mobile snapshot pulled while a draft existed contains zero
  sessions and the unchanged v1 keys: format, version, generated_at, exercises,
  sessions, body_observations.
- The phone's original 30-second screen timeout and free rotation setting were
  restored after temporary test configuration changes.

### Final save and stale-resurrection checks

On the same physical device, isolated tests render the production HomeScreen
and SessionScreen through debug-only, non-exported `DraftUiTestActivity`.
It uses only `trainlog-draft-ui-test.db` and performs no shared export writes.

The production final-save control creates exactly one completed session,
clears the draft, and returns Home without Resume. Activity recreation still
shows no Resume and exactly one completed session. A second repository
finalization returns Invalid and does not create another session. Both host
and real-SQLite tests cover this repeated-finalization check. Repository export
contains one completed session after successful finalization.

This isolation avoids fictitious completed workouts in the user's real history.
No actual user completed session was added/deleted for testing.

## User-data preservation

The original v3 database contained two exercises, one body observation, and no
completed sessions or session children. Migration and the entire device matrix
preserved **every original row exactly** across all six original domain tables.
The final database is v4, integrity check is `ok`, foreign-key check is empty,
and every draft table is empty. The comparison helper verifies complete rows,
not merely counts.

## Test evidence

- Host `./gradlew test`: 8/8 PASS, no skips/failures/errors.
- `assembleDebug` and `assembleDebugAndroidTest`: PASS.
- Final device `am instrument`: 5/5 PASS.
- Desktop Meson compile and suite: 22/22 PASS.
- Frozen JSON and import-contract validators: PASS.
- `git diff --check`: PASS.
- Entire `tui/` compared with pre-ticket tar snapshot: unchanged by this ticket.

Host test XML:
`android/app/build/test-results/testDebugUnitTest/TEST-com.labfytools.trainlog.data.TrainlogRepositoryDraftTest.xml`.

Device command:

```bash
adb -s RFCT10N6ZFP shell am instrument -w \
  -e package com.labfytools.trainlog \
  com.labfytools.trainlog.test/androidx.test.runner.AndroidJUnitRunner
```

## Backup and retained artifacts

Outside-repository directory: `/tmp/trainlog-android-draft-v1-mD9RFF/`.

Original quiescent database/preferences backup: `user-data-before.tar`.

SHA-256:
`3ea2e3b61e921918762a84b88abd82337b7afae25fc2d95ff2420d228fcea980`.

Also retained: baseline.patch, baseline-tui.tar, original databases/,
after-install.db, final-device.db, lifecycle-two-exercises.db,
after-lifecycle.db, removed-with-raw-form.db, removed-after-kill-verified.db,
after-discard-cancel.db, export-with-draft.json, completed-device-matrix.db,
verify_user_data.py and semantic ADB helper device_ui.py.

Only `/tmp` was writable outside the repository. The backup must be copied to
permanent storage before temporary-directory cleanup; no DB backup or generated
exchange artifact is in the repository.

## Final audit and closeout

The configured Terra-high `android_final_review` performed the single final
audit and returned PASS with no blocking findings. It noted that an intended
post-final-save recreation assertion had landed in the discard test instead.
The final-save test now also recreates the Activity and rechecks absent Resume
before verifying exactly one completed session. The corrected test APK was
installed with `-r`; the entire device suite passed 5/5 again (4.327 seconds).

The audit also recorded pre-existing production `MainActivity` repository
cleanup as non-blocking resource-lifetime maintenance: the helper is not
explicitly closed on Activity destruction. This was not classified as a new
durability regression or a prerequisite for this corrective checkpoint.

Final normal validation: desktop22/22, JSON21 cases, import6 cases and diff
check PASS. Android final `test assembleDebug assembleDebugAndroidTest` invocation
succeeded using retained writable tool homes; its76 tasks were up to date with
the actual8/8 host run after finalization assertions were strengthened. After
the last six-line UI-test correction, `assembleDebugAndroidTest` rebuilt
successfully and the on-device suite passed as recorded above.

Sandbox invocation (from `android/`):

```bash
GRADLE_USER_HOME=/tmp/trainlog-gradle-user-home \
ANDROID_USER_HOME=/tmp/trainlog-android-home \
JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew test assembleDebug assembleDebugAndroidTest
```

The default Gradle-home invocation initially failed before executing tasks
because its lock path was read-only; the retained writable homes resolved it.
SDK analytics and Kotlin-daemon cache paths emitted sandbox diagnostics; Kotlin
successfully used its fallback compiler. The generated diagnostic log was moved
outside the repository. No source-warning policy was weakened.

Canonical docs are synchronized and PASS markers now reflect executed evidence.
The original Notcurses implementation is unchanged by this ticket. User data,
backup, current APKs and full diff/source evidence are retained. No commit or
push was performed. There is no unfinished implementation or device-validation
step for this ticket; retain the backup outside temporary storage as noted above.
