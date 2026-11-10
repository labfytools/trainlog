# Generation capacity recovery V1

## Cause and preserved boundary

The 2026-11-10 private desktop incident reached its unchanged limit of 16:
14 `waiting_acknowledgement` generations plus two acknowledged recovery
members. Recovery retrieved 32 authentic Android ACKs, none for those 14.
The desktop worker always captured a new generation after inbound consumption,
even after a previous conversation had interrupted publication or ACK handling.
Later successful siblings advanced from the same acknowledged parent while
old pending branches continued consuming admission. The desktop admission SQL
also omitted the acknowledged-sibling exclusion already documented and used
by Android. Archiving acknowledged generations alone cannot repair this state.

The incident consumed an Android generation before desktop capture failed.
That inbound commit remains durable; the run is failed, never successful.
The retained Android `peer_capacity_exhausted` error has a different run ID
from this incident and is historical evidence, not proof of current saturation.

## Recovery invariant

A pending branch may leave active admission only if a distinct sibling has the
same producer, consumer and parent (including a null root), is acknowledged,
and owns a consumed `sqlite-commit-full` ACK matching its exact original run,
peer identities and manifest SHA-256. Archived siblings remain valid proof.
Neither elapsed time nor ACK absence proves non-consumption. All generation
rows, manifests, artifacts, hashes, causal relationships and memberships remain
unchanged. No sibling ACK advances the pending branch's domain publication
state. Its own authentic late ACK remains accepted and replay-protected.

The worker resumes the unique unresolved branch using verified immutable
bytes. Its mutable reference uses the current conversation run while the
manifest and ACK use their original run. ACK selection additionally correlates
the generation ID and manifest digest; the production validator still checks
all ACK fields. Multiple unresolved branches fail closed. The limit remains
16 and no schema or frozen exchange format changes.

## Private copy validation

The existing backup and live database both passed SQLite `quick_check`.
A fresh SQLite backup, complete owned/transport directory copies, table-count
inventory and reproduction evidence are retained privately under:

`~/.local/share/trainlog/backups/capacity-recovery-20261110T181125/`

The original source reproduced `generation retention capacity exhausted` on a
separate backup. The corrected predicate identifies 13 superseded branches,
leaving three active members: two acknowledged and one unresolved. All 14
pending immutable generations passed manifest and artifact verification. The
one unresolved generation was successfully republished byte-for-byte on the
copy. A current complete production capture then succeeded, including Programs
V2 and Sleep. Fingerprints covered 56 non-sync tables: the only capture change
was the expected Sleep generation-membership insertion. Session, Program,
body, Sleep facts, heart-rate and Cardio values remained unchanged.

No personal payload or database is committed as a fixture.

## Automated validation

- C17 strict-warning desktop build, including the embedded Web frontend: pass.
- `test_sync_generation_exchange.py`: 21 tests pass, including new capacity,
  exact-evidence rejection, late ACK, immutable replay, corruption rejection,
  divergent pending branches, committed inbound data surviving outbound
  saturation/restart, and archival regressions.
- Full Meson suite with Web enabled: 113/114 pass. The remaining failure is
  `tui_workflows`, `tui/tests/test_tui_workflows.c:1680`, asserting three
  Dashboard performance points in a dated fixture. Its C sources and binary
  inputs are unchanged by this Python-only correction; this is an existing
  baseline limitation, not claimed as a passing test.
- JSON catalog/schema validation and import-contract validation: pass.
- Changed C/H format check: pass (no changed C/H files).
- No C implementation changed, so a new C sanitizer checkpoint is not required.

The first full-suite configuration lacked installed Web dependencies and
failed the deployment-package test. After `npm ci` and Web-enabled rebuild,
that test passed. No production warning policy was weakened.

## Local deployment and rollback

The deployed helper directory is separate from development and previous
rollouts:

`~/.local/share/trainlog/rollouts/sync-capacity-recovery-v1-292bcc8/tools/`

It copies the daily Web rollout's tools and reference catalog with only the two
corrected helpers replaced; a SHA-256 inventory is retained beside it. Both user services use
`90-capacity-recovery.conf` drop-ins. Existing desktop binaries, Web assets and
transport adapters remain in their previous rollouts. Services were stopped
for a fresh coherent predeployment backup and restarted after configuration
publication. A private `rollback-services.sh` removes only these drop-ins and
restarts the previous services; it does not roll back business data or delete
any backup or generation evidence.

The initial deployment attempt showed that the daily Web shell launcher
unconditionally overwrites `TRAINLOG_SYNC_TOOLS_DIR`. The resulting first
controlled attempt still used the old helpers and failed after inbound commit.
Its failure evidence is retained. The corrected drop-ins bypass that wrapper
using its same underlying binary and explicit adapter paths. The sync daemon
uses the isolated tools and an explicit orchestrator path. Actual `/proc`
environment and command lines verified both paths after restart. No previous
rollout files were edited.

