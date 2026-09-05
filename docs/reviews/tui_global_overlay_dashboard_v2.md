# Global Body Overlay and Dashboard v2

## Status

```text
TUI_GLOBAL_BODY_OVERLAY=IMPLEMENTED
DASHBOARD_GRAPH_V2=IMPLEMENTED
TRAINLOG_FORMAT_V1=FROZEN
```

`g` in F4 toggles a global body view. Every metric is normalized so its first
real observation equals 100, which makes kg and cm trends comparable without
changing stored values. Series are aligned by timestamp, use distinct symbols,
cycle theme colors, and show `#` on overlapping terminal cells.

The dashboard now shows current weight, previous and total deltas, min/max,
latest waist when available, the largest recent same-observation G/D
difference, and a full-width recent weight graph whose latest point is `O`.

No database migration and no JSON v1 change are required.
