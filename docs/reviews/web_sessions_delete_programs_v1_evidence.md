# Web Sessions deletion and Programs V1 evidence

Date: 2026-09-18

This pre-deployment record covers the completed development branch. It does not
claim installation, migration of real data, service restart, phone validation,
tagging or release publication.

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

## Deployment gate

Desktop schema v25 and proposal synchronization V2 require the grouped gate:
retain the installed package, create consistent desktop and Android backups,
validate v24→v25 migration on a database copy, verify Android package signing
and monotonic versioning, then install/restart/synchronize and inspect the real
Web routes only after explicit authorization. No operation in this record
crossed that gate.
