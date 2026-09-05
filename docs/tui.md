# TUI

## 1. Purpose

The Trainlog TUI is the primary history, analysis, and visualization application.

It is implemented in C17 with `ncursesw`.

## 2. Primary screens

Initial screen plan:

- Dashboard;
- Sessions;
- New session;
- Exercises;
- Body;
- Import.

The final key bindings will be frozen before implementation.

## 3. Color

The TUI should be visually rich but remain readable.

Color is used to reinforce meaning, never as the only indicator.

Conceptual roles:

- accent: titles, active selection, highlighted metrics;
- success: completed target;
- warning: partial target or attention state;
- error: invalid input or failed operation;
- muted: secondary information;
- graph series: consistent distinguishable colors.

All color pairs must be centralized in a theme module.

Do not scatter raw `COLOR_*` decisions throughout screens.

## 4. Monochrome fallback

Every meaningful color state must also have a textual or symbolic representation.

Examples:

```text
✓ completed
! warning
x failed
> selected
```

## 5. UTF-8

The TUI uses wide-character ncurses support.

The implementation must initialize locale correctly before ncurses use.

Rendering must be tested with accented French text and common symbols.

## 6. Dashboard

The dashboard should eventually show:

- current body weight;
- recent body-weight change;
- number of sessions in a selected period;
- total training duration;
- recent performance highlights;
- compact terminal graphs.

## 7. New session

The TUI must be able to record a workout directly, using the same logical exercise catalog as Android.

A typical exercise form contains:

- exercise;
- target sets;
- target repetitions or duration;
- target load when relevant;
- rest duration;
- actual performed sets.

## 8. Graphs

Graphs must be terminal-native.

Possible graph types:

- weight trend;
- measurement trend;
- exercise load trend;
- estimated or measured max trend;
- training volume trend.

Graphs should adapt to terminal size.

## 9. Minimum terminal size

A minimum supported terminal size will be defined during the first TUI milestone.

Below the minimum size, Trainlog must display a clear message instead of rendering a broken layout.

## 10. Input safety

The TUI must validate numeric fields before committing data.

Invalid input must not partially modify persistent state.
