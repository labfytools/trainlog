# Sleep Diary PDF localization evidence

## Scope

This focused 0.1.4 correction changes only the presentation and encoding of the
local vector Sleep Diary PDF. It does not change Sleep Diary event codes,
timestamps, revisions, schemas, synchronization, publication or Android.

## French localization

The French PDF owns a complete vocabulary for column headings, timeline
ranges, point markers, legend, observations, draft warning and plurals.
`QUALITÉ DU RÉVEIL` remains an appreciation column; `Long réveil` identifies
the distinct temporal interval. Dates render as two localized lines such as
`20/09/2026` and `au 21/09/2026`. WinAnsi PDF strings preserve French accents
in rendering and text extraction.

The factual summary uses human-readable durations and correct singular/plural
forms. Medication rows keep the actual intake timestamp, name, localized dose
and unit; same-name intakes with different doses remain separate snapshots.
The English generator uses an independent English vocabulary.

## Long-awakening geometry

The regression fixture contains sleep from 23:00 to 03:00, a long awakening
from 03:00 to 03:45 and sleep from 03:45 to 07:00. The generated content draws
three ordered, non-overlapping proportional rectangles. The 45-minute interval
uses a distinct dark fill and visibly interrupts the two lighter sleep ranges;
the French legend names it `Long réveil`, never `AWAKE`.

## Extraction and visual evidence

Real Firefox exported a seven-night French PDF through the embedded production
Web bundle and an isolated synthetic database. `pdftotext` confirms the French
headings, legend, observations, pluralized seven-day warning and absence of the
forbidden English labels. `pdfinfo` reports one unencrypted A4 landscape PDF
1.4 page without JavaScript. `pdftoppm` produced the retained raster used for
visual inspection of headings, rows, temporal intervals, medications,
observations, legend and pagination.

- [Seven-night French PDF](evidence/sleep-diary-fr-7-days.pdf)
- [Rasterized first page](evidence/sleep-diary-fr-7-days.png)
