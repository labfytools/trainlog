# Tests and Validation

## 1. Principle

A feature is not complete without relevant validation.

The exchange validator is executable specification during the format gates.

## 2. Canonical exchange validation

Run:

```bash
python tools/validate_json.py
```

The command validates:

- `examples/session-v1.json`;
- all `tests/fixtures/valid/*.json` as valid;
- all `tests/fixtures/invalid/*.json` as invalid.

A negative fixture passes only when Trainlog rejects it.

## 3. Validation layers

A v1 document must pass:

1. JSON Schema validation;
2. Trainlog semantic validation.

Schema handles shape, enumerations, and primitive ranges.

Semantic validation handles cross-object and normalized rules.

## 4. Gate 1 semantic coverage

The canonical validator checks:

- unique `exercise_id`;
- normalized exercise-name uniqueness;
- explicit timestamp offsets;
- end time later than start time;
- exact catalog/reference set equality;
- one workout entry per exercise;
- catalog tracking mode matching target;
- catalog tracking mode matching actual sets;
- load-mode/weight consistency;
- non-blank notes.

## 5. Positive fixture coverage

Gate 1 includes:

- mixed loaded repetition + timed session;
- active session without `ended_at`;
- planned exercise with zero actual sets;
- bodyweight exercise with zero-repetition failed attempt;
- assistance load;
- completely interrupted session with zero exercises.

## 6. Negative fixture coverage

Gate 1 includes rejection of:

- duplicate exercise IDs;
- duplicate normalized exercise names;
- duplicate workout exercise entries;
- end timestamp before start;
- missing timestamp offset;
- target/actual tracking mismatch;
- target containing both repetitions and duration;
- unknown exercise reference;
- unknown JSON field;
- catalog tracking-mode mismatch;
- `load_mode=none` carrying weight;
- loaded target missing weight;
- loaded actual set missing weight;
- unreferenced catalog entries;
- blank session notes;
- blank exercise notes;
- negative actual repetitions.

## 7. Future database validation

Gate 2 must verify:

- foreign keys enabled;
- duplicate `session_id` idempotency;
- duplicate `exercise_id` barrier;
- transactional rollback;
- schema migration correctness.

## 8. Future C validation

C implementation gates will include:

- normal build;
- strict-warning build;
- ASan/UBSan build;
- formatter check;
- relevant unit/integration tests.

## 9. Pre-push checklist

Before every meaningful push:

1. run `python tools/validate_json.py`;
2. run relevant compiled tests when available;
3. run sanitizers when relevant;
4. run `git diff --check`;
5. inspect `git status --short`;
6. review documentation changes.
## 10. Catalog reconciliation contract

Before the C17 importer exists, Gate 1 defines local catalog merge behavior through an executable Python specification.

Run:

```bash
python tools/validate_import_contract.py
```

Canonical cases cover:

- exact existing exercise reuse;
- same identity with renamed display text;
- same identity with incompatible tracking mode;
- different identities with equivalent normalized names;
- new unique exercise creation.

The full Gate 1 validation command is:

```bash
python tools/validate_json.py
python tools/validate_import_contract.py
git diff --check
```
