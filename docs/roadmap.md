# Roadmap

This document owns future work. The implemented baseline is summarized in
[current_state.md](current_state.md); completed narratives and evidence belong
in [reviews](reviews/) and [CHANGELOG.md](../CHANGELOG.md).

## Product boundary

```text
Trainlog Core owns business truth and canonical desktop persistence.
The TUI administers, inspects, maintains, and imports/exports.
The local Web analyzes, visualizes, and prepares programs/sessions.
Android executes in the field, captures actual work/feedback, and returns data.
```

Future work must preserve the frozen Trainlog JSON V1 contract, stable
identities, actual-versus-planned separation, direct MTP transport, and the
distinction between measured values and estimates.

## Current baseline

Desktop schema v18, Android schema v17, Notcurses, direct
`Documents/Trainlog` storage, mobile export V3, Training Knowledge V1, Body
Zones V1, Training Feedback V1/V2, STATS V1, and Session Generator V1 are
implemented. Session Generator V1 is hidden pending V2. AI session-draft
exchange and APP_SHELL_V1 retain their explicit manual validation/review gates.
`TRAINLOG_WEB_V1=CONTRACT_FROZEN / IMPLEMENTATION_STARTED`; its local-only
CLI/HTTP infrastructure, embedded frontend shell and the frozen Dashboard data
contract, frozen interactive grid, versioned layout persistence and factual
tile rendering exist. Advanced visualizations remain a future slice.

## Current cursor

```text
CURRENT_OPERATIONAL_CURSOR=WEB_NEXT_MODULE_SELECTION_V1
```

`WEB_FRONTEND_SHELL_V1=PASS/FROZEN`. `trainlog -w` serves the embedded React,
TypeScript and Vite shell with its five client routes, Catppuccin Mocha design
system, permanent Header/Footer, empty-data states and health status. The next
Dashboard tranche is partitioned below; this cursor does not authorize an
undifferentiated implementation of every Dashboard concern.
`WEB_DASHBOARD_GRID_V1=PASS/FROZEN` adds the validated 12-column desktop grid,
explicit edit mode, drag/resize, keyboard alternative and derived responsive
projections without persisting layout state.
`WEB_DASHBOARD_LAYOUT_V1=PASS/FROZEN` persists only that canonical desktop
layout with atomic XDG storage, optimistic revisions and protected HTTP
mutations; responsive projections remain derived.
`WEB_DASHBOARD_TILES_V1=PASS/FROZEN` replaces every shell placeholder with a
size-adaptive component consuming only the frozen Dashboard snapshot.
`WEB_DASHBOARD_VISUALIZATIONS_V1=PASS/FROZEN` adds a factual time-series chart
for the Core-selected performance identity and an original accessible BODY
ZONES silhouette, without changing either domain's semantics.
`WEB_DASHBOARD_V1=PASS/FROZEN` closes the complete Dashboard after its final
functional, visual, responsive, accessibility, persistence and performance
review.

## TRAINLOG_WEB_V1 implementation gate

Implementation order is fixed as:

```text
WEB_DASHBOARD_CHARACTERIZATION_V1
        -> WEB_DASHBOARD_CORE_READ_MODEL_V1 [PASS/FROZEN]
        -> WEB_TUI_READ_MODEL_ADOPTION_V1 [PASS/FROZEN]
        -> WEB_CLI_HTTP_INFRASTRUCTURE_V1 [PASS/FROZEN]
        -> WEB_FRONTEND_SHELL_V1 [PASS/FROZEN]
        -> WEB_DASHBOARD_V1
```

The Web must not start with React. Characterization proves current behavior;
Core extraction establishes one business implementation; TUI adoption proves
reuse; only then may `trainlog -w`, the loopback HTTP adapter, embedded assets,
and browser UI be introduced. Initial API delivery is read-only. Later program
and session writes require explicit transactional Core command services.

`TRAINLOG_WEB_V1` preserves `/api/v1/` as an independently versioned boundary,
the `127.0.0.1:8080` default, explicit `--port`, no silent port fallback, a
single serialized Core/SQLite owner, build-only Node/npm, and no change to
business SQLite or frozen exchange formats. Dashboard Activité and Progression
semantics are now frozen by `WEB_DASHBOARD_DATA_CONTRACT_V1`; future rendering
must consume them without reconstructing or redefining their mathematics.

