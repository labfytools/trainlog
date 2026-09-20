# Changelog

## Unreleased — 0.1.3

Development opened after the stable v0.1.2 release.

- Unified the Android capture interface around compact dark cards, action
  tiles, restrained-radius button variants, wrapping selection grids, and
  two-column performed-set entry. Home now focuses on session capture, recent
  BODY ZONES exposure, synchronization, and measurements; mobile Statistics
  navigation was removed without changing stored data or exchange contracts.
  Programs now show compact session metadata, completed progress, and explicit
  start/resume/completed states using the existing schema-v25 durable Program
  execution provenance. Web Program administration gained adaptive cards, a
  visible progress bar, and green completed-session treatment while retaining
  its revision-guarded trash confirmation and causal deletion behavior.
  Android full lint remains at the exact ten-error `main` baseline (one
  `NewApi`, one `LocalContextConfigurationRead`, and eight bilingual
  `StringFormatMatches` diagnostics); this lot adds no lint error and disables
  no rule. Private daily candidates retain development `versionName=0.1.3` and
  may override only an increasing `versionCode` at build time.

- Added the loopback-only `/exercices` desktop catalogue administration
  surface with bounded search/profile/BODY ZONES filtering, detail and
  read-only equipment presentation, Core-owned creation and editing, complete
  optimistic concurrency, and protected causal retirement. Profile changes
  preserve stable exercise IDs and all historical occurrence snapshots.
  Mutations remain desktop-local until the existing USB-priority, Drive
  mirror/fallback synchronization publishes the existing catalogue companions;
  no Android code, schema, exchange format, or automatic synchronization was
  added.

## 0.1.2 — 2026-09-20

- Added USB-priority full-generation synchronization with a separately
  configured private Drive mirror/fallback transport, verified manifest-last
  publication, shared multi-transport identities/ACKs, and explicit per-
  transport Web status without ever transporting SQLite.
- Added an Android user-enabled foreground USB listener, private Drive folder
  connection with bounded WorkManager checks, and one shared concurrency-
  guarded generation coordinator for UI and background triggers.
- Added an accessible preparation-list send action with separate editing and
  delivery states, durable retry behavior, duplicate-delivery prevention and
  stable execution-identity transition from Preparation to History/Program
  completion.

- Implemented `TRAINLOG_PROGRAM_CALENDAR_V1` as the operational top-level
  `/programmes` daily active-Program calendar. It consumes typed Core reads and
  preparation creation, derives Monday–Sunday weeks from Program bounds and
  real planned dates without inventing rest days, keeps undated sessions
  separate, and rereads Core state after Prepare. Sessions → Programmes remains
  the administration/import/list/detail/archive/delete surface; this adds no
  Core, API, schema, format, synchronization, or Android changes.

- Fixed Program-to-preparation creation for Program V1 target weights written
  as integer JSON numbers. Imports now preserve both integer and real number
  spellings instead of persisting integer weights as zero. Existing affected
  Programs remain unchanged; when deriving an editable draft, an impossible
  legacy zero weight is represented as an unspecified target rather than
  blocking the entire preparation. Mixed continuous-duration, unloaded-reps,
  and externally loaded reps coverage verifies order, targets, provenance,
  zero delivery/execution side effects, readable detail, and request replay.

- Fixed the Web history deletion failure for Program-origin completed
  sessions. The history detail query now reads explicit MAX results from
  `max_results`; mobile history import seeds a deterministic causal live
  revision when one is missing; and desktop schema v28 gives durable Program
  execution provenance an explicit terminal `deleted` state. Deletion remains
  revision-guarded, atomic and replay-safe. Retained Program-execution
  generations cannot resurrect a causally deleted session, and Web errors now
  distinguish stale, absent, invalid and internal failures without exposing
  database details. The private rollout additionally fixed complete-generation
  dominance for occurrence equipment companions and made bounded ACK recovery
  prioritize recent terminal evidence. Two correlated direct-MTP exchanges,
  component restart and exact replay of a retained pre-deletion generation
  preserved the single tombstone without duplication or resurrection.

- Added the Program execution flow. Android schema v25 can start or resume one
  Program session through the existing durable singleton, preserves stable
  Program provenance on completion, and presents explicit `À faire`, `En cours`
  and `Effectuée` states. Desktop schema v27 stores the corresponding unique
  execution link. The optional `trainlog-program-executions` V1 full-generation
  companion advances Web Program sessions from persisted facts without changing
  mobile export V3, `programs-v1`, or `TRAINLOG_FORMAT_V1`. Web also exposes
  `À faire`, `Préparée`, `En cours`, and `Effectuée` session states, while the
  existing logical Program deletion and confirmation semantics remain intact.
  The coordinated private deployment advanced the daily Android installation
  from versionCode 16/schema v24 to versionCode 17/schema v25 and desktop from
  schema v26 to v27 without uninstalling, clearing, or losing user data. A
  disposable Program session was started, resumed, completed, synchronized and
  displayed as completed on Android and Web. Correlated ACKs, a second
  idempotence exchange, store/component restart, exact old-generation replay,
  causal Program deletion, tombstone acknowledgement and non-resurrection all
  passed while the real 24-session/164-entry Program remained unchanged.

- Completed the private coordinated deployment of
  `TRAINLOG_PROGRAMS_PRESENTATION_ANDROID_DELETE_V1`. Desktop schema v26 adds
  terminal logical Program deletion with
  request/deleted revisions, durable response replay, generation linkage and
  acknowledgment state; source definitions and derived preparations remain,
  while deleted Programs cannot resurrect or prepare. The responsive Sessions
  Programs cards/timeline now use imported/session/usage/provenance data and
  protect a discrete accessible trash confirmation with focus restoration and
  Escape/Cancel no-op behavior. Android schema v24 adds the read-only synced
  Programs list/detail projection. The optional staged controlled-generation
  `trainlog-programs` V1 (`programs-v1.json`) publishes nondeleted Programs and
  unacknowledged tombstones under capability `programs-v1`; it changes neither
  `TRAINLOG_FORMAT_V1`, mobile V3, catalog exchange, nor preparations. Real
  Firefox Programs screenshots and real rollout evidence are recorded in
  `docs/reviews/programs_presentation_android_delete_v1_evidence.md`. The
  signed private Android update advanced versionCode 15 to 16 without
  uninstall or data clearing. A real disposable Program tombstone was consumed
  and acknowledged, and a final correlated exchange after component restart
  proved persistence, idempotence, and non-resurrection.

