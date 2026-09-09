# Training Knowledge V1 — temporal contract and correction

Date: 2026-09-09. Contract analysis: advisor / Terra-high, completed.
Temporal implementation and executable validation: PASS. Independent temporal
delta review: `TEMPORAL_DELTA_REVIEW=PASS`, with no findings or repairs. The
initial full-tranche audit found non-temporal defects and its authorized repair
chain completed. Independent bounded verification and fresh final validation
subsequently passed; `TRAINING_KNOWLEDGE_V1=PASS`. This record preserves the
temporal contract and does not expand scientific scope or declare a frozen
format decision.

## Authority and compatibility

The frozen [exchange timestamp rule](../exchange_format.md#7-session-timestamps)
requires RFC 3339 / ISO 8601 date-time text with an explicit UTC offset. The V1
schema declares `format: date-time`. The operational reader profile is:

```text
YYYY-MM-DD[Tt]HH:MM[:SS[.digits]](Z|z|±HH:MM)
```

It uses extended Gregorian dates in years 0001–9999, valid calendar days,
hours 00–23, minutes 00–59 and ordinary seconds 00–59. Fractions require
seconds and at least one ASCII digit after a dot. Every fractional digit
participates in comparison; trailing zeros do not change an instant.

Numeric offsets range through ±23:59. Lowercase `t`/`z` and the offset grammar
follow [RFC 3339 §5.6](https://www.rfc-editor.org/rfc/rfc3339#section-5.6).
`-00:00` compares as a zero UTC offset while its distinct source spelling is
preserved. Omitted seconds are an application compatibility requirement:
Android persists `OffsetDateTime.now().toString()`, which can omit seconds
when seconds and fractional seconds are both zero.

Basic dates, ISO week dates, spaces instead of `T/t`, comma fractions, compact
offsets and hour-only offsets are **NOT CONTRACTUALLY REQUIRED**. They are
rejected by the selected profile. The observed acceptance of these forms by
Python `datetime.fromisoformat()` is not normative evidence: this environment's
jsonschema 4.26.0 has no registered default `date-time` checker. No Trainlog
writer, pre-existing fixture or inspected real session establishes a need for
those extensions. All three real session timestamps match the selected
profile and fit the existing C result fields.

Years outside 0001–9999 and `:60` remain outside the established operational
admission. Supporting real leap seconds would require a verified shared
leap-event schedule and matching validator behavior; the repair does not
guess a conversion or collapse a leap second into the following minute.
[RFC 3339 §5.7](https://www.rfc-editor.org/rfc/rfc3339#section-5.7)

## Root cause and implementation

The old cursor guard, SQLite `julianday()`, Java `OffsetDateTime.parse()` and
Python's ISO parser had different languages and precision. A `+14:30` source
produced a rejected cursor. SQLite returned NULL for valid `+15:00` and
lowercase-`t` values, allowing records to disappear. Floating-point Julian
days also did not preserve arbitrary fractional precision.

The C and Kotlin readers now parse source text into an integer UTC second
and an exact fractional digit sequence. Both occurrence pagination and the
new latest-explicit-MAX reader order descending by:

```text
(represented UTC instant, session_id bytes, entry_id bytes)
```

The cursor applies the same comparison exclusively. Equal instants with
different offsets or fractional trailing zeros use identity tie breakers,
never timestamp-text tie breakers. Cursor emission preserves the source
timestamp and both IDs. C supports aliasing the input and output cursor.

Each read scans all matching metadata candidates and retains at most
`limit + 1` occurrence candidates, or one MAX candidate. Only selected rows
are hydrated. It does not use a timestamp SQL predicate, SQL temporal order
or SQL limit before comparison. Scan work is O(N) for the exercise history;
retained candidate count is O(limit), with no global dataset-size cap. Each
read has a snapshot covering selection and hydration. Separate page calls
retain the existing current-data semantics, without a cross-call snapshot.

Malformed caller cursors fail before the scan. Malformed matching stored
timestamps cause an explicit database/consistency error; they are not
silently skipped. Source timestamps are never rewritten or normalized in
persistent storage.

The C public timestamp fields retain their existing 40-character capacity.
Longer valid SQLite text is compared with full precision; if selected for
output, it produces `DATABASE_ERROR`. It is never truncated or silently
omitted. Android can return longer strings. This is an existing C API
representation limit, not a new wire precision bound. Arbitrary-length C
output would require a separate ownership/length API change.

The Python validator uses the same grammar and exact chronology. It overrides
only `date-time` on a fresh per-validation format-checker instance, preserving
other checks and caller/global state. The document path therefore admits
Android's no-seconds form even when an optional strict RFC checker is present.

## Regression and validation evidence

Production C and Android tests cover mixed representations, page size one,
emitted cursors, continuation, exhaustion, no duplicates, chronological order,
equal-instant session/entry ties, near-equal fractions, malformed cursors and
stored values, and latest-MAX selection. C also checks selected and unselected
timestamps beyond its fixed output capacity. Python tests cover grammar,
exact chronology, date/offset bounds and missing/strict/permissive optional
format checkers through actual document validation.

An independent C production-API probe exercises twelve admitted spellings:
`+02:00`, `+14:30`, `+15:00`, lowercase `t`, `Z`, lowercase `t/z`, omitted
seconds, `+23:59`, `-23:59`, `-00:00`, and two long fractional forms. Each case
uses two sessions in a temporary database, page size one, exact source/cursor
equality checks and traversal through exhaustion. It passes normally and
under ASan/UBSan.

Full validation passes: 42 desktop tests; eight knowledge Python tests; four
timestamp Python tests; JSON/import/catalog validators; three standalone C17
headers; Android unit tests and debug assembly (56 tests, zero failures or
errors, one pre-existing missing-fixture skip); two targeted C sanitizer tests
plus the timestamp Python test; project skill validation; `git diff --check`.
Logs and reusable runner: `/tmp/trainlog-temporal-validation/`.

The independent temporal reviewer covered grammar, calendar validity, offset
ranges, exact fractions, bytewise ties, cursor aliasing and exclusivity, source
capacity, snapshot behavior, and Android/Python parity. It ran the targeted
Meson `training_context` and `timestamp_validation` tests and all four Python
temporal tests. It reported no temporal defect, repair, or unresolved temporal
boundary.

## Preserved boundaries and remaining scope

Desktop schema v11, Android schema v10, stable IDs, real database logical
contents and counts, BODY ZONES, equipment and all scientific catalog bytes
remain unchanged. Scientific review stays valid without a scientific delta.
No staging, commit, push, database deletion, migration, device installation,
uninstall, keystore change or history rewrite occurred.

Older general history/export/MAX-list readers with lexical timestamp ordering
are outside this bounded new-reader repair. Leap-event support, arbitrary-
length C output and manual device/MTP/visual checks remain explicit future or
manual work. The previously recorded secondary-zone-ID TUI presentation
inconsistency remains nonblocking. Final-review repair verification and fresh
final validation completed with `TRAINING_KNOWLEDGE_V1=PASS`.
