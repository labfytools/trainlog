# Web history deletion HTTP 500 corrective evidence

Date: 2026-09-18

## Incident and root cause

The disposable Program rollout session
`se_6bb66f59-a6fb-4759-a75d-c1a79ea107e8` was confirmed read-only as the
completed execution of `Séance jetable de bascule` in the already deleted
Program `BASCULE PROGRAM EXECUTION V1 — JETABLE — 2026-09-18`. It contained one
occurrence and one performed set.

On an SQLite backup copy, the deployed history detail endpoint returned HTTP
500 before the deletion mutation was reached. SQLite reported primary code 1,
extended code 1, and `no such column: se.max_weight_kg` while preparing the
history-entry query. The field belongs to `max_results`, not `session_entries`.
The same inspection also found no current causal revision for this imported
session and showed that completed Program execution provenance needed an
explicit terminal lifecycle state before its session could be removed.

## Corrective contract

The history detail query reads `max_results`. Mobile history import seeds a
missing causal live revision with the canonical deterministic snapshot
revision. Desktop schema v28 permits a Program execution to move from
`completed` to terminal `deleted` in the same transaction as the revision-
guarded session deletion and tombstone publication. An old completed execution
fact is dominated only by a matching durable deleted causal state. Browser
responses distinguish stale, absent, invalid and internal failures, while
bounded server diagnostics include the SQLite codes and failing stage without
exposing SQL or paths.

## Automated evidence

- GCC Meson: 91/91 tests passed.
- Clang built the complete target set; the five affected native tests passed.
- ASan/UBSan: the five affected native tests passed.
- Web TypeScript, 99 Vitest tests, production build and two real-Firefox
  Sessions scenarios passed.
- Android debug assembly and unit tests passed without Android source or schema
  changes.
- JSON/import validators, changed-C formatting and `git diff --check` passed.

The rollout-specific backup hashes, deployed commit, causal operation,
generation/ACK identifiers and restart/non-resurrection results are appended
after the controlled private rollout. No public tag or release is created.

During the first post-delete hardware exchange, the desktop correctly rejected
Android's still-live snapshot with `causal protection refuses legacy/live
replay`. This exposed a missing full-envelope dominance rule: Android publishes
its half before consuming the desktop tombstone in the same conversation. The
production full-generation consumer now filters only live facts already
dominated by causal state applied from that complete envelope; standalone
snapshot imports retain their strict rejection. A regression exercises this
ordering and proves the tombstone and absent session remain unchanged.
The recovery envelope also republishes both consumed and rejected correlated
ACKs. This lets Android close an interrupted generation with the real durable
desktop rejection evidence and reclaim normal bounded capacity; no generation,
ACK, tombstone or archive is manually purged or synthesized.
