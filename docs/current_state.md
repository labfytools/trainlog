# Current implementation state

Full-generation Web synchronization now selects an actually usable MTP peer
before USB, falls back to a separately configured private Drive generation
namespace, and mirrors successful USB conversations to Drive on a best-effort
basis without rolling back USB. Both transports move the same generation/ACK
bytes; neither transports SQLite. Android has an explicitly enabled foreground
USB listener with mandatory notification and a private-folder Drive grant with
bounded WorkManager checks. Both use the same concurrency-guarded generation
coordinator as SyncScreen.

Existing preparation rows expose separate editing and Android-delivery states
and an accessible send action. Draft/local advances the same preparation to a
ready revision before delivery; ready/local delivers the current revision.
Completed history hides the active preparation exclusively through the
delivery's stable execution-session identity and retains all provenance.

The opt-in full-generation path is operational on the private daily
installation. The loopback Web control and foreground Android coordinator use
the production direct-libmtp adapter, correlated manifest/ACK V1 objects, and
persistent peer identities. Controlled USB and private Drive validation proved
the complete Web-initiated request, generation, import, publication, and ACK
conversation without opening SyncScreen for each run. USB remains preferred;
the private `Trainlog/Sync/v1` namespace is configured as a verified mirror and
fallback, independently from `Trainlog/AI`. Cross-transport replay is
idempotent and neither transport carries SQLite.

The public Android release chain advances from v0.1.1 versionCode 2 to v0.1.2
versionCode 3 under certificate SHA-256
`5ef41117da107c9805ae3215c5ec823cff06216610e6e4f7542a754161c71bd2`.
Higher-numbered private validation builds used a separate Android Debug
certificate and are not members of the public update-signature chain.

`TRAINLOG_NATIVE_BUILD_AND_CANDIDATE_V1` closes the native warning gate. Strict
GCC 16.2.1 and Clang 22.1.8 builds now pass the same 81-test inventory, and the
Clang ASan/UBSan build passes it with leak detection. Bounded path assembly and
the Web sync accepted response reject insufficient capacity without publishing
partial output. Fresh desktop, current-Android and schema-v17 bridge candidates
were rebuilt from code commit `dd8c91a946a9c836965818d8cc35b8f8bce74658`.
The rollout packager now keeps the legacy request daemon and one-shot helper in
the same relocatable, hashed bundle as the Web generation coordinator, avoiding
mixed-checkout user-service execution during the controlled rollout.

Snapshot date: **2026-09-20**.

`TRAINLOG_CODE_READABILITY_V1` normalizes the recent generation-MTP,
generation/ACK, Android-backup, Web Dashboard serialization, tests, doubles and
packaging workflows without changing their contracts. The scoped C formatter
check is canonical for those files only; it does not claim that the historical
repository has been globally reformatted.

This document owns the current implemented state. Historical checkpoints and
closed incidents belong in [reviews](reviews/) and the
[changelog](../CHANGELOG.md).

## Product boundary

Android is the field companion: it captures training and body data, preserves
the active draft, receives AI proposals, triggers synchronization, and shows
quick summaries. The C17/Notcurses TUI is the administration, inspection,
maintenance, import/export, and technical-tooling surface. The local Web
sibling now provides its embedded application shell and implemented Dashboard;
its future business surfaces will own broader analysis, program/session
preparation, and exercise workflows through Trainlog Core.

Trainlog Core owns business truth. The desktop SQLite database remains the
canonical local source of truth and long-term history; no interface owns a
parallel implementation of its rules.

## Versions and compatibility

