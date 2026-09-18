# Program execution flow V1 validation evidence

Status: `PASS`

This record covers both the isolated implementation evidence and the completed
private coordinated rollout. It does not claim a tag or public release.

## Functional evidence

- Android starts one projected Program session through the production durable
  singleton, returns the same draft on replay, refuses an unrelated singleton,
  and refuses a second execution after completion.
- Finalization retains the stable `program_id`, `program_session_id`, and
  execution `session_id`; reopening preserves completion and creates no draft.
- A production full-generation scenario imports `programs-v1` on Android,
  performs and finalizes the session, publishes `program-executions-v1`, imports
  completed history and provenance on desktop, accepts the real correlated ACK,
  and exposes Web `completed` before continuing through deletion and replay.
- Existing logical Program deletion, tombstone ACK, restart and
  non-resurrection coverage remains in the same integrated scenario.

## Validation evidence

- Native Meson suite: 87/87 passed.
- ASan/UBSan Meson suite with Web assets: 89/89 passed.
- Android JVM suite: 231 passed, 5 intentionally skipped hardware cases.
- Android debug and private release builds passed; release lint passed.
- Web Vitest: 94/94 passed; TypeScript and production build passed.
- Real headless Firefox against the C server passed desktop and exact 390×844
  Programs flows, including readable state text and deletion confirmation.
- Populated desktop migration continues from v25 through v27 with identical
  business projections, integrity `ok`, empty foreign-key check, and an empty
  execution ledger. Android v24 to v25 migration preserves the active draft and
  invents no Program provenance.
- JSON/import validators, `git diff --check`, and
  `tools/check_changed_c_format.sh main` passed.

## Private deployment evidence

- Source commit: `949ace63013feb714657f81cf809029276ec8e79`.
- Desktop candidate SHA-256:
  `a6aa6745fe3f8cf55c25272d9b92c97b2ab3242041764d07555536733f04ab12`.
  Its 49-file inventory, source commit, desktop schema v27 and
  `program-executions-v1` companion were verified before installation.
- Android private release SHA-256:
  `7ce6aaa0e6bd889b4e369c4dac3785c6344515ba4ea8190daa78c01ad271ecdf`.
  Official Android tools verified application ID
  `com.labfytools.trainlog`, versionName 0.1.2, versionCode 17,
  nondebuggable release metadata, and certificate SHA-256
  `aa56c97f2781a0d01f007f4444c3970deb58ad8327ca3da7b0b90f37dbe2ad25`.
  The previous installed package was the same application and certificate at
  versionCode 16. Installation used `adb install -r`; no uninstall or data
  clear occurred.
- Verified pre-install backups are stored outside the repository under
  `program-flow-v17-rollout-20260918T193800Z`. The desktop snapshot is schema
  v26 with integrity `ok`; the Android archive is schema v24 and contains the
  complete SQLite store and preferences. A post-migration Android archive and
  the final post-replay archive both verify as schema v25 with integrity `ok`.
- Desktop migrated from schema v26 to v27 with integrity `ok` and an empty
  foreign-key check. Android migrated from schema v24 to v25 with integrity
  `ok`, an empty foreign-key check, no active draft invented, and its nine
  pre-existing completed sessions retained.

## Real execution, synchronization, and deletion

The rollout used only disposable Program
`pg_17a00000-0000-4000-8000-000000000001`. Its single projected session was
started on Android, left and resumed, then completed as
`se_6bb66f59-a6fb-4759-a75d-c1a79ea107e8`. Android displayed `Effectuée`.
The correlated execution exchange completed as run
`sy_b9c2e545-ec63-4836-beea-cff1aa7c5115`, consuming Android generation
`gen_9a0b7968-eeee-49ba-895b-36aa1783e412` and acknowledging desktop
generation `gen_0b52e009-f654-4da4-8f1b-f23665c97784`. The inbound generation
contained the production `program-executions-v1.json`; desktop and Web exposed
the same completed Program provenance.

The second idempotence exchange completed as run
`sy_81d12965-846d-4502-a864-8ded4d5f1f1d`, with inbound generation
`gen_60e615b9-d06f-43ca-8280-c3bd96cebe63` and outbound generation
`gen_55dd9234-0f8c-4a1a-9b57-d6d549888e82`. It retained one execution link,
one completed session, and no active draft.

The revision-guarded Web deletion created exactly one durable Program deletion.
Run `sy_fa74a008-6767-40e2-b973-7bda38eaad68` consumed Android generation
`gen_598eff4b-4b3d-4b8f-8202-1e0c153568ff`, published desktop generation
`gen_36f3dbbd-1610-463e-ab0d-f43c10392117`, and recorded its correlated
acknowledgement. Android retained exactly one tombstone and no live disposable
Program.

After Trainlog services and both stores were closed and reopened, the real
desktop production consumer replayed the preserved pre-deletion generation
`gen_9a0b7968-eeee-49ba-895b-36aa1783e412`. It returned the exact stored
`sqlite-commit-full` consumed ACK and changed no execution, session, deletion,
or Program cardinality. The final correlated run
`sy_ffb11cba-fa2a-43a4-89dc-7395b8a6cbc3` completed with inbound generation
`gen_020b4747-9260-4cd7-ac92-8381b93bfa73`, outbound generation
`gen_ff13e95b-274e-4abd-a006-c8bbddf6ded1`, zero reconciled sessions, and no
missing capability. The deleted Program did not reappear.

The protected Program
`Renforcement 4 semaines — priorité ceinture abdominale` remains active and
unique with its original identity, 24 sessions, 164 ordered entries, and no
`deleted_at`. Real Firefox loaded the deployed C server at 1440×1000 and exact
390×844 viewports; both had no document overflow, showed the protected Program,
and excluded the disposable Program. `trainlog-web.service` and
`trainlog-syncd.service` were active at closeout.

## Candidate boundary

The candidates were built from the exact functional source commit before this
deployment-only evidence update. This documentation commit changes no runtime
artifact; no tag or public release was created.
