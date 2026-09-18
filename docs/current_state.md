# Current implementation state

The opt-in full-generation path is operational on the private daily
installation. The loopback Web control and foreground Android coordinator use
the production direct-libmtp adapter, correlated manifest/ACK V1 objects, and
persistent peer identities. The controlled rollout verified real backups, the
schema-v17 to v20 Android update chain, desktop v18 to v21 migration, three
complete hardware conversations, restart, and idempotent replay. Drive was not
configured and remains outside this local MTP result. This is a private 0.1.2
development deployment, not a public release; v0.1.1 is unchanged.

`TRAINLOG_NATIVE_BUILD_AND_CANDIDATE_V1` closes the native warning gate. Strict
GCC 16.2.1 and Clang 22.1.8 builds now pass the same 81-test inventory, and the
Clang ASan/UBSan build passes it with leak detection. Bounded path assembly and
the Web sync accepted response reject insufficient capacity without publishing
partial output. Fresh desktop, current-Android and schema-v17 bridge candidates
were rebuilt from code commit `dd8c91a946a9c836965818d8cc35b8f8bce74658`.
The rollout packager now keeps the legacy request daemon and one-shot helper in
the same relocatable, hashed bundle as the Web generation coordinator, avoiding
mixed-checkout user-service execution during the controlled rollout.

Snapshot date: **2026-09-17**.

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
| Desktop SQLite | schema v23 |
| Android SQLite | schema v22 |
| Mobile snapshot | V3 active; V1/V2 readable legacy inputs; explicit V4 codec staged, not selected by transport |
| Desktop terminal backend | Notcurses only |
| Trainlog product version | `0.1.2` development, synchronized across Android and desktop; latest stable release: `v0.1.1` |
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
| Web Sessions V1 | `TRAINLOG_WEB_SESSIONS_V1=PASS/FROZEN` (isolated software validation; controlled deployment pending) |
| Sync orchestrator/report V1 | `TRAINLOG_SYNC_ORCHESTRATOR_REPORT_V1=PASS/FROZEN` |
| Web sync API V1 | `TRAINLOG_WEB_SYNC_API_V1=PASS/FROZEN` |
| Web sync button V1 | `TRAINLOG_WEB_SYNC_BUTTON_V1=PASS/FROZEN` |
| Sync Web end-to-end wrapper | `TRAINLOG_SYNC_WEB_END_TO_END_V1=PASS/FROZEN` (isolated) |
| Private full-generation rollout | `TRAINLOG_SYNC_FINALIZATION_AND_ROLLOUT_V1=PASS` (direct MTP, one authorized phone) |
| Local Web | `TRAINLOG_WEB_V1=CONTRACT_FROZEN / IMPLEMENTATION_STARTED` |
| Complete synchronization gap contract | `TRAINLOG_SYNC_GAP_CONTRACT_V1=CONTRACT_FROZEN / IMPLEMENTATION_IN_PROGRESS` |
| Isolated synchronization test environment | `TRAINLOG_SYNC_TEST_ENV_V1=PASS/FROZEN` |
| Synchronization characterization | `TRAINLOG_SYNC_CHARACTERIZATION_V1=PASS/FROZEN` |
| Synchronization data/lifecycle slice | `TRAINLOG_SYNC_DATA_LIFECYCLE_V1=PASS/FROZEN` |
| Synchronization causal deletion slice | `TRAINLOG_SYNC_CAUSAL_DELETE_V1=PASS/FROZEN` |
| Synchronization generation/ACK slice | `TRAINLOG_SYNC_GENERATION_ACK_V1=PASS/FROZEN` (explicit staged entry points; active transport remains V3) |
| Generation MTP and Android backup | `TRAINLOG_SYNC_MTP_AND_ANDROID_BACKUP_V1=SOFTWARE_COMPLETE` (physical-device and user-operation gates remain) |

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
explicit proposal derivation and generation-backed Android delivery. Analyse,
Programmes and Exercises remain explicit placeholders.

Desktop schema v23 owns immutable manual-preparation revisions, stable ordered
occurrences, persistent HTTP idempotency keys and delivery-to-execution identity.
Android schema v22 stores received preparations separately from AI proposals
and the active singleton. An exact correlated ACK is required before delivery
is shown as acknowledged.

The Next Session tile no longer treats the frozen Dashboard
`next_session.available` placeholder as a durable inventory. A separate bounded
`GET /api/v1/prepared-items` Core projection distinguishes imported AI
proposals from active or pending execution drafts and exposes stable identity,
state, title/date, occurrence count, and provenance. Reload and later sync
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
are neither traversed nor deleted during polling.

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
validated two consecutive full-generation exchanges around a Trainlog-service
restart. The expected AI proposal was verified in the persistent Android store
and the read-only Web proposal projection with its original identity and
content; no proposal-to-execution-draft conversion was performed.

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

- `WEB_NEXT_MODULE_SELECTION_V1`: Analyse, Programmes and Exercices remain
  shell placeholders. Exercises is the next proposed Web module.
- `APP_SHELL_V1` still awaits the recorded human visual/accessibility review.
- Session Generator V1 is hidden while V2 planning semantics are developed.
- Hardware MTP validation requires a connected unlocked Android device.
- Scientific knowledge is bounded to reviewed catalog entries; unknown custom
  exercises remain unclassified rather than inferred from names.
