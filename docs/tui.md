# Desktop TUI

## 1. Purpose

The Trainlog desktop application is a C17/Notcurses interface for durable
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
4 Équipements
5 Corps
6 Sync
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

Notcurses provides a true-color Catppuccin-derived dark palette: background
`#1E1E2E`, surface `#181825`, text `#CDD6F4`, and semantic accent, success,
warning, error, muted, and graph roles. Unicode frames and visible selection
markers enhance presentation without becoming application semantics.

The backend owns one standard plane for a run, translates terminal input into
Trainlog-owned keys, accepts complete UTF-8 code points in prompts, and
re-queries dimensions while rendering so the 72x20 minimum/fallback recovers
after a resize.

Input lifecycle is handled at that boundary: legacy/unknown terminal events,
Notcurses PRESS events, and deliberate auto-REPEAT events become one logical
Trainlog action; Notcurses RELEASE events are consumed and never reach screen
navigation or prompt handling. This prevents extended terminal keyboard
protocols from applying one physical keypress twice.

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

## 7. Equipment

`4 Équipements / F4` provides supplied-equipment browsing, search, detail, and
custom-equipment creation and selection. Supplied definitions are generated
from `catalog/equipment-v1.json`; user-created definitions persist in desktop
SQLite and synchronize separately through definitions V1.

Equipment can be selected while editing an exercise occurrence. Session detail,
exercise history, and historical occurrence views resolve an `equipment_id`
against the supplied manifest or a local custom definition. If an historic ID
is no longer resolvable, the view shows that unknown reference explicitly.
Session detail displays the selected occurrence's stable `entry_id` together
with its resolved equipment, so repeated appearances of one exercise remain
visibly distinct; `i` opens the resolved equipment detail.

## 8. Body tracking

`5 Corps / F5` provides:

- newest-first body observations;
- detail and correction;
- body trend visualization;
- normalized multi-metric overlay;
- left/right metric separation;
- no invented zero values for missing measurements.

Editing preserves observation identity, timestamp, and optional session link.

## 9. Dashboard

The dashboard includes a rolling 12-month normalized body graph.

Rules include:

- fixed calendar month slots;
- missing months remain empty;
- no zero fill;
- no interpolation;
- when multiple observations exist in one month, the last visible monthly value
  is used for the compact dashboard graph.

Detailed raw observations remain in `Corps`.

## 10. Exercise performance

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

## 11. Sync page

`6 Sync` uses the shared synchronization engine. The supported shortcut is the
numeric `6` key.

The page shows:

- connected MTP device status;
- storage availability;
- structured synchronization history.

Manual action:

```text
a   run Android -> PC synchronization
p   run PC -> Android synchronization
b   run bidirectional synchronization
r   refresh device status
```

`a` imports definitions V1, mobile V2, and associations V2 only. `p` publishes
definitions V1, catalog V1, mobile V2 (including body observations), and
associations V2 only. `b` completes that inbound sequence before beginning the
outbound sequence.

The direction keys are direct actions: pressing `a`, `p`, or `b` opens one
confirmation for that exact direction; there is no separate mode-selection
step. `Enter` accepts the pending action and can invoke the shared engine only
once. `Esc` cancels a pending confirmation without an operation; when no
confirmation is pending, `Esc` returns from Sync. `r` only refreshes connected
device status. The former `s` synchronization shortcut is retired and inert.

The footer states the same contract in both layouts. At 100 columns or wider it
reads `a Android→PC  p PC→Android  b PC↔Android  r actualiser  Échap retour`;
the compact footer uses `a A→PC  p PC→A  b A↔PC  r act.  Échap retour` without
changing any action or direction.

Before a confirmed run, Sync identifies the selected direction in its progress
feedback. It then shows the completed run summary on success, or a
direction-prefixed diagnostic on failure. Each structured local history run
persists its selected `a`, `p`, or `b` direction, and the history list displays
that direction label alongside its summary. `direction inconnue` is used only
for a legacy entry whose direction was never recorded, rather than being
guessed.

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

## 12. Shared sync engine

The TUI does not own a separate synchronization implementation.

It calls:

```text
trainlog_sync_run(TRAINLOG_SYNC_TRIGGER_TUI, ...)
```

The Android-triggered daemon calls the same engine.

This keeps import/export, MTP publication, locking, history, and diagnostics in
one implementation.

## 13. Direct MTP

Transport uses:

```text
libudev -> exact physical USB device
libmtp  -> storage/object operations
```

No filesystem mount is required.

Raw libmtp output is suppressed while Notcurses owns the terminal.

## 14. Error behavior

Input is validated before persistent mutation.

Escape cancels prompts without committing partial edits.

Synchronization failure displays a useful final diagnostic and records the
structured run when a transaction actually begins.

Conflict diagnostics identify the stable affected identity, the source
artifact/direction, and a concise source summary. They preserve existing
session, body-observation, association, and definition data rather than silently
overwriting it.

## 15. Build and test

```bash
meson compile -C build
meson test -C build --print-errorlogs
```

Validated current normal suite:

```text
34/34 Meson tests PASS
```

## 16. Measured max view

Creating or editing a `Test de max` session uses the existing exercise search
and optional equipment selector, then asks only for `Poids max (kg)`. It does
not prompt for sets or repetitions and persists the value in `max_results`.

The history detail for a max-test session is a compact selectable table:

```text
Exercice                Machine                    Max
Pec Fly                 Rear Delt / Pec Fly     100 kg
Rear Delt Fly           Rear Delt / Pec Fly      86 kg
```

Equipment is occurrence context. The rows remain distinct by movement and
stable `entry_id`; no machine-level maximum is calculated. A preserved
non-MAX entry, such as continuous warm-up, remains visible with `—` in the MAX
column.

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

## 17. Body analytics

`5 Corps` adds:

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
