# Gate 1 Review #2 — Exercise Identity Collision Rules

## Status

```text
GATE_1_REVIEW_01=IMPLEMENTED
GATE_1_REVIEW_02=IMPLEMENTED
GATE_1=VALIDATION_PENDING
TRAINLOG_FORMAT_V1=DRAFT
```

## Purpose

Review #1 defined the shape of Trainlog v1.

Review #2 closes the remaining identity ambiguity between the Android exercise catalog and the canonical TUI catalog.

This review is intentionally narrow.

## G1-R2-01 — Generated identifiers must be globally collision-resistant

The serialized format continues to treat `exercise_id` and `session_id` as opaque identifiers satisfying their documented syntax.

Trainlog implementations that create new identifiers must use random UUID version 4 values.

Recommended generated exercise identifier:

```text
ex_550e8400-e29b-41d4-a716-446655440000
```

Recommended generated session identifier:

```text
se_550e8400-e29b-41d4-a716-446655440000
```

The prefixes are part of the generated identifier.

Importers must not require the UUID generation pattern for already-existing conforming v1 documents.

This separates two concerns:

- the wire format accepts the documented opaque identifier syntax;
- official Trainlog implementations generate collision-resistant identifiers.

## G1-R2-02 — Existing ID and compatible metadata

If an incoming exercise has the same `exercise_id` and the same `tracking_mode` as an existing local exercise, the existing identity is reused.

If the normalized display name is also equal:

```text
action = reuse
```

No warning is needed.

## G1-R2-03 — Existing ID with a different display name

If `exercise_id` and `tracking_mode` match but the display name differs:

```text
action = reuse_with_name_warning
```

The session may import.

The local canonical exercise is not silently renamed.

The UI must surface a non-fatal metadata warning.

Deliberate rename/synchronization is a separate user action.

## G1-R2-04 — Existing ID with incompatible tracking mode

If the same `exercise_id` arrives with a different `tracking_mode`:

```text
action = reject_mode_conflict
```

The session import must not continue.

The conflict cannot be repaired by silently changing history because `reps` and `duration` have different semantics.

## G1-R2-05 — Different IDs with equivalent normalized names

If an incoming exercise has a different `exercise_id` but its normalized name equals an existing local exercise name:

```text
action = reject_name_identity_conflict
```

The importer must not:

- create a duplicate exercise;
- silently merge the two identities;
- silently rewrite the incoming session to the existing ID.

The user must explicitly reconcile the identity conflict.

This is the critical anti-duplicate rule for independently edited Android and TUI catalogs.

## G1-R2-06 — New ID and unique normalized name

If neither the identifier nor normalized name conflicts with the local catalog:

```text
action = create
```

The new exercise is inserted during the same import transaction as the session.

## G1-R2-07 — Reconciliation is atomic

Any hard catalog conflict rejects the entire session import transaction.

The TUI must not partially:

- create some new exercises;
- insert the session;
- insert sets.

The database remains unchanged after a rejected import.

## G1-R2-08 — Document validity versus local importability

A Trainlog JSON document can be structurally and semantically valid on its own yet still be non-importable into a particular local database because of a catalog identity conflict.

Therefore v1 has two distinct checks:

```text
document validation
local catalog reconciliation
```

Gate 1 freezes both contracts.

## Executable contract

Canonical reconciliation cases live in:

```text
tests/contract/catalog-import-cases.json
```

Validate them with:

```bash
python tools/validate_import_contract.py
```

This validator is an executable specification used before the production C17 importer exists.

## Freeze decision

After both Gate 1 reviews are validated, committed, pushed, and mirrored, the closure step may set:

```text
GATE_1_REVIEW_01=PASS
GATE_1_REVIEW_02=PASS
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```
