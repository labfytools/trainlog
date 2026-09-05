# Coding Style

## 1. General rules

Code must optimize for readability, correctness, and maintainability.

Avoid clever code when a straightforward implementation is easier to verify.

## 2. C standard

The TUI uses C17.

The project will initially target a strict warning profile similar to:

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

Comments are required for:

- public interfaces;
- invariants;
- non-obvious behavior;
- important ownership rules;
- format assumptions;
- error-handling decisions;
- algorithms whose intent is not immediately obvious.

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

The project will use `clang-format`.

A canonical `.clang-format` file will be added before substantial C implementation.

Formatting changes should not be mixed with unrelated semantic changes when avoidable.

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
