# Body observation history and editing

## Status

```text
BODY_OBSERVATION_HISTORY_UI=IMPLEMENTED
BODY_OBSERVATION_EDIT=IMPLEMENTED
BODY_OBSERVATION_SCROLLBAR=IMPLEMENTED
DATABASE_SCHEMA_V2=UNCHANGED
TRAINLOG_FORMAT_V1=FROZEN
```

`F4 Corps` is now record-oriented rather than metric-oriented.

The home view contains:

- a framed body-weight trend graph;
- a newest-first framed observation list;
- one compact summary per observation;
- keyboard scrolling plus PageUp/PageDown;
- a ncurses-drawn vertical scrollbar when history exceeds the visible window.

Enter opens the selected observation.

The detail view has two pages:

1. general measurements;
2. left/right limb measurements.

Both pages expose `e Modifier`.

Editing preserves the observation row identity, timestamp and optional session
link. A field can be kept with Enter, replaced with another positive number, or
cleared using `-`. Escape cancels the entire edit without persistence.

At least one metric must remain present.

No schema migration and no JSON v1 change are required.
