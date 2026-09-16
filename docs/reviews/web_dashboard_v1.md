# Web Dashboard V1 final review

Status: `WEB_DASHBOARD_V1=PASS/FROZEN`

The final review used the real user database through `http://trainlog.perf`.
All seven tiles, normal/edit modes, desktop and narrow projections, fixed
statusline, persistent layout reload/restart and the direct/proxied API were
exercised without changing training data.

Three bounded visual corrections close V1:

- Progression owns deterministic honest Y bounds. Constant series are centred,
  varying series retain proportional breathing room, and legacy zero remains
  visible. A large tile uses a two-column factual-summary/chart presentation.
- A large muscle tile assigns approximately 38% of its internal grid to the
  front/rear figure and increases its vertical presence, without changing the
  BODY ZONES mapping or `session_count` colour semantics.
- resize handles keep their 1.2 rem interactive target while their chevron and
  resting opacity are reduced; hover and focus-within restore emphasis.

The layout persistence smoke saved a moved/resized seven-tile layout, verified
it after HTTP reload and `trainlog-web` restart, then explicitly saved the
canonical default again. The final real file is regular, mode `0600`, revision
5, and contains only the desktop 12-column Trainlog layout.

The roadmap does not currently order Analyse, Programmes, Séances and
Exercices. The next cursor is therefore `WEB_NEXT_MODULE_SELECTION_V1`: a
bounded contract-selection pass, not an implicit implementation.