- Completed the private grouped rollout of Web Sessions deletion and Program
  V1 from desktop source `a0fc48f082b76dba2ae3a56fceb492169de89f89`.
  Desktop migrated from schema v24 to v25 after backup, restoration, and
  copy-migration verification. Android was updated in place to private
  versionCode 15 with no uninstall or data clearing; the installed APK is
  non-debuggable and retains the daily-installation certificate. Exact
  pre/post-update logical comparison preserved all 46 Android tables and 1,023
  rows. A preparation created explicitly as disposable was withdrawn through
  the real 390×844 confirmation UI. Two full-generation exchanges around a
  Trainlog-only restart retained exactly one Android withdrawal result, no
  matching delivery or performed session, and no resurrection or duplication.

- Completed Web Sessions deletion and Program V1 on desktop schema v25. Separate
  revision-guarded Core commands now withdraw manual preparations and AI
  proposals or causally delete inactive execution drafts and selected completed
  sessions. The accessible trash confirmation preserves derived preparations,
  active/concurrent drafts and unrelated performed history; proposal withdrawals
  use the bounded `trainlog-ai-session-drafts` V2 tombstone extension to prevent
  legacy replay resurrection on Android.

- Added Programs as the fourth Sessions subtab with complete paged listing,
  search/state filtering, stable detail links, archival, strict bounded
  `trainlog-program` V1 import preview and transactional commit. An explicit
  program-session action creates a distinct provenance-bearing manual
  preparation without automatic Android delivery or performed data.

- Completed the controlled Web Sessions presentation/withdrawal rollout on the
  paired private installation. Desktop migrated from schema v23 to v24 and
  Android from v22 to v23 without clearing application data. The requested
  validation preparation was withdrawn once; Android reported
  `execution_preserved` because its delivery had already started. Two
  post-withdrawal direct-MTP generations around component restart proved no
  resurrection or duplication while preserving the source proposal and all
  actual workout data.

- Fixed a rollout-discovered stale peer-advertisement failure in the
  full-generation MTP worker. Each MTP run now requires one successful pull
  from the expected phone before validating peer identity and capabilities, so
  a retained pre-upgrade advertisement cannot reject a compatible upgraded
  Android peer.

- Corrected Web Sessions presentation with one descending cross-type order,
  strict civil-date and zoned-timestamp formatting, contextual French states,
  readable persisted proposal provenance, and an installation-local versioned
  French/ISO date preference. The Dashboard prepared-item list, Footer, charts
  and synchronization timestamps use the same presentation helpers without
  changing machine values.

- Added revision-guarded, idempotent **Delete preparation** through the Core
  service and protected loopback API. Desktop schema v24 retains an immutable
  withdrawal ledger and all preparation/delivery evidence. The separate
  `trainlog-session-preparations` V2 generation participant propagates exact
  withdrawals; Android schema v23 cancels only pending deliveries, preserves
  started executions and permanently prevents older delivery replay from
  resurrecting a withdrawn preparation. V1 remains readable and
  `TRAINLOG_FORMAT_V1` is unchanged.

- Implemented `TRAINLOG_WEB_SESSIONS_V1` with paged preparation, resume and
  history views, stable details, optimistic/idempotent manual preparation,
  explicit proposal derivation, desktop schema v23, Android schema v22 and the
  separate `trainlog-session-preparations` V1 generation participant.

- Completed the controlled Sessions V1 deployment and delivered a six-entry
  validation preparation to Android without starting it or replacing an active
  draft. Foreground generation retries now emit a fresh daemon request and
  resume the exact interrupted generation until both peer halves are durable,
  preventing false transport timeouts and duplicate recapture.

- Added compatible non-destructive generation archival on desktop schema v22
  and Android schema v21. Only exactly acknowledged generations older than the
  two active recovery tips leave admission after a verified durable payload
  copy and audit record; all identities, manifests, ACKs, lineage and causal
  evidence remain stored. Older Android stores can recover exact missing
  producer-side ACK rows from the desktop consumer's durable ledger, with
  strict run, peer, generation and manifest revalidation. A correlated Android
  capacity error now reaches the Web without waiting for a transport timeout,
  and the prepared-items tile can inspect every proposal rather than only its
  first compact summary.

- Completed the controlled schema-v22/schema-v21 rollout without uninstalling
  or clearing Android data. The production Android archive uses a supported
  post-rename file durability barrier on emulated external storage, and the
  desktop publishes each complete MTP generation before its correlated
  reference. Two real post-migration bidirectional exchanges, separated by a
  Trainlog-service restart, completed with durable ACKs and no session
  duplication. The expected AI proposal retained its identity, six entries,
  and `pending` state on Android and remained independently visible in Web.

- Fixed a production full-generation timeout caused by recursively uploading
  retained staging and unrelated transport history on every MTP push. Each
  phase now publishes a private bounded outbox containing only its request,
  ACK, and current referenced generation, while preserving every retained
  generation, tombstone, and causal proof. Transport expiry has a stable
  browser-facing code and action instead of a truncated traceback.
  The reciprocal pull now reads only current coordination objects and the
  exactly referenced Android generation instead of recursively mirroring
  retained Android and desktop history.

- Replaced the Web Next Session placeholder with a separate read-only Core
  projection that distinguishes AI proposals from active/pending execution
  drafts without redefining the frozen Dashboard field or counting targets as
  performed work. The browser preserves the last successful durable view
  across refresh failures and uses one abortable, per-run ordered sync poller.

- Completed the authorized private full-generation rollout over production
  direct MTP: verified Android and desktop backups, signing-preserving Android
  updates, schema migrations, persistent peer pairing, bidirectional durable
  ACKs, restart and idempotent replay. This did not publish a release or modify
  v0.1.1.

- Fixed rollout-discovered feedback idempotence across SQLite row modes,
  packaged the complete generation exporter closure, ignored stale Android
  coordination objects until their run/generation correlation matches, and
  made the Android screen report completed generation conversations instead of
  waiting for a legacy receipt.

