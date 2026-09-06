# Desktop TUI

## 1. Purpose

The Trainlog desktop application is a C17/ncursesw interface for durable
history, correction, analysis, visualization, direct data entry, and manual
synchronization.

Desktop SQLite is the canonical long-term history.

## 2. Navigation

Large-layout primary navigation:

```text
0 Accueil
1 Séance
2 Historique
3 Exercices
4 Corps
5 Sync
```

Direct shortcuts include the matching function keys where implemented.

Common controls:

```text
↑ ↓            list navigation
Enter          open/activate
Tab            change focus on multi-zone pages
Esc / b        return or cancel
0 / Home       dashboard
q              quit from the application shell
```

Minimum terminal size:

```text
72x20
```

Smaller terminals display a clear fallback instead of corrupt layout.

## 3. Visual rules

The TUI uses centralized semantic theme roles.

Color is not the sole state carrier.

Typical roles:

```text
accent
success
warning
error
muted
graph series
```

Focused frames use the warning role for border/title without recoloring all
content.

## 4. Exercise catalog

Exercise behavior is driven by:

```text
recording_mode
tracking_mode
data_fields
```

No exercise-name heuristic determines an entry form.

Catalog identities are stable.

Unicode-aware normalized-name uniqueness prevents duplicate logical names.

## 5. Session entry

The TUI can record sessions directly.

Set-based entry supports planned targets and actual work.

For repetition work, compact actual-set input supports:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

For timed work, the shared duration parser accepts forms such as:

```text
90
90s
1:30
1m30
1m30s
2m
```

Persistent duration/rest units remain seconds.

Continuous exercise entry asks for duration and configured supplemental fields
without set/rest/load prompts.

## 6. Session history and editing

History is keyboard navigable.

`Enter` opens full session detail.

Persisted session editing preserves the parent session identity and timestamps
while replacing child exercise/set data transactionally.

Inside editable session exercise lists:

```text
d   delete selected exercise from the session
```

A failed replacement rolls back completely.

Removing an exercise from one session does not remove the exercise from the
catalog.

## 7. Body tracking

`4 Corps / F4` provides:

- newest-first body observations;
- detail and correction;
- body trend visualization;
- normalized multi-metric overlay;
- left/right metric separation;
- no invented zero values for missing measurements.

Editing preserves observation identity, timestamp, and optional session link.

## 8. Dashboard

The dashboard includes a rolling 12-month normalized body graph.

Rules include:

- fixed calendar month slots;
- missing months remain empty;
- no zero fill;
- no interpolation;
- when multiple observations exist in one month, the last visible monthly value
  is used for the compact dashboard graph.

Detailed raw observations remain in `Corps`.

## 9. Exercise performance

Exercise detail exposes recorded performance history.

Representative comparison semantics:

```text
load none
    greatest successful reps/duration

external
    greatest actual load
    tie -> greatest reps/duration

assistance
    lowest assistance
    tie -> greatest reps/duration
```

A best recorded set is not automatically a measured maximum.

## 10. Sync page

`5 Sync / F5` uses the shared synchronization engine.

The page shows:

- connected MTP device status;
- storage availability;
- structured synchronization history.

Manual action:

```text
s   run bidirectional synchronization
r   refresh device status
```

History behaves like a compact Git log:

```text
↑ ↓       select run
Enter     open run detail
```

The detail view behaves like a compact `git show` and contains:

```text
sync ID
trigger
time
status
request ID when applicable
Android -> PC counts
PC -> Android catalog count
summary
error when applicable
```

## 11. Shared sync engine

The TUI does not own a separate synchronization implementation.

It calls:

```text
trainlog_sync_run(TRAINLOG_SYNC_TRIGGER_TUI, ...)
```

The Android-triggered daemon calls the same engine.

This keeps import/export, MTP publication, locking, history, and diagnostics in
one implementation.

## 12. Direct MTP

Transport uses:

```text
libudev -> exact physical USB device
libmtp  -> storage/object operations
```

No filesystem mount is required.

Raw libmtp output is suppressed while ncurses owns the terminal.

## 13. Error behavior

Input is validated before persistent mutation.

Escape cancels prompts without committing partial edits.

Synchronization failure displays a useful final diagnostic and records the
structured run when a transaction actually begins.

## 14. Build and test

```bash
meson compile -C build
meson test -C build --print-errorlogs
```

Current normal suite:

```text
19/19 PASS
```

## 15. Measured max view

From `3 Exercices`, the selected exercise exposes:

```text
Enter   ordinary performance history
m       measured max
```

The measured-max page is deliberately separate from ordinary best-set history.

It shows:

- count of explicit max-test sessions;
- newest successful measured result;
- best historical result using the same load mode;
- dedicated max-test graph;
- max-test history;
- external-load working percentages at 60%, 70%, 80%, and 90%.

For external load, `r` cycles practical rounding increments:

```text
0.5 kg
1.0 kg
2.5 kg
5.0 kg
```

Working percentages are display calculations only.

Assistance remains inverse-direction:

```text
less assistance = better
```

No percentage-of-max working load is produced for assistance or no-load
performance.

## 16. Body analytics

`4 Corps` adds:

```text
v   analyse corporelle
```

The analytics view has two pages:

```text
Composition et tendance
Proportions et symétrie
```

`p` configures a desktop-only estimation profile containing height and the
circumference-formula branch.

Composition can display:

- circumference-based body-fat estimate;
- estimated fat mass when body weight is present;
- estimated lean mass when body weight is present;
- weight change from the oldest available weight;
- waist change from the oldest available waist;
- body-fat estimate change when comparable observations exist.

Proportions can display:

- waist/hip ratio;
- shoulder/waist ratio;
- chest/waist ratio;
- left/right asymmetry for arms, forearms, thighs, and calves.

All estimates are explicitly labeled as estimates. No result is converted into
a medical or diagnostic classification.
