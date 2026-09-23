# Roadmap

The controlled private rollout is complete for the paired daily installation:
physical libmtp, verified backups, signing continuity, migrations, correlated
ACKs, restart, and idempotent replay passed. Generation mode remains an
explicit per-installation opt-in. The additive private Drive transport and
Android background software are implemented and validated with fake transport
and the private production namespace. Stable v0.1.4 adds authenticated
Bluetooth Classic RFCOMM as the primary paired local transport and retains direct MTP as wired recovery; Drive
remains a separately configured supported path. Android background operation
retains its documented post-reboot platform limits.

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
identities, actual-versus-planned separation, the frozen synchronization
transport boundaries, and the distinction between measured values and
estimates.

## Current baseline

Stable v0.1.4 is the release baseline for the v0.1.5 development cycle.
Desktop schema v29, Android schema v26, Notcurses, direct `Documents/Trainlog`
storage, mobile export V3, Training Knowledge V1, Body Zones V1, Training
Feedback V1/V2, STATS V1, and Session Generator V1 are implemented. Session
Generator V1 is hidden pending V2. AI session-draft exchange retains its
explicit manual validation gate. APP_SHELL_V1 remains useful historical design
evidence, but its September 2026 observations must be revalidated against the
v0.1.4 baseline rather than treated as a current implementation plan.
`TRAINLOG_WEB_V1=CONTRACT_FROZEN / IMPLEMENTATION_STARTED`; its local-only
CLI/HTTP infrastructure, embedded frontend shell and the frozen Dashboard data
contract, frozen interactive grid, versioned layout persistence and factual
tile rendering exist. The Dashboard's bounded progression and BODY ZONES
visualizations and Sessions V1 are implemented. The top-level `/programmes`
route is an operational daily active-Program calendar, while
Sessions → Programmes remains the technical administration/import, list,
detail, archive, and delete surface. Web Exercises V1, the factual
Dashboard/Analyse read-model foundation, and Sleep Diary V1 are included in
stable v0.1.4.
The corrective prepared-item projection, bounded phase-owned MTP outbox,
Sessions presentation correction, local date preference and durable preparation
withdrawal are implemented. The withdrawal uses the separate versioned
session-preparations V2 participant and does not change the frozen Dashboard,
mobile snapshot or `TRAINLOG_FORMAT_V1` contracts.

## Current cursor

Stable v0.1.4 closes the Analyse/Sleep Diary cycle and freezes the Bluetooth
Classic synchronization transport independently from future sensor work.

The 0.1.5 cycle owns cardio end to end. Android alone discovers and reads the
heart-rate sensor, exposes the global ♥ BPM state, timestamps training
session/exercise boundaries, provides one-tap Sleep capture, and records the
new dedicated cardio session type. Cardio starts with a versioned Calibration
exercise and later drives phase guidance from fresh BPM. Desktop/Web consume
only synchronized recorded measurements and never connect directly to the
sensor. Heart-rate ingestion must not reuse or overload the frozen Trainlog
Bluetooth synchronization transport contract.

```text
CURRENT_OPERATIONAL_CURSOR=TRAINLOG_CARDIO_CALIBRATION_V1
```

The focused `TRAINLOG_SYNC_CAUSAL_DELETE_V1_CLOSEOUT` is complete before this
cursor: complete predecessor state, imported built-in authorization,
cross-platform draft revision identity and resource-bound admission are closed.
`TRAINLOG_SYNC_GENERATION_ACK_V1=PASS/FROZEN`: both platforms expose explicit
coherent capture, immutable manifest publication, whole-generation
transactional consumption, durable correlated ACK and restart/replay entry
points. The orchestrator, Web API and Web control are complete behind an
explicit trusted opt-in. The authorized private direct-MTP rollout passed on
real hardware; automatic V3 selection remains unchanged without that opt-in.
Sessions deletion and its Programs subtab are complete in software and are not
reopened by the future Exercises contract.
`TRAINLOG_PROGRAMS_PRESENTATION_ANDROID_DELETE_V1=PASS` after
its coordinated private desktop/Android deployment, correlated deletion ACK,
and post-restart non-resurrection validation. It must not be reopened by the
Exercises contract.

