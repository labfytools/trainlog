# Tests and validation

Sleep Diary coverage includes schema creation/migration, midnight and DST
offsets, revision conflict, logical deletion/reopen, companion replay and
rollback, Web periods/deep links, Android persistence, and vector PDF
construction/pagination for 7, 14, 21 and 30 synthetic days.
Lifecycle regressions additionally cover durable drafts excluded from export,
guarded validation, reopen, post-validation correction, exact
generation-to-revision capture and ACK-only acknowledgement.
Web component regressions add each point/interval capture type, verify that a
successful mutation rerenders without losing the workspace, preserve one entry
identity while advancing revisions, and exercise medication creation, immediate
intake availability, event/intake editing and deletion. The UUID regression
removes `crypto.randomUUID()` and proves the HTTP-origin-compatible UUIDv4
path. A real Firefox run through the production C server and `trainlog.perf`
creates events, medications and intakes, reloads their durable state, checks a
responsive viewport and captures console errors and unhandled rejections.

`web_analysis` covers empty history, every 7/30/90/all period selector,
loaded and unloaded repetitions, continuous duration/distance/speed, explicit
persisted MAX, multiple primary BODY ZONES, partial measurements, one- and
multiple-point measurement semantics, and active-Program projection. The HTTP
suite exercises the real bounded endpoint plus invalid and duplicate query
parameters. Vitest validates the typed parser, null preservation, route and
section changes, period/exercise/metric selection, invalid deep-link fallback,
critical French/English labels, and Dashboard layout regressions.

`test_sync_drive_transport` proves Drive artifact-before-manifest and
manifest-before-reference publication, rejects unsafe/AI namespaces, and keeps
a partial generation invisible. Orchestrator coverage proves that Drive mirror
failure cannot undo a completed primary transport. `web_sessions` additionally
proves duplicate-delivery rejection and stable-identity active-list removal
after the reserved execution session appears in completed history. Android's
normal unit suite builds the shared generation coordinator, foreground service
and WorkManager transport rather than a test-only protocol copy.

`web_sessions` exercises production preparation creation, durable request
replay, immutable successor revisions, stale-revision conflict, duplicate
exercise occurrences, list search, persisted detail serialization, revision-
guarded withdrawal, conflicting idempotency-key reuse, active-list exclusion,
and preservation of proposal/revision/delivery evidence. The HTTP test covers
the real protected DELETE route and the versioned local preference GET/PUT,
including CSRF, ETag, persistence and invalid-file fallback. The Web Vitest
suite additionally covers cross-type global ordering beyond 32 rows, timestamp
offsets, stable ties, invalid/absent dates, contextual states, confirmation
cancel, the DELETE request and preference conflict behavior. Date tests run
under `Pacific/Honolulu` and `Pacific/Kiritimati`.

`SessionPreparationExchangeTest` covers Android import replay, occupied
singleton refusal, explicit start, reserved execution identity, schema-v22 to
v23 migration, permanent withdrawal replay, pending cancellation, started
execution preservation, multi-delivery targeting, witness preservation and
absence of performed facts. `test_session_preparation_export.py` proves that a
withdrawn preparation is not redelivered, its exact evidence is exported in V2,
and an acknowledged withdrawal is not republished. Generation tests bind the
withdrawal to one immutable generation and acknowledge it only after the exact
durable ACK. `SyncGenerationServiceTest` and
`SyncDeploymentConversationTest` exercise the real optional artifact through
generation validation and 24 bidirectional conversations. It also asserts that
every foreground conversation emits exactly one fresh daemon request signal;
the coordinator's production resume predicate preserves an interrupted
generation identity until the counterpart generation has a durable terminal
consumption row.

`SyncDeploymentConversationTest` executes both production generation services
and reopens Android storage. `test_web_sync_browser.py` adds a real Firefox click
through the C adapter, observes the desktop commit at an Android test barrier,
then releases the real ACK. This proves the directory transport double, not
physical MTP. Orchestrator tests cover bounded streams and descendant cleanup;
deployment-tool tests cover consistent SQLite backup and candidate inventory.
The incident regressions also prove that a phase-owned MTP outbox excludes
retained staging and unrelated legacy files, adapter expiry produces the stable
`transport_timeout` code, and operational failure classes remain distinct.
The C MTP regression keeps a malformed retained generation beside the current
reference and proves that pull downloads only the referenced generation while
still rejecting a truncated object inside that current generation.

`web_session_deletions` exercises proposal withdrawal replay, preservation of a
derived preparation, active-draft conflict, causal draft/session deletion and
completed-session finalization. It also builds a Program-origin completed
session, deletes it through the production command, verifies one causal
operation/tombstone and terminal execution provenance, then replays the exact
request. The HTTP suite covers real history detail, stale and successful
session DELETE requests, invalid identity, post-delete deep-link absence and an
injected SQLite failure with complete rollback. Vitest covers the
focus-managed confirmation, Cancel and Escape zero-mutation paths, the real
DELETE call, server-confirmed refresh, useful typed errors and focus
restoration. AI companion tests prove that V2 exports a
withdrawal without the proposal and that Android's tombstone rejects later V1
replay.

`web_programs` exercises validation preview without writes, transactional
import, exact replay, responsive detail serialization, archive replay,
revision-guarded terminal deletion, response replay, preparation creation,
usage/provenance and absence of delivery. Mixed-profile preparation coverage
combines continuous duration, externally loaded reps with an integer JSON
weight, and unloaded reps; it verifies preserved order and targets, generated
entry identities, readable detail, request replay, and zero execution or
delivery creation. A separate compatibility fixture retains an affected source
Program's impossible zero weight while deriving an editable preparation with an
unspecified target. Schema migration coverage checks all v26 tables and
columns. `schema_v26_migration` additionally migrates a
deterministically populated v25 fixture containing 24 sessions and 48 entries,
with preparation provenance and history, and verifies exact typed projections
after migration. Programs Vitest covers preview before the only importing
mutation, explicit preparation creation/navigation, discrete trash confirmation,
focus restoration, and Escape/Cancel zero-mutation behavior. The full HTTP,
pagination, strict-parser and Core suites retain malformed, duplicate-key,
bounds, catalog/profile/equipment, conflict and rollback coverage.

Android Programs projection tests cover schema-v23 to v24 migration, full
nondeleted `trainlog-programs` V1 snapshot import, durable deletion tombstones,
read-only Sessions list/detail data, exact replay, capability gating, and
correlated ACK behavior. The integrated `SyncGenerationServiceTest` invokes the
real production chain from a live Program through capture, publication, Android
consumption, deletion/tombstone publication, real ACK, desktop acceptance and
reopen; an exact old generation is idempotently recognized and ACKed while the
durable tombstone prevents Program resurrection, and a second exchange creates
no duplication. Python Programs/synchronization suites cover the staged
companion's full snapshot, unacknowledged tombstone publication, and ACK-driven
acknowledgment without changing mobile V3, catalog, or preparation artifacts.

Program execution regressions additionally cover Android schema v24 to v25,
start/replay/singleton protection, target-only draft creation, completion,
restart persistence and companion serialization. The integrated generation
test uses the production desktop and Android producers/consumers to import a
Program, execute one session, import completed history and its V1 provenance,
acknowledge the correlated generation, expose Web `completed`, then continue
through Program deletion, restart and old-generation replay without duplication
or resurrection. The desktop populated migration test now continues through
schema v27 and proves the new execution ledger starts empty.

`schema_v28_migration` migrates a populated v27 fixture, preserves Program,
session and execution identities byte-for-byte, admits the terminal deleted
state, reports `integrity_check=ok` and an empty foreign-key check, and proves
that migration invents no causal operation or deletion. The mobile V3 importer
regression proves deterministic live causal revision seeding and exact replay.
The Program execution exchange regression proves an old retained completed
fact cannot override a session tombstone or duplicate terminal provenance.

`test_web_sessions_browser.py` runs headless Firefox against the embedded
production assets and C HTTP server with private temporary XDG roots. It creates
only a synthetic ready preparation and delivery, verifies French then persisted
ISO presentation, captures desktop and an exact BiDi-emulated 390x844 viewport,
performs the real confirmation click, checks the schema-v24 withdrawal ledger
after reload, and validates the resulting V2 export evidence. Android repository
tests exercise the actual V2 consumer rather than a permissive protocol mock.

`web_prepared_items` exercises the production SQLite projection and JSON
serializer with a proposal and an execution draft, then proves finalized and
causally deleted execution drafts are absent. The HTTP test covers the
read-only route and method rejection. Vitest distinguishes proposal, active
draft, loading, empty and failed-refresh states; it retains an already rendered
durable value across a reread failure. Sync-control tests cover a single
abortable polling scheduler, per-run revisions, rejection of an older run's
late terminal response, stable error/action rendering, and cleanup on unmount.

The `ai_history_export` test covers an empty database, a simple session,
distinct multi-occurrences of one exercise, ordered sets, exact occurrence
feedback, session-owned follow-up feedback, JSON escaping/UTF-8, MAX history,
body measurements, deterministic content, and byte-for-byte database
immutability.

## 1. Principle

A Trainlog feature is not complete without relevant validation.

Frozen formats, persistence migrations, synchronization semantics, and user-data
mutations require executable coverage where practical.

`WEB_CLI_HTTP_INFRASTRUCTURE_V1` adds pure CLI tests for the default TUI mode,
both Web switches, help, version, unknown and repeated options, positional
arguments, Web-only `--port`, missing/non-numeric/negative/zero/out-of-range
ports, 65535, and option ordering. Process-level tests execute `--help` and
`--version` without initializing the database, TUI or server.

The isolated HTTP test opens one in-memory database, starts the production
single-threaded adapter on an explicitly reserved loopback port, and verifies
health JSON/version, content type, `nosniff`, CSP, absence of CORS, 404, 405
with `Allow: GET`, invalid Host, oversized body, oversized-header rejection,
SIGTERM shutdown and explicit failure on an already occupied port. It never
uses the Internet. A separate real-launch smoke uses an isolated XDG directory,
performs the health request and stops the production binary with SIGINT.

