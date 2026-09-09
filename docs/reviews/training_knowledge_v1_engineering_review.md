# Training Knowledge V1 — engineering checkpoint

Date: 2026-09-09. Status: `TRAINING_KNOWLEDGE_V1_ENGINEERING_REVIEW=PASS`.

Scientific review separately passed within its documented scope and uncertainty.
The independent temporal delta review returned `TEMPORAL_DELTA_REVIEW=PASS`
with no findings. One configured final-reviewer completed an initial full-tranche
audit, reported the findings recorded below, and its authorized bounded repair
chain completed. Independent bounded verification returned
`FINAL_REVIEW_REPAIR_VERIFICATION=PASS`; fresh final executable validation also
passed. `TRAINING_KNOWLEDGE_V1=PASS`.

## Current implementation and contract

Six canonical scientific catalogs feed generated C data and the Android asset
loader. Both clients expose scientific lookup/filter APIs and runtime context
composition. Android has a collapsible knowledge section; the TUI has a UTF-8
cell-aware scrolling knowledge screen tested at 72×20. No schema, scientific
catalog, frozen exchange artifact or stored user history changed in this resume.

The temporal repair covers the two new C readers and their Android counterparts:
occurrence pagination and latest explicit MAX. The established operational
profile and parser-only extension classification are recorded in the
[temporal contract](training_knowledge_v1_temporal_contract.md). All use exact
integer-second/fraction comparisons and bytewise stable-ID ties. Pagination
scans all matching metadata, retains at most `limit + 1`, then hydrates only
selected rows under one read snapshot. Emitted cursors preserve source text.

C has the existing 40-character timestamp output limit: long valid candidates
are compared exactly, and a selected unrepresentable result fails explicitly.
Invalid caller cursors and malformed stored timestamps have distinct error
paths. No malformed date can disappear through SQLite NULL comparison.

Python validation uses the same explicit grammar and exact chronology. The
parent reproduced and repaired an optional-checker dependency issue after the
worker implementation: a strict JSON Schema checker could reject omitted
seconds before Trainlog validation. A private per-call override now aligns the
schema-format check while preserving all other checker configuration. Actual
validation-path tests cover absent, strict and permissive optional checkers.

## Original defect retained as historical evidence

The original implementation combined an offset-limited cursor guard with
SQLite `julianday()` and a different Android parser. Its production probe was:

| Newer source timestamp | Original outcome | Current independent probe |
|---|---|---|
| `2026-09-10T10:00:00+02:00` | control passed | PASS |
| `2026-09-10T10:00:00+14:30` | emitted cursor rejected | PASS |
| `2026-09-10T10:00:00+15:00` | newer occurrence omitted | PASS |
| `2026-09-10t10:00:00Z` | newer occurrence omitted | PASS |

The independent current probe extends this to twelve spellings. Every case
uses two sessions in a temporary database, page size one, exact source/cursor
assertions, continuation and exhaustion. Both normal and sanitized builds pass.
Original evidence remains `/tmp/trainlog-knowledge-validation/cursor-probe.c`
and `cursor-probe.log`; fresh final probe evidence is under
`/tmp/trainlog-knowledge-review-resume/`.

## Final validation evidence

| Check | Result |
|---|---|
| `meson compile -C build` | PASS, fresh strict warning-as-error build |
| `meson test -C build --print-errorlogs` | 42 passed |
| Knowledge validator/generation tests | PASS; eight Python tests |
| Temporal Python tests | PASS; four tests including optional-checker independence |
| JSON/import-contract validators | PASS; six import cases |
| Three standalone public C17 headers | PASS, including `-pedantic-errors` |
| Two targeted C tests under ASan/UBSan | PASS; timestamp Python test also passes in that test invocation |
| Independent twelve-form C production probe | PASS normally and under ASan/UBSan |
| Android unit tests and debug assembly, Java 17 | PASS; 56 tests, zero failures/errors, one known fixture skip |
| Project skill validator and `git diff --check` | PASS |

The known Android skip is `RealAndroidV9BodyZonesMigrationTest`, because its
external `TRAINLOG_ANDROID_V9_FIXTURE` is unavailable. No knowledge/temporal
test is skipped. Manual MTP, installed-device UI and real-terminal visual
checks were not performed or claimed. Sanitizer evidence is scoped to the
new affected C paths, not the complete GUI executable.

## Independent review and final-review repair record

The isolated temporal review was independent of the original implementation
and reviewed grammar/calendar constraints, offset bounds, fractions, byte ties,
cursor aliasing/exclusivity, source capacity, snapshots, and Android/Python
parity. It ran Meson `training_context` and `timestamp_validation` plus four
Python temporal tests, returned `TEMPORAL_DELTA_REVIEW=PASS`, and required no
repair.

The configured final-reviewer then audited the full tranche. Its initial result
was FAIL and identified these concrete findings:

| Severity | Finding | Authorized repair recorded in `/tmp/trainlog-knowledge-review-resume/repair.md` |
|---|---|---|
| BLOCKER | Stale temporal-defect documentation in `architecture.md`, `exercise_data_model.md`, and `tui.md` | Replaced claims of an unresolved reader defect with the independently passing temporal contract and correct repair/validation state. |
| BLOCKER | Android loader parity holes | Enforced canonical exercise/equipment identity syntax, reverse capability compatibility, HIGH evidence source type, and non-unresolved BODY ZONE audit evidence; added loader regressions. |
| BLOCKER | Meson generator inputs omitted `body-zones-v1.json` and `equipment-v1.json` although validation reads them | Declared both files as `training_knowledge_generated` inputs and proved regeneration without output-byte change. |
| HIGH | C query accepted a muscle role without a muscle ID, unlike Android's paired filter | Rejected role-only queries, documented the paired contract in the public header, and added a C regression. |

The repair report records strict C17 syntax, knowledge validation, generated
output equality, targeted Meson test, Android loader tests, dependency proof,
and `git diff --check` as PASS. Independent bounded verification closed all
four findings and explicitly returned
`FINAL_REVIEW_REPAIR_VERIFICATION=PASS` and
`TRAINING_KNOWLEDGE_V1_ENGINEERING_REVIEW=PASS`. The fresh tester matrix then
passed: strict build; 42 Meson tests; eight knowledge and four temporal Python
tests; knowledge/JSON/import validators; three strict C17 headers; affected C
knowledge/context tests under ASan/UBSan plus timestamp validation; normal and
sanitized 12-form temporal probes; Android 56 tests with zero failures/errors
and one known missing-fixture skip; Java 17 debug assembly; skill validation;
and diff check. Fresh generated C is byte-identical with SHA-256
`e8c099f67eb111d61621b5d76592c049823af5508d43e73ec646f22e4c377fca`; all six
Android assets are byte-identical.

## Remaining observations

The prior bounded review's secondary scientific BODY ZONE IDs on the TUI
knowledge page remain a nonblocking presentation inconsistency. Local machine
model/execution uncertainty and missing real exercise UUIDs remain as recorded
in the scientific review; no phantom identity or scientific reassessment was
introduced. Older lexical timestamp readers, arbitrary-length C output and
verified leap-event support are outside this bounded repair.
