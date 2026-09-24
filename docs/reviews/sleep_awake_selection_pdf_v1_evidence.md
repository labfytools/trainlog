# Sleep awake selection and PDF V1 evidence

`TRAINLOG_SLEEP_AWAKE_SELECTION_PDF_V1=PASS` on 2026-09-24.

The Web now derives the agenda, selected-night heart-rate overlay, and vector
PDF rows from one pure temporal projection. It retains every structured
`long_awake` interval separately from point-in-time `night_get_up` events,
medication intakes, and sleep ranges. Factual `sleep` intervals determine sleep
duration when present. Otherwise the displayed Couché +45 minutes to Levé -10
minutes range supplies a presentation-only duration after subtracting the union
of overlapping structured awakenings; no estimate is persisted.

One agenda row is the effective selection. A click updates the detail dates,
editor snapshot, and HR context in memory without issuing a write. PDF preview
and download receive the same one-entry snapshot, and all observations are
recomputed from that subset. Before an explicit click, the active night or the
first deterministically ordered row is the documented fallback.

## Automated validation

- Web targeted suites: 34/34 passed.
- Complete Web suite: 208/208 passed.
- `npm run typecheck`: passed.
- `npm run build`: passed.
- Embedded production bundle `meson compile -C build`: passed.
- The tests cover multiple awakening intervals in agenda/HR projection, exact
  factual sleep duration, estimated duration with awakening subtraction,
  selection dates/HR identity, no write on selection, identical preview/export
  subsets, PDF interval geometry, medications, point markers, and no-HR
  fallback without an invented BPM curve.

## Read-only real-data smoke

The rebuilt user Web service was exercised through real headless Firefox at
`http://trainlog.perf/analyse?section=sleep` without a Sleep mutation.

- The real 2026-09-23 → 2026-09-24 row contains no factual `sleep` interval;
  its displayed estimated band now reports `5 h 14 min 16 s`, not `—`.
- The real 2026-09-22 → 2026-09-23 row renders its structured awakening in
  both the agenda and the no-invented-HR detail overlay.
- Selecting that row updates the detail inputs to exactly 2026-09-22 and
  2026-09-23.
- Firefox export produced one PDF page whose extracted text contains only
  `22/09/2026`, `au 23/09/2026`, and `1 nuit`; it contains no 24/09 row.
- The selected-subset observation is `Sommeil déclaré : 6 h`,
  `Longs réveils : 1`, `Siestes : 1`, and `Prises de médicaments : 7`.
- Raster inspection confirms the long-awake band remains visibly distinct
  inside the sleep timeline and point/medication markers retain their positions.

The change affects no database schema, synchronization artifact, frozen format,
or Android behavior.
