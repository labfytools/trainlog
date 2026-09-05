# Gate 0 Review #1

## Status

```text
GATE_0_REVIEW_01=IMPLEMENTED
GATE_0=VALIDATION_PENDING
TRAINLOG_FORMAT_V1=DRAFT
```

## Scope

This review hardens the initial project contract before any C17 or Android implementation begins.

## Findings corrected

### G0-R1-01 — Completed timestamp was structurally mandatory

The initial schema required `ended_at`.

That contradicted the architecture requirement to preserve an active or interrupted session without inventing a completion time.

Correction:

- `ended_at` is optional;
- when present, semantic validation requires it to be strictly later than `started_at`.

### G0-R1-02 — Repetitions and duration were not exclusive

The initial schema used `anyOf`.

A target or set containing both `reps` and `duration_seconds` therefore satisfied both alternatives and could be accepted.

Correction:

- schema uses an exclusive representation;
- every target/set contains exactly one activity mode.

### G0-R1-03 — Exercise identifier uniqueness was undocumented executable behavior

JSON Schema cannot enforce uniqueness of one property across different objects in an array.

Correction:

- semantic validator rejects duplicate `exercise_id` values;
- negative fixture added.

### G0-R1-04 — Display-name duplicates could create duplicate exercises

Stable identifiers alone do not prevent accidental creation of two exercises with visually equivalent names.

Correction:

- semantic normalization algorithm documented;
- semantic validator rejects duplicate normalized display names;
- negative fixture added.

### G0-R1-05 — Session exercise references were not checked against the catalog

JSON Schema cannot validate this cross-reference.

Correction:

- semantic validator requires every session exercise to exist in the top-level catalog;
- negative fixture added.

### G0-R1-06 — Duplicate exercise entries inside one workout were ambiguous

A session could contain the same exercise twice, complicating editing and analytics.

Correction:

- v1 requires one workout entry per `exercise_id`;
- all actual sets belong to that entry;
- negative fixture added.

### G0-R1-07 — Target and actual set modes could disagree

A repetition target could contain duration-based actual sets or vice versa.

Correction:

- semantic validator enforces mode consistency;
- negative fixture added.

### G0-R1-08 — Timestamp offset and chronology required semantic enforcement

The contract requires explicit timezone information and meaningful ordering.

Correction:

- validator rejects offset-less timestamps;
- validator rejects `ended_at <= started_at`;
- negative fixtures added.

### G0-R1-09 — Unknown-field behavior was not frozen

Silently accepting misspelled fields would risk data loss.

Correction:

- v1 draft explicitly rejects unknown fields;
- schema keeps `additionalProperties: false`;
- negative fixture added.

## Validation command

```bash
python tools/validate_json.py
git diff --check
```

Expected result:

- every valid fixture reports `PASS valid`;
- every invalid fixture reports `PASS invalid`;
- `git diff --check` prints nothing.

## Gate decision

Review #1 does not itself mark Gate 0 as PASS.

Gate 0 becomes eligible for PASS after:

1. the canonical local validation succeeds;
2. the review is committed;
3. the commit is pushed to Forgejo and GitHub;
4. the mirrored repository is reviewed.
