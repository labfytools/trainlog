# Human Duration and Body Graph Checkpoint

## Status

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
TUI_SESSION_DETAILS=IMPLEMENTED
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
GATE_2=IN_PROGRESS
TRAINLOG_FORMAT_V1=FROZEN
```

## Implemented

### Human duration entry

Accepted examples:

```text
90
90s
1:30
1m30
1m30s
2m
45s
2:05
```

Persistence remains canonical seconds.
Display uses human minute/second formatting.

### Body tracking

`F4 Corps` now provides per-metric history for weight, neck, shoulders, chest,
waist, hips, left/right arm, left/right forearm, left/right thigh and
left/right calf.

Each metric has latest value, first value, change, graph and recent dated
values. Paired limbs expose same-observation asymmetry.

## Next visualization contract

### Global body graph

All available body series are overlaid after normalizing each metric's first
real value to 100. The global graph is display-only.

### Dashboard graph v2

The home screen remains compact but gains:
- current weight;
- first-to-current delta;
- previous-to-current delta;
- min/max;
- richer recent weight graph;
- latest waist;
- compact asymmetry status.

## Validation before push

```bash
meson compile -C build
meson test -C build --print-errorlogs
python tools/validate_json.py
python tools/validate_import_contract.py
git diff --check
git diff --cached --check
```

Sanitizer validation:

```bash
meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

## Next

```text
TUI_GLOBAL_BODY_OVERLAY
DASHBOARD_GRAPH_V2
```