| Boundary | Current state |
|---|---|
| Frozen project exchange | `TRAINLOG_FORMAT_V1=PASS/FROZEN` |
| Desktop SQLite | schema v28 |
| Android SQLite | schema v25 |
| Mobile snapshot | V3 active; V1/V2 readable legacy inputs; explicit V4 codec staged, not selected by transport |
| Desktop terminal backend | Notcurses only |
| Trainlog product version | `0.1.3` development, synchronized across Android and desktop; latest stable release: `v0.1.2` |
| Interface language | `TRAINLOG_I18N_V0_1_1=PASS`: French default; English selectable in Settings → Language on both surfaces |
| AI history export | `TRAINLOG_AI_EXPORT_V1` active |
| AI session proposals | `TRAINLOG_AI_SESSION_DRAFT_V1=VALIDATION_PENDING` |
| Training knowledge | `TRAINING_KNOWLEDGE_V1=PASS` |
| Session generator | `SESSION_GENERATOR_V1=PASS`, hidden pending V2 |
| Application shell | `APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2` |
| Statistics | `STATS_V1=IMPLEMENTED` |
| Dashboard characterization | `WEB_DASHBOARD_CHARACTERIZATION_V1=PASS/FROZEN` |
| Dashboard Core read model | `WEB_DASHBOARD_CORE_READ_MODEL_V1=PASS/FROZEN` |
| TUI Dashboard adoption | `WEB_TUI_READ_MODEL_ADOPTION_V1=PASS/FROZEN` |
| Web CLI/HTTP infrastructure | `WEB_CLI_HTTP_INFRASTRUCTURE_V1=PASS/FROZEN` |
| Web frontend shell | `WEB_FRONTEND_SHELL_V1=PASS/FROZEN` |
| Web Dashboard grid | `WEB_DASHBOARD_GRID_V1=PASS/FROZEN` |
| Web Dashboard layout | `WEB_DASHBOARD_LAYOUT_V1=PASS/FROZEN` |
| Web Dashboard tiles | `WEB_DASHBOARD_TILES_V1=PASS/FROZEN` |
| Web Dashboard visualizations | `WEB_DASHBOARD_VISUALIZATIONS_V1=PASS/FROZEN` |
| Web Dashboard V1 | `WEB_DASHBOARD_V1=PASS/FROZEN` |
| Web Sessions V1 | `TRAINLOG_WEB_SESSIONS_V1=PASS/FROZEN` (controlled desktop/Android deployment validated) |
| Web Sessions deletion and Programs V1 | `TRAINLOG_WEB_SESSIONS_DELETE_AND_PROGRAMS_V1=PASS` (private grouped rollout validated) |
| Programs presentation, Android projection, and deletion | `TRAINLOG_PROGRAMS_PRESENTATION_ANDROID_DELETE_V1=PASS` (private coordinated deployment and restart/replay validated) |
| Program execution flow | `TRAINLOG_PROGRAM_EXECUTION_FLOW_V1=PASS` (private coordinated deployment, real execution, correlated ACK, restart, replay, and deletion validated) |
| Web history deletion incident | `TRAINLOG_HISTORY_DELETE_HTTP500_FIX_V1=PASS` (private rollout, correlated full-generation exchanges, restart, retained-generation replay, and non-resurrection validated) |
| Sync orchestrator/report V1 | `TRAINLOG_SYNC_ORCHESTRATOR_REPORT_V1=PASS/FROZEN` |
| Web sync API V1 | `TRAINLOG_WEB_SYNC_API_V1=PASS/FROZEN` |
| Web sync button V1 | `TRAINLOG_WEB_SYNC_BUTTON_V1=PASS/FROZEN` |
| Sync Web end-to-end wrapper | `TRAINLOG_SYNC_WEB_END_TO_END_V1=PASS/FROZEN` (isolated) |
| Private full-generation rollout | `TRAINLOG_SYNC_FINALIZATION_AND_ROLLOUT_V1=PASS` (direct MTP, one authorized phone) |
| Local Web | `TRAINLOG_WEB_V1=CONTRACT_FROZEN / IMPLEMENTATION_STARTED` |
| Web Exercises V1 | `TRAINLOG_WEB_EXERCISES_V1=PASS` |
| Current operational cursor | `TRAINLOG_ANDROID_UI_REDESIGN_V1_CONTRACT` |
| Complete synchronization gap contract | Frozen dependency contract; operational USB/Drive slices required by v0.1.2 are delivered |
| Isolated synchronization test environment | `TRAINLOG_SYNC_TEST_ENV_V1=PASS/FROZEN` |
| Synchronization characterization | `TRAINLOG_SYNC_CHARACTERIZATION_V1=PASS/FROZEN` |
| Synchronization data/lifecycle slice | `TRAINLOG_SYNC_DATA_LIFECYCLE_V1=PASS/FROZEN` |
| Synchronization causal deletion slice | `TRAINLOG_SYNC_CAUSAL_DELETE_V1=PASS/FROZEN` |
| Synchronization generation/ACK slice | `TRAINLOG_SYNC_GENERATION_ACK_V1=PASS/FROZEN` (explicit staged entry points; active transport remains V3) |
| Generation MTP and Android backup | Software complete and validated in the authorized private physical-device rollout |

