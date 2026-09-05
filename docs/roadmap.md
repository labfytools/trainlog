# Roadmap

## Gate 0 — Project contract

Status: PASS

Canonical result:

```text
GATE_0=PASS
GATE_0_REVIEW_01=PASS
```

Deliverables:

- repository structure;
- development contract;
- architecture documentation;
- coding-style documentation;
- exchange-format v1 draft;
- JSON Schema draft;
- valid example fixture;
- semantic validator;
- positive and negative fixture suite;
- Gate 0 review report.

Exit criteria:

- documentation reviewed;
- JSON example validates;
- valid fixtures are accepted;
- invalid fixtures are rejected;
- semantic invariants are documented;
- repository clean after commit.

Gate 0 passed after:

- canonical local fixture validation succeeded;
- `git diff --check` succeeded;
- review commit `bc54d6b4ce10d098916823b6a79f72b39d9c7703` was pushed;
- the GitHub mirror was independently read back and reviewed.

Gate 1 is now the active gate.

## Gate 1 — Exchange format v1 freeze

Deliverables:

- complete field list;
- exercise identity rules;
- session identity rules;
- repetitions and timed-set representation;
- rest representation;
- body weight;
- measurement list;
- strict unknown-field policy;
- valid fixture suite;
- invalid fixture suite;
- stable semantic-validation contract.

Exit criteria:

- `TRAINLOG_FORMAT_V1=FROZEN`;
- schema tests pass;
- semantic tests pass;
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
- centralized color theme module;
- dashboard shell;
- exercise list;
- session history;
- direct session entry;
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
