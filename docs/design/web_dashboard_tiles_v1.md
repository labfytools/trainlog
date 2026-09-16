# Web Dashboard Tiles V1

Status: `WEB_DASHBOARD_TILES_V1=PASS/FROZEN`

## Boundary

All seven tile components consume the single validated
`TrainlogWebDashboardSnapshot` returned by `GET /api/v1/dashboard`. React may
format values, select a bounded subset for the current logical size, and sum
the already projected daily counters. It does not derive business truth.

## Density contract

`compact` exposes the essential fact, `medium` adds context, and `large`
exposes all useful bounded fields. Resize never changes a metric. Responsive
layouts reuse the same rule and never write a second layout truth.

## Factual representations

- Prochaine séance and Cardio render explicit unavailable states without
  exposing backend reason identifiers or synthesizing values.
- Activité maps each supplied day to inactive, active-one-session, or
  active-multiple-sessions CSS classes. The colour is not physiological
  intensity. Totals are direct sums of `session_count` and `set_count`.
- Progression shows only the selected comparable identity, supplied points and
  Core-owned `improved` flags. Legacy `0.0 kg` remains visible.
- Dernière séance exposes supplied timestamps and counts. A null duration stays
  unavailable; primary zones are the Core projection.
- Records / MAX shows 1, 3 or at most 8 explicit records. A partial snapshot is
  labelled as recent rather than exhaustive.
- Répartition musculaire sorts the supplied primary zones deterministically
  for display. Bar length represents only `session_count`; occurrences and
  sets stay separate textual facts.

The global shell reports loading/transport failure once. `invalid_data` is a
discrete global warning; valid surviving projections remain usable. No
ECharts, inferred score, demo datum or production fixture is part of V1.

Next cursor: `WEB_DASHBOARD_VISUALIZATIONS_V1`.
