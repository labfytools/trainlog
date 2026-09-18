# Web Sessions deletion and Programs V1 evidence

Date: 2026-09-18

This record covers the completed development branch and its authorized private
grouped rollout. It does not claim a tag, public release, key rotation, Android
uninstall, or application-data clearing.

## Implemented boundaries

- Separate Core commands withdraw manual preparations and AI proposals, or
  causally delete inactive execution drafts and explicitly selected completed
  sessions. They require current revisions and durable request identities.
- Proposal withdrawal retains its import ledger and every derived preparation.
  The V2 AI-proposal companion publishes a permanent Android tombstone; an
  older V1 proposal cannot recreate it. Started executions and performed work
  are not proposal-deletion effects.
- The row trash action is independent from detail navigation. The accessible
  confirmation dialog has initial safe focus, an Escape path, a focus trap and
  trigger focus restoration.
- Program V1 remains desktop planning data. Strict preview and transactional
  import share the Core validator. Archive is lifecycle state, and explicit
  preparation creation retains program/session provenance without delivery or
  performed data.

## Validation

- GCC/Web Meson suite: 88/88 passed.
- Clang strict suite: 86/86 passed.
- Clang ASan/UBSan suite: 86/86 passed.
- Frontend typecheck, production build and 93 Vitest tests passed.
- Android `testDebugUnitTest` and `assembleDebug` passed.
- JSON, import-contract and Python contract tests passed; the two Firefox tests
  passed separately in an isolated temporary Python environment.
- The real embedded server was exercised at desktop size and at an exact
  390×844 viewport. Its synthetic-only captures include the Sessions list,
  destructive confirmation and Programs empty state.
- `tools/check_changed_c_format.sh main` and `git diff --check` passed.

## Controlled private rollout

- Desktop source commit:
  `a0fc48f082b76dba2ae3a56fceb492169de89f89`.
- Coordinated desktop archive SHA-256:
  `9f50757009fbdbe5145995daf4593caf658be695df62bd090cc91e29118202ed`.
- Private Android APK SHA-256:
  `905590f84faa69ab7d4eff959f6518164c8ed7bd99e135223242a7e214755074`.
  The installed package is `com.labfytools.trainlog`, versionName 0.1.2,
  versionCode 15, non-debuggable, and signed by certificate SHA-256
  `aa56c97f2781a0d01f007f4444c3970deb58ad8327ca3da7b0b90f37dbe2ad25`.
  The update used `adb install -r`; it did not uninstall or clear the app.
- The desktop schema-v24 backup passed integrity and foreign-key checks. A
  restored copy retained the same SHA-256, and the v24→v25 rehearsal preserved
  all 40 pre-existing tables and 820 rows before the live migration did the
  same. The five new v25 tables were initially empty.
- The Android production `.tlbackup` archive passed manifest digest, SQLite
  integrity, and foreign-key checks. Logical comparison before and after the
  APK replacement preserved all 46 tables, 1,023 rows, and preferences.
- Real Web smoke checks passed at 1440×1000 and exactly 390×844. Program import
  reached the Core preview and was cancelled before commit. The only destructive
  proof used the explicitly disposable preparation
  `sp_4e662fe4-f5bc-47a8-b9cb-7ed75c0cec8a`; no program or performed session
  was created.
- Full-generation runs `sy_0623ed4c-eb69-40ab-ae68-bc5a57b5e3f5` and
  `sy_3cf9e9bb-8631-4caa-9423-478467028ad8` completed around a restart of only
  Trainlog components. Android retained exactly one correlated withdrawal with
  result `no_matching_delivery`, zero corresponding deliveries, and zero
  corresponding performed sessions. Desktop retained one acknowledged
  withdrawal and excluded the preparation from active Web lists after replay.
- No debug APK, public-lineage APK, tag, or public release was used or created.
