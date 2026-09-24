# Android application

## Sleep Diary V1 and active V2 companion

Android schema v26 additively mirrors the Sleep Diary revision/event model for
local-first capture. The unreleased schema-v26 contract also includes the
revisioned medication catalog and entry-revision-owned intake snapshots. A
catalog default dose is only a form default; each intake retains its actual
optional positive dose and unit. Existing development databases already at
v26 receive the missing additive tables during the normal open transaction.
local-first field capture. Its compact Morning and Day sections edit one
durable night/day entry without reproducing the desktop/PDF grid. Creation,
correction and logical deletion are revision guarded and survive repository
reopen. Full-generation exchange transports the companion, never SQLite.
Each event, intake, appreciation, date or note edit is saved locally at once;
closing the screen or killing the process does not require validation or sync.
The screen resumes the most recent entry and distinguishes draft, ready,
synchronized and modified tips. “Validate day” marks the current revision for
the existing global synchronization action and never makes it immutable.

Android schema v32 additively stores each immutable medication-intake
`quantity` as an integer in `1..99`, with `1` backfilled for historical rows.
It also adds a separate quick-publication marker for the exact current
revision. Quick publication makes a current fact eligible for generation; it
does not validate a day and does not change the existing validation/ACK
lifecycle.

The quick Sleep capture derives one night identity from the existing
18:00-to-18:00 date rule. A medication action can create that pending entry
before `BED_TIME`, later medication actions reuse it, and Couché appends
`BED_TIME` to the same stable `entry_id`. Réveil and Levé remain unavailable
until bedtime. The compact quantity control defaults to one and resets after a
successful intake; the per-unit dose and unit remain independent structured
facts. Pending entries persist through restart and can synchronize without
inventing bedtime.

The persistence regression begins from an empty pending revision, records two
pre-bed intakes (one with quantity two), and reopens the repository after
intake, Couché, Réveil and Levé. It then proves the same stable entry and intake
snapshots through generation, desktop import/ACK, exact replay, rejection of a
stale intake-free ancestor and a later current revision. This is the executable
contract for lifecycle and synchronization retention; it does not infer facts
that are absent from a device database.

The one-tap Réveil action records `long_awake` rather than a point
`night_get_up`: its factual start is the tap timestamp and its policy end is 30
minutes later. Another tap inside that window extends the same interval to 30
minutes after the latest tap; Levé clamps an overlapping interval to the exact
final-get-up timestamp. This explicit default requires no Rendormi interaction,
remains revisioned/undoable, and is not derived from heart-rate measurements.

Sleep-owned HR capture remains sample-driven. A pending medication alone opens
no capture. After Couché, the first fresh real measurement creates or resumes
one `context_kind=sleep` capture for that entry, with `started_at` equal to the
factual bedtime and each measurement retaining its observed timestamp, BPM and
raw RR values. Réveil neither stops nor splits it; Levé closes it at the exact
final-get-up timestamp. No sensor measurement means no fabricated capture or
sample.

## Background USB and private Drive synchronization

SyncScreen and the explicitly enabled background service invoke one
`SyncGenerationCoordinator` above `SyncGenerationService`. The shared guard
permits one device conversation at a time. The USB listener is a real
`dataSync` foreground service with a dedicated visible notification; disabling
the setting stops it. It validates a request and generation before any domain
mutation and never starts a prepared workout.

Private Drive access uses a user-selected system document-provider folder and
persisted folder-only permission. Disconnect revokes that permission and
cancels periodic work. WorkManager performs a connected-network check at
connection and periodic checks thereafter; Android's minimum periodic interval
is 15 minutes and OEM scheduling can add latency. Drive therefore provides
durable fallback, not an undocumented instant push promise. Android 15+ does
not permit a `dataSync` foreground service to be launched from
`BOOT_COMPLETED`, so the USB listener requires user activation again after
reboot. Local data remains usable offline and no SQLite file enters Drive.

Android schema v22 adds a pending manual-preparation store which is separate
from AI proposals and the exactly-one active-session singleton. A received
preparation is inert until explicit start; an occupied singleton is never
overwritten. Starting preserves the desktop-reserved execution `session_id`,
copies targets only and creates no performed facts. A Program-owned preparation
also preserves `source_program_id` and `source_program_session_id` through the
active draft and completed session. A legacy stored delivery may receive those
two omitted fields later only when its revision, complete prior payload and
reserved execution identity match exactly; this repairs provenance without
altering performed workout facts.
Acknowledged historical deliveries use the same exact-identity recovery. A
local continuous duration target is validated during generation capture while
the completed duration, speed and distance remain represented by the existing
`continuous` history payload rather than an invented set target.

Android schema v23 adds the durable manual-preparation withdrawal ledger. The
schema is unchanged by Web proposal withdrawal: the existing durable AI draft
state stores the V2 companion's `deleted` tombstone.
Consumption is part of the outer generation transaction. Exact pending
deliveries are changed to `cancelled`; exact started deliveries remain started,
and their active or completed execution data is untouched. The stored result is
`pending_cancelled`, `execution_preserved`, or `no_matching_delivery`.
Withdrawal replay is idempotent, and an older preparation delivery is skipped
once the withdrawal identity for that preparation exists.

Android schema v24 adds a read-only synchronized Programs projection:
`synced_programs`, `synced_program_sessions`, `synced_program_entries`, and
`synced_program_deletions`. It presents received program and session detail in
Sessions without offering import, archive, delete, preparation, or other
program mutation actions. The projection is separate from capture data, AI
proposals, manual preparations, and the active-session singleton.

Android schema v25 adds paired nullable Program provenance to the active draft
and completed session. Starting a Program session allocates one ordinary stable
execution identity, copies targets without fabricating performed values, and
respects the existing singleton. Replay resumes the matching draft, refuses an
unrelated draft, and refuses a second execution after completion. Finalization
copies the same Program identities into completed history transactionally.

## 1. Purpose

The Android application is Trainlog's low-friction capture client.

It is a native Kotlin/Jetpack Compose application with local SQLite persistence.

The desktop remains the canonical long-term history and analytics store.

## Capture-first interface

The Android interface uses one compact dark design system for cards, action
tiles, buttons, selection states, and destructive actions. Controls share a
restrained corner radius instead of screen-specific pill shapes. The Home
screen is limited to starting or resuming capture, recent BODY ZONES exposure,
synchronization, and body measurements. Analytics and Statistics are not
mobile navigation destinations; the desktop remains their owner. The local
statistics-compatible data and exchange contracts are unchanged.

