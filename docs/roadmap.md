# Roadmap

This document owns future work. The implemented baseline is summarized in
[current_state.md](current_state.md); completed narratives and evidence belong
in [reviews](reviews/) and [CHANGELOG.md](../CHANGELOG.md).

## Product boundary

```text
Android captures and summarizes.
The TUI analyzes and tracks over time.
Desktop SQLite is canonical long-term history.
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

## Current cursor

```text
CURRENT_OPERATIONAL_CURSOR=REAL_DATA_BASELINE_V1
```

Collect real body observations and training sessions through Android, sync them
to desktop, and record MAX only through explicit `max_test` results. Do not seed
fictitious user history into canonical databases.

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
REAL_DATA_BASELINE_V1
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
