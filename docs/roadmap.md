# Roadmap

## Gate 0 — Project contract

Status: IN PROGRESS

Deliverables:

- repository structure;
- development contract;
- architecture documentation;
- coding-style documentation;
- exchange-format v1 draft;
- JSON Schema draft;
- valid example fixture.

Exit criteria:

- documentation reviewed;
- JSON example validates against the schema;
- repository clean after commit.

## Gate 1 — Exchange format v1 freeze

Deliverables:

- complete field list;
- exercise identity rules;
- session identity rules;
- repetitions and timed-set representation;
- rest representation;
- body weight;
- measurement list;
- unknown-field policy;
- invalid fixture suite.

Exit criteria:

- `TRAINLOG_FORMAT_V1=FROZEN`;
- schema tests pass;
- Android and TUI can implement against the contract without ambiguity.

## Gate 2 — TUI persistence core

Deliverables:

- Meson C17 project;
- SQLite open/create;
- schema versioning;
- exercise catalog;
- session import transaction;
- idempotent import tests.

Exit criteria:

- database tests pass;
- sanitizer validation passes.

## Gate 3 — Minimal TUI

Deliverables:

- ncursesw initialization;
- theme module;
- dashboard shell;
- exercise list;
- session history;
- import screen;
- minimum-terminal fallback.

Exit criteria:

- usable color TUI;
- monochrome fallback;
- UTF-8 test pass.

## Gate 4 — Android recorder

Deliverables:

- local exercise catalog;
- start/stop session timestamps;
- target entry;
- actual-set entry;
- rest entry;
- body data;
- JSON export.

Exit criteria:

- exported fixture validates against frozen v1;
- exported file imports successfully into the TUI.

## Gate 5 — Analytics

Deliverables:

- body-weight trend;
- measurement trend;
- exercise performance trend;
- training volume summaries;
- terminal graphs.

## Gate 6 — Hardening

Deliverables:

- broader fixture coverage;
- migrations;
- import/export robustness;
- documentation cleanup;
- packaging;
- first tagged release.
