# Dashboard rolling 12 months

## Status

```text
DASHBOARD_12_MONTHS=IMPLEMENTED
```

The dashboard always displays the current calendar month plus the previous
11 months.

Empty months remain explicit empty positions on the X axis.

There is no zero filling and no line interpolation across a missing month.

When multiple observations exist in one month, the last observation of that
month is used for the compact dashboard series.

No persistence or Trainlog JSON v1 changes are required.
