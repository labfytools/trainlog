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

Selecting a workout and pressing `Enter` opens its detail view.

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

Inside one workout, left/right or up/down changes the selected exercise. `e` opens the persisted session editor without replacing the parent session identity or timestamps.

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

## 19. Body screen and measurement history — implemented

`F4 Corps` is record-oriented.

The primary body screen shows:

- the weight evolution graph;
- recorded body observations newest first;
- a compact summary per observation;
- a visual scrollbar when the history is longer than the visible area.

Controls:

```text
↑↓ / PgUp / PgDn  select a recorded observation
Enter              open observation detail
e                  edit the selected observation
a                  add an observation
g                  open the normalized global overlay
b / Esc            return
```

Each observation detail is split into two framed pages:

```text
GENERAL
MEMBRES
```

Left/right changes page and `e` edits the observation.

Editing preserves the observation identity, timestamp, and optional session
link. Enter keeps the existing value, `-` clears a metric, and Escape cancels
the complete edit without persistence.

Canonical persisted metrics remain:

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

Missing observations are never invented as zero values.

## 20. Current priority before Android

The TUI editability checkpoint is complete enough to move toward the Android
input workflow.

Current order:

```text
1. finish visual/navigation consistency
2. keep persisted session/body editing safe
3. detect an Android phone over USB/ADB
4. build the minimal Android recorder
5. transfer/import through the frozen Trainlog JSON v1 contract
```

Measured-max analytics remain separate from ordinary best-set performance and
are not required for the Android transport milestone.

<!-- TRAINLOG_TUI_V02_CURRENT_AND_NEXT _END -->

<!-- TRAINLOG_GLOBAL_BODY_GRAPH_NEXT -->
## 21. Global body evolution graph — implemented

`F4 Corps` provides a record-oriented observation history plus a global
normalized overlay view.

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

The global view is toggled from `F4 Corps` with `g`. The normal `F4 Corps` screen remains a newest-first observation history; left/right navigation is used inside observation detail pages.

The global view also shows a compact percentage summary from first to latest
recorded value for each available metric.

## 22. Dashboard graph v2 — implemented

The dashboard body graph is implemented as a compact rolling 12-month
multi-metric summary.

It shows available body metrics normalized to their first visible value in the
window, while the legend preserves each latest raw value and percentage change.

The dashboard remains intentionally compact. Detailed absolute observation
history and the complete normalized overlay belong to `F4 Corps`.

## 23. Current implementation cursor

```text
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
TUI_GLOBAL_BODY_OVERLAY=IMPLEMENTED
DASHBOARD_GRAPH_V2=IMPLEMENTED
DASHBOARD_12_MONTHS=IMPLEMENTED
TUI_EXERCISE_PERFORMANCE=IMPLEMENTED
DATABASE_SCHEMA_V2=IMPLEMENTED
SESSION_TYPE_PERSISTENCE=IMPLEMENTED
SESSION_TYPE_TUI=IMPLEMENTED
TUI_SESSION_EDIT=IMPLEMENTED
TUI_BODY_OBSERVATION_EDIT=IMPLEMENTED
TUI_PRIMARY_NAVIGATION=IMPLEMENTED
TUI_SECONDARY_VIEW_POLISH=IMPLEMENTED
ANDROID_USB_DETECTION=NEXT
```

The frozen Trainlog JSON v1 contract remains unchanged.

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

## Exercise performance history

`F3 Exercices` opens an exercise performance screen with `Enter`.

The current slice tracks representative actual performance per workout without
inventing a maximum.

Semantics:

```text
load none
    greatest successful reps/duration

external load
    greatest actual load
    tie -> greatest reps/duration

assistance
    lowest actual assistance
    tie -> greatest reps/duration
```

Assistance graphs keep kilograms as actual assistance and explicitly state that
lower assistance is better.

A recorded best set is not a measured maximum.

Measured maxima, max-session scheduling and working-load percentages remain a
separate later contract.

<!-- TRAINLOG_TUI_EDITABILITY_NAV_CHECKPOINT -->
## Current navigation and editability checkpoint

Primary large-layout screens share the same visual identity:

```text
TRAINLOG ASCII banner
top navigation bar
framed page content
footer shortcuts
```

