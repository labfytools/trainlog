# Current implementation state

Snapshot date: **2026-09-15**.

This document owns the current implemented state. Historical checkpoints and
closed incidents belong in [reviews](reviews/) and the
[changelog](../CHANGELOG.md).

## Product boundary

Android is the field companion: it captures training and body data, preserves
the active draft, receives AI proposals, triggers synchronization, and shows
quick summaries. The C17/Notcurses TUI is the detailed consultation,
correction, catalogue, analytics, graphing, and long-term tracking surface.

Android captures and summarizes. The TUI analyzes and tracks over time. The
desktop SQLite database remains the canonical long-term history.

## Versions and compatibility

| Boundary | Current state |
|---|---|
| Frozen project exchange | `TRAINLOG_FORMAT_V1=PASS/FROZEN` |
| Desktop SQLite | schema v18 |
| Android SQLite | schema v17 |
| Mobile snapshot | V3 active; V1/V2 readable legacy inputs |
| Desktop terminal backend | Notcurses only |
| AI history export | `TRAINLOG_AI_EXPORT_V1` active |
| AI session proposals | `TRAINLOG_AI_SESSION_DRAFT_V1=VALIDATION_PENDING` |
| Training knowledge | `TRAINING_KNOWLEDGE_V1=PASS` |
| Session generator | `SESSION_GENERATOR_V1=PASS`, hidden pending V2 |
| Application shell | `APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2` |
| Statistics | `STATS_V1=IMPLEMENTED` |

Desktop and Android schema numbers are independent. Neither changes the frozen
Trainlog JSON V1 contract.

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
- exactly one durable active session draft with resume, confirmed discard, and
  atomic completion;
- completed-session consultation and correction;
- exercise catalogue editing with stable `exercise_id` and referenced-profile
  protection;
- supplied/custom equipment and machine-exercise metadata;
- body measurements and recent BODY ZONES summaries;
- immediate exercise feedback, immutable wording revisions, and J+1/session
  follow-ups;
- pending AI proposal collection, explicit start/delete, and durable tombstones;
- 7/30/90-day, year, and all-history quick statistics;
- direct-storage snapshot publication, sync triggering, receipt display, and
  companion application.

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
- detailed synchronization status and history through the shared engine.

Rendering does not own SQL or business rules. Statistics are read-only
projections; they are not persisted as facts.

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

The durable commands are owned by [tests.md](tests.md). On 2026-09-15 this audit
passed desktop compilation, **58/58 Meson tests**, Android unit tests, Android
debug assembly, the JSON validator, and the import-contract validator. Link and
diff safety checks also pass in the final audit evidence.

Hardware-dependent MTP and the final real Drive plus Android-triggered AI-draft
smoke test are not automated. The latter is why
`TRAINLOG_AI_SESSION_DRAFT_V1` remains `VALIDATION_PENDING`.

## Active limitations

- `APP_SHELL_V1` still awaits the recorded human visual/accessibility review.
- AI proposal exchange still awaits one real Drive plus Android-triggered
  bidirectional smoke test.
- Session Generator V1 is hidden while V2 planning semantics are developed.
- Hardware MTP validation requires a connected unlocked Android device.
- Scientific knowledge is bounded to reviewed catalog entries; unknown custom
  exercises remain unclassified rather than inferred from names.
