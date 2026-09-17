# Coding Style

## 1. General rules

Code must optimize for readability, correctness, and maintainability.

Avoid clever code when a straightforward implementation is easier to verify.

These rules apply equally to production code, tests, I/O doubles, migration
bridges, and repository tooling. Code must not be manually compacted to save
lines or prompt tokens. A grouped mission remains compatible with small,
reviewable commits.

## 2. C standard

The TUI uses C17.

Meson enforces C17, warning level 3 and `werror=true`. Desktop targets also use:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wshadow
-Wformat=2
```

Warnings must be reviewed individually.

Do not disable a warning globally merely to hide one inconvenient case.

## 3. Naming

Public symbols use a `trainlog_` prefix.

Examples:

```c
trainlog_database_open(...)
trainlog_session_import(...)
```

Internal static functions use descriptive snake_case names.

Types use descriptive names and avoid unnecessary abbreviations.

## 4. Functions

Functions should:

- have one clear purpose;
- validate arguments when required by their contract;
- return explicit status values;
- avoid hidden global state.

Prefer small composable functions over long functions mixing unrelated responsibilities.

## 5. Comments

New or modified source comments are written in English. Exact external text,
protocol literals, UI strings, user data, and historical quotations retain
their required language.

Comments are required for:

- public interfaces;
- invariants;
- non-obvious behavior;
- important ownership rules;
- format assumptions;
- error-handling decisions;
- algorithms whose intent is not immediately obvious.

For synchronization, persistence, stable identity, serialization, ownership,
ABI boundaries, concurrency, and scientific interpretation, comments identify
the relevant intent explicitly as `WHY`, `CONTRACT`, or `INVARIANT`. They belong
in the same change as the behavior and explain the non-obvious rule rather than
restating syntax.

Bad:

```c
i++; /* Increment i. */
```

Good:

```c
/*
 * Keep the external exercise identifier unchanged after creation.
 * Historical session rows refer to this stable identifier even when
 * the user later renames the exercise.
 */
```

## 6. Formatting

The repository `.clang-format` defines the C presentation used by normalized
files. It targets 100 columns, four-space indentation, one statement per line,
explicit control-flow braces, vertical long parameter lists, and no short
function or control body on one line. Contract literals may exceed the target
when splitting them would make the protocol less reviewable.

Do not apply the formatter blindly to the entire historical tree. A change
normalizes the files it owns and checks that explicit scope. The reproducible,
non-mutating check for the currently normalized C readability scope is:

```bash
clang-format --dry-run --Werror \
  tui/src/dashboard_layout.c \
  tui/src/generation_mtp.c \
  tui/src/json_writer.c \
  tui/src/web_dashboard.c \
  tui/src/web_dashboard_json.c \
  tui/include/trainlog/generation_mtp.h \
  tui/tools/generation_mtp_adapter.c \
  tui/tests/generation_mtp_adapter_double.c \
  tui/tests/test_generation_mtp.c \
  tui/tests/test_web_server.c
```

Formatting changes should not be mixed with unrelated semantic changes when
avoidable. Review readability explicitly before the final commit, including
tests and cleanup/error paths.

## 7. Error handling

Do not silently ignore errors.

Do not use process termination for normal recoverable library errors.

Return explicit status codes from internal APIs where practical.

User-facing errors must be understandable and actionable.

## 8. Ownership

Every API that allocates, borrows, or transfers ownership must document that behavior.

Do not rely on ambiguous lifetime assumptions.

## 9. Integer and conversion safety

Avoid implicit narrowing conversions.

Validate external numeric input before conversion.

Use types appropriate to the stored range and document any format bounds.

## 10. Kotlin

The Android side should favor:

- immutable data where practical;
- explicit state transitions;
- small view models;
- no unnecessary framework abstraction;
- clear serialization types matching the Trainlog JSON contract.

Python and Kotlin follow the same human-readability requirement: avoid
single-line compound statements, compressed exception handling, and opaque
chains of unrelated mutations. Formatters are aids, not substitutes for review.