Sessions exposes manual capture, prepared drafts, completed history, and
Programs as structured actions. Program cards show status, dates, session
count, and completed progress. Program detail shows each planned session as
`todo`, `in progress`, or `completed`, with type, date, exercise count, and a
start/resume action. Starting continues to use the schema-v25 durable singleton
and stable Program provenance described below.

The active editor uses side-by-side Training/MAX selection and compact set
cards. Repetitions and load share one row when space permits; the per-set
delete action stays inside the card and retains its confirmation contract.
Exercise filters use wrapping selection tiles, while catalog rows, drafts,
exercise actions, synchronization actions, Settings actions, and latest MAX
entries use the same card/button vocabulary.

## Interface language (`TRAINLOG_I18N_V0_1_1`)

Trainlog version 0.1.2 is synchronized with the desktop TUI as one product
version; Android does not have an independent interface version. French is the
default interface language. The only alternate language is English, selected
from **Settings → Language**. The selection updates the Compose UI immediately.

`LanguageSettingsOwner` stores exactly one language tag in its dedicated
`trainlog_presentation_settings` SharedPreferences file. An absent or invalid
value deterministically selects French. This is presentation state only: it
does not call the repository and is never stored in Android SQLite, exported,
imported, or synchronized.

Translations apply to Trainlog-owned UI text, accessibility text, and visible
number/date formatting. They never translate user exercise or catalogue names,
stable IDs, JSON/protocol literals, training data, schemas, AI proposals, MAX,
or feedback. Recognized BODY ZONES are presented from their stable IDs; an
unknown future ID deliberately retains its catalogue label rather than changing
domain data.

The current synchronization view renders local typed status and counters. It
does not translate, reinterpret, or inject a raw desktop/core/receipt/history
summary: those protocol and operational bytes remain opaque. This presentation
boundary changes no Android import, export, receipt, or history format.

The active-session editor presents its add action as a full-width semantic
success button. Existing occurrences can be moved from a dedicated right-side
drag handle; the resulting list is persisted once the gesture completes. The
permutation retains `session_id`, `entry_id`, `exercise_id`, equipment, targets,
actual form values, MAX results, and feedback ownership. A failed write reloads
the last durable ordering instead of leaving presentation and SQLite divergent.
The pointer gesture is keyed by stable `entry_id`, accumulates its full vertical
delta, and calculates a direct bounded destination from the gesture origin. A
single drag can therefore cross several positions while the handle follows the
residual pointer offset and surrounding occurrences move away. Cancellation
restores the origin permutation; completion performs one draft write.

Decimal capture fields use a decimal-capable Android keyboard and retain raw
editing text. Both `.` and `,` are accepted independently of interface language,
including intermediate forms such as `0.`, `0,`, `.3`, and `,3`. Commit replaces
`,` with `.` and parses a finite locale-independent number before applying the
existing positive/non-negative domain rule. Distance remains explicitly km; no
unit guessing or automatic correction occurs.

## Session exchange V3 and optional Programs companion V1

`trainlog-programs` V1 is an optional staged controlled-generation companion,
not a replacement for mobile export V3. The PC publishes `programs-v1.json` as
a full snapshot of nondeleted programs plus unacknowledged program-deletion
tombstones. Both peers advertise the `programs-v1` capability; Android applies
the projection and deletion state durably, and desktop records the correlated
ACK before marking the tombstone acknowledged. It changes neither
`TRAINLOG_FORMAT_V1`, mobile V3, catalog exchange, nor preparation exchange.

Android publishes the separate optional `trainlog-program-executions` V1
companion in full generations. It contains only stable Program/session/execution
identities, `in_progress` or `completed`, and an observation timestamp. It does
not publish Program definitions and therefore does not transfer Program
ownership to Android. Older consumers may ignore this optional companion.

Completed session occurrences persist an `entry_id`; it is never regenerated
for exchange. Android schema v14 added feedback roots and completion `ended_at`
anchors; v15 adds immutable correction history and the separately versioned
`trainlog-training-feedback-v2.json` (while still reading V1). No audio is stored. Android publishes
`trainlog-mobile-export-v3.json` as the active
desktop snapshot and imports `trainlog-pc-mobile-export-v3.json` after the PC
catalogue. The artifact preserves occurrence order, continuous metrics, set
weights and equipment. The legacy V1 contract remains separate and readable.

Android schema v16 adds deterministic current exercise-profile ancestry. Its
v15 migration seeds a shared legacy root without changing catalogue or
historical occurrence values; later local edits and peer reconciliation advance
lineage explicitly inside the same repository transaction as the current-profile
change. The insert trigger owns only initial root creation.

Android schema v17 adds the separate `trainlog-ai-session-drafts` V1 proposal
collection. It is not a second active workout: the existing
`active_session_draft(id=1)` remains the exactly-one durable capture draft.
The companion is imported only after definitions, aliases, profile state,
catalogue, sessions, and equipment associations have been reconciled. It
accepts at most 256 strict, idempotent `aid_<uuid-v4>` proposals; each has at
most 64 contiguous ordered SETS entries, target sets 1..99 and REPS targets
1..999 (or the profile-required duration target). Entries must match the
resolved exercise profile and known equipment.

Pending proposals can be explicitly started or deleted. A desktop V2 proposal
withdrawal uses that same permanent tombstone boundary, removes only proposal
entries, and leaves started drafts and performed sessions untouched. Start is one local
transaction: it refuses when the singleton active draft exists, copies targets
only, marks the proposal `started`, and removes its proposal entries. Delete
is gated by a destructive confirmation naming the proposal; Cancel, Back and
outside dismissal perform no mutation. Only explicit confirmation invokes the
existing repository operation, which marks it `deleted` and removes entries.
Both retained state rows are tombstones,
so an unchanged companion replay is a skip and cannot resurrect a started or
deleted proposal. Neither action manufactures performed sets, completed history,
MAX, or feedback.

Android schema v18 additively gives the execution draft its future stable
`session_id`, revision ancestry and `started_at`; adds bounded pending draft,
revision and finalization storage; and adds causal note state plus optional
observation-to-session ownership. Finalization inserts the completed session
with the same stable identities and records its ledger in the transaction that
removes the active draft. Retrying finalization is idempotent, and a stale
draft replay cannot recreate the finalized draft. Raw partial form strings
remain local and are absent from the lifecycle artifact.

