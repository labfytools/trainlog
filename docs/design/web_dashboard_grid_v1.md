# Web Dashboard grid V1

Status: `WEB_DASHBOARD_GRID_V1=PASS/FROZEN`

## Library spike

The spike was performed on 2026-09-16 against React 19.3.0, TypeScript 7 and
Vite 8.3.0. Package metadata and upstream documentation were checked before
installation.

| Candidate | Version checked | Fit | Decision |
|---|---:|---|---|
| `react-grid-layout` | 2.2.4 | MIT; React peer `>=16.3`; TypeScript v2 implementation; 12-column layouts, drag, two-axis resize, per-item min/max, configurable enablement, vertical compaction, responsive primitives and serializable layouts. Active 2026 releases. Mouse interaction is mature but has no complete keyboard grid editor. | Selected, with a Trainlog keyboard layer and independent model/validator. |
| `gridstack` | 13.3.0 | MIT; active maintenance; framework-neutral drag, resize, responsive columns and constraints. Its package is substantially larger and React integration remains an imperative wrapper around a DOM engine rather than a native typed React state boundary. | Rejected for this React-owned surface. |
| `@dnd-kit/core` | 6.3.1 | MIT; strong sensors and keyboard support, but it is a drag/drop toolkit rather than a heterogeneous dashboard-grid engine. It supplies neither resize nor the required collision/compaction policy; upstream guidance requires a custom sorting strategy for mixed grid sizes. | Rejected because Trainlog would have to build the missing engine. |

`react-grid-layout` has no external service, runtime telemetry, CDN or backend
dependency. Trainlog imports the v2 API directly and does not expose the
library's `{i,w,h}` representation outside its adapter.

## Trainlog model and invariants

The authoritative frontend shape is:

```text
TileLayout { id, x, y, width, height }
```

The seven stable IDs are `next-session`, `activity`, `progression`,
`last-session`, `max-records`, `muscle-distribution`, and `cardio-recovery`.
The desktop layout alone is canonical and uses 12 columns. The adapter maps it
to and from the library shape. A separate Trainlog validator requires exactly
those IDs, integral coordinates and dimensions, each tile's min/max bounds,
`x >= 0`, `y >= 0`, `x + width <= 12`, `y <= 200`, and no overlap.

The deterministic default is:

| Tile | x | y | width | height | min/max width | min/max height |
|---|---:|---:|---:|---:|---:|---:|
| Prochaine séance | 0 | 0 | 5 | 4 | 4–8 | 3–7 |
| Activité | 5 | 0 | 7 | 4 | 5–12 | 3–7 |
| Progression | 0 | 4 | 8 | 5 | 6–12 | 4–9 |
| Dernière séance | 8 | 4 | 4 | 3 | 3–7 | 3–6 |
| Records / MAX | 8 | 7 | 4 | 3 | 3–8 | 3–7 |
| Répartition musculaire | 0 | 9 | 6 | 5 | 5–12 | 4–9 |
| Cardio / récupération | 6 | 10 | 6 | 4 | 5–12 | 4–8 |

Vertical compaction is deterministic; overlap is forbidden and collisions
push/reflow other items. Library output is accepted into React state only after
conversion and Trainlog validation.

## Interaction and responsive contract

Normal mode disables drag and resize and emits no resize handles. Explicit
`Modifier l'agencement` mode enables them on desktop. `Annuler` restores the
entry snapshot, `Réinitialiser` restores the Trainlog default in the edit
draft, and `Enregistrer pour cette session` accepts the draft only in current
React memory. Reload deliberately restores the default: persistence belongs to
`WEB_DASHBOARD_LAYOUT_V1`.

Focused tiles support Arrow keys for movement and Shift+Arrow keys for resize.
Focus is visible and a polite live region announces logical coordinates and
dimensions. Mouse resize exposes east, south and south-east handles. Reduced
motion disables grid transitions.

Widths below 1100 px derive a six-column projection; widths below 700 px derive
a stable one-column projection. These projections preserve canonical ordering,
remain non-authoritative and never update the desktop layout. Editing at those
breakpoints is deliberately disabled with an explicit explanation.

The existing `/api/v1/dashboard` fetch and factual Footer remain unchanged.
Tile bodies retain placeholders; logical `compact`, `medium`, and `large`
presentation levels depend only on tile dimensions and add no business facts.

## Bundle impact

Vite production output before the dependency was 229.07 kB JavaScript and
7.14 kB CSS. V1 output is 301.61 kB JavaScript and 13.42 kB CSS: an increase of
72.54 kB JavaScript and 6.28 kB CSS before transport compression. This is
accepted for the complete drag, resize, constraint and compaction engine.

Upstream references:

- <https://github.com/react-grid-layout/react-grid-layout>
- <https://github.com/gridstack/gridstack.js>
- <https://github.com/clauderic/dnd-kit>