Desktop and Android schema numbers are independent. Neither changes the frozen
Trainlog JSON V1 contract.

Language is device-local presentation state. Android keeps it in dedicated
SharedPreferences; desktop keeps it in the XDG configuration file
`trainlog/presentation.conf`. A successful selection updates the UI
immediately. It does not enter either SQLite database, training data, stable
IDs, schemas, exchange artifacts, synchronization protocols, AI proposals,
MAX, or feedback. User-defined exercise and catalogue names are never
automatically translated. Where a recognized stable body-zone ID has a local
presentation label, only that label is localized; unknown IDs retain their
catalogue label. Numbers, dates, and other visible formatting are owned by the
selected presentation language.

Synchronization remains a protocol boundary: core, receipt, and persisted
history summaries stay opaque operational bytes. Android and the TUI render
their current synchronization state from local typed status and counters; they
never inject a raw cross-device French summary into the visible interface. No
raw exchange or protocol format changed for interface language support.

Schema v19/v18 now persists enriched history (`ended_at`, causal session,
occurrence and observation notes, and observation-to-session links), stable
execution-draft identities, bounded pending drafts and finalization ledgers.
The explicit `trainlog-mobile-export` V4 and `trainlog-execution-drafts` V1
codecs are validation-stage entry points only. The active sync engine and
Android inbox/outbox still select V3 and do not move the draft artifact.

## Storage and synchronization

- Android stores capture data in its private SQLite database.
- Android reads and writes exchange artifacts only in
  `/storage/emulated/0/Documents/Trainlog` after the user grants all-files
  access through system settings.
- The old SAF/tree-URI path and `Download/Trainlog` are historical recovery
  boundaries, not active storage choices.
- Desktop synchronization uses direct libudev/libmtp discovery and object
  access. No GVFS/FUSE mount is required.
- `trainlog_sync_run()` is shared by the TUI and `trainlog-syncd`.
- The desktop imports Android snapshots, publishes catalog/profile/feedback/AI
  companions, and returns a receipt. Stable IDs, artifact idempotence and the
  few domain-specific explicit states make defined replay paths idempotent.
  Explicit staged generation services now provide coherent V4/companion
  capture, immutable manifests, whole-generation SQLite consumption and
  durable peer ACKs. A trusted opt-in configuration may select the bounded
  production generation MTP adapter; absent configuration leaves V3 as the
  default.
- SQLite database files are never synchronized.
- The desktop AI flow uses external `rclone` for Drive inbox/archive and
  read-only history export. Android owns no Drive credentials.

The frozen [complete synchronization gap contract](design/sync_gap_contract_v1.md)
remains the authority for the later orchestrator. Mobile V3 still omits
`ended_at`, session/occurrence/body-observation notes, the body-observation to
session link and the active Android draft. Legacy imports remain transactional
per artifact. Explicit generation consumption instead validates every listed
artifact, applies all domains and its consumption record in one transaction,
and emits `consumed` only after commit. A legacy receipt remains distinct from
the correlated generation ACK.
Characterization now also freezes the observed preservation of local-only
session/body values on an identical replay, loss of an occurrence-local note
when V3 correction reconstructs that occurrence, delete-before-send exposure,
request replay/receipt marking, and real inter-process lock contention. These
are current-behavior proofs, not lifecycle repairs.

