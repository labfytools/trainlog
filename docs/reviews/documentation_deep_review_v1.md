# Documentation deep review V1 — 2026-10-02

## Scope and evidence hierarchy

This review covers the linear development source at
`f4ddcfdb05e9627397f7225c05f40212e9de1ea6`
(`trainlog-sleep-single-selector-weeks-v1`). Its ancestors include `3a4ec34`
(Program rollout), `8995843` (current Program projection), `c2935b8` (Web body
figures), and the Sleep sport/PDF/paging changes. It does not integrate or
describe as delivered the uncommitted Dashboard layout work in the separate
`trainlog-dashboard-polish-v1` worktree. Stable v0.1.6 and the 0.1.7
development line are different publication states.

Normative frozen contracts take priority over source implementation; source and
tests establish capabilities of this commit; dated rollout records establish
only the installation they observed; design/review files retain history. The
observed PC/Web launcher path names the `f4ddcfd` package. That observation
alone is not a byte comparison of a running process, an Android APK inspection,
or proof of every synchronization service's package. The earlier verified
Program rollout records desktop/Android schemas v37/v34. No private health
data, PDF, database or device identifier was read for this review.

## Coverage inventory

The 105 tracked baseline Markdown files include root `README.md`, `AGENTS.md`,
`CHANGELOG.md`, `android/README.md`, `format/README.md`, 17 primary `docs/*.md`
owners, domain and design records, release notes and historical reviews. This
revision adds an indexed Sleep guide and this review. The primary owners were
inspected against code, migration constants, Meson/Gradle settings, tests and
retained rollout records. Designs, releases and reviews were mapped as dated
evidence rather than rewritten into current status. The tracked Markdown
relative-link scan found no missing target before publication. External link
availability was not verified.

| Document/section | Claim | Evidence (commit, path, symbol or test) | Gap | Correction | Status |
| --- | --- | --- | --- | --- | --- |
| `AGENTS.md` scope | Analyse, Programmes, Sessions and Exercises are placeholders | `web/src/routes/`; `docs/web_sessions.md`; `docs/web_exercises.md` at `f4ddcfd` | Obsolete present tense | Names implemented routes | Corrected |
| `AGENTS.md`, database and Android status | Current schemas v26/v24, v28/v17 or v36/v33 | `tui/include/trainlog/database.h` `TRAINLOG_DATABASE_SCHEMA_VERSION=37`; `TrainlogRepository.kt` helper `version=34`; migration tests | Mixed historical and current versions | Current source v37/v34; historical migration steps retained | Corrected |
| `current_state.md`, roadmap | `c2935b8`/`3758875` Web package served | Observed launcher target names `sleep-single-selector-weeks-v1-f4ddcfd`; ancestor graph | Package claim stale; running bytes unverified | Reports launcher observation and separate Program rollout | Corrected with limit |
| Sleep agenda | Old selector or calendar-week interpretation | `SleepDiaryWorkspace.tsx` `initialAgendaPageSize`, `recentEntries`, `visibleEntries`; Sleep component tests | Seven recorded rows can span more than seven dates | Current 7/14/21/28/All row counts, default seven; old values migration only | Corrected |
| Sleep PDF and sport | Page controls could be confused with export range | `SleepDiaryWorkspace.tsx` `pdfStartDate`/`pdfEndDate`; `sleepVisualProjection`; Web PDF tests | Selection and visible page have different scopes | Documents independent inclusive range and factual sport bands | Verified in source |
| Programs | Rescheduling labelled future/candidate | `docs/program_format_v1.md`; `docs/sync_exchange.md`; `tui/tests/test_web_programs.c`; Android v34 projection | Development capability mislabelled future | Describes implemented development contract and partial V1-peer convergence | Corrected |
| Web package | `npm test` could be mistaken for embedded runtime proof | `meson_options.txt` `web`; `tui/meson.build` frontend target/tests; `tools/package_sync_candidate.py` asset gate | Source tests and package differ | README requires `-Dweb=enabled`, Meson tests, compatible bundle | Corrected |
| Synchronization | One ACK might imply universal convergence | `docs/sync_exchange.md` Programs V2 matrix; generation and ACK implementation | V1-only peer omits rescheduled Program while other domains proceed | Added domain/format/selection/ACK matrix | Corrected |
| Transport narrative | Current Web edits and architecture described USB as universal priority | `docs/sync_exchange.md` Bluetooth selector; `tools/sync_peer_worker.py`; private rollout record | Conflated paired Bluetooth path with separate USB/Drive workflow | Separated automatic paired choice from legacy configured workflow | Corrected |
| Rollout readiness | Historical eight-generation limit could look universal/current | `tools/sync_generation_exchange.py` `MAX_RETAINED_PER_PEER=16`; Android `SyncGenerationService.MAX_RETAINED_PER_PEER=8`; `tools/sync_peer_worker.py` MTP timeout | Desktop/Android ceilings differ; timeout is not success proof | Separated limits and historical bridge instructions | Corrected |
| Sleep user path | Capture, agenda and PDF behavior had no short indexed guide | `TrainlogRepository.kt`; `SleepDiaryWorkspace.tsx`; Sleep projection/PDF tests | Readers had to assemble workflow from state and code | Added indexed factual guide with privacy and evidence boundaries | Corrected |
| Backup/rollback | Old executable could be mistaken for schema rollback | `tools/backup_trainlog_sqlite.py` SQLite backup API; `docs/sync_rollout_readiness.md` | Needs paired compatibility and retained backups | Existing procedure retained and linked from README | Verified in documentation |
| Tests | Largest historical totals could look like current validation | `docs/tests.md` dated checkpoints; `tui/meson.build`; Web scripts | Build configuration and skipped tests were implicit | Added evidence-reading rule | Corrected |
| 0.1.8 | Possible training ideas could look implemented | `docs/roadmap.md`; present catalog/feedback/Sleep/Cardio owners | No approved algorithm or medical rule | Proposed-only section with human validation | Corrected |

## Validation performed in the isolated documentation worktree

`python3 tools/validate_json.py` and
`python3 tools/validate_import_contract.py` passed. With a local
`-Dweb=enabled` Meson build, strict compilation and **114/114** registered
tests passed, including the embedded frontend and package self-containment
checks. A preliminary `-Dweb=disabled` run passed 111/112; its deployment
package test rejected the intentionally absent Web assets as designed. The
changed C/H format gate passed with no changed C/H file. Relative links across
the tracked baseline plus new Markdown files and `git diff --check` passed.
These checks used build artifacts in this worktree and synthetic test data;
they did not exercise a personal device, database or service.

## Boundaries and open evidence

The source can prove format selection, persistence rules and test coverage; it
cannot prove that a particular uninspected phone or service is currently
running that source. The `f4ddcfd` launcher observation identifies a package
path, not a live process hash. The private rollout and Firefox/PDF records are
dated evidence for their stated configurations. Android path reconstruction for
the Web silhouette is source parity evidence, not an Android runtime
screenshot. The local desktop 16-generation and Android eight-generation
admission limits, and the 90-second MTP operation budget, are bounded
configuration, not a guarantee against interruption or proof that transient
storage occupation is always lower than either ceiling.

No code defect was confirmed by this documentation pass. A broad line-by-line
revalidation of every historical review and every external URL is outside the
verified coverage; their current status must not be inferred from this matrix.
No application, database, service, synchronization or personal-data operation
belongs to this documentation branch.
