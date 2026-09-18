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

- Full Meson suite: **88/88**.
- Focused ASan/UBSan suite: **3/3**.
- Web Vitest: **94** tests.
- Web production build completed.
- Android unit suite: **229** tests, **5 skipped**; `assembleDebug` completed.
- Python Programs and synchronization suites completed.
- Format and diff validators completed, including `git diff --check`.
- Real Firefox Programs screenshots passed and are retained as
  [desktop cards, 1440x1000](evidence/web-programs-v1-card-desktop.png) and
  [mobile deletion dialog, exact 390x844](evidence/web-programs-v1-delete-dialog-mobile-390x844.png).

These results are implementation and presentation-validation evidence. They do
not substitute for separate deployment validation.

## Remaining validation

- Validate deployment separately before representing the feature as deployed.