`WEB_FRONTEND_SHELL_V1` adds TypeScript typechecking and 10 Vitest/jsdom
component tests for the persistent shell, five client destinations, active
navigation, browser history, unavailable factual fields, successful/failed/
invalid health responses and accessible landmarks. Production Vite output is
then generated and embedded for the C HTTP test, which additionally verifies
HTML root and SPA routes, JS/CSS MIME, HTML and immutable cache policies,
SHA-256 ETag, strict CSP, HEAD, unknown assets, traversal rejection and the
invariant that unknown `/api/` paths never receive `index.html`.

The generator contract test proves byte-for-byte deterministic output, stable
path ordering and the 128-asset bound. Runtime smoke coverage starts the linked
binary with Node/npm absent from `PATH`, removes any dependency on `web/dist`,
and checks root, health and SIGINT. A separate `-Dweb=disabled` build verifies
the explicit diagnostic instead of pretending that a frontend exists.

Frontend preparation and direct validation are:

```bash
cd web
npm ci
npm run typecheck
npm test
npm run build
```

Release-oriented Meson builds must use `-Dweb=enabled`; Meson never provisions
or downloads npm dependencies.

The history-deletion regression suite also exercises a real Program-origin
completed session through the Core and HTTP command paths, including stale
revision, cancellation, injected transaction failure, exact request replay and
terminal execution provenance. Full-generation tests apply causal deletions
before older history and equipment-association companions, prove that
standalone imports remain strict, recover recent consumed and rejected ACKs
without letting an older bounded window starve active producer capacity, and
replay an already consumed generation without mutation.

`WEB_DASHBOARD_DATA_CONTRACT_V1` adds a standalone bounded Core test for empty
history, explicit unavailable domains, two observable sessions on one local
day, an unfinished session, continuous activity, MAX-only sessions, primary
zone deduplication, catalogue-without-history exclusion, 90/30-day windows,
recent-MAX truncation/`partial`, Footer facts and JSON escaping/overflow. The
HTTP lifecycle test additionally proves `GET /api/v1/dashboard`, JSON content
type/version, unavailable reasons, corrupt-timestamp `invalid_data`, Core-error
translation to bounded HTTP 500 and `POST` rejection without weakening Host,
CSP, CORS or unknown-route behavior. Vitest validates strict parsing, real
Footer population, empty fallback, malformed snapshots and absence of invented
tile metrics; the seven tile bodies remain placeholders.

`WEB_DASHBOARD_GRID_V1` adds Vitest coverage for the seven stable identities,
default validity/non-overlap, every min/max constraint, independent rejection
of unknown/duplicate/out-of-grid/non-integral layouts, lossless library adapter
round trips, deterministic collision compaction, horizontal/vertical keyboard
resize, responsive six/one-column projections, normal-mode locking, absent
normal-mode handles, edit actions, cancel/reset/session-only save, visible
keyboard focus and live announcements. Existing shell tests continue to prove
the factual Footer, navigation and absence of invented data.

`WEB_DASHBOARD_LAYOUT_V1` adds Core-independent filesystem tests for default,
strict parsing, bounds, corruption, oversized/non-regular/symlink inputs,
private permissions, atomic replacement, revision conflicts, preservation on
invalid writes and reset. A canonical JSON parity test prevents C/TypeScript
constraint drift. HTTP tests cover GET/PUT/DELETE, ETag/If-Match, two stale
clients, CSRF/Origin rejection and `trainlog.perf` Origin acceptance. Vitest
covers default/persisted/invalid sources, ETag/token parsing, canonical-only
PUT, conflict translation, loading, local cancel/reset and durable Save.

Sleep Diary Web tests cover canonical snapshot adoption after every successful
autosave, ordered revision chaining, rejection of stale asynchronous reads,
and live Agenda updates for bedtime, final get-up, split sleep, time in bed,
long awakenings, naps, sleepiness and grouped medication intakes. They also
cover localized dose/unit labels, same-name medications with different usual
doses and editable intake-dose prefill. `test_web_sleep_browser.py` exercises
the embedded production bundle in real Firefox against an isolated synthetic
database, observes every mutation without reload, verifies HTTP success and
browser error collectors, then reloads and compares the persisted Agenda.
The PDF suite separately asserts complete French and English vocabularies,
accent-preserving WinAnsi text, human-readable duration and plural forms,
localized dates, medication dose snapshots, pagination and the exact ordered
geometry of sleep / 45-minute long-awakening / sleep intervals. The same real
Firefox scenario exports a seven-night French PDF from the embedded production
bundle, extracts its text with `pdftotext` and rasterizes its A4 landscape page
with `pdftoppm` for retained visual inspection.

`WEB_DASHBOARD_TILES_V1` adds strict TypeScript parsing for every consumed
field and Vitest fixtures confined to test sources. Coverage proves the 90-day
activity map and direct totals, all compact/medium/large disclosure levels,
legacy 0 kg visibility, `improved` use, unfinished-session duration absence,
1/3/8 MAX bounds, factual muscle bars, unavailable cardio/next-session states,
global loading/error/invalid signals, accessible textual equivalents and live
density changes during keyboard resize.

`WEB_DASHBOARD_VISUALIZATIONS_V1` adds Vitest coverage for lazy chart presence
by tile size, factual progression tooltips, one/multiple/legacy-zero points,
accessible series descriptions, the complete canonical BODY ZONES SVG mapping,
unmapped aggregate preservation, deterministic session-only intensity levels,
front/back views, keyboard-focusable regions and factual muscle tooltips. The
existing Activity, MAX, session, unavailable, Footer and Grid/Layout tests
remain unchanged and passing.

`WEB_DASHBOARD_V1_FINAL_REVIEW` adds explicit value-axis coverage for an empty
defensive domain, one point, identical points, low amplitude, normal amplitude
and a legacy zero. Every observation must remain within the computed bounds and
a positive constant series must have visible space on both sides. Tile-density,
BODY ZONES text preservation, edit handles, keyboard editing and responsive
non-persistence remain covered by the existing Grid and tile suites.

## 2. Frozen Trainlog JSON v1

Run:

```bash
python tools/validate_json.py
```

It validates:

- `examples/session-v1.json`;
- all positive fixtures;
- all negative fixtures.

A negative fixture passes only when Trainlog rejects it.

Android field-workflow regression coverage additionally permutes distinct and
duplicate active occurrences, checks stable IDs/equipment/targets/actuals and raw
form state, recreates the repository, injects a transactional write failure, and
finalizes the reordered draft. Completed-session tests edit reps, loads, set and
continuous durations, distance, and speed; preserve feedback/follow-up roots;
exercise atomic rollback; and inspect the corrected V3 export identity.
Completed-order coverage also exercises duplicate exercises, SETS, CONTINUOUS,
MAX, planning, equipment, feedback revisions, J+1 follow-up, Cancel/no-write,
successful position persistence, injected Save rollback, and desktop V3
reconciliation without a duplicate canonical session.
Field-input coverage maps one accumulated drag directly from index 5 to 1 and
between both list extremes, restores the origin permutation on cancellation,
and checks decimal editing/final parsing for both separators. It covers `0.3`,
`0,3`, quarter-unit values, whole values, incomplete separator states, malformed
text, non-finite spellings, negative final distance, stored numeric equality,
and locale-independent V3 JSON publication.

Semantic validation includes:

```text
stable-ID uniqueness
normalized exercise-name uniqueness
timestamp offsets
end > start
catalog/reference equality
tracking-mode consistency
load-mode/weight consistency
unknown-field rejection
non-blank notes
zero actual repetitions allowed
```

## 3. Import reconciliation contract

Run:

```bash
python tools/validate_import_contract.py
```

Coverage includes:

```text
same ID + same name/profile -> reuse
same ID + renamed display text -> controlled reuse/warning
same ID + incompatible mode -> reject
different ID + equivalent normalized name -> reject
new unique identity -> create
```

That command validates the frozen Trainlog JSON V1 importer contract. Active
mobile V3 synchronization has a separate, stricter safe-reconciliation policy
covered below; it does not modify the frozen V1 expectation.

## 4. Desktop Meson suite

Current registered set (Meson executes tests in parallel, so displayed order is
not contractual):

```text
database
catalog
equipment_catalog
body_zones
training_knowledge
session_generation
session_generation_policy_validation
training_context
custom_equipment
session_detail
duration
body_metrics
bodyviz
exercise_performance
session_type_schema
session_edit
body_observation_edit
usb
mtp
exercise_profile_schema
continuous_session
continuous_detail
reps
variable_sets
schema_v5_migration
schema_v9_migration
schema_v7_migration
timestamp_validation
mobile_import_variable_sets
mobile_import_multi_occurrence
session_exchange_v3
equipment_associations_exchange
equipment_definitions_exchange
exercise_reconciliation
body_zone_sync
body_zone_catalog_validation
sync_direction
sync_history
sync_screen_action
measured_max
max_results
max_sync
body_analytics
terminal_input_event_type_policy
tui_workflows
app_shell
presentation_i18n
```

The desktop suite includes APP_SHELL_V1 production-transition coverage and the
AI-draft exchange regressions. The configured suite size and latest result are
recorded once in `current_state.md`; the following generator-specific checkpoint counts remain
historical evidence:

```text
45/45 Meson tests PASS
```

`SESSION_GENERATOR_V1` validation covered policy shape and shared fixtures,
full-history exposure/recency boundaries, selection and observed-load anchors,
Android v10 -> v11 planning migration, V3 round trips and malformed-V3
priority, and ordinary draft/editor acceptance. The checkpoint also passed 4/4
selected ASan/UBSan tests and Android 73 tests with zero failures/errors; one
known historical real-v9 fixture skipped while the structural v10 migration
test executed and passed. No hardware MTP or real app-upgrade/install validation
is claimed for this checkpoint.

The completed post-repair matrix passed 45/45 Meson tests, named ASan/UBSan
4/4, and Android 75 tests with zero failures/errors and one unavailable
external Android-v9 fixture skip. The structural Android v10 planning migration
executed and passed. Validators, strict C17 headers, deterministic policy/
fixture/knowledge regeneration, and APK asset byte comparisons passed. The
earlier sanitizer invocation that selected no tests is not used as evidence.

The desktop executable is additionally smoke-checked in isolated tmux PTYs at
100x30, the exact 72x20 minimum, and the 60x15 fallback; a resize down/up must
recover before a clean keyboard quit. Notcurses is verified as the executable's
direct terminal dependency with `readelf -d`.

