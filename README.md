# Trainlog

Trainlog is a local-first workout log composed of:

- a lightweight Android data-entry application;
- a colorful Unix/Linux TUI for history, statistics, graphs, and progress tracking.

## Project goals

Android is optimized for fast use during a workout:

- start and end time;
- exercise selection;
- new exercise creation;
- planned sets and repetitions;
- actual sets and repetitions;
- load;
- rest time;
- body weight;
- body measurements;
- JSON export.

The TUI is the main application:

- import Android exports;
- create sessions directly from the terminal;
- maintain the canonical exercise catalog;
- maintain SQLite history;
- display workout history;
- track body weight;
- track body measurements;
- track performance;
- display colorful terminal graphs;
- export data for external use.

## Architecture

```text
Android
   |
   | Trainlog JSON
   v
Trainlog TUI
   |
   v
SQLite
```

The TUI SQLite database is the canonical long-term store.

The JSON exchange format is versioned and designed for idempotent imports.

## Repository layout

```text
android/        Android application
tui/            C17 ncursesw application
docs/           Canonical project documentation
format/         JSON schema and exchange-format material
examples/       Valid exchange examples
tools/          Development and validation tools
```

## Development principles

- local-first;
- no mandatory cloud account;
- user-owned data;
- versioned persistent formats;
- stable exercise identifiers;
- idempotent imports;
- documentation and tests are part of every feature;
- clear separation between UI, business logic, and persistence.

See `AGENTS.md` for the development contract.