The manual synchronization action publishes one prepared Android→PC bundle
before it exposes `trainlog-sync-request-v1.json`. The bundle contains mobile
V3, mobile equipment definitions, equipment associations V2, exercise aliases
V1, BODY ZONES V1, Training Feedback V2, and Exercise Profile State V1. The
request is not written if any required publication fails. After the user
enables `MANAGE_EXTERNAL_STORAGE` in Android settings, the fixed direct path
`/storage/emulated/0/Documents/Trainlog` is the sole authority for inbound and
outbound exchange. Trainlog creates it if absent and requires a readable,
writable directory. No tree URI, persisted URI permission, directory picker,
`DocumentFile`, `DocumentsContract`, or MediaStore row participates.

Each JSON payload is written to a uniquely named temporary file in that same
directory, flushed and fsync'd, then moved over the exact canonical filename
with atomic replacement where the filesystem supports it. This prevents SAF
provider suffixes. Existing `name (N).json` files are unrelated historical
objects and are never selected, overwritten, moved, or deleted. The next sync
enumerates again so PC/MTP changes are not hidden by a long-lived cache.

When permission is absent, synchronization fails before publication and the UI
offers **Autoriser l'accès au dossier Trainlog**. This opens the package-scoped
all-files settings screen, falling back to the global all-files screen only if
the OEM exposes no package activity. Android itself grants the permission; the
application never does. Legacy SAF preferences are ignored and removed without
touching `Download/Trainlog` or its recovery backup.

V3 preserves ordinary plan metadata atomically with occurrence identity,
equipment, actual sets and MAX. V1/V2 remain readable legacy artifacts and are
never silently rewritten as V3.

Completed Session Detail exposes **Modifier la séance**. Its bounded form is
initialized from occurrence snapshots and writes nothing before Save. Save
replaces all children in one transaction while retaining `session_id`, every
retained `entry_id`, feedback roots/revisions, and the session follow-up parent.
The form edits only facts allowed by each stored occurrence profile, including
repetitions, set load, set duration, continuous duration, distance in kilometres,
and speed in km/h. The existing V3 snapshot republishes those corrected facts,
so synchronization updates the same stable session and occurrences rather than
creating correction IDs.
The same dedicated right-side handle can reorder completed occurrences in this
form. Its list permutation remains transient until Save; Cancel or Back performs
no repository mutation. Stable `entry_id` keys keep Compose editor/focus state
with the same occurrence even when duplicate exercises exchange positions. Save
persists facts and positions together through the same atomic replacement, and
a failed Save restores the original completed order with all child ownership.
Completed decimal fields likewise retain keyed raw text until Save, so typing a
separator is not collapsed into a premature numeric value. Invalid incomplete,
malformed, non-finite, or negative final values block the transaction with a
localized error.
Set and occurrence removal require explicit confirmation; Cancel writes
nothing. Exercise Detail separately exposes **Modifier l'exercice**. A
confirmed incompatible catalogue edit controls future Android occurrences;
completed and active-draft occurrence snapshots are never converted.

## 2. Implemented navigation

```text
Accueil
Séances
Exercices
Statistiques
Synchronisation
Paramètres
```

The fixed Android application shell uses a Material 3 modal drawer, a
`Scaffold`, system sans-serif typography, and local small VectorDrawable
icons. Each of the seven destinations is a complete drawer item with a
minimum 48 dp touch target. The active drawer section is derived from the
canonical route and exposes selected semantics; it is not maintained as a
second navigation state. The drawer itself scrolls when vertical space is
limited.

The section screens use the same dark semantic theme and existing typed-route
callbacks. Home presents Synchronisation and Mensurations as two equal quick
action tiles. Sessions presents manual creation and imported drafts in one
row, with completed history below. The exercise catalogue keeps its search and
zone filters visible above a bounded, internally scrollable result viewport.
The active-session create/save actions share one button row, while completed
session edit, exercise feedback, and session follow-up actions use prominent
full-width buttons. These are presentation rules only; they do not introduce
navigation, repository, draft, synchronization, or persistence behavior.
All user-facing removal triggers use the shared full-width destructive button:
its only visible content is the trash icon, its container uses the semantic
error color, and its context-specific accessible name remains exposed to
TalkBack. Existing confirmation gates and their mutation callbacks are
unchanged. Side-by-side action rows use equal weights and an intrinsic common
height so the shorter action stretches to match its sibling.

## STATS_V1

The Statistics landing page defaults to 30 days and is a Material 3 dashboard using the existing
Catppuccin Mocha/Lavender semantic theme. It filters a read-only projection by
7 days, 30 days, 90 days, one year, or all history, then presents performance,
body-measurement, and weekly-frequency cards plus detail routes.

The landing performance chart contains weekly classified event counts, never
summed or averaged kilograms. Every actual performed set is an observation. A
working event is strictly later in canonical time and greater than the prior
best for the same canonical exercise, nonempty resolved equipment, external
equipment semantics, external-load mode and exact performed reps/duration dose.
Explicit MAX uses a separate strictly-later-record-above-prior-MAX rule in the
same exercise/resolved-external-equipment context. Equal-instant IDs only order
presentation and never create events. A one-point class
cannot create an event; context-owned raw series remain available in the
exercise drill-down. Assistance, plans and missing actual loads are omitted.

The selected-period summary reports actual sessions, performed sets, distinct
actual exercises and explicit MAX records. Body curves use only recorded
observations (no fabricated interpolation), are bounded and ordered by the
most recently observed metric; one point has no delta or graph. Observable session
history and frequency include a stable session ID exactly once when one of its
occurrences owns a persisted performed set, continuous activity, or explicit
MAX result; `ended_at` is not an inclusion gate. Planned targets and empty
occurrences do not count. MAX sessions are labelled separately. Calendar buckets use the local date
written in each persisted timestamp (including its literal offset), with Monday
as the first day of the week; rolling windows and chronology still compare the
exact represented instant and never use the device timezone.
Malformed legacy timestamps are omitted only from the statistics projection;
the dashboard remains available and reports that invalid history was ignored.

The section roots are:

```text
Accueil          durable-draft resume, manual capture, recent BODY ZONES exposure, concise local links
Séances          current session, programme a session, manual entry, completed sessions
Exercices        catalogue, detail, create, and contextual edit
Legacy equipment catalogue/detail routes remain compatibility-only and are
not exposed in the normal drawer.
Statistiques     STATS_V1 dashboard, conservative graphs and detailed local drill-downs
Synchronisation  existing request, result, and diagnostic workflow
Paramètres       exchange-folder authorization and explicit PC-catalog refresh
```

`AppRoute` is typed and carries stable IDs and an explicit caller where a
detail or creation workflow needs one. `AppNavigationController` is the sole
owner of route transactions in the Compose root. Its bounded history returns
inline exercise/equipment creation to the caller exactly once. Back first
resolves visible input/overlays and the drawer; it then follows the caller or
route history, falls back to the section root, then Home.

