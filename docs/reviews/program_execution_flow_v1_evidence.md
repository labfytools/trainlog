# Program execution flow V1 validation evidence

Status: `IMPLEMENTED_VALIDATED_AWAITING_ROLLOUT`

This record covers the isolated `program-execution-flow-v1` worktree. It does
not claim installation, user-database migration, service restart, device smoke
testing, tag creation, or public release.

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

## Candidate boundary

The coordinated candidates are built only after the final source commit. Their
exact paths, source commit, hashes, Android package metadata and signing digest
belong in the consent-gate report so the archive inventory can bind to the exact
final HEAD without circular documentation changes.
