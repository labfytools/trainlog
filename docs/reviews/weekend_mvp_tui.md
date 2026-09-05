# Weekend MVP — First Usable TUI

## Objective

Target:

```text
Monday 2026-09-07 05:00 Europe/Paris
```

The project moves temporarily in larger vertical slices.

The first safety milestone is a TUI that can be used without Android.

## Delivered user flow

```text
trainlog
  |
  +-- Dashboard
  |     +-- session count
  |     +-- exercise count
  |     +-- latest weight
  |     +-- compact weight sparkline
  |
  +-- New session
  |     +-- automatic start timestamp
  |     +-- select exercise
  |     +-- reps or duration
  |     +-- load mode
  |     +-- target load
  |     +-- planned sets
  |     +-- actual sets
  |     +-- actual reps/duration per set
  |     +-- actual load per set
  |     +-- planned rest
  |     +-- automatic end timestamp
  |
  +-- History
  |
  +-- Exercise catalog
  |     +-- add exercise
  |     +-- Unicode anti-duplicate normalization
  |
  +-- Body
        +-- weight
        +-- waist
        +-- chest
        +-- shoulders
        +-- left/right arm
        +-- left/right thigh
        +-- left/right calf
```

## Visual contract

The TUI uses centralized color roles:

- accent;
- success;
- warning;
- error;
- muted;
- graph.

Color never replaces textual meaning.

Minimum terminal size remains:

```text
72x20
```

## Persistence

The TUI writes to:

```text
$XDG_DATA_HOME/trainlog/trainlog.db
```

or, when `XDG_DATA_HOME` is unset:

```text
~/.local/share/trainlog/trainlog.db
```

## Explicit MVP limits

The first usable TUI does not yet include:

- Android import;
- JSON export from the TUI;
- session detail editing after save;
- advanced statistics;
- 1RM calculations;
- muscle-group analysis.

These are not forgotten features; they are deliberately below the Monday usability cut.

## Next vertical slice

Immediately after this milestone:

```text
Android recorder
        |
        v
Trainlog JSON v1
        |
        v
C17 import transaction
        |
        v
same SQLite history
```
