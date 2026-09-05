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