## Android

Android currently provides:

- metadata-driven SETS+REPS, SETS+DURATION, and CONTINUOUS+DURATION capture;
- actual per-set values, optional occurrence equipment, and explicit MAX rows;
- exactly one durable active session draft with identity-preserving continuous
  multi-position drag reorder,
  resume, confirmed discard, and atomic completion;
- completed-session consultation, transient drag reorder, and atomic factual or
  order correction while retaining session/occurrence/feedback identities for
  synchronization;
- exercise catalogue editing with stable `exercise_id` and referenced-profile
  protection;
- supplied/custom equipment and machine-exercise metadata;
- body measurements and recent BODY ZONES summaries;
- immediate exercise feedback, immutable wording revisions, and J+1/session
  follow-ups;
- pending AI proposal collection, explicit start/delete, and durable tombstones;
- 7/30/90-day, year, and all-history quick statistics;
- direct-storage snapshot publication, sync triggering, locally rendered typed
  receipt status/counters, and companion application.
- French-default/English-selectable presentation from **Settings → Language**,
  applied immediately without changing repository or synchronization state.

Android does not own canonical long-term analytics. The V1 session generator is
implemented but intentionally hidden from normal navigation pending V2.

## Desktop TUI

The desktop is a strict C17 application using Notcurses. It currently provides:

- a keyboard-first seven-section shell, semantic true-color roles, UTF-8 input,
  resize recovery, and a 72x20 minimum-terminal fallback;
- session creation, history, detail, and transactional correction;
- exercise and equipment catalogue management;
- explicit measured-MAX consultation and working-load calculations;
- body observation history, correction, derived analytics, and longitudinal
  graphs;
- statistics derived from canonical occurrence-owned actual work, including
  exact calendar-week/month buckets, performance, body, frequency, exercise,
  and BODY ZONES views;
- detailed locally rendered typed synchronization status/history through the
  shared engine.
- French-default/English-selectable presentation from **Settings → Language**,
  including selected-language formatting without changing canonical data.

Rendering is not permitted to own SQL or business rules. Statistics are
read-only projections; they are not persisted as facts. The former Dashboard
exception is resolved: its typed Core read model owns the SQLite projection and
all characterized calculations, while the TUI supplies query state and renders
the result. The Web aggregate now exposes the distinct factual snapshot through
`GET /api/v1/dashboard` without changing the frozen TUI projection.

## Local Web

`TRAINLOG_WEB_V1` architecture, API independence, local-network boundary,
browser shell, Dashboard, layout ownership, build/runtime separation, and
security invariants are canonical. The CLI/local HTTP infrastructure and
embedded frontend are implemented: `-w`/`--web`, optional `--port`, exact IPv4
loopback binding, `/api/v1/health`, React navigation, and the persistent
Header/Body/Footer shell. Production assets are generated from the npm lockfile
and linked into the binary. The read-only Dashboard uses one typed Core
snapshot, a seven-tile responsive grid, factual size-adaptive tiles, an ECharts
progression series, an original BODY ZONES SVG, and private versioned layout
persistence with optimistic conflict and CSRF/Origin protection. It provides no
automatic browser launch. Sessions provides complete paged preparation,
resume and history views, stable details, optimistic manual-preparation writes,
explicit proposal derivation and generation-backed Android delivery. The
top-level `/programmes` route is an operational daily active-Program calendar;
Sessions → Programmes remains the technical administration/import, list,
detail, archive, and delete surface. `/exercices` is the operational desktop
catalogue administration surface; Analyse remains an explicit placeholder.