`TRAINLOG_PROGRAM_EXECUTION_FLOW_V1=PASS` after its coordinated private
desktop/Android rollout, real start/resume/completion, correlated
`program-executions-v1` exchange, idempotence exchange, component restart,
preserved-generation replay, and disposable-Program deletion proof. It also
must not be reopened by the Exercises contract.

`TRAINLOG_SYNC_GAP_CONTRACT_V1` defines the complete-sync target and its bounded
dependency order; the v0.1.2 USB/Drive delivery closed the operational slices
needed by the current baseline.
`TRAINLOG_SYNC_TEST_ENV_V1=PASS/FROZEN` supplies the reproducible isolated
desktop/Android validation entry point, JDK 17 gate and private JVM/XDG paths.
`TRAINLOG_SYNC_CHARACTERIZATION_V1=PASS/FROZEN` reuses and completes the
current-behavior evidence for cross-implementation V3 exchange, identities,
ordering, replay, partial publication, request/receipt processing and the real
lock. Data lifecycle, causal deletion, generation/ACK, orchestrator/report, Web
API/control, direct-MTP, backup, and private rollout slices are complete. Their
remaining per-installation opt-ins are deployment policy, not the v0.1.3 cursor.
[Contract details](design/sync_gap_contract_v1.md).

`WEB_FRONTEND_SHELL_V1=PASS/FROZEN`. `trainlog -w` serves the embedded React,
TypeScript and Vite shell with its five client routes, Catppuccin Mocha design
system, permanent Header/Footer, empty-data states and health status. The
Dashboard slices below are complete; this does not authorize implementation of
the remaining route placeholders.
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
        -> WEB_DASHBOARD_V1 [PASS/FROZEN]
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

The completed slice order preserves Core ownership, keeps visual layout outside
business SQLite and frozen formats, and specifies Activity/Progression before
rendering those metrics. Later Dashboard evolution must preserve those frozen
boundaries.

The former `WEB_NEXT_MODULE_SELECTION_V1` documentary cursor is closed by the
v0.1.3 ordering below. It selected Exercices after the Android redesign; it did
not authorize implementation and it did not select Analyse.

## Trainlog v0.1.3

Trainlog v0.1.3 retains two priorities. Web Exercises was completed first; the
cursor now returns to the Android redesign:

1. the complete Web Exercices surface;
2. the global Android experience redesign.

```text
TRAINLOG_WEB_EXERCISES_V1
        -> TRAINLOG_ANDROID_UI_REDESIGN_V1
```

The Web lot is implemented and validated. The Android lot still requires its
own reviewed contract before implementation. Analyse and every other new Web
module remain after this priority unless a later explicit decision changes the
order.

### TRAINLOG_ANDROID_UI_REDESIGN_V1

The goal is to make the Android application clearer, faster, and more pleasant
during real gym use. This is primarily a UX/UI lot. By default it must not
redefine business rules, change synchronization formats, rewrite data, break
stable IDs or durable drafts, break Program sessions, or regress USB/Drive.

The contract must cover these axes:

- **Navigation:** review the screen hierarchy, remove unnecessary paths, expose
  primary actions immediately, preserve coherent caller-aware return behavior,
  and avoid overloaded screens.
- **Actions and buttons:** replace isolated action text with real buttons where
  that improves scanning; use relevant icons for evident actions; retain a
  label, tooltip/content description, or accessible text whenever an icon alone
  is ambiguous; keep sufficient Android touch targets; and preserve TalkBack
  and keyboard navigation.
- **Presentation:** improve visual hierarchy; make cards, buttons, states,
  spacing, and titles consistent; retain the Trainlog/Catppuccin identity; and
  never trade readability for density.
- **Active session:** make in-workout entry the main UX priority. Redesign the
  current-exercise view, set addition, repetition and load entry, duration entry
  for DURATION exercises, previous/next exercise navigation, exercise
  addition/removal/reordering, equipment selection, immediate feedback, and
  session completion. The measured objective is fewer manipulations and taps
  during training.
- **Exercise entry:** define a detailed contract before implementation covering
  at least `SETS + REPS`, `SETS + DURATION`, `CONTINUOUS + DURATION`, external
  load, assistance, unloaded work, equipment, heterogeneous sets, MAX, and
  feedback. This roadmap deliberately does not choose the final layout; that
  decision follows a real screen-by-screen visual review with the user.