The top navigation is:

```text
0 Accueil   1 Séance   2 Historique   3 Exercices   4 Corps
```

Direct shortcuts keep `1`-`4` / `F1`-`F4`; `0` or `Home` returns to the
dashboard. On multi-zone pages, `Tab` / `Shift+Tab` changes focus. Only the
border and title of the focused frame use the warning/yellow role; content
colors are unchanged.

Large-layout framed/bannnered views include:

- dashboard;
- history;
- exercise catalog;
- body history;
- new-session type selection;
- in-progress session review;
- session detail;
- exercise performance;
- body-observation detail;
- exercise selection during session entry.

Session entry can create a missing exercise directly from the exercise chooser
with `a`, then return to the chooser.

Text prompts treat Escape as immediate cancellation. A cancelled draft or edit
does not persist partial state.

Persisted session replacement edits only session children. The parent session
row, stable ID, timestamps, session type, notes, and body-observation links are
preserved.

The dashboard rolling 12-month axis clamps the final `MM/YY` label inside the
dashboard frame so the current-month label does not overwrite the right border.
<!-- TRAINLOG_TUI_EDITABILITY_NAV_CHECKPOINT _END -->

<!-- TRAINLOG_SYNC_PAGE -->
## Sync page

The primary TUI navigation includes:

```text
5 Sync / F5
```

The Sync screen uses the same ASCII banner, top navigation, ncurses frames and
footer conventions as the other primary pages.

The screen is a live overview of the direct USB/MTP transport:

- connected physical MTP device;
- USB bus/device and VID:PID;
- device serial when available;
- selected MTP storage;
- free/capacity values;
- readiness of the root `Trainlog` exchange area.

Incoming categories are presented explicitly:

```text
Séances
Exercices
Mensurations
```

New exercise metadata arriving inside a valid session is intended to reconcile
automatically against the canonical local catalog.

Body measurements carried by a valid session are imported with that session.

The opposite direction is also explicit: exercises created directly on the PC
must be exportable to the Android application so both sides use the same stable
exercise IDs, names and tracking modes. This catalog synchronization uses a
separate versioned catalog snapshot; it does not overload or modify the frozen
Trainlog session JSON v1 contract.

The first Sync-page slice displays remote JSON candidates and the local exercise
count. Actual JSON classification/import and catalog-snapshot export are the
next synchronization slices.
<!-- TRAINLOG_SYNC_PAGE _END -->

<!-- TRAINLOG_SYNC_FINAL_CHECKPOINT -->
## Sync page checkpoint

Primary navigation now includes:

```text
0 Accueil
1 Séance
2 Historique
3 Exercices
4 Corps
5 Sync
```

`5 Sync / F5` opens a dedicated page rather than drawing over the dashboard.

The page contains:

- `APPAREIL CONNECTE`;
- `SYNCHRONISATION`.

Focus rules:

```text
Tab / Shift+Tab  switch focused frame
Left/Right       move inside top navigation
Enter            activate selected navigation item
Up/Down          move through synchronization rows
PgUp/PgDn        scroll synchronization content
r                rescan USB/MTP state
b / Escape       return
```

Only the focused frame uses the yellow border/title role.

The live page reports the connected Android MTP device and exposes these sync
directions:

```text
Android -> PC
    sessions
    exercises carried by sessions
    body measurements carried by sessions

PC -> Android
    canonical exercise catalog
```

The session exchange remains frozen Trainlog JSON v1.
Catalog synchronization is a separate versioned contract.
<!-- TRAINLOG_SYNC_FINAL_CHECKPOINT _END -->

<!-- TRAINLOG_PROFILE_AWARE_ENTRY -->
## Profile-aware exercise entry

The TUI form is driven by exercise metadata.

```text
SETS + REPS
    series, reps, optional load, rest

SETS + DURATION
    series, duration, optional load, rest

CONTINUOUS + DURATION + SPEED_KMH
    duration, speed
```

Continuous exercises do not display a set count.

`Marche` will use the continuous form only after its catalog metadata is
explicitly changed; behavior is never inferred from its name.
<!-- TRAINLOG_PROFILE_AWARE_ENTRY _END -->