Exercises uses bounded Core-owned list/detail reads, canonical name
normalization, BODY ZONES taxonomy, profiled creation/update commands and
causal retirement. Its deterministic optimistic token covers all editable
catalogue state. Built-in identities cannot be retired, equipment associations
are presented read-only, and profile edits affect future use only: occurrence
snapshots, performed sets, feedback and MAX history are never rewritten. A
successful mutation changes only the canonical desktop database; the existing
USB-priority, Drive mirror/fallback synchronization later carries the existing
catalogue/profile/BODY ZONES/causal artifacts. No Web mutation invokes sync.

The calendar uses the existing typed Core services `fetchAllPrograms(active)`,
`fetchProgram`, and `createPreparationFromProgram`; it derives no local program
or execution state. With no active Program it shows an empty state and
navigation to Sessions administration; with one it selects it automatically;
with several it offers a non-persisted selector. Its continuous Monday–Sunday
weeks derive from Program bounds and real `planned_for` dates, do not invent
rest-day sessions, and retain undated sessions in a separate section. It shows
all five Core execution states; only `todo` offers Prepare. Each Prepare result
is followed by an authoritative Core reread and may expose the returned
preparation link. This calendar adds no Android delivery, start, or execution
behavior.

Desktop schema v26 owns immutable manual-preparation revisions, stable ordered
occurrences, persistent HTTP idempotency keys, delivery-to-execution identity,
and durable revision-bound withdrawal records. Android schema v24 stores
received preparations and permanent withdrawal results separately from AI
proposals and the active singleton. Pending deliveries are cancelled by an
exact V2 withdrawal; started executions are preserved. An exact correlated ACK
is required before delivery or withdrawal is shown as acknowledged.

Sessions and the prepared-items Dashboard projection now share descending
planned-date presentation with timestamp and stable-identity tie-breaks. Web
dates default to French presentation, with an installation-local versioned ISO
preference outside business SQLite and synchronization. Contextual French state
labels and persisted proposal provenance replace raw enums and digest-only
presentation. The preparation detail exposes a protected, idempotent **Delete
preparation** action; withdrawn rows remain technically inspectable but cannot
be edited or delivered.

Sessions rows expose independent right-edge trash actions and an accessible
destructive confirmation dialog. Proposal withdrawal preserves derived
preparations and is propagated by `trainlog-ai-session-drafts` V2; inactive
execution drafts and completed sessions use revision-guarded causal deletion.
Active or concurrently changed drafts conflict instead of being erased.

Program V1 is desktop-owned planning data in schema v26. The responsive
Sessions Programs subtab provides cards and timeline detail backed by real
imported definitions, sessions, usage, and provenance; it keeps strict bounded
preview/commit import and deterministic replay/conflict behavior. A discrete,
accessible trash action uses confirmation, focus restoration, and Escape-safe
cancel semantics. Active and archived programs may be terminally logically
deleted; sources and derived preparations remain, while deleted programs cannot
resurrect or reprepare and archived programs cannot prepare.

Android schema v24 contains a read-only synchronized Programs projection with
program, session, entry, and deletion tables. Its Sessions list/detail has no
program mutation actions. The optional staged controlled-generation
`trainlog-programs` V1 companion publishes a PC-to-Android full nondeleted
snapshot plus unacknowledged tombstones under capability `programs-v1`; a
durable correlated ACK records acknowledgment. It changes neither
`TRAINLOG_FORMAT_V1`, mobile V3, catalog exchange, nor preparations.

Android schema v25 makes those projected sessions executable through the
existing durable singleton. Stable paired Program provenance survives draft
restart and completion. The optional Android-to-desktop
`trainlog-program-executions` V1 companion records `in_progress` or `completed`
against desktop schema v27 without transferring ownership of Program
definitions. Web and Android derive their session state from these persisted
identities; no display-field heuristic is used.

