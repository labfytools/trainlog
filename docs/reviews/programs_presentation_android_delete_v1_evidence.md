# Programs presentation, Android projection, and delete V1 evidence

## Scope and lifecycle

This record captures the implementation and validation evidence presently
available for `TRAINLOG_PROGRAMS_PRESENTATION_ANDROID_DELETE_V1`.

The feature is implemented, validated, and deployed on the authorized private
desktop/Android pair. `TRAINLOG_PROGRAMS_PRESENTATION_ANDROID_DELETE_V1=PASS`.
This is not a public release and does not change a FROZEN exchange contract.

## Implemented contract

- Desktop schema v26 adds `programs.deleted_at` and the terminal
  `program_deletions` logical-deletion ledger. The ledger retains request and
  deleted revisions, response replay, `generation_id`, and `acknowledged_at`.
  Active and archived programs may be deleted. Source definitions and derived
  preparations remain; deleted programs cannot resurrect or reprepare, and
  archived programs cannot prepare.
- The Web Sessions Programs subtab renders responsive cards and timeline detail
  from real imported, session, usage, and provenance data. Its discrete trash
  action has destructive confirmation, focus management/restoration, and
  Escape/Cancel paths that make no mutation.
- Android schema v24 adds the read-only `synced_programs`,
  `synced_program_sessions`, `synced_program_entries`, and
  `synced_program_deletions` projection. Android provides Sessions list/detail
  consultation only; it exposes no Program mutation actions.
- `trainlog-programs` V1 is an optional staged controlled-generation companion
  named `programs-v1.json`. With the `programs-v1` capability, desktop sends a
  full snapshot of nondeleted programs and unacknowledged tombstones; Android
  durably acknowledges correlated consumption. The companion does not change
  `TRAINLOG_FORMAT_V1`, mobile export V3, catalog exchange, or preparations.

## Validation recorded

- Full Meson suite: **89/89**.
- Focused ASan/UBSan suite: **3/3**.
- Web Vitest: **94** tests.
- Web production build completed.
- Android unit suite: **230** tests, **5 skipped**; `assembleDebug` completed.
- Python Programs and synchronization suites completed.
- Format and diff validators completed, including `git diff --check`.
- Real Firefox Programs screenshots passed and are retained as
  [desktop cards, 1440x1000](evidence/web-programs-v1-card-desktop.png) and
  [mobile deletion dialog, exact 390x844](evidence/web-programs-v1-delete-dialog-mobile-390x844.png).

### Pre-switch migration and exchange gates

`schema_v26_migration` deterministically migrates a populated v25 fixture with
24 sessions, 48 entries, preparation provenance, and historical evidence. It
asserts the exact typed post-migration projections rather than only table or
column presence.

The integrated Android `SyncGenerationServiceTest` executes the real production
chain from a live Program through capture and publication, Android consumption,
Program deletion and tombstone publication, the real correlated ACK, desktop
acceptance and reopen. An exact old generation is idempotently recognized and
ACKed while the durable tombstone prevents Program resurrection; a second
exchange adds no duplicate data.

### Private real-copy v25 to v26 migration evidence

A private real-copy migration gate preserved user-data confidentiality while
validating the source backup at v25 and its post-migration v26 result. The
post-migration integrity check passed, foreign-key violations numbered **0**,
and the business-data SHA-256 was identical before and after migration:
`b264dd1b5fd7cdc599b2f6e2a20c01af794949e7871b8e3107ff3b29a06c6e11`.

The Program hash was
`1b14ba7d8dbfbd3d3b19b6584e399fce3f161e1aa20591ca21676530dc0ebd43`;
the 24 ordered sessions hash was
`593f91572cccb109f76175be249afad5b0e311c429b56ef0cee141a5deb7f3a4`;
the 164 entries hash was
`9ca6742dd168777b7653a857bc695dbf1f59f10abf489bfaae24660b69fdb1cc`;
and the two preparations hash was
`2e2733b2624248210c390e5d592fe03e4130775ddb24e7fdb93280b4f778ec25`.
The copy contained no `deleted_at` values and no
`program_deletions` rows. All 45 unchanged business tables matched their
canonical field-level hashes.

## Private deployment validation

The authorized rollout used source commit
`f431b6ec9cab68e0d41d04815ba41bac55787485`. The coordinated desktop archive
SHA-256 was
`072fb61c8ddc86ebd9fed650c3928ddfd07010823358e0cd6cfefb3de11d4597`;
the private release APK SHA-256 was
`bbf3fa5b3ddce3d83b13d22170c23ae0196991b31b70466f85fa124ed72fb297`.
The installed `com.labfytools.trainlog` remained non-debuggable and retained
certificate SHA-256
`aa56c97f2781a0d01f007f4444c3970deb58ad8327ca3da7b0b90f37dbe2ad25`.

The real desktop database migrated from schema v25 to v26 with
`integrity_check=ok`, no foreign-key violations, and unchanged field-level
Program data. The real Program retained its stable identity, title, active
state, 24 ordered sessions, 164 entries, occurrences, targets, equipment,
rests, notes, and preparation provenance. Android migrated from schema v23 to
v24 without changing any of its 46 pre-existing tables.

The real Web import created only the disposable Program
`pg_dd90fbc1-8131-41f1-9d49-f516e34a17da`. Its first confirmation cancellation
made no mutation. Confirmed deletion produced revision
`pgr_5c34b25a-678e-49e5-89a2-eecebd4e361f`; Android consumed its tombstone and
desktop correlated acknowledgment with generation
`gen_5a260b7c-6377-4805-82bb-d793fc41e35f`.

After both components restarted, the first attempted control run failed before
either generation was created: `device_unavailable` followed a real
`PTP_ERROR_IO`. The failure was retained and was not rewritten as success. A
stale `trainlog-sync-once --request-only` worker owned the MTP descriptor;
stopping only `trainlog-syncd` released it. A bounded libmtp probe found the
same Samsung SM-G990B, and restarting the local ADB server restored the same
serial and peer advertisement without reconnecting, reinstalling, clearing, or
restoring either store.

The final post-restart run
`sy_2b2320a8-e182-41a5-b238-ac04f0d4e5f2` completed with desktop generation
`gen_9afcc6f9-f7a5-4eec-a24e-696182addfe8`, Android generation
`gen_0b101149-5ba8-4ba2-9912-eca8cc9d9953`, two durable `consumed` ACKs, no
missing capability, and `sessions_reconciled=0`. A final Android backup has
schema v24, `integrity_check=ok`, no foreign-key violations, one synchronized
Program with 24 sessions, and exactly one tombstone for the disposable Program.
Its archive SHA-256 is
`cad5830062578984e77f7900246701b5e1a6bc7629054d012ef34c7cd5f36669`.
Desktop has schema v26, one acknowledged deletion ledger row, no visible
disposable Program, and one unchanged real Program. Closing and reopening the
services and Android application preserved those results. Both Trainlog user
services were active at closeout.
