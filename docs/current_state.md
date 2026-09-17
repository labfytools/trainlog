# Current implementation state

Snapshot date: **2026-09-17**.

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
| Desktop SQLite | schema v19 |
| Android SQLite | schema v18 |
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
| Local Web | `TRAINLOG_WEB_V1=CONTRACT_FROZEN / IMPLEMENTATION_STARTED` |
| Complete synchronization gap contract | `TRAINLOG_SYNC_GAP_CONTRACT_V1=CONTRACT_FROZEN / IMPLEMENTATION_IN_PROGRESS` |
| Isolated synchronization test environment | `TRAINLOG_SYNC_TEST_ENV_V1=PASS/FROZEN` |
| Synchronization characterization | `TRAINLOG_SYNC_CHARACTERIZATION_V1=PASS/FROZEN` |
| Synchronization data/lifecycle slice | `TRAINLOG_SYNC_DATA_LIFECYCLE_V1=PASS/FROZEN` |

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
  Current snapshots do not provide general tombstones, a common generation or
  proof that Android durably consumed the desktop publication.
- SQLite database files are never synchronized.
- The desktop AI flow uses external `rclone` for Drive inbox/archive and
  read-only history export. Android owns no Drive credentials.

The frozen [complete synchronization gap contract](design/sync_gap_contract_v1.md)
documents the future target without implementing it. Mobile V3 still omits
`ended_at`, session/occurrence/body-observation notes, the body-observation to
session link and the active Android draft. Imports are transactional per
artifact, not globally across a logical publication. A receipt records desktop
processing; publication alone does not prove durable peer consumption.
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
business mutation or automatic browser launch. Analyse, Programmes, Sessions,
and Exercises remain explicit placeholders.

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

Hardware-dependent MTP and the final real Drive plus Android-triggered AI-draft
smoke test are not automated. The latter is why
`TRAINLOG_AI_SESSION_DRAFT_V1` remains `VALIDATION_PENDING`.

## Active limitations

- `WEB_NEXT_MODULE_SELECTION_V1`: Analyse, Programmes, Séances and Exercices
  remain shell placeholders until their Core/API ownership and implementation
  order are explicitly contracted.
- `APP_SHELL_V1` still awaits the recorded human visual/accessibility review.
- AI proposal exchange still awaits one real Drive plus Android-triggered
  bidirectional smoke test.
- Session Generator V1 is hidden while V2 planning semantics are developed.
- Hardware MTP validation requires a connected unlocked Android device.
- Scientific knowledge is bounded to reviewed catalog entries; unknown custom
  exercises remain unclassified rather than inferred from names.