The private Program execution rollout installed desktop schema v27 and the
signed nondebuggable Android versionCode 17/schema v25 update without
uninstalling or clearing application data. A strictly disposable Program was
synchronized to Android, started, left and resumed through the durable
singleton, and completed as session
`se_6bb66f59-a6fb-4759-a75d-c1a79ea107e8`. Android displayed `Effectuée` and
desktop/Web received the same completed provenance through the real
`program-executions-v1` producer and consumer. A second exchange created no
duplicate and no second active draft. The disposable Program was then deleted
through the revision-guarded Web command; Android retained exactly one
tombstone and desktop retained exactly one acknowledged deletion ledger row.
After both stores and the Trainlog components were reopened, exact replay of
the preserved pre-deletion Android generation returned its production durable
ACK without duplicating the execution or resurrecting the Program. A final
correlated exchange remained completed with no missing capability. The real
Program remains active and unique with 24 sessions and 164 ordered entries.

The private Programs rollout migrated desktop schema v25 to v26 and Android
schema v23 to v24 after verified backups, then installed the signed
non-debuggable versionCode 16 APK without uninstalling or clearing data. The
real 24-session Program remained unique and readable on Web and Android. A
clearly disposable Program was imported, synchronized, cancelled once without
mutation, then logically deleted through the Web confirmation. Android consumed
the real tombstone and desktop recorded its correlated generation ACK. A first
post-restart attempt failed before generation with `device_unavailable` and
`PTP_ERROR_IO`; no success state was recorded. Releasing the stale bounded MTP
worker and restarting the local ADB server restored the same physical peer. The
subsequent correlated exchange completed in both directions, and store reopen
confirmed one durable Android tombstone, one acknowledged desktop deletion,
no duplicate Program, and no resurrection.

The controlled deployment preserved the installed Android signing identity and
advanced its private `versionCode` from 13 to 14 without uninstalling or
clearing data. The requested six-occurrence validation preparation was already
`started` on Android at preflight, so withdrawal retained its immutable
delivery and returned `execution_preserved`; it did not delete or complete the
reserved execution. The source proposal retained its identity, `archived`
state, content and publication timestamp. Two correlated post-withdrawal
generation conversations, separated by component restart, completed without
resurrection, duplicate withdrawal, performed fact, or draft mutation.

The Next Session tile no longer treats the frozen Dashboard
`next_session.available` placeholder as a durable inventory. A separate bounded
`GET /api/v1/prepared-items` Core projection distinguishes manual preparations,
imported AI proposals, and active or pending execution drafts and exposes stable
identity, state, title/date, occurrence count, and provenance. Reload and later sync
failure do not erase the last successfully read projection. Proposal targets
remain planning data and never alter Dashboard actual-work metrics.

Full-generation MTP publication now uses a private phase-owned outbox. The
request, desktop consumption ACK, and current desktop generation are published
only when their phase requires them; retained staging, inbound objects, legacy
files, prior ACKs, and causal evidence are not recursively re-uploaded. Adapter
timeouts are persisted as the stable `transport_timeout` status instead of a
truncated Python traceback. The browser polls through one abortable scheduler,
orders revisions per `run_id`, and rejects stale responses from older runs.
The reciprocal MTP read is bounded to the Android peer advertisement, current
generation reference and consumption ACK, plus the single Android generation
named by that reference. Historical generations and other retained evidence
are neither traversed nor deleted during polling. Before peer validation, MTP
mode now requires one successful current-device pull; a retained pre-upgrade
advertisement can no longer cause a false missing-capability failure.

## Data semantics

- `exercise_id` identifies a catalogue exercise; `entry_id` identifies one
  ordered occurrence in a session.
- Plans and performed work are distinct. A continuous activity is not a fake
  performed set, and actual repetitions may be heterogeneous.
- Explicit MAX results are occurrence-owned and distinct from ordinary best
  sets. Equipment/resistance/execution context remains relevant.
- BODY ZONES are a UX/business projection over explicit scientific knowledge,
  not a complete anatomical taxonomy.
- Feedback roots retain identity while append-only revisions preserve wording
  history. J+1/session follow-ups are session-owned.
- AI proposals contain target-only plans and never create performed work,
  history, MAX, or feedback until the user explicitly starts and completes a
  normal capture draft.

## Validation status