Leaving an unaccepted generator proposal or a dirty exercise, equipment, or
measurement form installs an explicit keep/discard guard. Keeping retains the
transient editor state and its current route; discard clears only the
transient state that raised the guard. Route changes do not save, synchronize,
finalize, or otherwise write repository data. A successful generator
acceptance clears its transient proposal before routing; an
`existing_active_draft` result remains guarded and retains the proposal until
the user resolves it.

Route content uses a `SaveableStateHolder`, keyed by stable route identity,
with a 16-entry bound for modest list/scroll state. The root `ViewModel`
retains navigation and transient forms across Activity recreation. It does not
serialize a session or generated proposal into a Bundle: after process death,
only the repository-owned durable active draft is restored through Home.

## 3. Local persistence

Android local database version:

```text
11
```

Domain tables cover:

```text
exercises
sessions
session_exercises
performed_sets
continuous_activity
body_observations
exercise_body_zones
exercise_body_zone_sync
```

This database is Android-local. It is not copied to the PC.

Schema v4 introduced `active_session_draft`, `draft_session_exercises`,
`draft_performed_sets` and `draft_continuous_activity`. The implemented
additive v4 -> v11 chain preserves catalog, completed sessions/actuals, body
observations and the draft while adding the shared equipment catalogue,
occurrence-level equipment links and stable completed/draft `entry_id` values.
Exactly one active draft is supported; it is separate from completed history.
Schema v14 also gives its occurrences durable Android-local
`draft_exercise_feedback`. The active-session **🎙 Ressenti** action saves there
immediately and reads all observations back in timestamp/ID order. Multiple
records are allowed. They survive process restart and transfer by stable
`entry_id`, without regenerated feedback IDs, in the atomic finalization
transaction. Explicit draft discard cascades this draft-owned data.
Schema v9 adds explicit completed/draft MAX results. Schema v10 additively
stores direct primary/secondary body-zone relations and their private sync
baseline; the taxonomy itself remains the shared manifest asset. Schema v11
additively stores optional occurrence/draft planning metadata after the explicit
v10 -> v11 migration; existing rows retain `load_mode=none`, zero rest and NULL
targets.

## Session generator V1

The V1 generator remains implemented but hidden from normal UI; Home exposes no
generator action. The generator uses the shared frozen
policy for all 11 selectable BODY ZONES, four goals, policy duration presets
and custom bounds. A generated preview is read-only until acceptance and
explains target dose, rest, equipment, observed-load source or absence,
exposure/recency, and explicit shortages. It never claims measured recovery.

Users can edit, remove, reorder, regenerate or cancel the in-memory proposal.
Acceptance of a nonempty proposal is one transaction into the ordinary active
draft, with target plans and zero actual rows. An existing draft yields the
non-mutating `existing_active_draft` conflict. Empty results cannot be accepted;
nonempty partial results may be accepted and edited normally. Final completion
continues to require actual captured work.

## Body focus Home V1

Home displays **Zones à travailler** immediately after the active-session or
manual-session action. This is a read-only description of recent recorded
training exposure. It does not estimate recovery, readiness or fatigue and it
does not prescribe or generate a session.

The production `getBodyZoneHomeOverview(now)` read model processes exactly the
eight meaningful child zones `chest`, `back`, `shoulders`, `arms`, `core`,
`glutes`, `thighs` and `calves`. Grouping zones `upper_body`, `lower_body` and
`full_body` are never overlays and never contribute duplicate exposure. Only
completed occurrence-owned `performed_sets`, `continuous_activity` and
`max_results` are evidence; targets, empty occurrences and active drafts are
excluded. Timestamps use the canonical parser and inclusive rolling instant
boundaries, never SQLite date functions or lexical ordering. Alias history is
resolved to the canonical exercise identity and counted once.

Each zone reports the last primary and secondary exposure, primary/secondary
actual-work counts for 7 and 30 days, distinct exposed-session count for 30
days, active canonical exercise availability, a textual state and reasons.
Primary exposure is the dominant signal. Secondary exposure is only a weaker
modifier and can never stand in for missing primary exposure.

Eligible zones have at least one present canonical exercise with a direct
resolved child-zone relation. Unsupported zones remain visible with **Aucun
exercice résolu disponible** but are never recommended. The top three (or
fewer) use this exact stable ordering: no primary exposure first; oldest last
primary exposure; lower primary work over 7 days; lower primary work over 30
days; lower secondary work over 7 days; lower secondary work over 30 days;
oldest last secondary exposure; stable `zone_id`. Empty history says **Pas
encore d’historique d’entraînement.** and ranks only supported zones.

