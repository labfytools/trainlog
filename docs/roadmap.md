# Roadmap

## Gate 0 — Project contract

Status: PASS

```text
GATE_0=PASS
```

## Gate 1 — Exchange format v1 freeze

Status: PASS

```text
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

## Gate 2 / Weekend MVP — Usable persistence + direct entry

Status: IN PROGRESS

Current vertical slice:

```text
WEEKEND_MVP_TUI=IMPLEMENTED
GATE_2=IN_PROGRESS
DATABASE_SCHEMA_V1=DRAFT
```

Delivered in this slice:

- SQLite persistence foundation;
- UUIDv4 creation;
- frozen Unicode exercise-name normalization;
- direct exercise creation;
- direct session creation;
- automatic start/end timestamps;
- reps/duration tracking;
- none/external/assistance load modes;
- actual set recording;
- planned rest;
- standalone body measurements;
- session history;
- dashboard;
- colored ncursesw interface;
- weight sparkline.

Validation required before push:

```bash
python tools/validate_json.py
python tools/validate_import_contract.py

CC=clang meson setup build
meson compile -C build
meson test -C build --print-errorlogs

CC=clang meson setup build-asan \
  -Db_sanitize=address,undefined \
  -Db_lundef=false
meson compile -C build-asan
meson test -C build-asan --print-errorlogs

git diff --check
```

Next vertical slice:

- Android v0.1 recorder;
- JSON v1 export;
- C17 JSON importer;
- catalog reconciliation;
- idempotent import.

Gate 2 remains open until import transactions and idempotency are complete.

## Gate 3 — TUI polish

The minimal colored TUI has been pulled forward for the Monday usability target.

Gate 3 later adds:

- richer navigation;
- session details;
- editing;
- more graphs;
- advanced layout polish.

## Gate 4 — Android recorder

Pulled forward immediately after Weekend MVP TUI.

## Gate 5 — Analytics

- body-weight trends;
- measurement trends;
- exercise performance;
- volume;
- max/estimated max;
- balance analysis.

## Gate 6 — Hardening

- migrations;
- packaging;
- broader tests;
- first tagged release.
