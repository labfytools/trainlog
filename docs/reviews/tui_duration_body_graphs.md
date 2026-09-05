# Human Durations and Body Metric Graphs

## Status

```text
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
TRAINLOG_FORMAT_V1=FROZEN
```

Durations accept `90`, `90s`, `1:30`, `1m30`, `1m30s`, `2m`, `45s`
and `2:05`, while persistence remains integer seconds.

F4 now browses all frozen body measurements with left/right navigation,
latest/first/change summaries, a graph, recent values, and same-observation
left/right asymmetry for arms, forearms, thighs, and calves.

No database migration and no JSON v1 change are required.