- Packaged `trainlog-sync-once` and `trainlog-syncd` with the relocatable sync
  candidate so user services cannot mix a versioned install with live-checkout
  helpers during rollout.

- Added a bounded Android package-version override and made the schema-v17
  backup bridge produce a signed release APK, allowing a strictly increasing
  private update chain without changing Trainlog schemas or protocols.

- Generalized bridge restore/migration validation to preserve every historical
  SQLite value whether a real backup contains an active draft or legitimately
  contains none.

- Made relocatable launchers resolve stable user-bin symlinks to their physical
  versioned bundle before loading matching binaries and helpers.

- Made the Web sync button generate cryptographic UUIDv4 request IDs on the
  loopback HTTP hostname where `crypto.randomUUID()` is unavailable.

- Included the exercise-name and training-knowledge modules required by the
  packaged generation worker, with an isolated runtime-import regression.

- Closed the native candidate warning gate with checked path and HTTP response
  bounds, strict GCC and Clang builds, full Clang ASan/UBSan validation, and a
  stable concurrent object-I/O MTP double. Rebuilt the desktop, current Android
  and non-installed schema-v17 bridge candidates with recorded digests; no
  service, device, user database, signing secret or deployment was touched.

- Normalized the recent synchronization, generation-MTP, Android backup,
  Web Dashboard serialization, tests, I/O doubles and packaging code for human
  readability. Added scoped C formatting rules and review guidance without
  changing synchronization protocols, assertions, deployment state or runtime
  behavior.

- Added the production libudev/libmtp generation adapter with bounded peer
  selection, object transfer and manifest-last publication; wired the trusted
  orchestrator mode and proved the full Firefox/Android/desktop conversation
  with an I/O-boundary object double. Added complete versioned Android
  backup/verify/restore and a reproducible, non-installed schema-v17 bridge
  candidate with v17 round-trip and v17-to-v20 migration tests. Physical MTP,
  signing, device installation, user-data operations and rollout remain
  pending.

- Added opt-in foreground generation coordination, peer-evidenced capabilities,
  truthful bounded Web progress, descendant cleanup, an integrated Android ↔
  desktop Firefox proof, and synthetic candidate/SQLite recovery tooling on the
  development branch. Physical MTP and deployment remain pending.

- Completed the grouped `TRAINLOG_SYNC_WEB_END_TO_END_V1` mission: the opt-in
  full-generation orchestrator, protected asynchronous Web API, global sync
  control, durable reconnect reporting, Dashboard refresh and read-only draft
  summary pass isolated C, Python, Android, frontend and Firefox validation.
  Default V3, physical MTP, Drive and deployment remain unchanged.

- Completed `TRAINLOG_SYNC_GENERATION_ACK_V1` with desktop schema v21 and
  Android schema v20. Added coherent per-platform capture, strict bounded
  manifest V1, immutable manifest-last publication, whole-generation SQLite
  consumption, pair-scoped lineage, durable consumed/rejected ACK V1 replay,
  and a separate causal-operation publication ledger. Real isolated Android →
  desktop and desktop → Android chains use production entry points and retain
  operation identity across generations. Automatic V3/MTP selection and all
  operational services remain unchanged.

- Completed the residual causal-deletion evidence with real Android and
  desktop producers for exercise, body-observation, custom-equipment, feedback
  and BODY ZONE relation targets, including production replica exchange,
  retained-history/current-reader checks, reopen/replay and stale-companion
  refusal.

- Closed the causal-deletion correctness review: deletion predecessors now
  cover affected session children, measurements, links, notes and immutable
  feedback revisions; supported corrections preserve an intervening mutation
  marker even after a content revert. Imported operations share built-in
  authorization with local deletion, execution drafts use their stable
  cross-platform revision identity, protected legacy companions refuse unsafe
  replay, and causal artifact reads/exports enforce count and byte bounds
  before destructive effects.

- Completed `TRAINLOG_SYNC_CAUSAL_DELETE_V1` with desktop schema v20 and
  Android schema v19. Added a strict bounded staged causal-deletion artifact,
  durable operation/state records, domain-specific deletion effects, causal
  conflict and replay handling, and legacy snapshot non-resurrection gates.
  Android and desktop both produce and consume the artifact in isolated tests;
  automatic transport, generation acknowledgement and deployment remain
  inactive.

- Completed `TRAINLOG_SYNC_DATA_LIFECYCLE_V1` with desktop schema v19 and
  Android schema v18. Added explicit staged V4 enriched-history codecs,
  causal notes, body-observation session links, stable execution-draft
  identities, bounded pending drafts, explicit activation and atomic,
  idempotent finalization with stale-replay protection. Active synchronization
  remains V3; no service, user database, phone, tag or release artifact was
  changed or deployed.

- Closed the lifecycle validation evidence with real Android/desktop V4
  history and execution-draft round trips in both producer directions. Fixed
  Android V4 import so validated target, load, and rest fields are persisted,
  reconciled the 73/75 native count to the two `web=auto` frontend tests, and
  documented identical pre-existing GCC 16 `-Wmisleading-indentation`
  failures on baseline and branch while retaining Clang 22 strict green
  builds. V4 remains staged and inactive.

- Completed `TRAINLOG_SYNC_CHARACTERIZATION_V1` without changing production
  synchronization, schemas, or formats. Added a real Android V3 exporter →
  desktop importer/exporter → fresh Android importer round trip, and completed
  current-behavior evidence for stable identities, occurrence/set ordering,
  exact replay versus correction, local-only field handling, inter-artifact and
  delete-before-send partial failure, request/receipt replay and marking, and
  contention on the real private-XDG synchronization lock. V4, draft transport,
  general tombstones, coherent generations, causal merge, and durable peer
  consumption acknowledgement remain future work.

- Reconciled canonical documentation with the implemented 0.1.2 development
  state, kept v0.1.1 and future synchronization scope explicit, established
  English as the repository documentation/release-note language, and added a
  reusable tag-scoped English source for the v0.1.1 public notes. No production
  behavior, schema, format, service, tag, or release asset changed.