The compact side-by-side Compose **Face**/**Dos** maps use independent filled,
anatomically contoured cubic `Path` regions for shoulders, chest/back, arms,
core/glutes, thighs and calves. The canvas contains no zone-name text and is
only a stylized BODY ZONES selector, never medical anatomy or a recovery model.
Direct taps are resolved against the same normalized paths used to draw each
region. Selection changes both fill and a high-contrast outline. Separate
transparent accessibility controls follow the region bounds, provide at least
48 dp targets, use the Button role, and announce view, zone, textual exposure
state and selected state; color is never the sole signal. Zone details remain
below the maps and show last-primary age, 7-day primary/secondary work and
available exercise count. **Voir les exercices** opens the existing Catalogue
with its body-zone filter, while **Voir les statistiques** opens the existing
Statistics landing route.

Load editing offers compact choices for automatic V1 qualification, a
user-selected `%MAX`, or no numeric target; direct manual kg remains available.
`%MAX` accepts an integer 1..100 and uses exactly
`MAX × percentage / 100` only for the chronologically latest explicit MAX of
the same exercise ID and same external-resistance equipment ID. Assistance and
incompatible/missing contexts produce an empty target labelled
`compatible_max_unavailable`; this is unavailable rather than a fallback or a
recommendation. Only the resulting plan `target_weight_kg` is persisted on
acceptance; the percentage and MAX provenance are transient. Zone, objective,
duration and load choices use compact localized chips and do not expose
internal IDs.

The ordinary set-based manual exercise form exposes the same separation from
actuals with **Valeur en kg / % de mon MAX / Aucune**. It composes the confirmed
choice into the existing `SessionExerciseDraft.plan`; performed-set weights are
never used as target storage. Editing an existing generated/manual occurrence
preserves its target dose and rest while allowing the numeric target to change.
The read-only `%MAX` lookup shows the compatible MAX date/value and calculated
target before confirmation, recomputes for exercise/equipment/percentage
changes, and performs no draft mutation by itself.

The preview shows requested and estimated duration, warns on a meaningful
shortfall without padding, and states that warm-up and cool-down are absent in
V1. `SESSION_GENERATOR_V2` remains future-only.

## 4. Exercise catalog

Exercise creation records:

```text
name
recording_mode
tracking_mode
data_fields
primary_zone_id       nullable only for explicit unclassified/history cases
secondary_zone_ids   zero or more distinct canonical IDs
```

Stable identity:

```text
ex_<uuid-v4>
```

The UI rejects invalid profile combinations and local normalized-name
collisions.

An exercise may be created standalone or inline while building a session.

The form groups translated values from `catalog/body-zones-v1.json` under
**Membres supérieurs** and **Membres inférieurs**, with autonomous **Corps
entier** and **Abdominaux / tronc** entries. Group nodes organize and filter;
they cannot be assigned directly. A new set-based exercise requires one
primary zone. Selecting it removes/disables that same ID among secondaries.
Historical unclassified exercises remain visible as **Zone : Non renseignée**
and may be classified later.

The existing exercise list combines SQL-backed normalized prefix search with a
zone filter. Parent filters include descendants, **Non renseignés** selects
only exercises with no relations, and **Toutes les zones** never hides those
rows. Each row displays primary, secondaries and the primary's derived group.

### Editing an exercise

Every existing catalog item exposes **Modifier**. Editing a name trims its
input, recomputes `normalized_name`, and rejects a normalized-name collision.
The row retains its existing `exercise_id`; naming is presentation metadata,
not identity. Completed session rows and the active draft retain their catalog
row relationship and immediately resolve the renamed display text after reopen.

Profile fields (`recording_mode`, `tracking_mode`, `data_fields`) are editable
only while an exercise has no completed-session or active-draft reference. Once
referenced, Android displays the lock and returns an explicit incompatible
profile result rather than silently reinterpreting work or creating another
exercise. Renaming remains available independently.

The shared Compose shell supplies navigation context and the page header. Its
content host provides one scrollable destination area; screens do not redraw a
global banner or own a competing navigation control.

## 5. Session recording

Stable session identity:

```text
se_<uuid-v4>
```

Session entry is profile-aware.

### Sets + repetitions

Actual set values may be heterogeneous. The `SETS + REPS` editor presents
ordered rows, each with its own repetitions and optional load. **Ajouter une
série** appends one blank row and **Supprimer la série** removes only the chosen
row; editing or removing a row does not alter the remaining row values.

The load heading is **Charge (kg)** for external resistance and
**Assistance (kg)** for assistance equipment. A blank load is no recorded load,
not `0`; an entered load is finite and non-negative, and French decimal commas
are accepted. The durable raw form preserves
partial row input (including a blank row or a fragment such as `32,`) across
draft save and restore, and a failed validation or draft write presents a
specific error without claiming the row was saved.

When an older compact raw draft is reopened, its repetition text can be
expanded into the row editor from:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

### Sets + duration

Each performed set stores its own duration.

### Continuous + duration

The form asks for duration and only the configured supplemental fields such as
speed or distance.

Continuous work does not create fake sets.

### Occurrences, equipment and actual loads

The same catalogue `exercise_id` may be added more than once to a session.
Every occurrence receives its own stable `entry_id`, position, actual sets and
optional equipment selection. Editing one occurrence replaces only that entry;
it does not merge or alter another passage of the same exercise.

`Machine / équipement (optionnel)` searches the shared manifest by display
name, physical-machine label and aliases. A selected equipment identity is
stored on that occurrence in both the active draft and completed session.
For `SETS + REPS`, the form is a row editor: every set owns an independently
editable repetitions field and optional load field, and rows can be added or
deleted without changing their neighbours. French decimal commas are accepted.
`Assistance (kg)` is an explicit alternative load semantic, not an external
charge. Empty load and an entered zero remain distinct. The completed-session
detail renders the persisted rows in order with the matching Charge or
Assistance heading, including an explicit empty-load marker.

## 6. Session draft editing

The repository durably saves every meaningful mutation, including session type,
exercise selection/addition/removal, actual values and raw per-set form edits.
Partial row text such as `32,` is retained without normalization. A failed write
displays a specific error and does not claim the latest change was saved.

Home shows **Reprendre la séance en cours** and an exercise-count/type summary.
The ordinary new-session action opens an existing draft without overwriting it.
Back returns Home and preserves the draft. Backgrounding, switching apps,
Activity/configuration recreation, background process death and force-stop with
relaunch preserve the draft; these paths were validated on the Samsung SM_G990B.

**Retirer <exercice>** removes only that draft exercise and its actual values.
It does not change the catalog or completed history. Removal survives restart.
**Supprimer la séance en cours** requires deliberate confirmation; cancellation
preserves the draft. Confirmed deletion leaves no completed session or stale
resume action after relaunch.

Final save validates the durable draft, inserts the completed session and actual
values, and removes the draft in one SQLite transaction. Failure rolls back and
retains the draft for retry; repeated completion does not create duplicates.
Planned targets remain prescriptions: finalization accepts fewer or more valid
performed sets and actual repetitions or loads that differ from their targets.
Finishing a target-only continuous occurrence with no supplemental fields
materializes its performed duration from that occurrence's already persisted
timeline start/end interval. This also repairs a preserved draft whose interval
was closed by an older build without creating the performed row. Existing
performed values are never overwritten, and continuous occurrences that require
speed or distance still require those measured values explicitly.

Ordinary completion uses the save-time timestamp. A private recovery build may
embed one exact session identity and one explicit offset-aware factual end time;
only that matching draft uses the supplied time. The repository rejects an end
before the session start or latest exercise marker. When immutable heart-rate
samples arrived after a failed factual session end, the session retains the
factual end while the stopped capture remains bounded by its latest retained
sample; no BPM or RR measurement is deleted or retimed.

PC catalog reconciliation preserves draft references through catalog row
ownership. If an editing selection no longer resolves, only the selection is
cleared; added exercises and raw text remain, with a specific diagnostic.
When a received catalog entry has the same `exercise_id`, a changed display
name is reconciled in that same row. When the incoming PC identity differs but
the normalized name matches, Android rekeys or merges only if recording and
tracking modes match, the bounded `data_fields` masks are comparable by
inclusion, and overlapping equipment metadata has the same load semantics.
The incoming PC identity is canonical and the bit-mask union retains the richer
profile. Completed and draft occurrence row IDs, `entry_id`, positions, values,
equipment references and the active selection are moved transactionally. Any
incompatible condition rejects the catalog transaction; name equality alone
never authorizes a merge.
Zone relations follow the same row-identity move only when one side is empty or
both states are equal. Two different non-empty states are an explicit conflict;
the repository never invents an automatic union of secondary zones.

## 7. Session history

Android exposes persisted local session history and profile-aware detail.

Set-based history renders ordered performed sets.

Continuous history renders its one activity record with configured supplemental
values.

## 8. Body measurements

Supported metrics:

```text
weight
neck
shoulders
chest
waist
hips
left/right arm
left/right forearm
left/right thigh
left/right calf
```

Rules:

```text
empty field = not measured
at least one positive metric required
comma or dot accepted for decimal entry
```

Stable identity:

```text
bo_<uuid-v4>
```

## 9. Automatic mobile snapshot

Android maintains:

```text
Documents/Trainlog/trainlog-mobile-export-v3.json
```

The V3 snapshot is refreshed after relevant local changes, including exercise,
session, body-observation, equipment association and PC-catalog updates. It
preserves `entry_id`, occurrence position, optional equipment and actual
per-set weights and ordinary planning metadata. Android also publishes the V2 companion
`trainlog-equipment-associations-v2.json`; its `set` and `cleared` states are
targeted by `(session_id, entry_id)`.

Before either dependent artifact, Android publishes its user-created equipment
definitions as `trainlog-mobile-equipment-definitions-v1.json`. The strict
`trainlog-equipment-definitions` v1 format uses stable IDs and the fields
`equipment_id`, `display_name`, `label_name`, `equipment_type`, and
`load_semantics`. Absence never deletes a definition; equal same-ID definitions
are idempotent and divergent same-ID definitions conflict. Supplied-manifest
IDs are reserved.

Android also writes `trainlog-exercise-body-zones-v1.json`, the sole zone
companion used in both directions. It contains stable exercise/zone IDs only.
After the file write succeeds, an identical local replay establishes/refreshes
the publisher's shared baseline. A one-sided change is applied transactionally,
and simultaneous divergence returns an explicit conflict without changing
local relations. The unclassified state contains neither a primary nor orphan
secondaries.

On PC → Android inbox import, zone reconstruction is a prerequisite: the app
applies catalog identities, flattened aliases and profile-state first, then the
body-zone companion before sessions, equipment associations, AI drafts and
feedback. Thus a later companion error cannot leave a freshly catalogued
database with an accidental empty zone state ready to be republished. Zone
resolution remains exclusively by stable `exercise_id` and the durable alias
mapping; display names are never used.

The user does not need a separate manual export step before synchronization.

An active draft is never included in completed history, session detail or this
snapshot. Synchronization continues to exchange completed data while the draft
stays local; no draft fields were added to the frozen mobile artifact.
Training Feedback V1 likewise excludes `draft_exercise_feedback`; only its
exact transferred completed rows are exported after finalization.

## 10. PC catalog access

PC-created files are accessed directly after the user enables Android's
all-files access for Trainlog.

The selected folder must be:

```text
Documents/Trainlog
```

The Sync screen always exposes the system authorization settings. There is no
stored folder selection.

No application-data reset is required to fix a wrong folder choice.

## 11. Android-triggered synchronization

The Sync screen exposes:

```text
Synchroniser maintenant
```

Android writes:

```text
trainlog-sync-request-v1.json
trainlog-sync-full-generation-request-v1.json
```

The request is replaced through the same direct `Documents/Trainlog` directory
as the bundle. No document provider can manufacture a numbered request sibling.
The stable `request_id` remains the replay boundary.

The application-start catalog import does not publish an outbound bundle.
Exactly one explicit `Synchroniser maintenant` action invokes one export and
one request publication unless a later, explicit retry is initiated.

and waits for a matching:

```text
trainlog-sync-receipt-v1.json
```

The receipt is matched by `request_id`.

On success Android then applies the latest PC catalog and renders the final
local typed status/counters. Raw receipt summaries remain opaque operational
bytes and are not displayed as injected cross-device text.

Before applying the PC catalog or its V2 artifacts, Android applies
`trainlog-pc-equipment-definitions-v1.json`. Thus custom definitions are known
before a received V2 association references them. Android schema v13 retains
the non-destructive v7 -> v8 migration required for `load_semantics = none`.
It also applies `trainlog-exercise-aliases-v1.json` before catalog and session
reconciliation. The additive v11 → v12 alias table lets legacy exercise IDs
resolve to one live canonical row; a live-source rekey preserves completed
occurrences, the durable draft, selected form exercise, equipment relations,
actuals, MAX and targets. Android republishes the same deterministic companion
with no exercise-merge UI.
After catalog/session/equipment reconciliation, Android applies the same
body-zone companion so custom exercises receive their classifications.

A receipt belonging to another request is ignored as pending rather than
misreported as the current result.

## 12. Synchronization ownership

Android does not initiate raw MTP operations itself.

MTP is host-initiated:

```text
Android request
    -> PC trainlog-syncd
    -> shared desktop sync engine
    -> receipt
```

## 13. Build

Example local configuration:

```bash
cd android

printf 'sdk.dir=%s\n' "$HOME/Android/Sdk" > local.properties

JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew testDebugUnitTest assembleDebug
```

Install to a connected test device:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

`local.properties` is local machine configuration and must not be committed.

The APP_SHELL_V1 Android validation ran the JVM command above successfully:
84 tests ran, with 83 passing and one skipped historical-fixture test. The skip
is `RealAndroidV9BodyZonesMigrationTest.realVersionNineCopyMigratesWithoutChangingExistingTables`,
whose external `TRAINLOG_ANDROID_V9_FIXTURE` was unavailable. The build
produced `app/build/outputs/apk/debug/app-debug.apk` (12,649,975 bytes;
SHA-256 `ddb1221d25db5be60eb2261d4b1dcf0fb7446e2a780c67aae862f37c15b7396d`).

The host regressions include the production Compose callback for an existing
active draft: it verifies that the generator's route, preview and raw input
remain intact until the explicit keep/discard decision, and that keeping does
not write the initialized SQLite database. The expected nine shell icons and
ten referenced catalogue assets were checked byte-for-byte in the APK.

No emulator is installed, and this checkpoint did not install or run on the
connected daily phone. Human visual/accessibility review remains pending for
320/360/393/412 dp widths, 100/130/200% font scale, IME behavior, drawer and
form reachability, long translated/source text, scroll restoration feel, and
TalkBack. See [tests](tests.md) for the broader validation boundary.

## 14. Non-goals

Android is not intended to own:

- canonical long-term analytics;
- complex body/performance graphs;
- cloud accounts;
- direct SQLite-file synchronization;
- exercise-name heuristics;
- a mounted-filesystem dependency.

## 15. Equipment and multi-occurrence exchange V2

During exercise entry, `Machine / équipement (optionnel)` searches the shared
catalogue by display name, physical-machine label and aliases. The selected
canonical ID belongs to that session exercise entry, is durable in the active
draft and completed session, and is visible in session detail. It may be
cleared. The active V3 exchange preserves multiple ordered occurrences of the
same exercise in one session through `entry_id`. The frozen V1 artifacts remain
readable only as legacy artifacts and keep their historical one-exercise
identity assumptions; V1 is not rewritten to claim V2 support.

For a `SETS + REPS` exercise, selecting equipment never changes that exercise
profile: the form retains per-set repetitions and exposes `Charge (kg)`. French
decimal input is accepted (`12,5`); one value applies to all sets or values may
be separated with `;`. Assisted equipment is explicitly labelled
`Assistance (kg)`. Empty load remains distinct from an entered zero.

`Créer un équipement` in that selector opens the existing catalogue creation
route. An explicit successful creation returns automatically to the same
exercise editor, restores the stable occurrence being edited, and selects the
new persistent `eq_…` ID; the draft's other raw fields and target remain
unchanged. No equipment is synthesized merely by opening the route.
The selector presents every matching catalogue entry inside a bounded,
independently scrollable result list; it does not truncate the selectable
catalogue to its first rows. An existing target-only prepared occurrence may
therefore change or create its equipment association and save in place while
retaining its `entry_id`, plan and sibling occurrences. This operation does not
fabricate a performed set; a partially entered performed row still follows the
normal validation path and is never silently discarded.
The shared bundled catalogue uses reserved canonical IDs. User-created IDs are
exchanged first by definitions V1, so a V2 association can resolve them on the
receiving side; conflicts remain explicit and are never converted to null.
The active-session list exposes `Modifier <exercice>`; saving replaces that
entry in place, while cancelling only discards the form and preserves it.

## 16. Test-max sessions

Android session entry exposes:

```text
Entraînement
Test max
```

The selection is persisted in the existing Android `sessions.session_type`
column and exported in the mobile snapshot as:

```text
training
max_test
```

History and detail visibly identify max-test sessions.

Selecting `Test max` is explicit metadata; Trainlog does not infer max tests
from large repetition or duration values.

Its exercise form contains only the existing exercise search, optional
machine/equipment selection, and `Poids max (kg)`. French decimal commas are
accepted; empty is distinct from zero and only a finite positive value can be
saved. Each saved line is immediately visible as `Exercice · Max : N kg`, can
be edited in place with the same `entry_id`, or cancelled without mutation.
Several movement results may be appended successively.

Schema v9 stores the result in `max_results` or `draft_max_results`, never in a
synthetic one-repetition set. Equipment remains occurrence context. History
also lists the newest explicit maximum for each `exercise_id`, with date and
equipment used.

A completed max-test detail exposes `Reprendre ce Test max`. The one durable
draft then records the source `session_id` and retains all existing occurrence
IDs, order, movement IDs and equipment. Finalization atomically replaces that
same completed session and may append new occurrences. The completed source is
left intact as the crash-safe baseline until finalization.

## 17. Audited limitations

Body Zones V1 defines no custom zone creation and no session generator. The
repository already exposes manifest lookup/hierarchy, descendant and
primary-only exercise filtering, direct exercise relations and the existing
latest-MAX/history reads needed for future composition; it calculates no
suggested load.

Android's stored `normalized_name` currently removes diacritics, while the
frozen desktop/Python normalization contract uses NFC, Unicode whitespace
collapse and case folding without accent removal. Existing Marche/Leg press
data is unaffected, but changing this safely requires an explicit Android
schema migration that recomputes every normalized key and handles newly exposed
collisions. Schema v13 changes only the additive machine-exercise metadata and
the approved exact-ID historical contexts.

The bundled exercise/equipment relationship metadata is seeded and preserved,
including during exercise-identity reconciliation, but the current equipment
picker searches all known supplied and custom definitions. Filtering or ranking
that picker by exercise relation remains future gym-catalog policy.

Custom equipment remains identified only by `equipment_id`: Android never
coalesces two different custom IDs merely because their labels match. This can
leave conceptually duplicated definitions created independently on two devices,
but avoids guessing across potentially different load semantics and persisted
occurrence references.

The PC-catalog V1 inbox validates required IDs, modes, names, and bounded field
masks, but unlike the newer mobile V2 and equipment-companion parsers it does
not reject every unknown root or item key. Tightening this published V1 reader
requires a compatibility decision rather than an incidental schema-v10 change.

Android requires `data_fields = 0` for `SETS`, while the desktop model/API
currently accepts known supplemental bits on either recording mode. Supplied
profiles do not exercise this difference. Supporting a future set-based
supplemental field requires an explicit shared-model decision.

## 18. Training knowledge V1 read APIs

Android bundles the six authored training-knowledge catalogs as immutable
assets. `TrainingKnowledgeCatalog` validates and exposes their stable-ID
records; it is not a second manually authored scientific table.
`TrainlogRepository.getTrainingExerciseContext()` composes an exact persisted
exercise with its direct/ancestor persisted zones, optional scientific mapping,
compatible equipment, latest explicit MAX and recent occurrence/set preview.
`listExerciseOccurrences()` and `listExerciseOccurrenceSets()` provide bounded
follow-up pages. Occurrence and set limits are 1–32 and 1–64 respectively.
Cursors order current data chronologically by original timestamp, session ID
and occurrence ID, and do not preserve a snapshot across calls.

This read-only feature makes no further Android schema change (the runtime
schema remains v11), does not seed rows, and does not export/synchronize new
training-knowledge data. It does not implement recommendations or fatigue
scores; planning metadata belongs to the separate schema-v11/session-generator
contract. Its occurrence and latest-MAX readers use the same explicit temporal
grammar, exact fractional comparison and bytewise ID tie breakers as C.
The Android writer's omitted-seconds form is admitted, and emitted cursors
retain the original source text. Production pagination/MAX parity tests pass.
The tranche is `TRAINING_KNOWLEDGE_V1=PASS`. The Android loader enforces canonical exercise
and equipment identity syntax, bidirectional exercise/capability compatibility,
HIGH evidence source type, and non-unresolved BODY ZONE audit evidence. Its
full source and uncertainty contract is in [Training knowledge system V1](domain/knowledge_system.md).

The normal shell exposes manual sessions but not SESSION_GENERATOR_V1 pending
V2. When importing the PC catalog, Android applies the versioned canonical
exercise-name mapping for mapped IDs; unmapped user exercises remain editable.
Equipment Detail reads the shared V2 relation asset, shows verified possible
exercises with French confidence/evidence status, and opens Exercise Detail.
Exercise Detail shows exercise-owned muscles, movements, BODY ZONES, and
compatible physical equipment. Custom equipment receives no inferred anatomy.
Saved exercise feedback and session follow-ups expose **Modifier**. The latest
text is prefilled, resumed dictation appends to it, and save creates an immutable
v15 revision without changing `observed_at` or the H+ label.
The feedback editor presents microphone and stop actions with accessible labels
and keeps the live transcript in a wrapped, multi-line area. Its speech intent
requests 1.5 seconds of complete silence (and 1.1 seconds of possibly complete
silence) before completion. These durations are best-effort hints: Android
recognition engines may ignore them. Trainlog neither fabricates results with a
local timer nor enters an automatic restart loop; natural completion preserves
the captured text, while an explicit Stop remains immediate.

Every user-initiated destructive commit uses an action-specific confirmation.
Cancel and Back are safe, outside taps cannot confirm, and the destructive
button is explicit. Draft occurrence warnings name nonzero performed-set,
continuous, MAX and feedback children; series deletion and whole-draft abandon
use the same gate.
Android schema v19 additively stores the same immutable causal-deletion
operations and current deleted-target state as desktop v20. Existing data is
preserved and no migration-time ancestry is invented. Repository entry points
create, validate, apply, export, and import the staged causal artifact.

Completed-session correction and body-observation relinking advance durable
non-deleted causal state in their mutation transaction. Session deletion
admission covers ordered occurrences, targets, confirmed sets/continuous/MAX,
notes, feedback revisions and linked observations. Draft admission uses the
stable draft revision for active and pending storage; unfinished form strings
remain local and never affect cross-platform identity.

Android schema v20 additively stores installation peer identity, outgoing
generation/artifact state, consumed-generation state, durable ACKs, and the
separate causal-operation publication ledger. Migration from v19 preserves
business, draft, finalization, revision, and causal rows and creates none of
those new records retroactively.

`SyncGenerationService` is an explicit staged entry point. It captures catalog,
V4 history, drafts, aliases, profiles, custom equipment, associations, BODY
ZONES, feedback, and causal deletions in one repository transaction; writes an
immutable private generation; publishes its manifest last; consumes all listed
domains plus the durable generation record in one outer transaction; and
replays the stored logical ACK after restart. The complete-envelope context is
internal and typed: standalone artifact methods retain their causal guards.
This service is not selected by the automatic V3 storage/request path.

## Complete local backup and restore

Settings exposes an explicit plaintext `.tlbackup` export and restore flow for
the same application identity. The versioned ZIP contains a SQLite-consistent
`VACUUM INTO` snapshot, active drafts and raw partial form input, synchronization
peer/generation/causal state, and the owned presentation/sync preferences. A
manifest written last records schema, package, sizes and SHA-256 digests.
Verification stages only the three exact bounded entries, validates integrity,
foreign keys, package, supported schema and digests before closing the live
repository. Restore retains the previous database and preferences until the
replacement validates; failure restores both. This is not an interchange
format and is intentionally unencrypted, so the UI warns the user to protect
the exported file.

The installed schema-v17 release has no backup UI. The reproducible
`tools/build_android_backup_bridge.py` recipe materializes that exact source,
adds only the backup surface, assigns a higher candidate version code and
builds/tests an APK without installing it. Its synthetic v17 archive is also
restored by current code and migrated through the normal v17-to-v20 path.

The authorized private rollout used the bridge only to add the backup surface
without clearing package data, verified the resulting plaintext archive, then
installed the current signed release variant through a strictly increasing
version-code chain. The final installed private candidate retains product
version 0.1.2 and schema v21. Schema v21 adds the verified generation archive
ledger without changing any domain identity. Its full-generation foreground
coordinator waits for correlated objects, tolerates stale objects left by
interrupted runs, strictly revalidates desktop-retained historical ACKs, and
shows completion only after both durable peer acknowledgements.

Each explicit foreground synchronization attempt publishes the Android peer
and enters the full-generation coordinator before emitting one fresh request
signal for `trainlog-syncd`. The signal is only admission/correlation; a
configured daemon routes it to the existing desktop orchestrator rather than
the standalone V3 importer. If transport is
interrupted after generation publication, a later attempt resumes the exact
stored generation instead of recapturing mutable data. An acknowledged Android
generation remains resumable only until the desktop generation for that run is
durably consumed; a fully completed run is then excluded from foreground
selection. This preserves late-ACK recovery without replaying completed runs.

The direct-MTP path confirms transport visibility independently from ordinary
`File.exists()` checks. Android fsyncs and atomically commits every immutable
generation artifact, asks MediaProvider to scan those files, waits for all
bounded callbacks, and only then atomically publishes and scans
`android-generation-v1.json`. The same boundary covers the peer advertisement,
explicit request signal, Android consumption ACK and bounded error reference.
Manifest/reference remain the commit markers; scanning does not change their
bytes or protocol meaning. Production logging records only `run_id`, phase,
artifact name, generation ID and result.

When background synchronization is enabled, an explicit UI request announces
foreground ownership before refreshing the foreground listener. Service
liveness and conversation ownership are separate: the service may keep the
process alive, but it cannot acquire automatic ownership while explicit intent
is pending. The process-wide arbiter gives `NEW_EXPLICIT` priority over
`RESUME_BACKGROUND`; a background waiter observes cooperative yield, returns a
typed superseded result, and releases ownership before the foreground emits a
fresh request UUID. The explicit coordinator baselines the existing desktop
run and accepts only a different run after that trigger, so it cannot attach
the click to a stale `request-v1.json`.

An interrupted explicit conversation leaves captured/published generations
durable. The listener may resume that same run without recapture or a second
generation. Automatic actionability separately classifies a new request, new
correlated desktop ACK/reference evidence, a legitimate active handoff, and a
stale resumable generation. A locally terminal run is suppressed until its
run ID changes or its correlated remote evidence advances; mere presence of a
resumable generation does not schedule the same run every five seconds.

The Mensurations grid uses two 140 dp compact fields plus a 10 dp gutter at
normal phone widths. The deployed 1080 px/480 dpi phone supplies approximately
312 dp inside the screen frame and therefore receives two columns. Layouts
below 290 dp, or font scales of 1.5 and above, retain the readable one-column
fallback.

Backup manifests retain format version 1 and the current Android schema. Their
`product_version` is read from installed package metadata, so development
builds record `0.1.3` without a second hard-coded product-version source.
