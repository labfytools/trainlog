# Web Dashboard Visualizations V1

Status: `WEB_DASHBOARD_VISUALIZATIONS_V1=PASS/FROZEN`

## ECharts boundary

Trainlog pins Apache ECharts 6.1.0 under Apache-2.0. Only `LineChart`,
`GridComponent`, `TooltipComponent` and `SVGRenderer` are registered from the
tree-shakable API. The chart module is dynamically imported only when a
medium/large available Progression tile needs it. SVG is appropriate for this
small series, stays sharp through Grid resize, and avoids a default ECharts
visual identity. Options derive from the live Catppuccin CSS tokens.

The x axis contains only supplied timestamps. The y axis contains absolute
`weight_kg`, uses a data-driven value scale, and is visibly labelled `kg`.
Every supplied point is shown, including legacy `0.0 kg`; the unsmoothed line
is only a visual connection between observations. Marker emphasis uses only
the Core-provided `improved` flag. Tooltip facts are date, weight, exercise,
equipment, dose and optional `Amélioration`; no delta or percentage exists.

## Original BODY ZONES SVG

The silhouette is original code-native Trainlog work, with a stable
`0 0 120 260` viewBox and no external asset. Regions map as follows:

| Canonical zone | SVG regions |
|---|---|
| `chest` | bilateral front chest |
| `back` | upper rear back and bilateral rear lats |
| `shoulders` | bilateral front and rear shoulders |
| `arms` | bilateral front and rear arms |
| `core` | front abdominal/trunk region |
| `glutes` | bilateral rear glutes |
| `thighs` | bilateral front and rear thighs |
| `calves` | bilateral front and rear calves |
| `full_body` | no localized V1 region; text list only |
| `upper_body` | no localized V1 region; text list only |
| `lower_body` | no localized V1 region; text list only |

Only a zone present in the snapshot is interactive. Focus/hover exposes its
supplied sessions, occurrences and sets, while the full textual list remains
available. Colour depends solely on `session_count / maximum_session_count` in
the displayed snapshot: zero uses Surface0, `(0, 1/3]` Sapphire, `(1/3, 2/3]`
Lavender and `(2/3, 1]` Mauve. This is a relative visual scale, not a score or
physiological assessment.

Compact tiles remain textual. Medium and large tiles show the figure; large
allocates more vertical space. Activity keeps its factual CSS heatmap, while
Next Session, Last Session, MAX and Cardio receive no decorative chart.

Next cursor: `WEB_DASHBOARD_V1_FINAL_REVIEW`.
