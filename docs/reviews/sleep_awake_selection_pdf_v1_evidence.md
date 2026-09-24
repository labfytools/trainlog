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
and download receive the same explicit inclusive range over each entry's
`night_start_date`, and all observations are recomputed from that subset. The
range initially spans the loaded agenda period; later detail/HR selection does
not alter it. Each of the 24 hour labels is centered in the equal visual cell
it names, using the same grid beneath every 18:00-to-18:00 agenda row; there is
no ambiguous duplicate `18` at the right edge.

## Automated validation

- Web targeted suites: 34/34 passed.
- Complete Web suite after the range/axis follow-up: 209/209 passed.
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

The original Web change affected no database schema, synchronization artifact,
frozen format, or Android behavior; the follow-up below deliberately corrects
the Android capture semantics without changing those compatibility boundaries.

## Android capture correction and real-night repair

Follow-up visual review established that Android one-tap Réveil had produced
three punctual `night_get_up` facts for 2026-09-23 → 2026-09-24, so there was
no structured interval for the shared projection to color. The user selected
an explicit 30-minute no-further-action policy. Future taps now create a
`long_awake` interval from the factual tap through +30 minutes; another tap
inside the window extends that same interval, while Levé clamps it to the
factual final-get-up timestamp. This is product policy, not BPM/RR inference.

Before repair, separate desktop and Android backups were retained under
`~/.local/share/trainlog/backups/sleep-wake-window-20260924T0733/`:

- desktop SHA-256: `ac2246dc699a5e11e9d8b52c4cad0f096dfc08191f54b6a86c44ce72f623c39c`;
- Android SHA-256: `a1a9017875757f27264556ea62a9d27a98f2efad17ca0234fb10f4095170a98a`.

The real night was advanced causally, retaining its stable `entry_id`, to:

- `01:35:43.554104 → 02:05:43.554104`;
- `04:11:59.245980 → 04:14:28.358915` (bounded by Levé).

Android and desktop current revision is
`slr_d6cac4c6-dab7-4327-bc06-5cc95f7a2dfa`. Its repaired linear ancestry
retains desktop revision `slr_2bbf573d-4262-49b0-896e-1bcabf3b002a`, including
the existing `TB` sleep and wake qualities.
Android database round-trip hashes matched, integrity/foreign-key checks passed,
and the non-destructive app update preserved first-install identity while
advancing the private build to versionCode 49.

The first post-repair full-generation attempts correctly rejected an already
published Android archive whose embedded ancestry still named the pre-desktop
mobile parent. Both live databases are repaired and aligned, but that stale
transport archive remains rejected until the normal archive-capacity recovery
retires it; no successful synchronization receipt is claimed by this evidence.

Post-correction validation passed 312 Android JVM tests (five optional skips),
`assembleDebug`, 208 Web tests, typecheck, and build. Real Firefox then observed
two orange agenda bands, two orange HR bands, `Réveils 2 · 32 min 29 s`, and two
orange PDF intervals. The selected-only PDF reports `Sommeil déclaré : 4 h 44
min 16 s` after subtracting the repaired awakening intervals from the visual
estimate.