- Completed `TRAINLOG_SYNC_TEST_ENV_V1` without changing production behavior,
  schemas or formats. `tools/validate_sync_isolated.py` now validates JDK 17 and
  the effective Gradle JVMs, creates unique private HOME/XDG/native/JVM tmp,
  exchange, database and report paths, runs bounded preflight/smoke/full suites,
  counts Android XML results, propagates failures and interruptions, and cleans
  only its owned successful run. Unit and Robolectric coverage proves bounded
  cleanup, distinct runs, process termination, no default real transport and
  effective `java.io.tmpdir` propagation.

- Froze `TRAINLOG_SYNC_GAP_CONTRACT_V1` without changing production sync,
  schemas or published formats. The contract distinguishes current evidence
  from future behavior and defines domain ownership, active-draft lifecycle,
  lost fields, causal deletion, conflicts, coherent generations, verified
  consumption/acknowledgement, compatibility, reporting, a future Web service
  boundary and dependency-ordered implementation lots. Added isolated
  production-path characterization of V3 field loss, snapshot resurrection and
  per-artifact rollback; existing Android coverage remains the proof of draft
  exclusion followed by finalized-session export.

- Development opened after the stable `v0.1.1` release. The complete Web
  Dashboard is now `WEB_DASHBOARD_V1=PASS/FROZEN`; Analyse, Programmes,
  Sessions, and Exercises remain placeholders. The current operational cursor
  is `TRAINLOG_SYNC_CHARACTERIZATION_V1`; its future protocol work has not
  started.

## 0.1.1 — 2026-09-16

- Completed `TRAINLOG_ANDROID_FIELD_INPUT_REFINEMENT_V1`: active and completed
  occurrence handles now keep one stable `entry_id`-keyed pointer gesture alive,
  accumulate total drag distance, jump directly across multiple positions, show
  residual pointer-following motion, restore the origin on cancellation, and
  commit only once at completion where persistence applies. Decimal distance,
  speed, MAX, target-load, and set-load editors use decimal keyboards, accept
  both `.` and `,`, preserve incomplete typing states, and parse finite
  locale-independent values only at commit. Units, schemas, formats, identities,
  active/completed ownership, and version 0.1.1 remain unchanged.

- Completed `TRAINLOG_ANDROID_COMPLETED_SESSION_REORDER_V1`: the Android
  completed-session correction form reuses the dedicated localized right-side
  drag handle and accessibility move actions. Reordering remains transient until
  Save; Cancel/Back writes nothing, while Save atomically replaces positions and
  any factual corrections under the same session/entry/exercise identities.
  Stable Compose keys keep editor state attached to `entry_id`; equipment,
  targets, actuals, MAX, feedback revisions, and J+1 ownership survive. Existing
  V3 synchronization reconciles the same desktop session without duplication.
  No schema, exchange format, active-session behavior, or version changed.

- Completed `TRAINLOG_ANDROID_FIELD_WORKFLOW_REFINEMENT_V1`: Android now uses a
  full-width semantic-success **Add to session** action, provides a dedicated
  accessible right-side drag handle for durable active-draft reordering, and
  supports atomic completed-session factual correction. Reorder and correction
  retain stable session, occurrence, exercise, equipment, plan, actual, feedback,
  and follow-up ownership; corrected facts continue through the existing V3
  synchronization identity. Continuous values reject non-finite input and show
  explicit km, km/h, and seconds units. No SQLite schema, exchange format,
  desktop/TUI behavior, or product version changed.

- Documented stable-release distribution through GitHub and Forgejo Releases,
  including
  prebuilt Android and architecture-specific dynamically linked Linux assets,
  SHA-256 verification, runtime/build/optional dependencies, and the release
  signing and exact-tag requirements. The Android artifact is built with the
  durable Trainlog release identity and is verified before publication.

- Completed `TRAINLOG_I18N_V0_1_1`: Trainlog 0.1.1 is one synchronized Android
  and desktop product version, with French as the default interface language
  and English selectable through **Settings → Language** on both surfaces.
  The preference is local presentation state (dedicated Android
  SharedPreferences; desktop XDG `trainlog/presentation.conf`) and successful
  selection updates the interface immediately. Localized Trainlog-owned text,
  BODY ZONE labels selected by stable IDs, and visible number/date formatting
  do not change training data, user exercise/catalogue names, IDs, SQLite,
  schemas, exchange/protocol artifacts, AI, MAX, or feedback. Desktop and
  Android synchronization views render local typed status/counters; raw
  core/receipt/history summaries remain opaque protocol/operational bytes and
  are never injected as cross-device French text. Final validation passed
  60/60 normal and 60/60 ASan/UBSan desktop tests, Android 202 total tests
  (198 passed, 4 skipped, 0 failed), both validators, source-derived
  `TRANSLATABLE_UI=0`, and Android resource parity. Real device/manual visual
  switching remains an explicit smoke validation.

- Completed `TRAINLOG_FULL_REPOSITORY_DOCUMENTATION_AUDIT_V2`: added the
  canonical documentation index, made README a concise entry point, separated
  current state from future roadmap ownership, corrected current schema and
  validation descriptions, and added concise English ownership headers across
  previously undocumented C, Kotlin, Python, and shell sources. The audit
  changes documentation comments only and introduces no functional behavior.

- Fixed fresh Android database body-zone reconstruction by applying the
  PC-published body-zone companion immediately after catalog, flattened aliases
  and strict profile-state, before fallible downstream session, equipment, AI
  draft and feedback imports. Regression coverage rebuilds several mappings in
  one inbox pass, resolves a retired exercise ID only through its durable alias,
  preserves the one-sync AI draft, and proves idempotent replay without identity
  mutation or duplicate zone rows.

- Redesigned the Android Home, Sessions, Exercise Catalogue, active-session
  and completed-session-detail action layouts with balanced icon tiles,
  bounded internal catalogue scrolling, and prominent accessible buttons,
  plus uniform full-width icon-only destructive actions with contextual
  TalkBack labels, without changing confirmation, navigation callbacks, or
  domain/storage/sync behavior.

- Added an Android confirmation dialog before deleting an imported AI session
  draft. Cancel, Back and outside dismissal are non-mutating; only the
  destructive confirmation invokes the existing deleted-tombstone operation.

