# Trainlog Development Contract

## 1. Scope

Trainlog is composed of two applications sharing a versioned exchange format:

- a lightweight Android application for fast workout data entry;
- a Unix/Linux TUI for storage, review, analysis, and visualization.

The TUI SQLite database is the canonical long-term history.

JSON files are the exchange contract between Android and the TUI.

## 2. General development rules

Every change must respect the following rules:

1. behavior is defined before implementation;
2. code must be readable and deterministic;
3. errors must be handled explicitly;
4. user data must never be silently discarded;
5. persistent formats must be versioned;
6. importing the same data repeatedly must not create duplicates;
7. every new feature must be documented;
8. affected tests must be added or updated;
9. compiler warnings are treated as defects unless explicitly justified;
10. an undocumented or untested feature is not considered complete.

## 3. TUI

The TUI is implemented in C17.

Planned dependencies:

- ncursesw;
- SQLite3;
- a deliberately selected JSON library;
- Meson;
- Ninja.

The business logic, persistence layer, and ncurses rendering layer must remain separated.

SQLite calls must not be scattered through rendering code.

Important business rules must not depend directly on ncurses.

## 4. Android

The Android application is a data-entry client.

It must remain intentionally simple and must not become the primary analytics or historical store.

It must support:

- starting a workout session;
- automatic recording of the start time;
- selecting or creating an exercise;
- entering planned sets and repetitions;
- entering actual sets and repetitions;
- entering load;
- entering planned rest time;
- entering body weight and supported measurements;
- automatic recording of the end time;
- exporting a valid Trainlog JSON file.

## 5. Documentation

Documentation is mandatory.

Primary documents:

- `README.md`: user-facing project overview;
- `docs/architecture.md`: architecture and component boundaries;
- `docs/coding_style.md`: coding and commenting conventions;
- `docs/exchange_format.md`: JSON exchange contract;
- `docs/database.md`: SQLite schema and migration policy;
- `docs/tui.md`: TUI behavior and visual rules;
- `docs/android.md`: Android behavior and scope;
- `docs/tests.md`: validation strategy and commands;
- `docs/roadmap.md`: implementation order and gates.

A behavior change must update the relevant documentation in the same change.

## 6. Code comments

Comments are mandatory when code expresses:

- an invariant;
- a format constraint;
- an architectural decision;
- non-obvious logic;
- special error handling;
- a public API;
- an important data structure;
- an assumption required for correctness.

Comments must not merely restate obvious code.

Prefer explaining why a decision exists when the reason is not obvious from the code.

## 7. Exchange format

The Trainlog format is versioned.

Each workout export must contain:

- a format identifier;
- a schema version;
- a unique session identifier;
- ISO 8601 timestamps including an explicit UTC offset.

Exercise identifiers are stable and permanent.

An exercise display name may change without changing its identifier.

Imports must be idempotent.

A published format version must never receive an incompatible semantic change.

## 8. TUI visual rules

The TUI uses color when it improves understanding.

Color must never be the sole carrier of information.

Important states must remain understandable in monochrome terminals.

Colors must be centralized in a dedicated theme module.

The TUI must use `ncursesw` and handle UTF-8 correctly.

Raw ANSI escape sequences are forbidden in ncurses rendering code unless explicitly documented and justified.

## 9. Database

SQLite is the canonical TUI store.

The database schema must be versioned.

Incompatible schema evolution requires an explicit migration.

Integrity constraints must be used where appropriate, including:

- foreign keys;
- unique identifiers;
- anti-duplication constraints.

## 10. Validation

Before every meaningful push:

- build;
- run tests;
- verify formatting;
- verify compiler warnings;
- validate JSON examples against the schema;
- verify documentation impacted by the change.

The repository must not knowingly be pushed in a broken state.

## 11. Git workflow

Forgejo is the primary repository.

Primary remote:

`ssh://git@git.labfytools.com:2223/fy59/trainlog.git`

GitHub is a mirror:

`git@github.com:labfytools/trainlog.git`

Normal development must push to Forgejo.

Do not develop directly against the GitHub mirror.

## 12. Definition of Done

A task is complete only when:

- the expected behavior is implemented;
- the code builds without accepted warnings;
- relevant tests pass;
- new error paths are handled;
- documentation is current;
- examples and schemas are updated when required;
- no known regression is intentionally left behind.
