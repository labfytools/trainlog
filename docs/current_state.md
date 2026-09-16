# Current implementation state

Snapshot date: **2026-09-16**.

This document owns the current implemented state. Historical checkpoints and
closed incidents belong in [reviews](reviews/) and the
[changelog](../CHANGELOG.md).

## Product boundary

Android is the field companion: it captures training and body data, preserves
the active draft, receives AI proposals, triggers synchronization, and shows
quick summaries. The C17/Notcurses TUI is the administration, inspection,
maintenance, import/export, and technical-tooling surface. The documented but
not yet implemented local Web sibling will own analysis, visualization, and
program/session preparation through Trainlog Core.

Trainlog Core owns business truth. The desktop SQLite database remains the
canonical local source of truth and long-term history; no interface owns a
parallel implementation of its rules.

## Versions and compatibility

| Boundary | Current state |
|---|---|
| Frozen project exchange | `TRAINLOG_FORMAT_V1=PASS/FROZEN` |
| Desktop SQLite | schema v18 |
| Android SQLite | schema v17 |
| Mobile snapshot | V3 active; V1/V2 readable legacy inputs |
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
| Local Web | `TRAINLOG_WEB_V1=CONTRACT_FROZEN / IMPLEMENTATION_NOT_STARTED` |

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
  companions, and returns a receipt. Stable IDs and tombstones make defined
  replay paths idempotent.
- SQLite database files are never synchronized.
- The desktop AI flow uses external `rclone` for Drive inbox/archive and
  read-only history export. Android owns no Drive credentials.

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
read-only projections; they are not persisted as facts. The current TUI
Dashboard still has a known exception: its direct SQLite/
`database_internal.h` fact projection is now frozen by characterization tests.
It must next be extracted into a typed Core read model and consumed by the TUI
before any Web Dashboard endpoint is created. This is an incremental
extraction, not authorization for a global `tui.c` refactor.

## Local Web

`TRAINLOG_WEB_V1` architecture, API independence, local-network boundary,
browser shell, Dashboard, layout ownership, build/runtime separation, and
security invariants are now canonical. No Web server, frontend, CLI option,
HTTP endpoint, embedded asset, or layout persistence is implemented yet.

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

The durable commands are owned by [tests.md](tests.md). On 2026-09-15 final
validation passed desktop compilation, **60/60 normal Meson tests** and
**60/60 ASan/UBSan Meson tests**, Android debug assembly and **202 Android
tests (198 passed, 4 skipped, 0 failed)**, the JSON validator, and the
import-contract validator. The source-derived TUI
`TRANSLATABLE_UI=0` check and Android resource parity also passed. Link and
diff safety checks pass in the final audit evidence.

`TRAINLOG_I18N_V0_1_1=PASS` is covered by desktop presentation, persistence,
formatting, layout-invariance and source-derived text-boundary tests, plus
Android resource-parity, language-owner, typed sync-presentation, and
stable-data presentation tests. A real-device/manual visual language-switch
smoke remains a manual validation; it does not change the validated status.

Hardware-dependent MTP and the final real Drive plus Android-triggered AI-draft
smoke test are not automated. The latter is why
`TRAINLOG_AI_SESSION_DRAFT_V1` remains `VALIDATION_PENDING`.

## Active limitations

- `TRAINLOG_WEB_V1` is contract-only; Dashboard characterization is frozen and
  the next implementation step is Core read-model extraction, not React or
  HTTP infrastructure.
- `APP_SHELL_V1` still awaits the recorded human visual/accessibility review.
- AI proposal exchange still awaits one real Drive plus Android-triggered
  bidirectional smoke test.
- Session Generator V1 is hidden while V2 planning semantics are developed.
- Hardware MTP validation requires a connected unlocked Android device.
- Scientific knowledge is bounded to reviewed catalog entries; unknown custom
  exercises remain unclassified rather than inferred from names.