APP_SHELL_V1 remains an important design source. Its proposal, mockups, and
review record preserve useful navigation, accessibility, ownership, and visual
reasoning. They were produced against an older baseline, however: technical
observations are historical until revalidated against 0.1.3, the Android
redesign must not apply an old mockup mechanically, and final 0.1.3 decisions
will be made screen by screen.

### TRAINLOG_WEB_EXERCISES_V1

`TRAINLOG_WEB_EXERCISES_V1=PASS`. The top-level `/exercices` route is the
desktop catalogue administration surface. It uses bounded Core read/command
services and a deterministic optimistic token; it does not put business rules
in React.

The delivered lot includes:

- **Catalogue:** a complete or paged list according to the Core contract,
  search, relevant filters, available/retired state when the model supports it,
  and access to an exercise detail.
- **Exercise detail:** show only actually persisted data made available by Core,
  including name, `exercise_id`, `recording_mode`, `tracking_mode`,
  `data_fields`, primary and secondary BODY ZONES, equipment/associations when
  available, and other real structured metadata. React must not reconstruct
  business truth.
- **Creation:** create through a real transactional Core command service, with
  name, profile, tracking, `data_fields`, primary and secondary zones, and
  equipment associations remain read-only because no bounded association
  command is part of this contract.
- **Modification:** expose only Core-authorized changes. The contract must
  distinguish mutable and immutable fields, compatible changes, changes that
  require a new identity, and exercises already referenced by history.
- **Retirement/deletion:** never naively delete a referenced identity. Reuse the
  existing causal model where applicable and preserve history.

**Historical exercise invariant:** changing the current definition of an
exercise must never rewrite past session history. Historical occurrences keep
their own recorded data. Catalogue changes apply to future uses according to
the Core contract. If a requested change is incompatible with the existing
identity, Core must reject it or create a new identity/mechanism under an
explicit contract. No silent migration may rewrite the past merely to match the
current catalogue.

Desktop/Core is the administration source. After creation, modification, or
retirement, Android synchronization must reuse the v0.1.2 engine: USB first,
with Drive as mirror/fallback. React must not introduce an Exercises-specific
synchronization path. The future contract must first audit the existing
catalogue, profile, BODY ZONES, and equipment artifacts to determine whether
the planned changes are representable. Frozen formats remain frozen; if a new
datum cannot be transported, a future contract must state that explicitly
before any protocol change. No automatic synchronization is initiated by this
surface.

### Bounded v0.1.3 order

```text
0.1.3
|
+-- WEB_EXERCISES_V1
|   |
|   +-- Core/catalogue characterization
|   +-- read-model contract
|   +-- command-service contract
|   +-- Web catalogue
|   +-- exercise detail
|   +-- creation
|   +-- modification
|   +-- causal retirement
|   +-- Android synchronization
|   +-- real validation
|
+-- ANDROID_UI_REDESIGN_V1
    |
    +-- current-state audit
    +-- UX/navigation contract
    +-- shell / buttons / components
    +-- active-session workflow
    +-- exercise-entry workflow
    +-- real visual review
    +-- accessibility validation
```

## Next

### Complete synchronization baseline

The frozen contract orders future bounded lots as environment/test,
characterization, data/lifecycles, causal deletion, generation/consumption/ack,
orchestrator/report, Web API, then button/refresh. The environment/test and
characterization and data/lifecycle lots are complete. Enriched V4 history and
the separate execution-draft codec are implemented behind explicit entry
points, while active transport remains V3. Causal deletion is implemented and
validated through explicit staged entry points, including both real producer
directions for every contracted target kind; generation/consumption
acknowledgement, orchestration/reporting, Web sync, the production generation
MTP adapter and complete Android backup/restore are implemented on the
development branch. The authorized private rollout, physical-device validation,
signed update procedure, real backups, USB-priority exchange, and configured
Drive mirror/fallback validation are complete. Broader deployment remains an
explicit per-installation decision rather than a v0.1.3 product-development
gate.
`WEB_DASHBOARD_V1=PASS/FROZEN` remains closed and unchanged.

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
TRAINLOG_WEB_EXERCISES_V1
        -> TRAINLOG_ANDROID_UI_REDESIGN_V1
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

## Independent outstanding validation gates

- `APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2`: its recorded legacy
  visual/accessibility review remains useful evidence, but the 0.1.3 Android
  redesign owns its own screen-by-screen decisions and validation.
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