The durable commands are owned by [tests.md](tests.md). At commit
`14b863eb0f159d46891f4ad5cf076b3aa0a11bba` on 2026-09-17, isolated validation
passed desktop compilation, **75/75 normal Meson tests** and
**75/75 ASan/UBSan Meson tests**, Android debug assembly and **203 Android
tests (199 passed, 4 skipped, 0 failed)**, the JSON validator, and the
import-contract validator. The source-derived TUI
`TRANSLATABLE_UI=0` check and Android resource parity also passed. Link and
diff safety checks passed in that checkpoint evidence. These are historical
results for that commit, not executions performed by later documentation work.
The four Android skips were the optional historical v9, v12, v13, and v14
database-fixture migration cases. Hardware MTP, real Drive, and Android
instrumented tests were not part of that isolated run.

For `TRAINLOG_SYNC_CHARACTERIZATION_V1` on 2026-09-17, the isolated JDK 17
preflight, smoke, and full modes passed. The final full run passed desktop
compilation, **75/75 normal Meson tests**, **75/75 ASan/UBSan Meson tests**, the
explicit production-path sync-gap characterization, both JSON/import contract
validators, the targeted Android draft lifecycle test, Android debug assembly,
and **204 Android tests (200 passed, 4 skipped, 0 failed/error)**. The added
Android↔desktop↔fresh-Android V3 round trip runs inside that Android total. The
four skips remain the documented optional historical v9, v12, v13, and v14
database fixtures. No hardware MTP, real Drive, adb, instrumented device test,
user database, exchange directory, or user service participated.

For the `TRAINLOG_SYNC_DATA_LIFECYCLE_V1` closeout on 2026-09-17, real staged
V4 history and execution-draft entry points completed both Android/desktop
producer directions through fresh destination databases. The closeout fixed
one Android V4 import defect that validated but failed to persist occurrence
plan fields. Isolated validation passed **75/75 normal Meson tests**,
**75/75 ASan/UBSan Meson tests**, Android debug assembly, and **207 Android
tests (203 passed, 4 skipped, 0 failed/error)**. The four skips remain the
optional real historical v9, v12, v13, and v14 database fixtures. The active
transport remains V3; no phone, MTP, Drive, service, user database, deployment,
or instrumented device test participated.

The Dashboard Core extraction tranche passes **62/62 normal Meson tests** and
**62/62 ASan/UBSan Meson tests**, including the standalone public-header and
Core characterization targets, plus both canonical JSON validators.

The CLI/HTTP infrastructure tranche passes **67/67 normal Meson tests**, adding
pure CLI parsing, process-level help/version checks and an isolated loopback
HTTP lifecycle/security suite. The same suite is required under ASan/UBSan.

The frontend shell tranche passes **70/70 normal and ASan/UBSan Meson tests**,
including TypeScript typecheck, 10 Vitest component tests, deterministic asset
generation bounds, real embedded-asset HTTP coverage, SPA/API separation and
runtime execution without Node/npm or `web/dist`.

The Dashboard data-contract tranche passes **71/71 normal and ASan/UBSan
Meson tests**, including the bounded Core/JSON target, HTTP endpoint coverage
and 12 Vitest tests for parsing and factual Footer integration.

`WEB_DASHBOARD_GRID_V1=PASS/FROZEN` uses `react-grid-layout` 2.2.4 behind an
independent Trainlog model/validator. Its 12-column desktop layout has seven
stable tile IDs, bounded per-tile dimensions, vertical collision compaction,
explicit edit/cancel/reset behavior, mouse drag/two-axis
resize, an announced keyboard alternative, and derived six/one-column
responsive projections. Persistence is supplied by the following frozen slice.

`WEB_DASHBOARD_LAYOUT_V1=PASS/FROZEN` adds a strict versioned seven-tile file
under the private XDG configuration directory, backend revalidation, atomic
0600 replacement, ETag/If-Match conflict control, Origin plus ephemeral CSRF
protection, and durable React load/save/reset behavior. Only the desktop
12-column canon persists.

