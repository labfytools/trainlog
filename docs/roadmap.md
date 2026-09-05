# Roadmap

## Gate 0 — Project contract

Status: PASS

Canonical result:

```text
GATE_0=PASS
GATE_0_REVIEW_01=PASS
```

Gate 0 established:

- repository structure;
- development contract;
- architecture documentation;
- coding-style rules;
- exchange-format draft;
- structural and semantic validation;
- positive and negative fixture strategy.

Reviewed hardening commit:

```text
bc54d6b4ce10d098916823b6a79f72b39d9c7703
```

Gate 0 closure commit:

```text
ebd4316ed68c58598a471e567edf13455d00f92b
```

## Gate 1 — Exchange format v1 freeze

Status: VALIDATION PENDING — REVIEW #1

Canonical state:

```text
GATE_1_REVIEW_01=IMPLEMENTED
GATE_1=VALIDATION_PENDING
TRAINLOG_FORMAT_V1=DRAFT
```

Review #1 freezes the proposed model for:

- exercise identity;
- stable repetition/duration tracking mode;
- session identity;
- session ordering;
- external/none/assistance load semantics;
- planned versus actual sets;
- zero-repetition failed attempts;
- planned exercises with zero actual sets;
- planned rest;
- body weight;
- final v1 body-measurement field list;
- optional notes;
- strict catalog completeness;
- strict unknown-field behavior.

Exit criteria:

- all valid fixtures accepted;
- all invalid fixtures rejected for the intended reason;
- Android documentation aligned;
- TUI documentation aligned;
- schema and semantic validator aligned;
- review commit pushed to Forgejo and GitHub;
- mirrored review passes;
- `TRAINLOG_FORMAT_V1=FROZEN`.

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
