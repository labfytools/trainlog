# Tests and Validation

## 1. Principle

A feature is not complete without its relevant validation.

## 2. Validation layers

Trainlog will use:

- unit tests;
- integration tests;
- JSON Schema validation;
- database constraint tests;
- TUI smoke tests;
- sanitizer builds where practical.

## 3. Exchange-format tests

The repository must contain valid and invalid fixtures.

Valid fixtures must pass the schema.

Invalid fixtures should cover:

- missing required fields;
- duplicate exercise identifiers;
- invalid timestamps;
- invalid negative values;
- empty set data;
- malformed targets;
- unsupported format version.

## 4. Database tests

Tests must verify:

- foreign keys are active;
- duplicate `session_id` is rejected or handled idempotently;
- duplicate `exercise_id` is rejected;
- failed imports roll back completely;
- migrations preserve data.

## 5. C validation

Initial build validation should include:

```text
normal build
strict warning build
ASan/UBSan build
```

Exact commands will be frozen when `meson.build` exists.

## 6. TUI tests

At minimum:

- application starts in a supported terminal;
- small-terminal fallback works;
- navigation does not corrupt state;
- UTF-8 labels render correctly;
- monochrome fallback remains understandable.

## 7. Pre-push checklist

Before a meaningful push:

1. format code;
2. build;
3. run tests;
4. validate JSON fixtures;
5. run sanitizer suite when relevant;
6. run `git diff --check`;
7. inspect `git status`;
8. update documentation.
