# Roadmap

## Gate 0 — Project contract

Status: PASS

Canonical result:

```text
GATE_0=PASS
GATE_0_REVIEW_01=PASS
```

Reviewed hardening commit:

```text
bc54d6b4ce10d098916823b6a79f72b39d9c7703
```

Gate 0 closure commit:

```text
ebd4316ed68c58598a471e567edf13455d00f92b
```

## Gate 1 — Exchange format v1 freeze

Status: VALIDATION PENDING — REVIEW #2

Canonical state:

```text
GATE_1_REVIEW_01=IMPLEMENTED
GATE_1_REVIEW_02=IMPLEMENTED
GATE_1=VALIDATION_PENDING
TRAINLOG_FORMAT_V1=DRAFT
```

Review #1 defined:

- exercise identity and tracking mode;
- session identity and ordering;
- load semantics;
- target versus actual work;
- rest;
- body weight and measurements;
- notes;
- strict document validation.

Review #2 closes:

- official UUIDv4 identifier generation;
- Android/TUI catalog collision handling;
- hard tracking-mode identity conflicts;
- different-ID/same-name anti-duplicate conflicts;
- atomic catalog reconciliation.

Exit criteria:

- `python tools/validate_json.py` passes;
- `python tools/validate_import_contract.py` passes;
- `git diff --check` passes;
- Android documentation aligned;
- TUI documentation aligned;
- both review commits pushed to Forgejo and GitHub;
- mirrored review passes;
- closure sets `TRAINLOG_FORMAT_V1=FROZEN`.

## Gate 2 — TUI persistence core

Deliverables:

- Meson C17 project;
- SQLite open/create;
- schema versioning;
- UUIDv4 identity generation;
- exercise catalog;
- atomic catalog reconciliation;
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
- UUIDv4 identity generation;
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
