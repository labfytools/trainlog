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

Status: PASS

Canonical result:

```text
GATE_1_REVIEW_01=PASS
GATE_1_REVIEW_02=PASS
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

Reviewed commits:

```text
9d9a9223a0c46df72f5c3ab208107c0ac6698438
dfd6717cb7978d009670f1a49029c62c9154af55
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

Exit criteria satisfied before closure:

- `python tools/validate_json.py` passes;
- `python tools/validate_import_contract.py` passes;
- `git diff --check` passes;
- Android documentation aligned;
- TUI documentation aligned;
- both review commits pushed to Forgejo and GitHub;
- GitHub mirror read back and reviewed.

Gate 1 is closed.

Gate 2 is now the active gate.

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