- Replaced Android SAF folder grants with user-enabled
  `MANAGE_EXTERNAL_STORAGE` access to the single fixed path
  `/storage/emulated/0/Documents/Trainlog`. Android opens the system all-files
  permission screen, writes canonical artifacts through fsync plus
  same-directory replacement, reads PC artifacts directly, and removes stale
  tree-URI preferences. Desktop publication remains `Documents/Trainlog`; the
  historical Download tree and recovery backup remain untouched.

- Added `TRAINLOG_AI_SESSION_DRAFT_V1` as a separate strict proposal path:
  desktop schema v18 imports one Drive source with `aid_` identity/digest
  idempotence and post-commit archive bookkeeping, then publishes the bounded
  `trainlog-ai-session-drafts` V1 Android companion in durable deterministic
  batches, marking only successful MTP publications. Entry notes are rejected;
  only draft-level notes belong to the source contract. Inbound helper outcomes
  remain visible without turning Drive/archive/fetched-invalid results into a
  failed device sync. Android schema v17 keeps
  those proposals separate from its singleton active capture draft and retains
  start/delete tombstones against replay. Automated contract coverage exists;
  the required real Drive and Android-triggered bidirectional smoke validation
  remains pending.

- Fixed automatic Android→PC exercise-alias imports when the desktop binary is
  installed outside the repository build tree. Python sync helpers now resolve
  from Meson's authoritative tools directory, stale result files are truncated
  before resolution, and bounded child stderr/exit diagnostics reach sync
  history instead of collapsing to a generic import failure.
- Added `TRAINLOG_AI_EXPORT_V1`, a deterministic local JSON history export for
  manual external analysis. It opens desktop SQLite explicitly read-only,
  preserves occurrence-owned immediate feedback and session-owned follow-ups,
  and emits no inferred recommendations or progression.
- Added the shared desktop post-sync publication: successful TUI and
  Android-requested sync runs regenerate the AI export and invoke external
  `rclone copyto` for Google Drive. Export/Drive failures remain visible but do
  not invalidate an already successful Trainlog synchronization.

- Fixed Android-triggered synchronization so the complete current Android→PC
  companion bundle is published before the request signal, including Training
  Feedback V2 and Exercise Profile State V1. Desktop orchestration now refuses
  stale temporary profile-state reuse and reports a specific missing causal
  companion when a modern same-ID profile mismatch requires it.
- Made Android exchange-file reuse exact and directory-scoped: pending,
  trashed, unrelated, and conflict-numbered MediaStore rows no longer enter
  canonical selection, while genuine duplicate exact names fail closed.
- Hardened the shared Android SAF publisher so every outbound companion and
  sync request rewrites an existing exact canonical document with truncation,
  never selects historical `(N)` copies, and reports
  `SAF_CANONICAL_NAME_CONFLICT` before writing when a provider renames a newly
  created document.
- Added an exact-document fallback for Samsung Android 16
  `ExternalStorageProvider`, whose child query can omit ownerless MTP files even
  though creation detects their names. Canonical rows are reconstructed only
  beneath the granted tree and verified by exact provider display name;
  historical suffixes remain untouched.

## Unreleased — Desktop historical tracking snapshot

- Added desktop schema v17 / Android schema v16 causal current-profile state,
  bounded 32-revision transitive ancestry, deterministic bidirectional
  convergence, and explicit concurrent-edit conflicts; legacy feedback without
  `ended_at` now shows approximate H+ from
  valid `started_at` instead of a misleading during-session label.
- Added desktop schema v16 with an atomic v15 backfill of occurrence-owned
  `session_exercises.tracking_mode`, preserving identities and all child facts.
- Switched historical readers and V3 exchange to the occurrence snapshot while
  retaining catalogue profile modes as future-entry defaults.
- Enabled explicitly confirmed future-only tracking/profile edits in the TUI
  without rewriting completed sessions.

## Unreleased — Training feedback UX and safety

- Added additive schema v15 immutable feedback revisions and Training Feedback
  V2 with V1 import compatibility.
- Made saved exercise feedback and session follow-ups correctable on Android
  while preserving root identity, observation time, H+ placement and history.
- Preserved occurrence `entry_id` and feedback through draft edit,
  reconstruction, finalization and completed-session correction.
- Added explicit destructive confirmation to the reachable Android and TUI
  session removal/abandon paths.

## Unreleased — MACHINE_EXERCISE_MODEL_V1

- Add desktop and Android schema v13 for machine-specific exercise metadata.
- Preserve stable IDs for pure renames and introduce the five approved UUIDv4
  identities for Plate Loaded Leg Press, Treadmill, Pec Fly, Chin Assist, and
  Dip Assist.
- Make exact manifest splits transactional and idempotent while preserving all
  occurrence child facts and unresolved provenance.
- Remove Equipment from normal Android and TUI navigation while retaining its
  Phase 1 storage and sync compatibility roles.

- Added `PERCENT_MAX_INPUT_V1` as a transient user calculator in TUI planning
  and both generator previews: exact exercise/equipment external-load context,
  integer 1..100, `MAX × percentage / 100`, no recommendation, and only the
  resulting target kg persisted. Automatic generator policy V1 is unchanged.
- Repaired TUI `/` through the stable action registry, deduplicated actions by
  stable ID, strengthened Lavender plus textual selection states, compacted
  Android generator choices into localized chips, and made generator duration,
  shortfall, warm-up and cool-down limitations explicit.

All notable changes to Trainlog are documented here.

Detailed implementation chronology remains available in Git history and
`docs/reviews`. This file summarizes the current unreleased product baseline.

## Unreleased

### Added

- `TRAINLOG_TUI_STATISTICS_CALENDAR_BUCKETS_V1=PASS`: Training `7j` now means
  the current Monday–Sunday calendar week and compares calendar weeks; `30j`
  means the current Gregorian month and compares calendar months. Membership
  follows the civil date persisted in each timestamp, empty intermediate
  periods remain zero bars, localized labels expose the calendar boundaries,
  and exact month endpoints cover ordinary and leap-year February.

- `TRAINLOG_TUI_STATISTICS_PERIOD_BUCKET_SIZE_FIX=PASS`: the selected Training
  period now controls both summary depth and comparison-bucket size. `7j` uses
  exact consecutive 7-day buckets, `30j` uses exact consecutive 30-day buckets
  anchored on the current local date, and `Tout` selects one stable weekly or
  calendar-month scale from total historical duration. Empty intervals remain
  explicit zero bars and boundary observations are counted once.

