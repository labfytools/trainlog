# Sleep Diary: capture, review and PDF

This guide describes the Web and Android behavior in the 0.1.7 development
source. The [current state](current_state.md) owns rollout
and validation status; [synchronization exchange](sync_exchange.md) owns the
versioned Sleep companion. Sleep facts and medication intakes are not a medical
interpretation or an automatic training decision.

## Capture and correction

Android's quick actions record bedtime (`Couché`), a waking interval
(`Réveil`), final get-up (`Levé`) and time-stamped medication intakes. An intake
may be recorded during the day or before bedtime. A pre-bed intake creates or
reuses the same durable night/day entry; `Couché` later adds to that entry.
There is no separate medicine log to fill out. Quantity is a count of the
separately stored per-unit dose. Each meaningful edit advances the entry's
causal revision; correction uses the existing entry rather than inventing
another night. The Android owner and draft/quick-publication rules are detailed
in [Android](android.md#sleep-diary-v1-and-active-v2-companion).

The Sleep day is assigned using the local 18:00-to-18:00 boundary. Each Web
agenda row and its heart-rate detail use that night's exact offset and a
24-hour elapsed timeline. A missing factual sleep interval may receive a
clearly labelled presentation estimate when both bedtime and final get-up
exist: bedtime plus 45 minutes through final get-up minus 10 minutes, minus
structured waking overlap for the displayed duration. It does not write a
Sleep fact. No missing heart-rate samples or sleep stages are manufactured.

## Review the agenda

In Analyse → Sommeil, the agenda is newest first. Its single display selector
shows 7, 14, 21, 28 or All **recorded entries** per page; seven is the first-use
default. These are row counts, not complete calendar weeks: seven entries can
span more than seven dates. The choice is browser-local and survives reload;
previous 5/10/15 choices migrate to 7/14/21. The other Analyse sections keep
their own period control. Select a night to inspect and edit that entry and
its measured heart-rate timeline. Paging does not discard the selected night,
editor state, loaded statistics or PDF range.

Saving a Web entry creates a durable local revision. The editor's **Allow
synchronization** action publishes that exact revision; only a current
published revision is eligible for the Sleep synchronization companion. A
subsequent edit needs a new publication action. The editor shows local draft,
ready, synchronized or modified state so this choice remains explicit. Agenda
rows and the PDF do not display publication status because it does not describe
the recorded sleep or affect PDF inclusion. The publication API and durable
state are unchanged.

Blue sport bands come from completed session start/end history, not the
Program plan. A missing or invalid end is shown as a point marker with no
invented duration. The Web agenda omits the redundant sport text list; the PDF
retains a dated sport appendix. Sport does not become a Sleep event or change a
sleep total. If the full read reaches its 3,660-night cap, the Web reports that
completeness cannot be established instead of silently treating a truncated
set as All. Missing data and load errors have explicit states.

## Preview and export

The vector PDF preview and download share an inclusive range of **night start
dates**. That range is initialized from loaded history and is independent of
both the visible agenda page and selected detail night. Sport is read completely
before preview/export. The PDF uses the same 18:00 geometry as Web, prints
factual sport intervals in blue, and lists sessions even when there is no
matching Sleep row. Treatments and remarks wrap by available width and continue
on dated pages instead of truncating silently. Review the chosen range before
export; a PDF is a local copy of potentially sensitive information.

The PDF prints the newest selected night first and orders its sport appendix by
actual session start, newest first. Events and medication intakes within each
night keep their chronological order. A light, printable header shows the
selected period and generation date; alternating pale rows and a restrained
accent distinguish nights. Publication status and global validation warnings
are omitted from the report. This presentation does not alter Sleep facts,
timing, sport attribution, synchronization or the database schema.

The descriptive heart-rate rise/fall overlays compare measured BPM with a
recent reference. They do not infer sleep stages, causes, readiness or medical
conditions. The source-backed projection and evidence limits are summarized in
[current state](current_state.md#versions-and-compatibility) and the
[documentation review](reviews/documentation_deep_review_v1.md).
