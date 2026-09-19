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

## Controlled private rollout

The rollout source was advanced through bounded corrective commits to
`a8c50a9512d8754477a2491b73c0f9e0f92263bf`. The coordinated desktop archive is
`trainlog-0.1.2-a8c50a9-desktop-v28.tar.gz`, SHA-256
`e9e08b317fb97807987b8e027a072cc8235bd9ee6dff7d393df4acb8f04c8a46`.
Android code and schema were unchanged; the installed private application
remained `com.labfytools.trainlog`, versionCode 17, versionName 0.1.2, schema
25.

Before deployment, the real desktop v27 database was backed up as
`trainlog-desktop-v27.db`, SHA-256
`ffa4cd5c5fc832b4054ee6543ff89b74be7148b19ffb9af718aa0dfe787ab20a`.
The real Android product backup was
`trainlog-android-v25.tlbackup.zip`, SHA-256
`8d198dc5fb6ca5c410029342f0e3074d288dac844ce686e3a70782bdad9e8bce`.
A populated copy migration changed only `PRAGMA user_version` from 27 to 28:
all 47 business-table counts and canonical typed hashes were identical,
`integrity_check` was `ok`, and `foreign_key_check` was empty. The production
migration reached the same integrity state.

The real deletion used the Web HTTP command with the session's current causal
revision. It removed only
`se_6bb66f59-a6fb-4759-a75d-c1a79ea107e8`, created operation
`del_af8a82de-2825-45af-9f62-ec94cc61a556`, and retained exactly one deleted
causal state. Its Program execution provenance moved from `completed` to
terminal `deleted` while preserving the original observation timestamp. The
session detail then returned the bounded absent response and reload did not
restore it.

The first hardware exchange exposed two real recovery defects before a final
PASS. Generation `gen_42f8d681-373e-4f07-b08d-792a1e8e516d` was rejected
because the live Android snapshot reached the importer after its tombstone;
the subsequent generation `gen_c9d1fda8-8375-4d89-8c29-a5ae120c6c7d` proved
that the equipment-association companion required the same dominance rule.
Later retries reported `peer_capacity_exhausted`: a product backup showed four
older immutable Android generations still waiting for ACK. Their retained
manifests were processed by the production consumer and received genuine
correlated stale-lineage rejection ACKs. The recovery envelope then accepted
that exact terminal evidence. No ACK, tombstone, generation, archive or ledger
row was fabricated, deleted or edited manually.

The final post-correction exchange completed as run
`sy_b343619f-a1d2-4bea-a0af-c57aa5792aca`: Android generation
`gen_b2290cad-0554-46f9-bc30-0fbd0c8190e8` was consumed with
`sqlite-commit-full`, desktop generation
`gen_21d7efa0-bd97-4843-a0d6-111e94d669b4` was acknowledged, no capability was
missing, and `sessions_reconciled` was zero. The second exchange completed as
run `sy_58b38f82-a70c-48c3-834c-81436c687c8c`, with generations
`gen_65e4833c-5383-4374-b37f-d8add8a106e8` and
`gen_76e15f8a-1258-49f1-ac0e-b04fe9e6a8a5`, again with no missing capability
and zero reconciled sessions.

After both stores and the Web component were reopened, the Android product
backup `trainlog-android-postrestart-20260919.tlbackup.zip` had SHA-256
`7ae8f88c60bd44355c19c89e7996d8c71ec178c75aabff0979065a5a70936ed5`,
schema 25, `integrity_check=ok`, an empty foreign-key check, no disposable
session, and exactly one matching causal state and operation. Desktop remained
schema 28 with the same integrity results and terminal provenance. Canonical
hashes for every other session, occurrence, performed set, continuous result
and body observation were identical to both pre-rollout backups.

Finally, the retained pre-deletion generation
`gen_965fdf80-4412-41fb-9a46-28744a3e115b` was replayed through the production
consumer. It returned its byte-identical stored ACK; the before/after tuple was
unchanged (`target session=0`, `target operation=1`, `deleted provenance=1`,
`all sessions=9`). Real Firefox against the deployed C server passed at
1440x1000 and exactly 390x844 with no horizontal overflow. Evidence screenshots
are retained privately under
`~/.local/state/trainlog/history-delete-http500-fix-v1-web-evidence/`; their
SHA-256 values are respectively
`9c9fd42aa58b0818488ef9f8b3412abfb56e96ea3f9d30c8da2fcfe3da8c10f1` and
`70838305fe6d17485f3d5d80cf60e2639baf4ed87d148f4b55e4b6e8c25fdbca`.