- `TRAINLOG_TUI_STATISTICS_PERIOD_BUCKETING_V1=PASS`: Training summaries keep
  their rolling 7-day/30-day facts while charts compare consecutive exact
  Monday–Sunday buckets. The 7-day view shows up to four available weeks, the
  30-day view shows all intersecting weeks, empty periods remain zero bars, and
  complete history adapts from weekly to monthly totals beyond 128 active
  weeks without sampling away totals.

- `TRAINLOG_TUI_STATISTICS_REFINEMENT_V1=PASS`: finite statistics windows now
  retain their complete time domain; weekly totals use local Monday buckets,
  explicit empty weeks, zero baselines and discrete Notcurses bars. Wide
  training summaries are grouped into semantic panels, zone bars expose raw
  counts on one shared scale, exercise lists show factual summaries, and MAX
  defaults to exercises with an explicit recorded result while preserving an
  opt-in all-compatible view and prefix search.

- `TRAINLOG_TUI_STATISTICS_V1=PASS`: Statistiques is now the desktop analysis
  center for body history, factual global training activity, canonical
  exercise histories, persisted primary/secondary body-zone activity and
  explicit MAX history. Read-only C17 aggregations feed responsive Notcurses
  views and the shared Braille chart; valid loaded volume, planned/actual and
  feedback ownership rules deliberately exclude unsupported inference. No
  Android, database-schema, synchronization or exchange-format behavior changed.

- `TRAINLOG_TUI_MEASUREMENTS_GRAPH_V1=PASS`: Statistiques / Mensurations now
  uses a reusable Notcurses chart with true elapsed-time spacing, labelled
  padded value scales, a 2x4 Unicode Braille line raster, distinct accent
  markers for stored observations, responsive sparse-profile layout, and
  terminal-free geometry/raster regressions. No database, measurement, Android,
  or synchronization semantics changed.

- `APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2`: a shared seven-root
  application shell—Accueil, Séances, Exercices, Équipements, Statistiques,
  Synchronisation and Paramètres—on the Notcurses TUI and Android Material 3
  drawer. Séances now contains the existing generator, current/manual session
  entry and completed history; existing body and MAX views are reached from
  Statistiques. The TUI adds its run-scoped multi-plane shell, one event loop,
  stable-ID list/focus restoration, bounded UTF-8 search/forms, F6 Navigation,
  F7 shared actions, compact/sidebar thresholds, and explicit transient leave
  guards. Android adds typed controller-owned routes, local vectors, 48 dp
  actions and guarded navigation. The change adds no statistics, schema,
  synchronization artifact or exercise-domain semantics. Automated validation
  passed. A second automated repair review covered root search/action dispatch,
  F6/F7 focus/selection restoration, shell hubs/catalogues and Android compact
  presentation/latest-MAX semantics; human visual/accessibility review remains
  pending.

- `SESSION_GENERATOR_V1=PASS`: one frozen shared policy; deterministic bounded previews for
  11 BODY ZONES and four goals; explicit incomplete coverage and recorded-dose
  recency; observed exact-equipment 28-day load anchors without MAX-derived
  numeric fallback; Android normal-draft and TUI normal-editor acceptance; an
  additive Android v10 -> v11 plan migration; and separate strict V3 mobile
  exchange preserving plans with actual occurrence data. V1/V2 remain readable.

- `TRAINING_KNOWLEDGE_V1` read-only scientific knowledge infrastructure:
  six authored, versioned JSON catalogs with cited references; deterministic C
  generation; Android immutable asset loading; stable-ID catalog queries; and
  desktop/Android composition of persisted exercise zones, compatible
  equipment, explicit MAX and bounded occurrence/set history. The feature
  introduces no schema migration, database seeding, synchronization artifact,
  runtime prescription or generated UUID association. Scientific review,
  independent temporal review, final engineering review, repair verification,
  and final executable validation passed. The initial audit's four findings
  were closed by one bounded repair chain. See
  `docs/reviews/training_knowledge_v1_temporal_contract.md` and
  `docs/domain/knowledge_system.md`.

- `BODY_ZONES_V1`: the canonical `catalog/body-zones-v1.json` taxonomy with
  stable IDs, French display metadata, hierarchy, deterministic sort order and
  exact stable-exercise-ID migration evidence;
- one primary plus multiple distinct secondary body-zone relations on desktop
  and Android, with explicit unclassified history, transactional creation/edit,
  exercise detail summaries, parent/descendant filters, prefix-search
  composition, and custom-exercise support;
- one strict bidirectional `trainlog-exercise-body-zones` v1 companion carrying
  `exercise_id`, nullable primary ID and ordered secondary IDs; replay is
  idempotent, one-sided edits reconcile, simultaneous divergence conflicts,
  secondary lists are never unioned automatically, and the publishing peer
  records the exact published snapshot as its common baseline;
- generator-ready read composition for descendant and primary-only exercise
  selection, direct zone relations, existing performance history and explicit
  MAX history without duplicating historical or MAX data;
- taxonomy, schema migration, relation constraint, filtering, identity-merge,
  reopen, companion replay/update/conflict and Android repository regressions;

- explicit MAX result mode for `max_test` sessions: exercise, optional
  equipment context and positive `max_weight_kg`, with no synthetic set or
  repetition;
- Android and TUI max-only entry/edit/detail flows, Android latest max per
  exercise, and stable-ID continuation of a completed Test max;
- V2 round-trip of explicit max results in both directions, including bounded
  reconciliation of an appended or corrected continuation of the same
  max-test session;
- desktop and Android schema v9 migrations that convert only an unambiguous
  sole `1 rep × positive load` legacy result and preserve ambiguous attempts;
- explicit-max persistence, migration, sync and same-machine/different-movement
  regression coverage;

- desktop equipment v8: a supplied catalogue generated from
  `catalog/equipment-v1.json`, browse/search/detail views, and local custom
  equipment creation/selection; occurrence links resolve supplied or local
  definitions and show unknown historic references explicitly;

- multi-occurrence session V2: stable per-occurrence `entry_id`, repeated
  catalogue exercises in one session, per-set actual weights, and occurrence
  equipment associations across Android, desktop, import and export;
