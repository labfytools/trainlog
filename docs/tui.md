# TUI

## 1. Purpose

The Trainlog TUI is the primary local history, analysis, and visualization application.

It is implemented in C17 with `ncursesw`.

## 2. Primary screens

Initial screen plan:

- Dashboard;
- Sessions;
- New session;
- Exercises;
- Body;
- Import.

## 3. Color

The TUI is intentionally colorful.

Color reinforces meaning but is never the sole indicator.

Conceptual roles:

- accent: titles and current selection;
- success: completed target;
- warning: partial target or attention state;
- error: invalid input or failed operation;
- muted: secondary information;
- graph series: consistent distinguishable colors.

All color pairs must be centralized in a dedicated theme module.

Raw screen code must not scatter `COLOR_*` decisions.

## 4. Monochrome fallback

Meaningful states also use text or symbols.

Examples:

```text
✓ completed
! warning
x failed
> selected
```

## 5. UTF-8 and exercise-name normalization

The TUI uses wide-character ncurses support and initializes locale before ncurses.

Trainlog v1 duplicate-name validation requires Unicode NFC normalization, whitespace normalization, and Unicode case folding.

The C implementation must use a tested Unicode library or equivalent implementation that reproduces the v1 contract exactly.

A likely implementation dependency is `utf8proc`; the final dependency choice is frozen before the relevant C module is implemented.

## 6. Exercise catalog

Each exercise stores:

- stable `exercise_id`;
- mutable display name;
- stable `tracking_mode` (`reps` or `duration`).

The TUI uses this metadata to select the correct data-entry control.

A session import may introduce a previously unknown exercise.

If an existing ID arrives with a different display name, the session may import but the TUI must surface a metadata warning and must not silently rename the canonical local exercise.

## 7. New session

The TUI can record a workout directly using the same logical exercise model as Android.

Per exercise:

- exercise;
- load mode;
- target sets;
- target repetitions or duration;
- target load when applicable;
- planned rest;
- actual sets;
- optional note.

## 8. Load semantics

The TUI must distinguish:

- no separate load;
- external resistance;
- assistance.

Analytics must not rank assistance as though more assistance represented more strength.

Machine-displayed kilograms are stored faithfully but must not be presented as exact cross-machine mechanical equivalence.

## 9. Dashboard

The dashboard should eventually show:

- current body weight;
- recent weight change;
- sessions in a selected period;
- total training duration;
- recent performance highlights;
- compact terminal graphs.

## 10. Body tracking

The TUI database may store standalone body observations independently from workout imports.

The session exchange format can also attach weight and measurements to one session timestamp.

Body-trend graphs operate on the canonical database representation, not directly on raw JSON files.

## 11. Graphs

Terminal-native graph targets include:

- body-weight trend;
- measurement trend;
- external-load trend;
- measured or estimated maximum trend;
- training-volume trend.

Assistance exercises require direction-aware analytics.

## 12. Minimum terminal size

A minimum supported terminal size will be defined during the first TUI milestone.

Below that size, Trainlog displays a clear fallback message rather than a corrupted layout.

## 13. Input safety

Numeric input is validated before persistent state is committed.

Invalid input must never partially mutate a saved session.

Imports use full validation before the database transaction commits.
## 14. Catalog reconciliation during import

Before creating any exercise or session rows, the TUI classifies incoming exercise metadata against the canonical local catalog.

Rules:

```text
same ID + same mode + same normalized name
    -> reuse

same ID + same mode + different name
    -> reuse + metadata warning

same ID + different mode
    -> reject entire import

different ID + same normalized name
    -> reject entire import

new ID + unique normalized name
    -> create inside import transaction
```

The TUI must never silently merge different exercise IDs merely because names match.

The TUI must never create two identities with equivalent normalized display names.

Any hard catalog conflict aborts the complete session import transaction.

## 15. Generated identifiers

When the TUI creates a new exercise directly, it generates:

```text
ex_<random UUID v4>
```

When the TUI creates a new session directly, it generates:

```text
se_<random UUID v4>
```

The database stores these identifiers as opaque stable text.

<!-- TRAINLOG_TUI_V02_CURRENT_AND_NEXT -->
## 16. Current usable TUI checkpoint

The first usable ncurses interface is implemented.

Current daily-use flow:

```text
Dashboard
  -> New session
  -> History
  -> Exercises
  -> Body
```

Implemented interaction:

- colored centralized theme;
- bordered screens;
- arrow-key navigation;
- `Enter` activation;
- `F1` new session;
- `F2` history;
- `F3` exercises;
- `F4` body;
- `q` quit;
- minimum terminal fallback at `72x20`.

The dashboard shows:

- session count;
- exercise count;
- latest body weight;
- recorded weight delta;
- terminal-native body-weight graph.

A flat weight history with only one distinct value renders one centered axis
value instead of repeating the same minimum and maximum label.

## 17. Session history and detail

History is navigable with the keyboard.

Selecting a workout and pressing `Enter` opens a read-only detail view.

For every exercise the detail view exposes:

- exercise name;
- repetition or duration tracking mode;
- load mode;
- planned rest;
- planned number of sets;
- planned repetitions or duration;
- target load when applicable;
- actual set count;
- ordered performed sets and their actual loads.

Example:

```text
Presse à cuisses

Mode   : répétitions
Charge : externe
Repos  : 1 min

Cible  : 4 séries × 5 reps
Charge cible : 80.0 kg

Réalisé:
5@80.0 / 5@80.0 / 5@80.0 / 3@80.0
```

Inside one workout, left/right or up/down changes the selected exercise.

Assistance remains direction-aware: more assistance kilograms mean more help,
not greater strength.

## 18. Duration input and display — implemented

Persistent duration and rest units remain **seconds**.

No SQLite schema or Trainlog JSON v1 change is required.

The TUI parser will accept these equivalent forms:

```text
90
90s
1:30
1m30
1m30s
```

All represent 90 seconds.

Additional examples:

```text
2m    -> 120 seconds
45s   -> 45 seconds
2:05  -> 125 seconds
```

A bare integer remains seconds for fast backward-compatible entry.

Canonical display formatting:

```text
45 seconds  -> 45 s
60 seconds  -> 1 min
90 seconds  -> 1 min 30 s
120 seconds -> 2 min
125 seconds -> 2 min 5 s
```

The same parser and formatter are reused for:

- timed exercise targets;
- timed actual sets;
- planned rest.

Invalid malformed forms are rejected before persistence.

## 19. Body screen and measurement graphs — implemented

`F4 Corps` becomes a full history and visualization screen.

Canonical selectable metrics:

```text
body_weight_kg
neck_cm
shoulders_cm
chest_cm
waist_cm
hips_cm
left_arm_cm
right_arm_cm
left_forearm_cm
right_forearm_cm
left_thigh_cm
right_thigh_cm
left_calf_cm
right_calf_cm
```

The screen supports left/right navigation between metrics.

For the selected metric it shows:

- latest value;
- first recorded value;
- absolute change;
- terminal-native history graph;
- recent dated values.

Units:

```text
body weight  -> kg
measurements -> cm
```

Paired measurements also expose asymmetry:

```text
left arm  : 34.2 cm
right arm : 34.8 cm
difference: 0.6 cm right
```

Relevant pairs:

- arm;
- forearm;
- thigh;
- calf.

The graph layer must not invent zero values when one side or one date is
missing.

## 20. TUI priority before Android

Android remains intentionally deferred until the TUI is comfortable for daily
use.

Immediate order:

```text
1. session details
2. human duration parsing/formatting
3. full F4 measurement history and graphs
4. exercise performance history and graphs
5. previous-session defaults
6. safe editing/deletion
7. Android recorder and JSON import workflow
```
<!-- TRAINLOG_TUI_V02_CURRENT_AND_NEXT _END -->

<!-- TRAINLOG_GLOBAL_BODY_GRAPH_NEXT -->
## 21. Global body evolution graph — implemented

`F4 Corps` keeps its current per-metric graph and gains a global normalized
overlay view.

The global graph must not overlay raw kilograms and centimeters directly.

Each metric is normalized to its own first real observation:

```text
first recorded value = 100
```

Examples:

```text
waist 100 -> 96 = -4 %
right arm 100 -> 103 = +3 %
weight 100 -> 98 = -2 %
```

This makes unlike units visually comparable without changing persisted data.

Rules:
- no missing observation becomes zero;
- each metric begins only at its first real value;
- original dates remain ordered;
- every series has both a color and a distinct text/symbol identity;
- left/right limb metrics remain separate;
- normalization is display-only;
- canonical SQLite values remain untouched.

The global view is toggled from `F4 Corps` with `g`.
Per-metric navigation remains left/right.

The global view also shows a compact percentage summary from first to latest
recorded value for each available metric.

## 22. Dashboard graph v2 — implemented

The dashboard body-weight graph becomes a richer summary.

It will show:
- current body weight;
- change from first recorded weight;
- change from previous recorded weight;
- minimum recorded weight;
- maximum recorded weight;
- recent weight graph;
- latest waist measurement when available;
- latest left/right asymmetry alert when meaningful.

The dashboard remains intentionally compact.
The complete multi-metric overlay belongs to `F4 Corps`.

## 23. Immediate TUI implementation order

```text
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
TUI_GLOBAL_BODY_OVERLAY=NEXT
DASHBOARD_GRAPH_V2=NEXT
EXERCISE_PERFORMANCE_GRAPHS=AFTER
```

Android remains deferred until these TUI daily-use views are satisfactory.
<!-- TRAINLOG_GLOBAL_BODY_GRAPH_NEXT _END -->

## Dashboard graph-only layout

The home screen avoids duplicating numeric summaries already visible in the
graph.

The dashboard uses:

```text
X = recorded date
Y = percentage evolution from the first real value of each metric
```

The legend identifies every available series using a symbol, theme color,
human metric name, unit (`kg` or `cm`), and current percentage evolution.

Detailed absolute values remain available in `F4 Corps`.

## Dashboard rolling 12-month window

The dashboard graph uses a rolling calendar window ending in the current month.

Exactly 12 month slots are displayed.

Rules:

- months with no observation remain visible and empty;
- missing months are never filled with zero;
- missing months are never interpolated;
- if several observations exist in one month, the last one is used on the
  dashboard;
- each metric is normalized from its first visible month in the 12-month
  window;
- the detailed F4 history keeps the exact original timestamps and values.

The dashboard legend keeps the last visible raw value and the percentage
change over the visible 12-month window.
