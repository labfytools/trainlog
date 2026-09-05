# TUI v0.2 Checkpoint

## Status

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
TUI_SESSION_DETAILS=IMPLEMENTED
GATE_2=IN_PROGRESS
TRAINLOG_FORMAT_V1=FROZEN
```

## Purpose

This checkpoint records the first Trainlog interface that is already usable as
a local workout journal while development continues.

Android is intentionally not required for this checkpoint.

## Implemented user-facing functionality

### Dashboard

- colored centralized theme;
- arrow-key navigation;
- F1/F2/F3/F4 shortcuts;
- session and exercise counts;
- latest weight;
- weight delta;
- terminal weight graph;
- sensible flat-series rendering.

### Workout entry

- exercise selection;
- repetitions or timed exercises;
- none/external/assistance load modes;
- target sets;
- target repetitions/duration;
- target load;
- planned rest;
- performed sets;
- actual per-set load;
- automatic start/end timestamps.

### Workout history

- navigable session list;
- complete session detail;
- exercise-by-exercise browsing;
- target versus actual work;
- ordered actual-set summary.

### Body

- body observation persistence;
- weight entry;
- measurement entry;
- initial weight visualization.

## Next frozen implementation contract

### Human durations

Accepted forms:

```text
90
90s
1:30
1m30
1m30s
2m
```

Storage remains seconds.

Display becomes human-readable minutes and seconds.

### F4 Body graphs

Every stored body metric becomes selectable and graphable.

Paired measurements additionally display left/right asymmetry.

## Validation required before checkpoint push

```bash
meson compile -C build
meson test -C build --print-errorlogs

python tools/validate_json.py
python tools/validate_import_contract.py

git diff --check
git diff --cached --check
```

The sanitizer build remains required when the current `build-asan` directory
matches the latest source tree:

```bash
meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

## Next checkpoint

```text
TUI_DURATION_HUMAN_INPUT
TUI_BODY_METRIC_GRAPHS
```