`WEB_DASHBOARD_TILES_V1=PASS/FROZEN` renders all seven domains from the one
validated `/api/v1/dashboard` snapshot. Compact, medium and large densities
only reveal existing facts. Activity uses a factual CSS day map, MAX lists are
bounded 1/3/8, and muscle bars encode `session_count` only. Unavailable,
partial, invalid and transport-error states never synthesize values.

`WEB_DASHBOARD_VISUALIZATIONS_V1=PASS/FROZEN` uses a lazy, modular Apache
ECharts 6.1.0 SVG line chart for medium/large Progression tiles. Every real
point remains visible, `smooth=false`, legacy zeroes are preserved, and only
Core-owned `improved` flags alter markers. An original React SVG supplies
front/back BODY ZONES regions whose discrete colour level depends solely on
`session_count`; the factual text list remains authoritative and accessible.

`TRAINLOG_I18N_V0_1_1=PASS` is covered by desktop presentation, persistence,
formatting, layout-invariance and source-derived text-boundary tests, plus
Android resource-parity, language-owner, typed sync-presentation, and
stable-data presentation tests. A real-device/manual visual language-switch
smoke remains a manual validation; it does not change the validated status.

Hardware-dependent MTP is not automated. The controlled real-device rollout
validated two consecutive post-withdrawal full-generation exchanges around a
Trainlog component restart. Android retained exactly one withdrawal result as
`execution_preserved`, retained the linked delivery as `started`, and exposed
no active preparation or start action after replay. Desktop retained exactly
one acknowledged withdrawal and no active preparation. The source AI proposal
was verified with its original identity, state, entry content and publication
timestamp; no proposal-to-execution-draft conversion was performed.

## Active limitations

`TRAINLOG_SYNC_CAUSAL_DELETE_V1=PASS/FROZEN` provides isolated Android and
desktop producers/consumers for `trainlog-causal-deletions` V1. All contracted
domains are durably protected; exact replay is idempotent and unprovable or
concurrent ancestry conflicts before mutation. Protected legacy snapshots are
refused before resurrection. The default automatic daemon path remains V3,
while the trusted local Web opt-in now runs the deployed full-generation path.
Its acknowledged generations can leave the active admission window only
through the verified schema-v22/schema-v21 archive ledger; pending, rejected
and ambiguous generations remain protected.

The focused causal closeout additionally proves child-data delete/update
conflicts including change-and-revert, imported built-in and alias protection,
active/pending/desktop draft-revision equivalence, protected companion refusal,
current-view retirement and bounded pre-effect admission. The later generation
slice additively advances schemas to desktop v22/Android v21 without changing
default automatic V3 selection.

The residual causal evidence also proves legitimate Android and desktop
producer directions separately for exercise retirement, populated body
observation deletion, custom-equipment retirement, multi-revision feedback
withdrawal and BODY ZONE relation removal. Production exchanges establish each
replica before deletion; stored effects, current readers, stale-companion
refusal and exact replay are checked after reopening the receiving store.

The isolated closeout passed 76/76 normal Meson tests, 76/76 ASan/UBSan
tests, Android debug assembly, and 211 Android tests (207 passed, four optional
historical-fixture skips). Strict desktop builds use Clang 22.1.8; the compared
pre-existing GCC 16.2.1 Web warning gate remains documented rather than green.

- `TRAINLOG_WEB_EXERCISES_V1=PASS`. The next planned work is
  `TRAINLOG_ANDROID_UI_REDESIGN_V1`; it remains unimplemented. Analyse remains
  a placeholder and is not a v0.1.3 priority.
- `APP_SHELL_V1` retains a recorded legacy visual/accessibility review gate; its
  observations are inputs to, not decisions for, the new 0.1.3 Android contract.
- Session Generator V1 is hidden while V2 planning semantics are developed.
- Hardware MTP validation requires a connected unlocked Android device.
- Scientific knowledge is bounded to reviewed catalog entries; unknown custom
  exercises remain unclassified rather than inferred from names.