`WEB_DASHBOARD_V1` advances through independently reviewable slices:

```text
WEB_DASHBOARD_DATA_CONTRACT_V1 [PASS/FROZEN]
        -> WEB_DASHBOARD_GRID_V1 [PASS/FROZEN]
        -> WEB_DASHBOARD_LAYOUT_V1 [PASS/FROZEN]
        -> WEB_DASHBOARD_TILES_V1 [PASS/FROZEN]
        -> WEB_DASHBOARD_VISUALIZATIONS_V1 [PASS/FROZEN]
        -> WEB_DASHBOARD_V1_FINAL_REVIEW [PASS/FROZEN]
```

The slice order may be refined by the next contract, but it must preserve the
existing Core ownership, keep visual layout outside business SQLite and frozen
formats, and specify Activité/Progression before implementing those metrics.

`WEB_NEXT_MODULE_SELECTION_V1` is the next documentary cursor. The current
roadmap does not yet establish a contract order between Analyse, Programmes,
Séances and Exercices. This cursor must inventory their existing Core services,
read/write boundaries and dependencies, then select one bounded module contract;
it does not authorize silently implementing a page.

## Next

### Gym catalog V1

Build an inventory from the equipment actually available in the user's gym.
Keep physical equipment, exercises, movement patterns, muscles, and BODY ZONES
distinct. One photographed machine may support several exercises. Record model
or manufacturer only when evidence identifies it.

### Exercise metadata V1

After the gym inventory is normalized, design any remaining structured metadata
such as movement family and laterality. Reuse existing equipment and BODY ZONES
identities; do not encode semantics in display names or notes.

### Session Generator V2

Replace the hidden V1 product surface with duration-aware construction that can
represent warm-up, ramp-up, main work, cool-down, transitions, setup time, and
candidate scarcity without claiming unsupported physiological precision.

### Session templates V1

Offer editable conveniences built on the real catalogue and planning model.
Templates are not rigid prescriptions and never become performed work before
normal capture.

## Later

- evolve training analytics only where sufficient real history supports the
  metric, without aggregating incomparable loads;
- add body baseline and trend comparisons while keeping direct measurements
  distinct from estimates;
- add conservative, editable progression suggestions with correct external-load
  and assistance interpretation;
- support explicit user objectives without mandatory gamification;
- add user-controlled dated backup and complete/period-limited exports.

## Canonical order

```text
WEB_DASHBOARD_CHARACTERIZATION_V1
        -> WEB_DASHBOARD_CORE_READ_MODEL_V1
        -> WEB_TUI_READ_MODEL_ADOPTION_V1
        -> TRAINLOG_WEB_V1
        -> REAL_DATA_BASELINE_V1
        -> GYM_CATALOG_V1
        -> EXERCISE_METADATA_V1
        -> SESSION_GENERATOR_V2
        -> SESSION_TEMPLATES_V1
        -> ANALYTICS_EVOLUTION
        -> BODY_ANALYTICS_V2
        -> PROGRESSION_ASSIST_V1
        -> OBJECTIVES_V1
        -> BACKUP_EXPORT_V1
```

## Outstanding validation gates

- `APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2`: complete the recorded
  human visual/accessibility matrix without redefining product semantics.
- `TRAINLOG_AI_SESSION_DRAFT_V1=VALIDATION_PENDING`: complete one real Drive
  plus Android-triggered bidirectional synchronization smoke test.

## Permanent constraints

Do not regress to SQLite-file synchronization, a mandatory mounted Android
filesystem, exercise-name identity heuristics, fake sets for continuous work,
synthetic uniform actuals, ordinary training promoted to measured MAX,
assistance interpreted as external load, or estimates presented as direct
measurements. Incompatible exchange semantics require a new version.
Web UI configuration remains outside training data, scientific/AI exports,
session synchronization, and business SQLite. Browser, React, TUI, and Android
must not query desktop tables or duplicate Core calculations.