A second attempt received a current Android generation but exposed a missing
reference catalog in the isolated deployment, before its import transaction.
The original catalog was copied into the isolated package. The deployed
consumer then accepted that exact Android generation on a fresh database copy,
and all 14 production capture participants exported successfully using that
same deployed package on the copy. Its private test ACK was never published to
the phone. The failed attempt remains recorded as failed.

## Physical validation

The Android-triggered recovery run
`sy_f9f0f9eb-e804-4149-a438-755ce0f3fdac` completed over Bluetooth on
2026-11-10. It consumed Android generation
`gen_5f1d5c2c-d87c-4f35-9ca1-2ea3254c65c1` and replayed the exact pending
desktop generation `gen_cf95dd3b-6fff-4791-9101-e0eceb24ee7d`. Android returned
its authentic original-run consumed ACK; the desktop ledger corroborated both
halves. The active count is three and the 13 superseded pending branches
remain retained. This proves historical recovery, not publication of current
PC edits.

After the initial deployment faults were repaired, a Web waiter and the
phone's new explicit conversation had incompatible freshness baselines. The
stale waiting orchestrator was sent SIGTERM, recorded `interrupted`, released
its owned worker and lock, and preserved evidence. The Android-triggered daemon
then completed the recovery. A concurrent Web attempt correctly failed the
common lock instead of starting a second engine. A later Web waiter was also
interrupted before import so the remaining controlled validations could be
conducted through the phone's explicit trigger and daemon ledger alone.

Before current-snapshot convergence, the returned Android snapshot contained
31 sessions, 7 body observations and 44 exercises. Desktop had 46 exercises,
including the two requested new ones; their actual Android return remains a
separate convergence check. All 5,817 pre-existing owned immutable files were
verified unchanged. The live non-sync table comparison found only Sleep
publication validation/acknowledgement timestamp updates, with no change to
business values or revision identities.
No Android APK or app data has been changed. ADB exposes no connected device;
the paired Bluetooth device is connected. The transport advertisement includes
manifest/ACK/archive/recovery V1, history V4 and Programs V1/V2 capabilities.
An advertisement is protocol evidence, not proof of the installed APK version.

The current-snapshot run `sy_d18e8160-2917-458f-a1ce-3b3c58927a1b`
completed in both directions at 17:26:45 UTC. Its outbound
`gen_27f564c0-155b-41ba-8906-768894235499` has an authentic Android consumed
ACK. The user also confirmed completion without error on the phone. Comparing
all 56 non-sync tables against the predeployment backup found no business-value
or revision changes or deleted keys: only validation/ACK timestamps and 11 new
Sleep generation-membership rows changed. Existing sessions, Programs and
planning, body data, Sleep facts, heart-rate and Cardio tables were unchanged.

The distinct idempotence run `sy_97e963d7-189f-4c44-b49d-bf339b14347d`
also completed in both directions, with outgoing
`gen_283f98e0-13fe-4def-ae9d-ad5b10f3063f`. Both current outgoing manifests
contain Programs V2 and Sleep and have exact authentic consumed ACKs; each
inbound generation has a committed consumed ledger row. The user confirmed
that this attempt entered progress and completed, and that both requested
exercises are visible in Android. A received Android history artifact also
contains both names, 46 exercises, 31 sessions and 7 body observations; all
three stable-ID sets equal the canonical desktop sets.

The final 56-table comparison against the snapshot taken before idempotence
found no change to business values or revisions. Expected synchronization
metadata changes were limited to Sleep validation/ACK timestamps, 11 additional
Sleep generation-membership rows, and two body-zone synchronization markers
for the two newly propagated exercises. Existing body-zone rows were retained.
No session, set, Program, body, Sleep fact, heart-rate sample or Cardio row was
deleted or duplicated. Desktop retains 517 sets, 6 Programs, 98 Program
sessions, 23 execution links, 17 Sleep entries, 19 heart-rate captures and
93,903 samples plus 93,903 RR intervals. SQLite `quick_check` remains `ok`.
All 13 historical pending generations still validate from their immutable
files, and no unresolved unsuperseded desktop generation remains. The active
window is three after capture (the normal next archive pass returns it to two);
the unchanged admission limit is 16.

Private proof owners include `final-convergence.json`, `android-roundtrip.json`,
the three completed-run reports and their authentic ACK files, coherent
predeployment/before-idempotence/final SQLite backups, copied immutable trees,
validation logs, installed-file digests and rollback instructions. The Web's
separate last waiter remains truthfully `interrupted`; the Android-triggered
service ledger owns the successful controlled runs. No success status was
fabricated or copied into a different conversation's history.

`TRAINLOG_SYNC_GENERATION_CAPACITY_RECOVERY_V1=PASS` applies to this correction,
its deployment and the controlled bidirectional convergence/idempotence checks.
It does not claim the unrelated baseline TUI test passes. No Android APK,
identity, app data, database schema, or remote Git repository was changed.