- structured Android and Notcurses per-set row editing: independent actual
  repetitions and nullable Charge/Assistance values, ordered add/delete/edit
  operations, and ordered history/detail presentation without target-value
  substitution;
- shared versioned equipment catalogue, Android machine selection/search,
  Android-local custom equipment creation, and explicit rejection of unknown
  equipment identities rather than silent association loss;

- `trainlog-equipment-definitions` v1, with directional Android-to-PC and
  PC-to-Android filenames, strict stable definition fields, additive
  reconciliation, reserved supplied-manifest IDs, and explicit divergence
  conflicts;
- explicit TUI/shared-engine synchronization actions: `a` Android -> PC, `p`
  PC -> Android, and `b` inbound then outbound bidirectional synchronization;
  each opens one direction-specific confirmation, `Enter` runs it once, `Esc`
  cancels without an operation, `r` only refreshes device status, and the
  former `s` shortcut is inert;
- bounded V2 exercise reconciliation for distinct IDs with equal normalized
  names: modes and represented invariants must match, field masks must be
  comparable, desktop identity stays canonical, and the richer bit-mask union
  is retained without changing historical occurrence snapshots;
- regression coverage for identical/different-ID profiles, compatible
  subset/superset masks, incompatible collisions, the real Marche identity
  pair, full Android -> PC import and stable PC ↔ Android replay;

- Android `EXERCISE_EDIT_V1`: visible catalog editing, stable-ID name rename,
  explicit invalid/conflict/profile/database results, and profile locking once
  completed history or an active draft references the exercise;
- `ANDROID_BANNER_PARITY_V1`: the earlier Android `◆ TRAINLOG ◆` header
  component matched the compact Notcurses banner's accent and muted context
  rhythm. APP_SHELL_V1 supersedes that per-page presentation with the fixed
  Material 3 `AndroidAppShell` top bar;

- one durable Android active-session draft with Home resume, raw form restore,
  confirmed discard and draft-only exercise removal;

- native Kotlin/Compose Android capture client;
- Android local exercise, session, continuous-activity, and body persistence;
- C17/Notcurses desktop TUI with direct session entry and durable SQLite history;
- profile-aware exercise model using recording mode, tracking mode, and
  supplemental fields;
- continuous activity persistence without synthetic sets;
- explicit table-based desktop actual-set entry, with independently added rows
  and no compact performed-repetition input;
- persisted desktop session editing and exercise removal;
- Android current-session draft exercise removal;
- body-observation history, editing, graphs, and normalized overlays;
- exercise performance history;
- direct physical Android USB/MTP discovery with libudev/libmtp;
- direct MTP read/write/list/delete support without filesystem mounts;
- Android full mobile snapshot export;
- strict idempotent desktop mobile importer;
- PC canonical exercise-catalog publication to Android;
- automatic Android snapshot maintenance;
- Android `Synchroniser maintenant` request flow;
- shared C bidirectional synchronization engine;
- `trainlog-syncd` user-session synchronization agent;
- request/receipt synchronization protocol;
- stable `sy_<uuid-v4>` synchronization identities;
- structured synchronization history with selectable TUI list/detail views.

### Changed

- desktop schema v11 additively introduces `exercise_body_zones` and its
  internal sync-baseline table; Android schema v10 introduces the equivalent
  relations. Both migrations seed only manifest mappings proven by stable
  `exercise_id`, leave uncertain exercises unclassified, and preserve all
  session, occurrence, set, MAX, equipment and body-observation identities;
- Android and TUI exercise catalog workflows now select translated manifest
  values, derive group labels, exclude a primary from secondary selection and
  combine hierarchy-aware zone filtering with normalized prefix search;

- desktop schema v10 losslessly rebuilds only `performed_sets` to accept an
  explicit zero actual `weight_kg`; historic NULL and positive actual loads
  remain unchanged, while planned targets and explicit MAX results stay
  strictly positive;
- desktop and Android schema v9 add one-to-one completed/draft max-result rows;
  `TRAINLOG_FORMAT_V1` remains frozen and V1 export refuses explicit MAX data
  rather than losing or fabricating it;
- desktop and Android schema v8 preserve existing equipment references while
  permitting custom definitions with `load_semantics = none`; Android's v7 ->
  v8 migration is non-destructive;
- custom definitions reconcile before V2 mobile/association artifacts in each
  direction. The V2 mobile and association shapes are unchanged;

- desktop SQLite schema v7 and Android SQLite schema v7 preserve historic rows
  while adding durable occurrence identities and occurrence-level equipment;
- the then-active Android↔PC completed-session exchange was V2; frozen V1
  artifacts remain readable as historical formats and are not redefined for
  repeated occurrences; the current active artifact is separately versioned V3;
- synchronization invokes each local helper with the explicit XDG-resolved
  desktop database path and records the concrete equipment-import failure;

- same-ID Android ↔ PC catalog reconciliation updates display-name metadata in
  place. Compatible different-ID normalized-name collisions now merge through
  the explicit V2 policy; incompatible collisions still reject atomically;

- Android local SQLite v3 -> v4 additive migration for structured active drafts;
- completed-session insertion and draft clearing are atomic; drafts remain
  excluded from completed history and frozen mobile export;

- desktop SQLite schema evolved to v5;
- schema v5 permits targetless set-session rows for actual-only mobile data;
- heterogeneous performed sets are preserved without inventing a uniform target;
- Android/desktop synchronization uses dedicated versioned artifacts instead of
  modifying frozen Trainlog JSON v1;
- PC and TUI synchronization paths now share one engine;
- Android PC-created artifact access uses a persistent SAF grant for
  `Download/Trainlog`;
- the Android data package is no longer accidentally hidden by a broad
  repository `data/` ignore rule;
- user-facing session history timestamps use `DD/MM/YYYY HH:MM` while canonical
  storage remains RFC3339.

### Fixed

- a newly published custom-exercise zone state now establishes the publisher's
  comparison baseline as well as the receiver's; a later peer-only edit no
  longer produces a false simultaneous-conflict result;
- secondary-only body-zone states are rejected consistently by C, Kotlin and
  Python boundaries instead of being persisted but hidden by exercise detail;
- touched migration/custom-equipment/body-metric/TUI C regressions now return
  a failing process status from `main()`; the repaired workflow fixture uses a
  real in-memory database instead of passing an invalid null handle;
