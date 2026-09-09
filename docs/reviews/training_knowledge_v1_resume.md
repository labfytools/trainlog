# Training Knowledge V1 — continuation record

Checkpoint: 2026-09-09. Status: `TRAINING_KNOWLEDGE_V1=PASS`.

This is a completed closeout record. Do not restart science, temporal
implementation, broad audit, or the bounded repair chain. The independent
temporal review returned `TEMPORAL_DELTA_REVIEW=PASS` with no findings. One
deep final audit initially failed, one bounded repair chain closed its four
findings, independent verification returned
`FINAL_REVIEW_REPAIR_VERIFICATION=PASS` and
`TRAINING_KNOWLEDGE_V1_ENGINEERING_REVIEW=PASS`, and fresh final executable
validation passed.

## Baseline and protections

- Repository: `/home/fy59/Documents/trainlog`.
- Baseline HEAD: `1aad7b48b3143783ccfd326c0145f495fd568442`.
- The initial worktree was clean; the current delta belongs to this tranche.
- This resume initially matched every file hash in the preserved final manifest.
- No stage, commit, push, reset, restore, stash or clean was performed.
- Desktop schema remains v11 and Android schema remains v10.
- Real database logical contents/counts and all scientific/frozen catalogs are
  preserved. No device install, uninstall, keystore or user-history change.

## Completed during the temporal correction

1. `advisor` / Terra-high completed the isolated contract analysis. Its retained
   handoff is `/tmp/trainlog-temporal-contract-settled.md`; the durable repository
   record is [temporal contract](training_knowledge_v1_temporal_contract.md).
2. `worker` / Sol-medium implemented exact C/Kotlin timestamp parsers,
   scan/select/hydrate occurrence and latest-MAX reads, coherent exclusive
   cursors, nested read snapshots and production regressions. Python validation
   uses the same grammar and exact fractional comparison.
3. Parent's independent production C probe passed all twelve temporal spellings
   through two sessions, page size one, source/cursor equality and exhaustion.
4. A bounded additional validator defect was reproduced: a strict optional
   JSON Schema format checker could still reject the Android no-seconds form
   before semantic validation. A per-validation `date-time` override now uses
   the Trainlog parser without mutating global/caller checkers. Document-path
   regressions cover missing, strict and permissive checkers.
5. Full executable validation passed, including the independent C probe under
   ASan/UBSan. Canonical documentation describes the settled behavior.

Scientific review remains PASS within its recorded scope; no scientific delta
was introduced. Its reviewed catalog hashes remain applicable.

## Completed independent review and repair chain

The independent temporal reviewer covered grammar, calendar and offset ranges,
fractions, byte ties, cursor aliasing/exclusivity, source capacity, snapshots,
and Android/Python parity. It ran the targeted Meson and four Python temporal
tests and returned `TEMPORAL_DELTA_REVIEW=PASS`, with no temporal repair.

The one final-reviewer audited the complete tranche. Its initial FAIL found
stale temporal-defect documentation (BLOCKER), Android loader parity holes
(BLOCKER), missing Meson generator dependencies (BLOCKER), and C acceptance of
a role-only muscle query (HIGH). The authorized repair report is
`/tmp/trainlog-knowledge-review-resume/repair.md`: it records the exact loader
constraints, Meson input declarations, C paired-filter rejection, regressions,
and repair-scope validation. Independent review of that delta closed all four
findings in `/tmp/trainlog-knowledge-review-resume/repair-verification.md`.
No scientific catalog, schema, frozen format, temporal implementation, or
user-history content changed.

## Closeout evidence

Fresh final validation recorded in
`/tmp/trainlog-knowledge-review-resume/full-results.json`,
`android-results.json`, `sanitizer-results.json`,
`probe-normal-results.json`, and `android-counts.json` passed: strict build;
42 Meson tests; eight knowledge and four temporal Python tests;
knowledge/JSON/import validators; three strict C17 headers; affected C
knowledge/context tests under ASan/UBSan plus timestamp validation; normal and
sanitized independent 12-form temporal probes; Android 56 tests with zero
failures/errors and one known missing-real-v9-fixture skip; Java 17 debug
assembly; skill validation; and diff check. Fresh generated C is byte-identical
with SHA-256 `e8c099f67eb111d61621b5d76592c049823af5508d43e73ec646f22e4c377fca`;
all six Android assets are byte-identical.

Preservation before and after repair confirms desktop schema v11 and Android
schema v10; unchanged frozen/scientific catalogs and temporal implementation;
real database logical SHA-256
`26139cafeffbde3ec08f6ef23c5069e75afb9cd69ffffb40be5c099006fedc4d`; counts
of 1 body observation, 6 continuous activities, 3 custom equipment, 23 zone
sync rows, 32 exercise-zone rows, 23 exercises, 19 max results, 28 performed
sets, 31 occurrences, and 3 sessions; `PRAGMA integrity_check = ok`; clean
foreign-key check; and an empty index. Current closeout capture locations are
`/tmp/trainlog-knowledge-review-resume/final.patch` and
`/tmp/trainlog-knowledge-review-resume/final-manifest.json`. The completed capture
includes all 19 modified tracked files and 37 untracked files, validates their
hashes and whitespace, and confirms the real index remained untouched and empty.

## Validation and retained evidence

- 42 desktop tests; eight Python knowledge tests; four Python temporal tests.
- Android tests and debug assembly: 56 tests, zero failures/errors, one skipped
  `RealAndroidV9BodyZonesMigrationTest` because `TRAINLOG_ANDROID_V9_FIXTURE`
  is unavailable. No knowledge/temporal test is skipped.
- Catalog, JSON and import-contract validators; project skill validator.
- Three public C17 headers with `-pedantic-errors` and strict warning flags.
- Two affected C tests under ASan/UBSan, plus the temporal Python test; independent
  twelve-form production-API probe also passes under ASan/UBSan.
- `git diff --check` passes. Manual MTP/device/visual checks remain unclaimed.

Current logs/results and reusable runner include
`/tmp/trainlog-knowledge-review-resume/full-results.json`,
`android-results.json`, `android-counts.json`, `sanitizer-results.json`, and
`probe-normal-results.json`.
Reusable sanitizer build: `/tmp/trainlog-knowledge-validation/build`.
Implementation record: `/tmp/trainlog-temporal-implementation.md`.

The older `/tmp/trainlog-knowledge-final.*` files are a previous checkpoint,
not current complete evidence. Durable contract/evidence is in this review
directory when `/tmp` is absent.

Known explicit limits: C output timestamp capacity 40 characters with explicit
failure for a selected longer value; no leap-event table (`:60` rejected);
O(N) metadata scan per page; older unrelated lexical history readers remain
future scope. No source-text normalization, migration or history rewrite is
part of this correction.