`presentation_i18n` covers the deterministic French default, French/English
catalogue lookup, immediate in-process language switching, selected-language
decimal/date formatting even under a contrary host numeric locale, unchanged
layout at the 72x20 minimum, and local XDG configuration persistence/failure
semantics. It also proves that persisted catalogue names are not translation
keys, the source-derived `TRANSLATABLE_UI=0` boundary, and that synchronization
presentation uses local typed status/counters rather than raw opaque
core/receipt/history summaries.

Android Robolectric coverage verifies both resource locales, Settings language
ownership and dedicated SharedPreferences persistence, French fallback for an
absent/invalid preference, and localized presentation of known stable BODY ZONE
IDs without changing unknown catalogue labels or repository values. Resource
parity and typed synchronization-receipt presentation are also covered; raw
protocol/operational summaries remain opaque. Final i18n validation passed
60/60 normal desktop tests, 60/60 ASan/UBSan desktop tests, and 202 Android
tests (198 passed, 4 skipped, 0 failed), with both JSON validators passing.
Automated coverage does not replace a real device/manual visual language-switch
smoke; that remains explicit manual validation.

`WEB_DASHBOARD_CHARACTERIZATION_V1` now exercises the production Core service
directly through a real in-memory SQLite database, without including `tui.c` or
linking Notcurses. Its explicit fixed query instant
covers an empty training history, exact rolling-window inclusion immediately
before/at/after the 30-day boundary, valid actual history without `ended_at`,
and plan-only exclusion. It covers repeated occurrences, heterogeneous REPS
sets, SETS/DURATION, CONTINUOUS/DURATION without fake sets, assistance,
unweighted actuals, ordinary heavy sets versus explicit MAX, equal-instant
tie-breaks (including equal same-session performance points), the legacy
unweighted `0.0 kg` prior baseline, primary-only catalogue zones,
secondary-zone exclusion, and seeded catalogue behavior. Exact and exceeded
bounds are asserted for 128 observable
sessions and 4096 Dashboard facts, including the independently truncated totals
and `partial` flag. Existing regressions continue to cover malformed legacy
timestamps, body-metric selection, responsive rendering, all-history buckets,
strictly earlier exact-dose improvement, and explicit-MAX separation.
The standalone `dashboard_header` target also proves that
`trainlog/dashboard.h` compiles independently as C17. TUI workflow regressions
separately verify service adoption and snapshot presentation.

Notable regression coverage:

- transactional persisted-session replacement;
- exercise removal from a session;
- body-observation stable-identity editing;
- profile-aware exercise constraints;
- continuous activity without fake sets;
- table-only desktop SETS workflow: planning does not create actual rows,
  explicit add requires an actual metric, and normal zero-row completion is
  rejected; Android legacy compact-draft decoding remains separately covered;
- direct v4 -> current database migration and v7 -> v8 custom-equipment migration;
- bounded v8 -> v9 explicit-max migration, including ambiguous-attempt preservation;
- lossless v9 -> v10 `performed_sets` rebuild: historic NULL and positive
  weights/IDs/owners/positions/metrics survive, explicit zero is accepted, and
  injected failure rolls back with integrity, foreign-key and enforcement
  checks;
- additive desktop v10 -> v11 and Android v9 -> v10 body-zone migrations:
  exact stable row identities and history remain unchanged while only proven
  manifest mappings are seeded; the Android fixture also preserves an active
  draft, per-set loads, explicit MAX, a body observation and a custom equipment
  definition;
- canonical body-zone validation rejects duplicate IDs/order, missing parents,
  cycles, group/leaf disagreement, a taxonomy other than the exact V1 manifest,
  invalid `full_body`, boolean versions, malformed stable IDs, and direct group
  assignment;
- one primary/multiple secondary persistence, duplicate/unknown/group/orphan-
  secondary rejection, primary/secondary exclusivity, edit, unclassified and
  reopen;
- exact and parent-descendant zone filters, lower-body leaf coverage,
  primary-only participation, normalized-prefix composition and custom rows;
- body-zone companion export/import, empty mapping, identical replay,
  one-sided update, simultaneous conflict rollback and source-V2-proven
  exercise-ID reconciliation without name-only inference; custom-exercise
  publication establishes both peer baselines before a reverse one-sided edit;
  malformed `ex_<uuid-v4>` identities and timestamps without offsets are
  rejected;
- heterogeneous mobile-set import;
- per-set load persistence and correction: ordered rows retain mixed actual
  repetitions, nullable loads, positions and assistance semantics through
  desktop replacement and detail retrieval;
- V2 mobile round-trip and idempotent replay preserve each ordered set's own
  nullable `weight_kg`, without collapsing it to an occurrence target;
- V2 explicit-max Android -> desktop -> Android replay and resumed same-session update;
- Notcurses input lifecycle translation: PRESS/REPEAT are actionable while a
  RELEASE event is consumed without creating a second navigation action.
- targetless mobile SETS persistence;
- mobile-import idempotence.
- V2 mobile import with repeated exercise occurrences and stable `entry_id`;
- companion equipment import after session import, including idempotent
  reimport and independent associations for repeated occurrences;
- rejection of the historical equipment-import invocation without its required
  `--database` target, followed by the corrected complete V2 export chain.
- stable-ID mobile-to-desktop rename reconciliation without duplicate catalog
  rows or historical-reference replacement.
- supplied catalogue search/detail, local custom-equipment creation and
  selection, resolution of supplied/local occurrence links, and explicit
  visibility of an unknown historic equipment reference;
- strict custom-equipment definitions V1 import/export: reserved supplied IDs,
  additive omission, equal reimport, and divergent same-ID conflict;
- three-mode shared-engine plans and their directional receive/publish bounds;
- Sync-page action dispatch: direct `a`/`p`/`b` confirmation, one `Enter` run,
  `Esc` cancellation without a run, refresh-only `r`, inert retired `s`, and
  equivalent full/compact footer direction labels;
- definition-first ordering before V2 artifacts, so associations can resolve
  custom IDs without changing V2 shapes.
- exercise reconciliation with identical profiles, compatible subset/superset
  masks, explicit incomparable-profile rejection, current Marche identities,
  preservation of two historic occurrences and their `entry_id`/continuous
  values, ordered custom-equipment definitions/catalog/mobile/association
  replay, complete definitions/mobile/associations/body import, outbound
  publication, and stable second replay;
- a production PC-exporter to Android-importer regression over definitions,
  catalog, mobile V3, and association V2 artifacts: the fixture includes
  Marche, Leg press, two Marche occurrences, per-set loads, body data, a
  durable draft, and custom equipment; after the first import, both the second
  and third imports report zero additions and an exact snapshot of every
  Android business table remains unchanged;
- synchronization summary reporting where a reconciliation-only result keeps
  `+0 exercice(s)` and only an actual inserted row produces `+1`;
- all fourteen body metrics through the same complete reconciliation import;
- desktop-created occurrence identities use `sxe_<uuid-v4>`, not the `sy_`
  synchronization-run prefix.

## 5. Build

### Isolated synchronization validation environment

`TRAINLOG_SYNC_TEST_ENV_V1=PASS/FROZEN` provides one repository-owned entry
point for the desktop and Android validation needed by synchronization work:

```bash
python3 tools/validate_sync_isolated.py --suite preflight
python3 tools/validate_sync_isolated.py --suite smoke
python3 tools/validate_sync_isolated.py --suite full --jdk /usr/lib/jvm/java-17-openjdk
```

`TRAINLOG_SYNC_DATA_LIFECYCLE_V1` adds schema v19/v18 migration assertions and
Android lifecycle coverage for confirmed-only serialization, persistent
pending state, occupied-singleton refusal, explicit activation, identity-
preserving idempotent finalization and stale replay. The closeout adds
`v4HistoryRoundTripsAcrossRealAndroidAndDesktopImplementations` and
`executionDraftRoundTripsAcrossRealAndroidAndDesktopImplementations`. Their
bridge provisions only synthetic schema and invokes the real Android and
desktop staged entry points; it does not reconstruct wire data. The final
isolated run passed 75/75 normal native tests, 75/75 ASan/UBSan native tests
and 207 Android tests (203 passed, four unchanged historical-fixture skips),
plus both Python validators and `assembleDebug`. These are artifact/repository
proofs; they do not claim transport activation or global synchronization
atomicity.

The 75-versus-73 inventory contains no removed, renamed, merged, or disabled
native tests. With `web=auto` and no `web/node_modules`, Meson selects 73 tests
and omits exactly `web_frontend_typecheck` and `web_frontend`. After `npm ci`
from the locked package file and Meson reconfiguration, both the normal and
ASan/UBSan selections contain and pass all 75. Clang 22.1.8 is the compiler
used for those green builds. GCC 16.2.1 fails both baseline `77c1517` and the
lifecycle branch under the unchanged strict `-Werror` policy on
`-Wmisleading-indentation` in unchanged `json_writer.c`,
`web_dashboard_json.c`, and `web_dashboard.c`; this is a pre-existing
compiler-version limitation, not lifecycle evidence and not a relaxed gate.

Use `--jdk /absolute/jdk17/home` when auto-detection is unsuitable and
`--run-parent /absolute/private/parent` to select another parent outside the
problematic system `/tmp`. `--keep-run-dir` retains a successful run for
diagnosis; failures and interruptions are always retained. Without that flag, a
successful run removes only its own validated `run-*` directory.

Each invocation creates a mode-0700 unique root under
`${XDG_CACHE_HOME:-$HOME/.cache}/trainlog/test-runs/`, then gives its children
private HOME, XDG data/config/cache/runtime, native/JVM tmp, exchange, database
and report directories. Test-sensitive files and logs are mode 0600. The
already-provisioned `${GRADLE_USER_HOME:-$original_HOME/.gradle}` dependency
cache is deliberately shared; no Trainlog application data, rclone credential,
keystore or production XDG path is shared. This is path/environment isolation,
not a system sandbox.

The harness verifies Java 17 with `java -version`, then verifies both Gradle's
launcher and daemon JVM. `JAVA_TOOL_OPTIONS=-Djava.io.tmpdir=<run>/tmp` reaches
the forked Robolectric JVM; `IsolatedTestEnvironmentTest` checks the effective
property and every private path from inside that JVM. Android test tasks use
`--rerun-tasks`, so an up-to-date or cached task cannot masquerade as a new
execution. No default mode invokes rclone, adb, libmtp synchronization,
`trainlog-sync-once`, `connectedDebugAndroidTest`, a user service or a
production lock/database.

