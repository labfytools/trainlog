# Roadmap

## Gate 0 — Project contract

Status: PASS

## Gate 1 — Exchange format v1 freeze

Status: PASS

```text
TRAINLOG_FORMAT_V1=FROZEN
```

## Gate 2 — Persistence + usable TUI

Status: IN PROGRESS

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
GATE_2=IN_PROGRESS
```

Current TUI capabilities:

- direct workout entry;
- exercise catalog;
- body tracking;
- weight graph;
- colored dashboard;
- arrow/F-key navigation;
- navigable history;
- Unicode anti-duplicate exercise names.

Next TUI polish blocks:

1. full session detail;
2. exercise performance history and graphs;
3. previous-session defaults;
4. safe edit/delete flows.

Android remains deferred until the TUI daily workflow is satisfactory.

<!-- TRAINLOG_TUI_V02_ROADMAP -->
## TUI v0.2 checkpoint

Current state:

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
TUI_SESSION_DETAILS=IMPLEMENTED
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
GATE_2=IN_PROGRESS
TRAINLOG_FORMAT_V1=FROZEN
```

Completed before this checkpoint:

- C17/Meson persistence core;
- SQLite schema v1 foundation;
- UUIDv4 generation;
- Unicode catalog normalization;
- direct workout recording;
- direct body observation recording;
- colored ncursesw dashboard;
- keyboard navigation;
- body-weight graph;
- flat-series graph rendering;
- navigable workout history;
- full read-only session detail.

Next implementation slice:

1. shared duration parser accepting seconds and minute-oriented syntax;
2. shared human duration formatter;
3. generic body-metric history query;
4. F4 metric selector;
5. graphs for weight and every body measurement;
6. left/right asymmetry presentation.

No incompatible change to frozen Trainlog JSON v1 is required.
<!-- TRAINLOG_TUI_V02_ROADMAP _END -->

<!-- TRAINLOG_GLOBAL_BODY_GRAPH_ROADMAP -->
## Next TUI visualization slice

Canonical state after the current checkpoint:

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
TUI_SESSION_DETAILS=IMPLEMENTED
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
TUI_GLOBAL_BODY_OVERLAY=NEXT
DASHBOARD_GRAPH_V2=NEXT
GATE_2=IN_PROGRESS
TRAINLOG_FORMAT_V1=FROZEN
```

Next deliverables:
1. normalized global body graph in `F4`;
2. color + symbol identity for every overlaid metric;
3. global percent-change summary;
4. richer home weight graph;
5. previous-measurement delta on dashboard;
6. dashboard min/max weight;
7. latest waist summary when available;
8. compact asymmetry warning when relevant.

No database schema migration is expected.
No Trainlog JSON v1 change is expected.
<!-- TRAINLOG_GLOBAL_BODY_GRAPH_ROADMAP _END -->
