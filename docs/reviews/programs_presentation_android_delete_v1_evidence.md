# Programs presentation, Android projection, and delete V1 evidence

## Scope and lifecycle

This record captures the implementation and validation evidence presently
available for `TRAINLOG_PROGRAMS_PRESENTATION_ANDROID_DELETE_V1`.

The feature is implemented and validated in the recorded automated and real
Firefox Programs presentation checks. It is **not deployed**. This record does
not assign `PASS` or `FROZEN` status.

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

The program identity hash was `1b14...`; the 24 sessions hash was `593f...`;
the 164 entries hash was `b991...`; and the two preparations hash was
`2e273...`. The copy contained no `deleted_at` values and no
`program_deletions` rows. All 45 unchanged business tables matched their
canonical field-level hashes.

These results are implementation and presentation-validation evidence. They do
not substitute for separate deployment validation.

## Remaining validation

- Validate deployment separately before representing the feature as deployed.