The modes are:

- `preflight`: tools, writable parent, JDK 17, Gradle launcher/daemon and paths;
- `smoke`: preflight, production-path sync-gap characterization and the Android
  JVM isolation test;
- `full`: normal and ASan/UBSan desktop suites, explicit characterization,
  JSON/import validators, targeted draft lifecycle/export test, complete
  Android unit suite and `assembleDebug`.

Logs are private and capped at 8 MiB per command. Exit codes distinguish `0`
success, `2` blocked preflight, `3` missing dependency, `4` environment failure,
`5` business-test/command failure and `130` interruption. Commands, exit codes,
JDK, non-secret paths and Android test/pass/fail/skip counts are recorded in
`reports/`; the full environment and credentials are never printed. The
harness terminates only process groups that it started, waits before forced
termination, never stops a pre-existing Gradle daemon or Trainlog service, and
refuses cleanup outside its exact root or across a symlink.

At the 2026-09-17 reference checkpoint, the Android total retained four skips
because optional historical database fixtures were unavailable: one v9, one
v12, one v13, and one v14 migration-fixture case. These are explicit fixture
coverage gaps, not passing migration executions. The isolated harness does not
run hardware MTP, real Drive, or Android instrumented tests; those remain
separate validation classes.

Stable-release validation additionally builds the signed release variant and
checks the packaged product versions and binary linkage:

The release machine supplies all four `TRAINLOG_RELEASE_*` environment
variables, or a mode-0600 private file at
`~/.config/trainlog/release-signing.properties` with this shape:

```text
storeFile=/absolute/private/path/trainlog-release.jks
storePassword=<local secret>
keyAlias=<local alias>
keyPassword=<local secret>
```

`TRAINLOG_RELEASE_CREDENTIALS_FILE` may select another private properties file.
Environment variables take precedence over file values. The keystore and
credentials are the durable Android update identity: back them up through an
approved encrypted external mechanism, never Git or release assets. Missing
configuration makes `packageRelease` fail explicitly rather than producing an
apparently publishable unsigned APK.

```bash
meson compile -C build
meson test -C build --print-errorlogs

cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew test
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew assembleRelease

apksigner verify --verbose --print-certs \
  app/build/outputs/apk/release/app-release.apk

file trainlog-tui-linux-x86_64-v<version>
ldd trainlog-tui-linux-x86_64-v<version>
sha256sum trainlog-android-v<version>.apk \
          trainlog-tui-linux-x86_64-v<version>
```

An unsigned release APK is build evidence only and must not be published as the
canonical stable Android artifact. Release assets must come from the exact
tagged source state, and the tag, Android `versionName`, Meson project version,
TUI product version, asset names, and release title must agree.

Private update rollout may set `TRAINLOG_ANDROID_VERSION_CODE` to a positive
integer above the installed package while retaining the current development
`versionName=0.1.4`. The version-code override is build-only and does not change
any schema or exchange-format version. Private daily candidates must not
downgrade the logical product version to the earlier 0.1.2 stable line.

For `TRAINLOG_UI_ANDROID_WEB_UNIFICATION_V1`, `lintDebug` was also run from a
detached clean worktree at the exact `main` baseline and from the mission
branch. Both runs report the same ten errors: one `NewApi` diagnostic for
`windowLightNavigationBar`, one `LocalContextConfigurationRead`, and eight
`StringFormatMatches` diagnostics (four bilingual call sites). Comparison by
rule and complete diagnostic message reports zero added and zero removed
errors. No lint rule or severity was changed.

```bash
meson setup --reconfigure build
meson compile -C build
meson test -C build --print-errorlogs
```

Strict warning flags remain active. Do not weaken warnings to make a change pass.

## APP_SHELL_V1 production-transition coverage

The active desktop suite has 58 tests. `app_shell` verifies layout thresholds,
route/history bounds, overlays and focus restoration, bounded UTF-8 search and
form input, stable-ID list selection, shared action ordering, and leave guards.
`tui_workflows` covers the production single event-loop routes and controller
handoffs: sessions/generator, exercises, equipment, statistics/body/MAX,
settings, and confirmed synchronization. `custom_equipment` covers the
bounded deterministic read-only equipment page reader, including pagination,
invalid arguments, offsets and corrupt values. This coverage replaces former
nested-screen-loop workflow claims.

The Mensurations workflow regression separates the selected observation's
profile from its metric history: singleton circumference bars, independent
kg/cm rendering, missing-field omission, a real two-date high-resolution
Unicode series, historical selection updates, availability-filtered metric
navigation, and bounded 120x35, 100x30, 80x24 and 72x20 geometry without ASCII
chart glyphs. The terminal-free `chart` regression exercises zero, one and two
points; constant, increasing and decreasing series; irregular elapsed-time X
mapping; Y headroom; clipping and compact rectangles; Braille 2x4 dot encoding;
interpolated Braille output; and distinct stored-measurement markers.

The terminal-free `statistics` regression covers empty, 7-day, 30-day and
complete windows; actual-session and occurrence counts; sets, repetitions and
duration; strict external-load volume; same-occurrence planned/actual totals;
canonical alias reconciliation; primary and secondary persisted zones;
immediate and session-global feedback scopes; and zero/one/multiple explicit
MAX histories. `tui_workflows` additionally exercises the five-entry statistics
hub, period and graph cycling, compact zone bars, canonical exercise detail,
MAX facts, and bounded common-chart rendering without an interactive terminal.

Statistics-refinement regressions distinguish identical observations plotted
inside 7-day and 30-day domains, clamp non-negative scales to zero, render two
weekly totals as independent bars, retain empty local-calendar weeks, compare
unequal zone counts through one shared scale, and verify that the default MAX
list excludes never-measured exercises until its explicit toggle is used.

Period-bucketing regressions cover four consecutive 7-day comparison buckets,
Monday/Sunday assignment, localized consecutive Gregorian months, retained
empty middle periods, and boundary assignment without double counting.
Ordinary February, leap-year February and exact first/last dates verify month
length without fixed-day arithmetic. Offset-bearing fixtures verify persisted
civil-date membership independently of the machine timezone. Current partial
weeks/months, stable zero history, and duration-based adaptive full-history
aggregation are also covered.

The APP_SHELL PTY validation exercises six TUI sizes—72x20, 80x24, 100x25,
100x30, 120x31 and 120x35—plus help, search clear/close, F6/F7, compact focus,
resize/overlay restoration, clean exit, and navigation with no temporary
database write. Normal and ASan/UBSan Meson suites each passed 48/48; the
current normal and sanitizer real-PTY runs each passed 100/100 checks. The
sanitizer run used the upstream-prescribed Notcurses compatibility setting
`ASAN_OPTIONS=use_sigaltstack=0:detect_leaks=1:halt_on_error=1` and reported
no ASan/UBSan diagnostics.

Android unit/assembly evidence records 84 tests, 0 failures, 0 errors and one
external `TRAINLOG_ANDROID_V9_FIXTURE` skip. Required human checks remain:
the six TUI sizes above; Android widths 320, 360, 393 and 412 dp; large system
font; IME forms; durable-draft leave guards; and TalkBack. No emulator was
available; the daily installed application was not installed over or exercised
by instrumentation, so these are not marked visually passed.

## 6. Android build

When Android code changes:

```bash
cd android

printf 'sdk.dir=%s\n' "$HOME/Android/Sdk" > local.properties

JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew testDebugUnitTest assembleDebug
```

Install to the connected device when hardware behavior changes:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Android repository host tests additionally cover exercise editing:

- trimmed rename preserves `exercise_id` and recalculates `normalized_name`;
- duplicate and invalid names are rejected;
- completed history and active-draft references resolve the renamed catalog row;
- a referenced profile change is explicitly rejected;
- same-ID PC-catalog rename reconciles in place without a duplicate.
- canonical body-zone taxonomy, create/edit/reopen, parent filters,
  search+filter, unclassified rows, companion replay/update/conflict and
  zone-safe exercise-identity merging.

`BodyZoneHomeOverviewRepositoryTest` exercises the production
`getBodyZoneHomeOverview(now)` query with a caller-supplied deterministic
instant. It covers the settled chest/back/shoulders/thighs example,
secondary-only exposure remaining weaker than primary, stable ranking,
unsupported calves exclusion, parent-zone non-participation, retired alias A
to canonical B counted once, active/resolved availability, honest empty
history, inclusive exact 7/30-day instant boundaries, and target-only exclusion.
`BodyZoneHomePresentationTest` covers concise evidence and last-primary age
wording without recovery/fatigue/prescription language.
`BodyZoneHomeSemanticsTest` renders the production section at 288 dp content
width (the constrained 320 dp-screen case) with 1.6x font scale and at a
practical 380 dp content width. It verifies every view-specific Face/Dos zone
and textual state, Button actions, selected/not-selected transitions, leaf
canvases with no anatomical text children, exact normalized-path containment,
a real Canvas tap selecting core, and both existing-route callbacks. The Home
implementation keeps side-by-side maps with separate region-aligned, at-least
48 dp semantic controls; 320/360/393/412 dp and TalkBack visual checks remain
explicit device/emulator validation.

Focused host command:

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
./gradlew :app:testDebugUnitTest \
  --tests com.labfytools.trainlog.data.BodyZoneHomeOverviewRepositoryTest \
  --tests com.labfytools.trainlog.ui.BodyZoneHomePresentationTest \
  --tests com.labfytools.trainlog.ui.BodyZoneHomeSemanticsTest