- synchronization summaries no longer add exercise reconciliations to the
  `exercices ajoutés` value. A compatible different-ID lookup that makes no
  persistent change is reported as an idempotent skip, while real insertions
  and real reconciliation mutations retain distinct counters. A production
  PC-exporter/Android-importer regression proves zero additions and exact
  business-table stability on the second and third imports;
- equipment companion import previously omitted its required `--database`
  argument and blocked synchronization after a successful session import;
- PC catalogue/mobile export paths now accept schema v7 and preserve catalogue
  tracking metadata; Android completed-session equipment editing now targets
  the stable occurrence `entry_id`, not an ambiguous catalogue exercise ID;
- Android scoped-storage suffix selection now covers equipment associations as
  well as mobile/definition artifacts; a historic V1 snapshot cannot consume a
  neighboring V2 equipment companion;
- desktop-generated session occurrences now receive `sxe_<uuid-v4>` rather
  than the synchronization-run `sy_` prefix; public generator capacity and C17
  regression coverage include the three-character occurrence prefix;
- TUI footer help now advertises every active direct function key through F5;

- in-progress Android workout loss when leaving the foreground or recreating
  the Activity/process;
- missing selected-exercise recovery preserves raw partial input and reports a
  specific warning; draft write/finalization failures return explicit errors;

- stale schema-v4 importer call after desktop schema v5 migration;
- stale schema-v4 guard in the PC catalog exporter;
- missing `sy` prefix support in the UUID creator;
- synchronization failures that previously surfaced only as `error=unknown`;
- Android folder-selection UX so a wrong SAF folder can be changed without
  clearing application data;
- libmtp terminal output leaking into terminal rendering.

### Validation

Current validated baseline:

```text
TRAINLOG_FORMAT_V1=FROZEN

DESKTOP_SCHEMA_V11=PASS
DESKTOP_TESTS=39/39 PASS

ANDROID_BUILD=PASS
ANDROID_LOCAL_WORKFLOWS=PASS
ANDROID_LOCAL_DATABASE_V10=PASS
ANDROID_TEST_DEBUG_UNIT=44/44 PASS (real v9 fixture enabled)
ANDROID_SESSION_DRAFT_V1=PASS
ANDROID_MAX_V9_REAL_DATA_MIGRATION=PASS
ANDROID_MAX_V9_INSTALL_ADB=PASS

USB_MTP_DETECTION=PASS
MTP_HARDWARE_ROUNDTRIP=HISTORICAL_PASS
REAL_ANDROID_ARTIFACT_COPY_ROUNDTRIP=HISTORICAL_PASS
BODY_ZONES_DESKTOP_REAL_DATABASE=PASS
BODY_ZONES_ANDROID_DEVICE=PASS

ANDROID_TO_PC_MTP=HISTORICAL_PASS
PC_TO_ANDROID_MTP_PUBLISH=HISTORICAL_PASS
COMMON_SYNC_ENGINE=PASS
TRAINLOG_SYNCD=PASS
ANDROID_TRIGGERED_SYNC=PASS
ANDROID_SYNC_RECEIPT=PASS
TUI_SYNC_LOG_SHOW=PASS
BIDIRECTIONAL_SYNC_V1=PASS
MULTI_OCCURRENCE_SESSION_V2=PASS
EQUIPMENT_ASSOCIATIONS_V2=PASS
EQUIPMENT_DEFINITIONS_V1=PASS
EXERCISE_RECONCILIATION_V2=PASS
EXPLICIT_MAX_RESULTS_V1=PASS
BODY_ZONES_V1=PASS
BODY_ZONE_SYNC_V1=PASS
BODY_ZONES_DESKTOP_REAL_MIGRATION=PASS
BODY_ZONES_ANDROID_DEVICE_VALIDATION=PASS
```

For Body Zones V1, the real desktop database was backed up coherently and
migrated v10 -> v11 through the production binary. All eight pre-existing
tables compare equal row-for-row with the backup; integrity/FK checks pass and
the migration adds 32 direct relations for 20 of 23 exercises. On the Samsung
SM-G990B, the matching signed APK was installed with `adb install -r`; the real
Android database migrated v9 -> v10 with every pre-existing application row
unchanged, 32 relations for the same 20 exercises and the expected three
unclassified exercises. The Android zone/detail/filter/edit-cancel matrix and
two live PC <-> Android MTP passes completed successfully. The second pass
reported no additions or reconciliations and left every Android application
table and the semantic Body Zones companion unchanged. Scoped storage had
published the requests as exact `(N).json` collision siblings; the shared
engine now selects the newest request with the same deterministic helper used
for other Android-originated artifacts.

### Measured max v1

Added:

- explicit measured-max derivation from `max_test` sessions only;
- newest successful measured result and same-mode historical record;
- dedicated TUI measured-max history and graph;
- external working-load calculations at 60/70/80/90%;
- selectable 0.5/1/2.5/5.0 kg working-load rounding;
- direction-aware assistance measured-max semantics;
- Android `Entraînement` / `Test max` session selection;
- Android history/detail max-test identification;
- measured-max regression coverage.

No estimated 1RM, schema v6, or frozen Trainlog JSON v1 change was introduced.

### Body analytics v1

Added:

- desktop-only body analytics view;
- local height/formula estimation profile;
- circumference-based body-fat estimate;
- estimated fat and lean mass when real weight is available;
- waist/hip, shoulder/waist, and chest/waist ratios;
- arm, forearm, thigh, and calf left/right asymmetry percentages;
- weight, waist, and estimated-body-fat trend deltas;
- body analytics regression test.

Estimated analytics remain derived display values and are never persisted as
direct measurements. Android remains capture-only for this feature.

- Hid `SESSION_GENERATOR_V1` from normal UI pending V2; manual `%MAX` entry is unchanged.
- Added versioned canonical exercise display names, stale-peer-name convergence,
  and the STATS_V1 query-contract preparation (no analytics implementation).
# Unreleased

- Add `TRAINING_FEEDBACK_V1`: schema v14 append-only exercise observations and
  session follow-ups, Android French voice/manual capture with no audio storage,
  bidirectional strict companion sync, H+ timelines, and TUI read-only display.
