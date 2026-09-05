# Tests and Validation

## 1. Principle

A feature is not complete without relevant validation.

The exchange-format validator is part of the executable contract during early development.

## 2. Validation layers

Trainlog uses or will use:

- JSON Schema validation;
- Trainlog semantic validation;
- unit tests;
- integration tests;
- database constraint tests;
- TUI smoke tests;
- sanitizer builds where practical.

## 3. Exchange-format validation

Run the canonical suite with:

```bash
python tools/validate_json.py
```

The command validates:

- `examples/session-v1.json`;
- every file in `tests/fixtures/valid/` as valid;
- every file in `tests/fixtures/invalid/` as invalid.

A negative fixture passes only when validation rejects it.

## 4. Structural versus semantic validation

JSON Schema validates structure and primitive bounds.

`tools/validate_json.py` additionally validates rules JSON Schema cannot safely express, including:

- unique `exercise_id` values;
- normalized display-name uniqueness;
- catalog-reference integrity;
- one workout entry per exercise;
- target/actual mode consistency;
- explicit timestamp offsets;
- end-time chronology.

Android export and TUI import must eventually implement the same semantic rules.

## 5. Initial invalid fixture coverage

The Gate 0 suite covers:

- duplicate exercise identifiers;
- duplicate normalized exercise names;
- unknown exercise references;
- duplicate workout exercise entries;
- end timestamp before start timestamp;
- offset-less timestamp;
- target/actual mode mismatch;
- target containing both repetitions and duration;
- unknown JSON field.

## 6. Database tests

Future tests must verify:

- foreign keys are active;
- duplicate `session_id` is rejected or handled idempotently;
- duplicate `exercise_id` is rejected;
- failed imports roll back completely;
- migrations preserve data.

## 7. C validation

Initial C validation will include:

```text
normal build
strict warning build
ASan/UBSan build
```

Exact commands will be frozen when `meson.build` exists.

## 8. TUI tests

At minimum:

- application starts in a supported terminal;
- small-terminal fallback works;
- navigation does not corrupt state;
- UTF-8 labels render correctly;
- color roles render correctly;
- monochrome fallback remains understandable.

## 9. Pre-push checklist

Before a meaningful push:

1. format code;
2. run `python tools/validate_json.py`;
3. build when buildable code exists;
4. run relevant tests;
5. run sanitizer suite when relevant;
6. run `git diff --check`;
7. inspect `git status --short`;
8. update documentation.