```

## 7. Hardware MTP validation

Hardware probes and real synchronization are separate from the normal automated
suite because a test runner cannot assume an unlocked MTP phone.

Available probe binaries include:

```text
trainlog-usb-probe
trainlog-mtp-probe
trainlog-mtp-exchange-probe
trainlog-mtp-roundtrip-probe
trainlog-mtp-mobile-export-probe
```

Previously established physical baseline:

```text
USB_MTP_DETECTION=PASS
MTP_STORAGE_ACCESS=PASS
MTP_WRITE=PASS
MTP_LIST_FOLDER=PASS
MTP_READ=PASS
MTP_ROUNDTRIP=PASS
```

The automated `sync_direction` regression also covers selection of the newest
Android V2 export, equipment-definitions V1 artifact, and equipment-association
V2 artifact when scoped storage has retained collision-suffixed sibling files.
It rejects non-versioned/
non-numeric names and uses deterministic ties; JSON validation remains part of
the normal import path. `mobile_import_multi_occurrence` exercises the real
definition importer before the V2 session importer, including idempotent
reimport and rejection of a missing custom definition.

## 8. Bidirectional synchronization validation

`sync_gap_characterization` uses the production mobile V3 exporter/importer and
temporary SQLite databases to prove the current gaps without blessing them: V3
omits `ended_at`, three distinct note owners and the body-observation session
link; the imported values are `NULL`; deleting a local session and replaying an
old snapshot resurrects it; an invalid artifact rolls back while an earlier
artifact remains independently committed. The existing Android test
`TrainlogRepositoryDraftTest.finalizeIsAtomicAndDraftNeverExportsBeforeCompletion`
proves that the active draft is absent before finalization and the completed
session appears afterward. These tests do not implement the future contract.

`TRAINLOG_SYNC_CHARACTERIZATION_V1=PASS/FROZEN` completes the missing current-
behavior evidence without changing production code, schemas, or formats. The
coverage matrix is assertion-based; “future” rows remain criteria rather than
passing tests:

| Current behavior | Test and production boundary | Significant assertions | Coverage |
|---|---|---|---|
| V3 omissions, new-row loss, replay resurrection, and per-artifact rollback | `tests/test_sync_gap_characterization.py` through the real desktop V3 importer/exporter | Wire fields are absent; newly imported columns are `NULL`; an identical replay preserves local-only values; a V3 correction rebuilds its occurrence and loses its local-only occurrence note; replay after deletion resurrects the session; malformed input rolls back only its artifact | Existing proof extended |
| Active draft lifecycle | `TrainlogRepositoryDraftTest.finalizeIsAtomicAndDraftNeverExportsBeforeCompletion` through the real Android repository/exporter | No draft rows export; atomic finalization removes the draft and makes the completed session exportable | Existing proof reused |
| Cross-implementation V3 round trip | `TrainlogRepositoryDraftTest.androidV3RoundTripUsesRealDesktopImporterExporterAndFreshAndroidDatabase`, with `tests/support/roundtrip_desktop_v3.py` invoking the real desktop tools | A fresh Android database reconstructs stable session/entry/exercise/equipment identities, repeated occurrences, occurrence and set order, heterogeneous sets, targets, continuous values, and explicit MAX; exact replay adds nothing | Added |
| V3 correction and forbidden rebinding | Existing desktop `session_exchange_v3` and Android repository cases | Accepted same-identity correction is reported separately from insertion; conflicting `entry_id` rebinding is rejected | Existing proof reused |
| Companion identity and revision chains | Existing aliases, equipment, BODY ZONES, causal profile-state, and feedback suites | Stable foreign references, immediate-feedback roots, immutable revisions, session follow-ups, and replay behavior remain exact | Existing proof reused |
| Inter-artifact partial failure | Existing Android inbox ordering tests and `sync_gap_characterization` | Earlier valid catalogue/components remain committed when a later session/companion is invalid; intra-artifact invalid content rolls back | Existing proof reused |
| Desktop delete-before-send publication | `sync_body_zone_wiring` through `trainlog_sync_run()` and the MTP double | The old remote catalogue is deleted, the injected send fails, the run returns an error, and no replacement exists | Added |
| Selected-invalid V3 priority | `TrainlogRepositoryDraftTest.inboxPresentMalformedV3DoesNotFallBackToValidV2` through the Android inbox importer | A present malformed V3 fails and is not masked by a valid V2 | Existing proof reused |
| Request, receipt, and processed marker | `sync_body_zone_wiring` through `trainlog_sync_run()` | Receipt and report carry the exact `request_id`; identical request replay is refused; receipt-send failure leaves the prior processed marker unchanged | Added |
| Android post-receipt consumption | Desktop request/receipt proof plus Android typed receipt/inbox component tests | Desktop processing success and Android artifact-import success are separate component results; V1 has no durable peer-consumption acknowledgement | Component-scoped only |
| Inter-process lock | `sync_body_zone_wiring` against the real `sync.lock` acquisition with private `XDG_DATA_HOME` | A pipe-coordinated child holds the lock; the second trigger returns conflict before any MTP probe; every child is joined | Added |
| Enriched V4 completed history | `TrainlogRepositoryDraftTest.v4HistoryRoundTripsAcrossRealAndroidAndDesktopImplementations` through Android V4 repository methods and the real desktop V4 importer/exporter | Android → desktop → fresh Android and desktop → Android → fresh desktop preserve stable identities, order, heterogeneous sets, plans, equipment, causal null/empty/escaped UTF-8 notes and idempotent replay | Added cross-implementation proof |
| Execution-draft V1 | `TrainlogRepositoryDraftTest.executionDraftRoundTripsAcrossRealAndroidAndDesktopImplementations` through Android repository methods and `execution_draft_exchange.py` | Both implementation directions preserve stable session/entry identity and confirmed facts; raw unfinished form text remains local; replay is unchanged and pre-finalization replay is stale after reopen/finalization | Added cross-implementation proof |
| General tombstones, coherent generations, causal deletion, and consumption acknowledgement | Causal-deletion suites plus `sync_generation_exchange` and `SyncGenerationServiceTest` | Explicit staged production entry points pass cross-implementation, rollback, replay, bounds and restart assertions; automatic V3 remains unchanged | PASS/FROZEN staged evidence |

The cross-implementation helpers only provision minimal production-compatible
desktop schemas and launch production Python tools; they do not duplicate
their business rules. Kotlin tests use real Android exporters and importers on
distinct databases. Comparisons remove only volatile `generated_at`,
canonicalize object-key and unordered top-level collection order, and retain
occurrence/set array order as business data.

Request/receipt evidence remains deliberately bounded: the native test proves
desktop correlation, publication, replay handling, and marking order, while
Android tests prove local receipt presentation and inbox failure behavior. A
receipt still proves desktop processing only, not durable Android consumption.
README, `architecture.md`, and `sync_exchange.md` were reviewed for this lot and
need no correction; their current ownership and V1 limitations remain accurate.

The 2026-09-17 closing run used all three documented harness modes with the
explicit JDK 17 path. The final `full` mode passed 75/75 normal Meson tests,
75/75 ASan/UBSan Meson tests, both validators, the targeted draft lifecycle
test, Android debug assembly, and 204 Android tests (200 passed, 4 historical
fixture skips, 0 failed/error). No hardware, Drive, adb, instrumented test, or
user service was exercised.

Validated workflow:

```text
Android
-> Synchroniser maintenant
-> unique sr_ request
-> trainlog-syncd
-> shared engine
-> Android -> PC import
-> PC -> Android catalog
-> sy_ structured run
-> matching receipt
-> Android final status
```

Multiple distinct Android request IDs were processed successfully without
reprocessing one request as a new one.

The TUI also invokes the same engine manually and exposes structured run
details.

`post_sync_ai_drive` validates strict export-before-upload ordering, skips
upload after export failure, and reports unavailable/failing rclone. The
production sync wiring test exercises successful TUI and Android triggers
through `trainlog_sync_run()`, uses an isolated fake rclone, and proves that
failed sync runs do not invoke Drive publication. Post-sync failures are
non-fatal to the already successful Trainlog sync by contract.

`ai_session_draft` validates the strict desktop source parser and schema v18
transaction: exact/unknown-key rejection, `aid_` identity, alias resolution,
target-only SETS compatibility, 64-entry/99-set/999-rep bounds, exact replay
skip, changed-payload conflict, deterministic 256-at-a-time publication across
257 drafts, failed-publication retry, permanent import ledger, and full rollback on
an injected child failure. Its fake-rclone seam also verifies that the exact
fetched bytes are archive-copied only after import, same-name inbox replacement
is preserved, stale temporary content cannot import, and archive/status failure
converges through idempotent logical replay. Production orchestration
tests retain `DRIVE_FAIL`, fetched-invalid `REJECTED`, and `ARCHIVE_FAIL` in a
successful report/history. Android repository tests cover the 256-proposal
companion limit, full validation before replay skip, strict optional-text
trimming, replay idempotence, start/delete
tombstones, target-only copy to the unchanged singleton active draft, and
catalog/profile/equipment prerequisite ordering. Focused Android UI wiring tests
also cover pending-only deterministic listing, started/deleted exclusion, the
always-visible **Brouillons** count without losing manual/history actions,
proposal title/target details, inert display with no automatic active draft,
external-revision refresh after companion import, and the existing transactional
start path. These are automated fake
transport tests; no real Drive automated test exists. The final real Drive plus
Android-triggered bidirectional smoke test remains manual and
`TRAINLOG_AI_SESSION_DRAFT_V1=VALIDATION_PENDING`.

A September 2026 connected-device inspection established that the previously
installed 2026-09-13 APK still owned a schema-v16 database without
`ai_session_drafts`; it could not import the companion. Installing the current
APK and synchronizing once is therefore a prerequisite for the remaining real
acceptance test, not evidence that the acceptance test has passed.

## 9. Sanitizers

For meaningful C checkpoints:

```bash
CC=clang meson setup build-asan \
  -Db_sanitize=address,undefined \
  -Db_lundef=false

meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

## 10. Pre-push checklist

```bash
meson compile -C build
meson test -C build --print-errorlogs

python tools/validate_json.py
python tools/validate_import_contract.py

git diff --check
git status --short
```

