# Gate 1 Review #1 — Exchange Format Freeze Candidate

## Status

```text
GATE_1_REVIEW_01=IMPLEMENTED
GATE_1=VALIDATION_PENDING
TRAINLOG_FORMAT_V1=DRAFT
```

## Purpose

Gate 1 review #1 turns the Gate 0 format skeleton into a complete v1 implementation contract.

No C17 or Android production implementation should depend on v1 until this review passes and the format is explicitly frozen.

## Decisions

### G1-R1-01 — Stable exercise tracking mode

Each exercise declares exactly one stable `tracking_mode`:

```text
reps
duration
```

This keeps Android and TUI forms deterministic.

### G1-R1-02 — Load semantics belong to the session exercise

A given movement may be bodyweight, externally loaded, or assisted on different sessions.

Therefore `load_mode` belongs to the session exercise, not permanently to the catalog.

Frozen candidates:

```text
none
external
assistance
```

### G1-R1-03 — Assistance is not resistance

Positive assistance kilograms are stored as positive values.

Analytics must interpret higher assistance as more help, not more strength.

### G1-R1-04 — Actual load is per set

Loaded exercises require `weight_kg` on every performed set.

This permits truthful representation of drop sets, changed machine loads, and failed attempts.

### G1-R1-05 — Failed zero-repetition attempts are representable

Actual `reps` may equal zero.

Target `reps` remain strictly positive.

A skipped set is omitted; zero represents an actual attempt with no completed repetition.

### G1-R1-06 — Planned exercise with no actual sets is representable

`sets` may be empty.

This supports interrupted sessions and exercises planned but never started.

### G1-R1-07 — Session and set order are array order

No redundant serialized ordinal is needed.

Array order is canonical order.

### G1-R1-08 — Measurement list is explicit

The proposed final v1 list is:

- neck;
- shoulders;
- chest;
- waist;
- hips;
- left/right arm;
- left/right forearm;
- left/right thigh;
- left/right calf.

### G1-R1-09 — Notes are optional and bounded

Session notes: maximum 4000 characters.

Exercise notes: maximum 1000 characters.

Whitespace-only notes are semantically invalid.

### G1-R1-10 — Catalog metadata is session-scoped, not full synchronization

The top-level catalog contains exactly the exercise IDs referenced by the exported session.

This prevents an ordinary session import from silently importing unrelated Android catalog entries.

### G1-R1-11 — Existing ID/name mismatch is non-fatal

Stable identity wins over display metadata.

A session may import using an existing ID while surfacing a metadata warning.

Session import does not silently rename the canonical local exercise.

### G1-R1-12 — v1 non-goals are explicit

Distance, cardio telemetry, custom metrics, supersets, machine settings, muscle classification, and measured per-set rest are intentionally deferred.

## Validation

Run:

```bash
python tools/validate_json.py
git diff --check
```

The suite must include valid examples of:

- loaded repetition exercise;
- unweighted timed exercise;
- active session;
- zero-repetition failed attempt;
- assistance load;
- empty interrupted session.

The invalid suite must cover all frozen semantic boundaries.

## Freeze decision

This review is a freeze candidate, not the freeze itself.

After canonical validation, commit, push, and mirrored review, a separate closure patch may set:

```text
GATE_1_REVIEW_01=PASS
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```