When Android changed, add:

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew testDebugUnitTest assembleDebug
```

Documentation must describe the resulting state, not retain contradictory old
`NEXT` checkpoints.

## 11. Measured-max regression

The normal Meson suite contains:

```text
measured_max
```

Coverage proves:

- stronger ordinary training is ignored by measured-max classification;
- only `max_test` sessions participate;
- zero-repetition failed attempts are not promoted;
- newest successful explicit test is the current measurement;
- historical external-load record can remain older than current;
- lower assistance is better;
- no-load max tests compare actual reps/duration;
- external working loads round to the configured increment;
- working-load percentages reject assistance.
- `%MAX` target calculators use the exact unrounded formula, enforce 1..100,
  exact equipment identity and external resistance, and reject assistance;
- Android manual target-plan tests prove that calculation is read-only, actual
  set weights remain unchanged, existing target dose/rest survive direct-kg
  edits, and none removes the plan target;
- the real TUI `/` path resolves through the registered action, focuses search,
  filters live, then follows clear-before-close Escape semantics;
- action stable identifiers are unique, preventing footer/F7 duplication.
- Statistics-dashboard rendering covers global summary/event facts, primary-zone
  and unclassified active-catalogue projection without merged-alias double
  counts, zero/one-point without-graph and multi-point Unicode detail-chart
  states; readable selected-period frequency with non-duplicating MAX markers;
  daily/multi-day period projection and independent catalogue bars with
  right-side counts; exact-dose per-set parity, equal-instant
  working/MAX non-events, malformed-timestamp availability, distinct wide versus
  stacked compact geometry, and Unicode blocks used only as chart geometry;
  and footer F6/F7 single-occurrence output at 120x35, 100x30, 80x24, and
  72x20.

Historical validation checkpoint (not the current suite size):

```text
39/39 Meson tests PASS
```

## 12. Body analytics regression

The Meson suite adds:

```text
body_analytics
```

Coverage includes:

- male circumference-formula branch;
- female circumference-formula branch;
- estimated fat and lean mass from real body weight;
- waist/hip, shoulder/waist, and chest/waist ratios;
- left/right asymmetry;
- profile-independent analytics without a configured estimation profile;
- missing required circumference handling;
- invalid estimation-profile rejection.

Historical validation checkpoint (the current desktop suite is 48/48):

```text
39/39 Meson tests PASS
```

## 13. Android session draft v1

Android schema v4 introduced one durable active draft; the current additive
chain reaches schema v17 without clearing completed history or the draft. The
explicit v10 -> v11 migration adds optional planning metadata while preserving
existing rows with `load_mode=none`, zero rest and NULL targets. The
v11 -> v12 migration adds the durable flattened exercise-alias table. Its exact
physical-v11 fixture compares every pre-existing table cell before and after
migration, requires the new alias table to be empty, and checks foreign keys.
The dedicated physical v16 -> v17 fixture preserves representative completed
history and the active draft, adds empty AI proposal tables, and checks SQLite
integrity and foreign keys. AI companion parser coverage accepts title/notes
at their 120/2,000 Unicode-code-point limits (including non-BMP characters)
and rejects 121/2,001 code points atomically.
Host coverage includes exercise shapes and raw partial text, fresh repository
restore, remove/discard, atomic
finalization and repeated-finalize rejection, rollback, catalog reconciliation,
missing-selection recovery, explicit DB-open failure, historical migration,
equipment selection and occurrence identity.

Schema-v9 host coverage additionally proves French raw max-text persistence,
explicit max creation/edit/finalization without sets, distinct movement values
on the same equipment, latest-per-exercise history, V2 replay, stable-ID resume
and bounded conversion that leaves multiple legacy attempts untouched.

Schema-v10 host coverage adds the canonical body-zone asset, stable-ID mapping,
relation constraints, filters, transactional editing and companion conflict
policy without changing explicit MAX or per-set tables.

The current set-row-editor coverage additionally proves that row edits,
deletion and append preserve neighbouring rows; French-comma loads, blank
loads and explicit zero loads remain distinct; reopening preserves aligned raw
row fields; and completed history keeps the ordered per-set values. The
instrumentation source exercises row edit, deletion, append and Activity
recreation, but this document does not claim that instrumentation was executed
for the current documentation checkpoint.

For the settled implementation, the validation inventory is:

```text
Android JVM:             ./gradlew testDebugUnitTest
Android compilation:     ./gradlew assembleDebug
Android instrumentation: adb shell am instrument ... (execution is explicit;
                         no device execution is asserted here)
Desktop:                 meson compile -C build
                         meson test -C build --print-errorlogs
Frozen JSON/import:      python tools/validate_json.py
                         python tools/validate_import_contract.py
V2 regression:           mobile_import_variable_sets and the desktop/Android
                         V2 round-trip/idempotent-replay coverage
Sanitizers:              clang ASan/UBSan Meson build and test invocation
```

Executed Body Zones V1 evidence is recorded after each closeout run: Android
`testDebugUnitTest` 44/44 with the retained real v9 fixture enabled and
`assembleDebug`, a historical 39/39 Meson checkpoint, valid and invalid JSON checks,
import-contract checks, and the ASan/UBSan Meson suite. Device installation and
installed Android-store migration remain explicit hardware steps and are never
inferred from host tests.

The 2026-09-09 desktop closeout additionally backed up the real v10 database,
opened it through the production Notcurses binary, and verified v11 integrity,
foreign keys, all historical row counts and bidirectional row equality against
the backup. A real Kitty terminal validation exercised the zone list, parent
filter, classified/unclassified details and edit preloading/cancellation. The
Samsung SM-G990B then passed certificate matching, `adb install -r`, the real
v9 -> v10 migration, SQLite integrity/FK checks, row equality for every
pre-existing application table, the Android zone UI matrix and two live MTP
round trips. The second run reported zero additions/reconciliations and left
the Android application tables and semantic companion exercise states equal to
the first run. A regression also covers a current
`trainlog-sync-request-v1 (N).json` beside an older canonical request.

The optional `RealAndroidV9BodyZonesMigrationTest` is enabled by setting
`TRAINLOG_ANDROID_V9_FIXTURE` to a coherent copied v9 database. It makes two
test-owned copies before opening the production repository, then compares all
16 pre-existing application tables in both directions; it never opens or
modifies the supplied fixture through the migration helper.

```bash
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew test
JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew assembleDebug assembleDebugAndroidTest
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell am instrument -w -e package com.labfytools.trainlog \
  com.labfytools.trainlog.test/androidx.test.runner.AndroidJUnitRunner
```

The following device instrumentation evidence is a prior baseline, not a
blanket device-validation claim for schema-v8 definitions or three-mode sync.
That suite has **5 tests**: two real-SQLite repository
checks and three production-screen Compose UI checks. Coverage includes exact
raw form restoration through Activity recreation, cancellation and confirmation
of discard, final save with no stale Resume after recreation, and refusal of a
second completion. All tests use isolated databases; the debug-only,
non-exported Activity renders production Home/Session screens without exporting
synthetic data. The phone must be unlocked and interactive. Do not interpret a
locked-screen `No compose hierarchies found` failure as a passing UI check.

The Samsung SM_G990B additionally passed the normal `MainActivity` matrix:

- two real catalog exercises with distinct continuous values survived Home,
  another app, background `am kill` with verified PID exit, and force-stop;
- rotation recreated the Activity and Home Resume restored both exercises;
- removal of one exercise survived process death, with the other intact;
- raw duration/speed text survived process death and force-stop exactly;
- Back and the normal new-session action preserved the existing draft;
- discard cancellation preserved the DB exactly; confirmation and relaunch
  left all draft tables empty and no Resume action;
- completed History and mobile export excluded the populated draft;
- every original domain row survived migration and the entire device matrix.

Final-save UI and repeated-finalization tests ran on-device with isolated data;
no fictitious completed session was added to the user's history. Real migration
and final state passed SQLite integrity/foreign-key checks. The pre-upgrade DB
and preferences backup is outside the repository. No uninstall, package-data
clear, desktop schema change or frozen artifact change is part of this repair.

## 14. Current real-data reconciliation validation

Before mutation, coherent desktop and Android v8 copies were created and passed
`PRAGMA integrity_check` plus `PRAGMA foreign_key_check`. The newest real
Android V2 artifacts were selected, including scoped-storage `(N)` siblings.
The production tools then ran in protocol order on the desktop copy:

```text
equipment definitions -> exercise/session/body V2 -> equipment associations
-> PC definitions/catalog/mobile/associations export -> second import
```

The result retained the desktop Marche identity with catalog
`data_fields = 3`, both older speed-only Marche occurrences, the Android
speed-and-distance occurrence, all original `entry_id`/positions, custom
equipment, set weights and body observations. The retired Android Marche ID had
no remaining desktop catalog/reference owner. The second replay changed no row
counts and both integrity checks remained clean.

This exercises real artifacts and production import/export code on copies. It
does not substitute for a final direct-MTP run when USB ownership or sandbox
permissions prevent libmtp access.

`tests/test_equipment_associations_exchange.py` additionally covers the
bounded v13 stale-companion bridge for the five frozen machine-exercise splits.
Plate Loaded Leg Press and Treadmill pass only with matching current mobile V3
proof; generic Marche, the durable Marche+treadmill draft, Seated Leg, and Rear
Delt NULL provenance remain unchanged; an unrelated exercise mismatch still
fails; and replay is idempotent.

For this checkpoint, the USB probe discovered the connected Samsung interface
but libmtp failed at `libusb_open()`. The canonical desktop DB was also
read-only to the sandbox; the first attempted definitions import failed before
mutation, and a repeated dump hash plus integrity/FK checks proved it unchanged.
The Android package remained force-stopped and ADB was used only for read-only
inspection/pull. Applying the validated state to both real stores and running
the two transport directions remains an out-of-sandbox hardware validation.

Detailed retained evidence: [Android draft execution record](reviews/android_session_draft_v1_resume.md).

## 15. Training knowledge V1 validation checkpoint

The current implementation passes the training-knowledge catalog
validator, eight Python generation/catalog regressions, 42 desktop Meson tests,
and standalone C17 public-header checks for `training_knowledge.h`,
`training_context.h` and `database.h`. A targeted AddressSanitizer/Undefined-
Behavior-Sanitizer build passed the two new desktop API tests. Android
`testDebugUnitTest assembleDebug` passed with 56 tests, zero failures/errors,
and one skipped real-v9 fixture test because `TRAINLOG_ANDROID_V9_FIXTURE` was
not available. The TUI knowledge screen's UTF-8 cell-aware scrolling was tested
at the 72x20 minimum terminal.

The catalog validator, JSON validator and import-contract validator also pass.
`git diff --check` passes. Scientific-review metadata and
catalog hashes were verified separately.

Temporal regressions now cover the original `+14:30`, `+15:00` and lowercase-`t`
failure, lowercase `z`, omitted seconds, high/negative offsets, exact fractions,
stable identity ties, cursor reuse, traversal through exhaustion and malformed
stored values. C tests also cover selected/unselected values beyond its fixed
output capacity. Four Python tests cover the explicit grammar and exact
chronology, including actual document admission with missing, strict and
permissive optional JSON Schema format checkers. An independent twelve-form
production C probe passes normally and under ASan/UBSan.

Fresh final validation passed: strict build; 42 Meson tests; eight Python
knowledge tests; four Python temporal tests; knowledge, JSON, and import
validators; three strict C17 headers; affected C knowledge/context tests under
ASan/UBSan plus Python timestamp validation; normal and sanitized independent
12-form temporal probes; Android 56 tests with zero failures/errors and one
known missing real-v9 fixture skip; Java 17 debug assembly; skill validation;
and `git diff --check`. Generated C is byte-identical with SHA-256
`e8c099f67eb111d61621b5d76592c049823af5508d43e73ec646f22e4c377fca`; all six
Android assets are byte-identical.

`TRAINING_KNOWLEDGE_V1=PASS`. The independent temporal review passed with no
findings; its coverage included parser grammar and limits, exact chronology,
ties, cursor semantics, source capacity, snapshots, and Android/Python parity.
The initial full audit's stale temporal documentation, Android loader, Meson
dependency, and C role-only query findings were repaired and independently
verified. See the [temporal contract](reviews/training_knowledge_v1_temporal_contract.md)
for the established contract. No real Android install,
manual TUI visual exercise, or manual MTP hardware validation was performed
for Training Knowledge V1.

Shell/naming closeout coverage includes real-PTY shell behavior, canonical
name export/import replay on temporary databases, and preservation of sets,
MAX, equipment, BODY ZONES, aliases, and distinct unmapped exercises.
EQUIPMENT_KNOWLEDGE_V2 validation covers relation envelopes, canonical UUID and
equipment references, enum/source integrity, uniqueness and stable ordering,
the unresolved Seated Leg invariant, generic/custom non-inference, and generated
C/Android query parity for leg press, combo equipment, and chest-zone derivation.
`training_feedback_sync` covers first import, identical replay, post-sync
follow-up addition, PC export convergence, and immutable-ID conflict. Android
unit tests cover partial/final recognition, correction, resume/append, stop,
save/cancel, failure preservation, permission/unavailable fallback, lifecycle
destroy, and H+ flooring/unknown anchors. Draft-feedback coverage saves before
completion, reopens the repository, preserves multiple stable IDs and ordering
across ordinary draft rewrites, verifies active/completed Compose readback,
excludes the draft rows from sync, transfers them atomically on finalization,
rolls back on an injected insert failure, and checks discard cascading. The
real-v13-copy production migration test also requires the existing active draft
and all its occurrences to remain identical while the new draft-feedback table
starts empty and a second production open is logically idempotent.
`feedback_correction` reconstructs completed occurrence rowids while retaining,
reordering and changing the exercise context of stable entries; it verifies
exact feedback preservation, selective cascade on removal, no inheritance by a
new entry, unchanged follow-ups, and full rollback on injected reattachment
failure. Android additionally covers its bounded resumed-MAX replacement path.
`tests/test_profile_state_sync.py` covers the captured Planche droite sol
legacy `SETS+REPS`/`SETS+DURATION` repair, idempotent replay, direct-descendant
and multi-generation stale-ancestor convergence, sibling conflict, the explicit
32-record resource bound, exact JSON scalar types, immutable machine conflict,
and preservation of the old occurrence snapshot. Android
`ExerciseProfileStateSyncTest` covers the same transitive/type contract in the
reverse PC→Android path and future-occurrence snapshot.
`HistoricalSessionProfileImportTest` exercises the production V2/V3 repository
import with equal and evolved tracking/recording/data-field profiles, strict
snapshot-local payload rejection, unknown identity rejection, unchanged
historical values, unchanged current profile history, and idempotent replay.
`SyncBundlePublicationTest` exercises the production request coordinator and a
synthetic direct exchange directory. The exact BODY ZONES file is rewritten
beside 31 preserved historical copies, no `(32)` copy occurs, all seven
Android-origin companions precede the request signal, and replay reuses
canonical names without stable-feedback churn.
It also requires one direct-child enumeration for the entire export-plus-request
transaction, one additional enumeration for each later synchronization, index
update after a create without rescan, one write per canonical artifact, and
non-main dispatcher ownership for snapshot and stream operations. Focused
publisher cases additionally cover an empty directory, exact rewrite,
ten successive publications without numbered copies, historical numbered
copies alone and beside a canonical object, exact final content, and an
explicit canonical-name conflict without writing a wrongly named target.
`DirectExchangeStorageTest` verifies the manifest permission and settings
action, refusal without an authorized backend, directory creation, real
temporary-file replacement and truncation, ten publications without suffixes,
and preservation of legacy Download and recovery fixtures. It also reconstructs
a truly fresh Android database from catalog, alias, profile-state and body-zone
companions, including an old zone `exercise_id` resolved through the durable
alias. A deliberately malformed downstream session companion proves zones are
already durable before the error; the repaired pass imports the one-sync AI
draft, and replay preserves three canonical identities and exactly seven zone
rows without duplicates.
`AiSessionDraftUiWiringTest` additionally requires the delete action to expose
the titled confirmation before mutation, verifies Cancel and Back dismissal,
then proves that only confirmation removes the pending proposal while retaining
its replay-blocking deleted tombstone. Its existing Start coverage remains.
Native `sync_body_zone_wiring` verifies the
profile pre-pass before mobile import, strict post-pass gating, replay, legacy
peer compatibility, and the explicit missing-companion diagnostic even when a
stale local temporary profile artifact exists. It also receives a valid alias
V1 companion through the production MTP orchestration, imports it into an
isolated database through the production Python argv path, and proves that the
sync continues. Negative cases cover an unresolved helper, argparse failure,
Python exception/stderr propagation, sync-history visibility, and rejection of
a stale result-file `PASS`; the successful flow exercises the other shared
Python helper callers as well.
Every Android outbound artifact and the request use the shared direct-storage
exact-name resolver; app-scoped MediaStore visibility is irrelevant.
`profile_state_sync` includes the production persisted update-trigger dependency
on `trainlog_profile_revision`, imports through the same Python connection path
used by the PC sync worker, and requires first import plus identical replay to
pass without revision-state or history-count churn. It also checks that the
central factory installs both profile-revision schema functions.

`TRAINLOG_ANDROID_DIRECT_STORAGE_V1` coverage fixes Android storage at
`/storage/emulated/0/Documents/Trainlog`, preserves canonical rewrite/no-suffix
behavior, ignores legacy Download trees, and gives the MTP fixture both
Documents and Download roots while failing on any legacy access.
Android unit tests and any instrumented tests run on an emulator; the primary
personal phone is reserved for manual, non-destructive install-and-sync smoke
validation and must not run `connectedDebugAndroidTest`.

## Generation, transaction, and acknowledgement V1

The normal Meson inventory includes `sync_generation_exchange` and
`schema_v21_migration`. The service suite exercises deterministic desktop
snapshot capture against a controlled writer, immutable publication/replay,
partial and substituted input, strict paths/capabilities/bounds, wrong ACK
context, durable ACK replay after reopen, late-domain rollback, rejected ACKs,
and unchanged causal-operation IDs/digests across first emission and
retransmission for all seven causal target kinds. The v20 fixture migration
preserves populated business state and creates no historical generation/ACK.

Android `SyncGenerationServiceTest` uses production repository codecs and the
filesystem object adapter. It proves a transaction-held capture against a
deterministically blocked writer, v19→v20 preservation, manifest-last immutable
publication, whole-generation rollback on a late semantic failure, committed
ACK replay after close/reopen, and causal-operation byte stability. Its real
cross-implementation test runs both chains: an Android-created session is
captured and transactionally consumed by the desktop Python service, and a
separate desktop-created session written through the public C17 database API is
captured by the desktop service and consumed by Android. Both ACKs return to
the original producer. The bridge moves bytes and invokes real entry points; it
does not author expected business JSON or repair artifacts.

After these additions the direct normal suites report 78/78 Meson tests and
216 Android tests: 212 passed, four historical-fixture skips, zero failures.

The grouped Web end-to-end tranche raises the direct normal and sanitizer
inventories to 79/79 and adds `sync_orchestrator`, protected sync API
coverage, frontend sync assertions and a real headless Firefox scenario in
`tests/test_web_sync_browser.py`. Its detailed matrix is
[`sync_web_end_to_end_v1_evidence.md`](design/sync_web_end_to_end_v1_evidence.md).
The browser uses the production C server and worker with an isolated directory
transport fixture; physical MTP and Drive are not claimed.
The publication adapter is an isolated directory/object-store double. No
physical MTP, phone, Drive, active service, or user database participates.
## Causal deletion V1

The normal Meson inventory includes `causal_delete_exchange`, which invokes
the production desktop service and proves all target kinds, relationship
safety, exact replay, conflicts, finalization protection, and rollback. Android
`causalSessionDeletionRoundTripsWithBothRealProducersAndBlocksReplay` uses the
real Android repository and desktop service in both producer directions with
fresh destinations, reopen/replay, and legacy non-resurrection.

`causalFiveDomainDeletionsRoundTripWithAndroidAndDesktopProducers` closes the
remaining producer/consumer matrix. The Android repository creates exercises,
populated observations, custom equipment, multi-revision feedback and BODY
ZONE relations; production V4 and companion tools establish the desktop
replica. Android-local deletion is consumed and replayed by the desktop
service. A second production-provisioned desktop replica creates each deletion
locally, exports it to Android, and receives Android's exact replay.

| Kind | Producer -> consumer | Stored/read-model assertion | Result |
|---|---|---|---|
| `exercise` | Android -> desktop | Historical row/reference retained; new-work predicate excludes it; unrelated exercise and alias protection remain | PASS |
| `exercise` | desktop -> Android | Repository list excludes it after reopen; history row remains; stale V4/catalog state cannot restore it | PASS |
| `body_observation` | Android -> desktop | Populated target row is absent; session and unrelated populated observation remain | PASS |
| `body_observation` | desktop -> Android | Repository list excludes the target after reopen; unrelated observation and session remain | PASS |
| `custom_equipment` | Android -> desktop | Custom definition/history remains but new-work predicate excludes it; built-in occurrence equipment remains | PASS |
| `custom_equipment` | desktop -> Android | Historical equipment row/reference remains; repository list excludes it; built-in equipment remains | PASS |
| `feedback` | Android -> desktop | Root and two immutable revisions remain; current predicate excludes it; unrelated feedback remains | PASS |
| `feedback` | desktop -> Android | Root/revisions remain after reopen; repository current view excludes it; unrelated feedback remains | PASS |
| `body_zone_relation` | Android -> desktop | Target secondary relation is absent; primary relation and exercise remain | PASS |
| `body_zone_relation` | desktop -> Android | Target secondary relation is absent after reopen; primary relation and exercise remain | PASS |

Every row uses a legitimate replica established through production exchange
entry points. Repeated local deletion returns the original immutable operation;
the opposite exporter preserves its operation ID, target, predecessor and
payload; replay after a new database connection is unchanged. Protected V4,
equipment, feedback and BODY ZONES legacy imports refuse stale restoration and
leave all accepted rows and causal state unchanged. Generic codec reuse is not
counted as a domain result.

Strict compilation uses Clang 22.1.8. GCC 16.2.1 retains the compared
pre-existing `-Werror=misleading-indentation` failure in unchanged Web baseline
sources; it is not reported as green and warning policy is unchanged.

The causal closeout full harness result is 76/76 normal Meson, 76/76
ASan/UBSan, and 211 Android tests: 207 passed and four optional historical
database fixtures skipped. `assembleDebug`, JSON validation, import-contract
validation, both frontend tests, and the isolated characterization all pass.

Focused closeout assertions are `test_child_mutations_invalidate_session_observation_and_feedback_deletes`,
`test_imported_builtin_and_alias_retirement_are_refused_without_writes`,
`test_draft_identity_is_semantic_revision_not_json_serialization`, and
`test_operation_count_and_input_byte_bounds_precede_effects` in the desktop
production-service suite. Android adds
`causalDraftDeletionUsesTheSameRevisionForActivePendingAndDesktopReplicas` and
`importedCausalOperationsCannotRetireBuiltInIdentities`; the existing session
round trip now also changes and reverts a confirmed set before proving the
remote deletion conflicts without losing business or causal state.

## Generation MTP and Android backup

`generation_mtp` drives the production transport algorithm through typed fake
libudev/libmtp callbacks. It covers exact peer selection, bounded download,
manifest-last publication, ambiguity and truncated-object rejection. The
Firefox end-to-end scenario also runs in `mtp` mode: only the callback table is
replaced by a filesystem object store while the C transport, Python worker,
Android generation service, desktop consumers and durable ACK path remain real.

`AndroidBackupServiceTest` creates and restores a populated current-schema
backup including an active draft, raw partial input, stable peer identity and
preferences, and rejects corrupt and traversal entries. The bridge build runs
the analogous test on the exact schema-v17 source and feeds its archive to
`BridgeBackupMigrationTest`, which verifies normal migration to schema v21.
These are software proofs. They do not claim physical USB/MTP behavior,
installation over the signed release, a real user backup, or a real-device
restore.

The production Android coordinator and desktop peer worker also complete 24
successive bidirectional conversations against their real SQLite services with
no cleanup between runs. The test crosses multiple archive cycles and checks
that the active draft survives. After the eighth exchange it also reproduces
the deployed legacy evidence gap by retaining acknowledged generations while
removing only their producer-side ACK rows; the next production conversation
must restore the desktop consumer's exact correlated ACKs, archive eligible
generations, and complete without deleting or duplicating any generation.
Focused archive tests cover interrupted-copy resume, invalid archives,
simulated insufficient space, missing and late ACKs, and idempotent ACK replay
after archival.

The controlled Samsung SM-G990B rollout migrated the authentic Android backup
from schema v20 to v21 and an authentic desktop copy from v21 to v22 before
installation. The installed release retained its existing certificate and
advanced from versionCode 6 through 7 to 8 without uninstalling or clearing
data. Real MTP validation preserved the failed-run evidence that exposed an
Android external-storage directory-fsync incompatibility and an early desktop
generation reference. After their bounded repairs, run
`sy_06200dd7-0d51-4f33-b80d-9976822ba87d` completed, the two Trainlog services
were restarted, and run `sy_783014b0-f3f0-4f1e-8f4f-723acfc02dbc` completed.
Both exchanged distinct generations and durable ACKs with zero session
reconciliation. The final Android backup passed integrity and foreign-key
checks at schema v21 with 13 generations, five verified archives, 13 ACKs,
five consumed generations, and six active generations. The expected AI
proposal remained `pending` with its original identity and six entries.

## Readability validation

The normalized C scope is checked non-mutatively with the explicit
`clang-format --dry-run --Werror` command in `docs/coding_style.md`. Clang C17,
the normal Meson inventory and the ASan/UBSan inventory preserve the existing
behavioral assertions. Python syntax/orchestrator tests, Android generation and
backup tests, the I/O-boundary MTP browser proof, JSON/import validators and
the isolated harness cover the other normalized sources.

GCC 16.2.1 and Clang 22.1.8 both pass separate strict C17 builds and the full
**81/81** Meson inventory with `werror=true` and warning level 3. The former GCC
`format-truncation` findings are closed by exact checked path construction in
the generation-MTP tests/double and by a production accepted-response
serializer that reports its byte count. Boundary tests cover exact fit,
one-byte insufficiency and multibyte path components; failure leaves no partial
path or response available to a caller. Clang ASan/UBSan passes the same
**81/81** inventory with leak detection.

The readability closeout preserves the **81/81** normal Meson and **81/81**
ASan/UBSan inventories. The isolated Android run reports **220 tests: 215
passed, five skipped, zero failures/errors**, and `assembleDebug` succeeds on
JDK 17. The real Firefox/object-I/O-boundary MTP conversation also passes after
formatting. A source comparison against the pre-readability commit confirms
that the scoped test names and assertion counts are unchanged.

The deployment-tools regression also constructs the relocatable candidate and
requires its inventory to include the packaged `trainlog-sync-once` binary,
the `trainlog-syncd` launcher and daemon source, and the generation worker.
This prevents a user unit from silently executing a helper from a different
checkout than the installed desktop candidate.

`syncd_routing` proves that both Android-labelled and daemon-labelled
admissions invoke the packaged `sync_orchestrator.py` full-generation owner
when trusted configuration is present, and never invoke the legacy
`trainlog-sync-once` command in that mode. The generation-MTP boundary test
proves that polling copies the Android request signal without acknowledging or
deleting it. Session-exchange characterization keeps standalone V3 rejection
strict while its complete-envelope case filters a tombstone-dominated live
fact; the causal Android/Desktop round-trip and generation replay/ACK tests
cover restart, idempotence and non-resurrection. `AndroidBackupServiceTest`
injects a product version and verifies that exact value in the ZIP manifest,
preventing a return to a hard-coded release string.

The isolated JDK 17 harness reports **220 Android tests: 215 passed, five
skipped, zero failures/errors**. The skips are optional, externally supplied
copy gates: `BridgeBackupMigrationTest.verifiedSchemaSeventeenBridgeBackupMigratesThroughCurrentOwner`,
`RealAndroidV9BodyZonesMigrationTest.realVersionNineCopyMigratesWithoutChangingExistingTables`,
`RealAndroidV14FeedbackRevisionMigrationTest.freshRealCopyMigratesLosslesslyToVersionFifteen`,
and the version-thirteen and version-twelve methods in
`RealAndroidV12MachineExerciseMigrationTest`. They require respectively the
bridge backup input, a real v9 fixture, a real v14 fixture/output directory,
and real v13/v12 fixtures with explicit output directories. Synthetic bridge
and current-schema migration proofs run separately; no user database is read.

The bridge migration test accepts both legitimate backup states: an active
draft may be present or absent. It snapshots every original table, column,
SQLite value type and value before restore, migrates through the production
repository owner, and compares that complete historical shape afterward. The
draft assertion follows the archived state instead of requiring the synthetic
fixture's sample draft.

## Private full-generation rollout validation

Routing regressions cover a strict V3 refusal in the presence of causal
deletions, publication of the dedicated full-generation request before legacy
compatibility, same-identity dual-channel deduplication, full-generation
priority over stale legacy requests, and bounded durable seen-request state.

The MTP visibility regression suite uses injected clocks and transport
callbacks, with no multi-second sleeps. It covers delayed Android publication,
multiple throttled polls, stale run references, a never-visible generation,
an independently blocked adapter, and exact protocol-versus-transport timeout
classification. Android tests prove fresh request UUIDs, foreground/background
lock ownership, objects-visible-before-reference ordering, bounded phase
tracing, and a completed full-generation conversation. Compose tests exercise
the measured 312 dp phone content width as two columns, a true 280 dp narrow
layout as one column, 1.5 font-scale fallback, long French labels and `kg`/`cm`
suffixes.

Android conversation-ownership regressions reproduce the production race: an
automatic owner waiting on an old resumable run yields to explicit foreground
intent, the click publishes a fresh request UUID, the explicit coordinator
ignores the pre-click desktop run, and exactly one generation is captured for
the new run. Additional fixtures cover typed cancellation and durable handoff,
terminal-run suppression across five scheduling cycles, reactivation by new
correlated desktop evidence, and exclusive ownership. Desktop routing tests
retain the complementary invariant that an already-consumed Android request
UUID cannot create a second daemon run.

The resumed run `sy_9639f06e-58ba-4bc0-ab1b-1488aefe011c` subsequently proved
the MTP publication path: Android captured
`gen_532da95e-713f-4163-acc1-d838673c33bf`, published the immutable objects and
reference, and exposed bytes identical to the filesystem through MTP. It
stopped after `generation_reference_published` because no ACK could be created
for the daemon-deduplicated old request UUID. This evidence supersedes the
earlier pre-resume forensic snapshot without rewriting or deleting the
incomplete generation.

The authorized 2026-09-17 rollout added regression coverage for production
failures found only after installation: feedback import under `sqlite3.Row`,
the complete exporter inventory of the relocatable package, stale Android ACK
and generation-reference correlation, and the completed-generation UI state.
The real direct-libmtp smoke then completed three bidirectional conversations
on the paired phone. Restart and replay preserved identical logical hashes for
23 desktop business tables and 540 rows; SQLite integrity and foreign-key
checks passed, both outgoing desktop generations were acknowledged, and the
Android UI displayed completion. The private journal retains exact IDs and
hashes. Drive and `connectedDebugAndroidTest` were not run; the personal phone
was used only for non-destructive install, backup and manual sync validation.
