# Desktop database

## Schema v34: synchronized Cardio guidance V1

Schema v34 additively stores completed Android-owned guided-cardio runs. `cardio_guidance_runs` keeps the stable run/session identity and exact run bounds. `cardio_guidance_phases` stores the ordered `entry_id`-owned phase, its immutable resolved BPM target, optional exact calibration provenance, exit condition, phase bounds and final instruction. `cardio_guidance_events` stores only instruction changes, not another copy of the raw heart-rate curve.

The importer requires the referenced canonical session to be `cardio`, every phase `entry_id` to exist in that session, and every phase interval to remain inside the synchronized exercise timeline. Calibration-derived targets must reference an imported calibration with the same observed peak. Any guidance event carrying BPM must match an exact synchronized raw heart-rate sample at the same timestamp; suspended events intentionally carry no BPM.

No migration invents phases, instructions, BPM or calibration references.

## Schema v33: synchronized Cardio calibration V1

Schema v33 additively stores completed Android-owned Cardio calibration profiles. `cardio_calibrations` is keyed by stable `calibration_id` and retains protocol version, canonical cardio `session_id`, stable occurrence `entry_id`, exact calibration/effort/end timestamps, the synchronized heart-rate `capture_id`, observed peak BPM and import time.

`cardio_calibration_recovery` stores only factual recovery samples for the defined +60, +120 and +180 second references. The source curve remains Heart Rate V1; the calibration importer verifies that the observed peak and every recovery `(observed_at, bpm)` pair exist in that raw capture. It never derives, interpolates or rewrites HR samples.

Android owns the active calibration lifecycle. Desktop stores only completed profiles. A later calibration creates a new stable profile and never rewrites an older session or reference.

## Schema v32: canonical cardio session kind

Schema v32 adds the product-owned `sessions.session_kind` column with the
explicit domain `training | max_test | cardio`. The pre-existing
`sessions.session_type` column remains intentionally frozen to
`training | max_test` for compatibility with historical codecs and MAX
semantics. A cardio row therefore stores `session_type=training` only as a
legacy compatibility projection and `session_kind=cardio` as the canonical
product fact.

The v31→v32 migration introspects historical `sessions` layouts before
backfill. When the legacy `session_type` column exists it seeds
`session_kind` from it; very old historical fixtures that legitimately lack
that column keep the session kind produced by their earlier migration chain.
No existing session identity, occurrence, set, timeline, Program provenance or
heart-rate fact is rewritten.

Completed cardio sessions synchronize Android→desktop through the separate
optional `trainlog-cardio-sessions` V1 generation companion. The frozen
mobile/history V4 and execution-draft formats are not widened to a third
session type.

## Schema v31: synchronized session exercise timeline

Schema v31 additively owns Android-recorded execution timing for completed
session occurrences in `session_exercise_timeline`. Each row is keyed by the
stable `session_id + entry_id` pair and stores the matching catalogue
`exercise_id`, the real user-action `started_at` and `ended_at`, and import
time. The importer requires the existing canonical session and occurrence to
match; it never creates workout history or infers exercise timing from order,
sets, plans, or heart-rate samples.

The session bounds remain the existing canonical `sessions.started_at` and
`sessions.ended_at`. The separate `trainlog-session-timeline` V1 companion
must match those exact values before occurrence timing can be imported.

## Schema v30: synchronized Heart Rate V1

Schema v30 additively owns Android-recorded heart-rate captures after
full-generation synchronization. `heart_rate_captures` stores one immutable
stopped capture with its stable `hrc_` identity, owner kind (`session` or
`sleep`), stable owner identity, exact start/end timestamps, optional sensor
name, and import time.

`heart_rate_samples` stores the received timestamp, measured BPM, optional
exercise `entry_id` snapshot, optional sensor-contact state, and optional
cumulative energy value. `heart_rate_rr_intervals` preserves zero or more raw
Heart Rate Service RR values in their native 1/1024-second units. Desktop never
creates these measurements and never connects to the sensor.

The v29→v30 migration is additive and creates no cardio row. Heart-rate data
travels Android→desktop through the separate optional `trainlog-heart-rate`
V1 generation companion. Exact replay is idempotent; a stable capture/sample
identity with different measured content is rejected. Existing workout,
Sleep Diary, and frozen mobile formats are unchanged.

## Schema v29: Sleep Diary V1

Schema v29 additively owns `sleep_diary_entries`, immutable
`sleep_diary_revisions`, and revision-owned `sleep_diary_events`. An entry is
one night and its associated day. Point events have an offset-bearing
`start_at`; intervals additionally have an absolute `end_at` strictly after
their start. The 18:00-to-18:00 agenda is only a projection and is never stored
as cells.

The same stable v0.1.4 migration owns `sleep_medications`, immutable
`sleep_medication_revisions`, and revision-owned `sleep_medication_intakes`.
Catalog defaults assist capture but do not own historical facts: every intake
stores the displayed medication name and effective optional positive dose/unit
snapshot. Renaming or deactivating a catalog item therefore cannot rewrite a
past entry. Development databases that had already opened the earlier v29
draft are completed idempotently on open before the additive v30 migration;
no existing sleep row is rewritten.

Mutations advance `current_revision_id` only when the caller supplies the
current parent. Logical deletion advances the revision and retains the
tombstone. Qualities preserve `TB`, `B`, `Moy`, `M`, and `TM` exactly.

`sleep_diary_publication_state` separates the durable local tip from the tip
explicitly validated for publication and the exact revision covered by a
full-generation ACK. `sleep_diary_generation_entries` binds captured revisions
to immutable generation identities. Consequently a new entry is a draft, a
validated tip is ready, an ACKed tip is synchronized, and any later local tip
is modified without rewriting its acknowledged ancestor. One stable entry is
owned by each `night_start_date`; old drafts remain editable indefinitely.

The absolute timestamps are the future heart-rate correlation boundary. Sleep
Diary V1 does not store, infer, or simulate heart-rate data.

## Schema v28: terminal Program execution provenance

Schema v28 extends `program_session_executions.state` with the terminal
`deleted` value. When a completed Program-origin session is causally deleted,
the same transaction retires its execution provenance, removes the active
history row, appends the immutable delete operation and advances the durable
session tombstone. The provenance identity and timestamps remain intact; Web
Program projections no longer claim that the missing session is completed.
The v27 to v28 migration preserves all existing rows and creates no causal or
deletion record.

## Schema v27: durable Program execution provenance

Schema v27 adds `program_session_executions`, the stable link from one
desktop-owned Program session to its Android execution `session_id`. The state
advances from `in_progress` to `completed`; schema v28 additionally permits
the terminal `completed` to `deleted` transition. A Program session and an
execution session are each unique. Completed links are accepted only after the
referenced completed session has been imported. The migration is additive,
transactional, creates no execution row, and preserves Program definitions,
preparations and history unchanged.

## Schema v26: Programs presentation, Android projection, and logical deletion

Schema v26 adds nullable `programs.deleted_at` and the terminal
`program_deletions` logical-deletion ledger. A ledger row binds the stable
program identity to the request revision, deleted revision, response replay,
generation identity, and nullable `acknowledged_at`. Deletion is available for
active and archived programs. It retains the imported source definition and
all derived preparations, but a deleted program cannot be resurrected or used
to create another preparation. An archived program cannot be prepared.

The related Programs presentation reads imported definitions, their sessions,
usage, and provenance as real desktop data; it does not make planning targets
performed work. The v26 migration is additive and transactional.

## Schema v25: Web deletion requests and Program V1

Schema v25 adds durable Program V1 definitions, their ordered session and
occurrence children, immutable import digests, request replay records and
source provenance on derived manual preparations. It also adds the bounded Web
session-deletion request ledger and nullable AI-proposal `withdrawn_at` state.
Program import and each application command are transactional. Program rows
contain planning targets only and cannot own performed sets.

Proposal deletion is logical. Draft and completed-session deletion append an
immutable causal operation and advance deleted causal state in the same
transaction; completed sessions also receive a durable deleted finalization.
These paths do not implement a generic cascading browser delete.

## Schema v24: durable preparation withdrawal

Schema v24 adds nullable `session_preparations.withdrawn_at` and the immutable
`session_preparation_withdrawals` ledger. Each preparation has at most one
withdrawal identity, bound to the exact current revision, request timestamp,
generation and correlated ACK. The service transaction inserts this ledger row
and withdraws the preparation together. It does not delete or rewrite revision,
entry, delivery, proposal, execution, session, feedback or acknowledgement
evidence. A withdrawn preparation is excluded from active projections and
cannot be edited or delivered again.

## Schema v23: Web session preparations

Schema v23 adds `session_preparations`, immutable
`session_preparation_revisions` and ordered `session_preparation_entries`.
`session_preparation_requests` makes Web command replay durable, while
`session_preparation_deliveries` binds one immutable revision to one reserved
execution `session_id` and its correlated generation/ACK evidence. These tables
store no performed sets, MAX results or feedback and do not alter imported AI
proposal identity.

## Schema v13: machine-specific exercises

Migration v12→v13 is additive and transactional. It adds exercise-owned load
and provenance metadata, installs the five frozen UUIDv4 identities, applies
the approved same-ID display renames, and repoints only manifest-listed
`entry_id` + source exercise + source equipment triples. Session IDs, entry
IDs, positions, child set/MAX/continuous rows, notes, targets, timestamps,
aliases, and body observations are unchanged. Reopening v13 is idempotent.

Equipment tables and occurrence equipment columns remain in Phase 1 for sync,
durable drafts, old imports, and explicit historical provenance. Schema cleanup
is a later, separately versioned migration.

## 1. Status

```text
TRAINLOG_DATABASE_SCHEMA_VERSION=28
DATABASE_SCHEMA_V28=IMPLEMENTED
TRAINLOG_FORMAT_V1=FROZEN
```

The desktop SQLite database is the canonical long-term Trainlog history.

Its schema evolves independently from all JSON exchange-format versions.

## 2. Versioning

Schema version uses:

```sql
PRAGMA user_version;
```

Current value:

```text
27
```

The independent actual-set loads documented in the current desktop, Android
and V2 flows use the existing ordered `performed_sets.weight_kg` field and the
corresponding durable draft-set field. The per-set column itself pre-existed,
but desktop v10 is required: v9 -> v10 rebuilds `performed_sets` solely to
widen actual `weight_kg` from finite `> 0` to finite `>= 0`.

Supported historical databases are migrated explicitly through the implemented
migration chain. A database newer than the running binary understands is
rejected.

Schema v20 additively introduces `sync_causal_operations` and
`sync_causal_state`. Operations are immutable and identify their target,
creator, causal predecessor, timestamp, payload digest, and an unassigned
publication context. Target state records the durable deleted revision. The
v19→v20 migration creates empty protection tables and does not fabricate
ancestry for existing rows. Applying an operation, recording its protection,
and performing dependent changes share one transaction.

Non-deleted rows in `sync_causal_state` are also the durable revision boundary
for supported corrections whose content can later return to an earlier value.
They do not fabricate migration ancestry: they are written only by a real
post-migration mutation. Session exercise replacement writes this revision in
the same transaction and refuses to modify a deleted target.

Schema v21 additively introduces stable installation peer identity, outgoing
generation/artifact state, consumed-generation state, durable ACK records, and
the separate immutable-operation publication ledger. The v20→v21 migration is
one SQLite transaction, preserves all business and causal rows, and creates no
historical generation, ancestry, consumption, or ACK. A complete generation
consumer owns one outer transaction covering every domain mutation, its
consumption record, and the data required to replay the same ACK after restart.
Transport publication and ACK-file writes remain outside that transaction.

Version 19 additively stores causal note revisions/current state and staged
execution drafts, their immutable revisions, pending/active state and durable
finalization ledger. It changes no published V1 format and performs no user
database migration until a v19 binary is deliberately deployed and opens that
database.

Version 7 assigns `session_exercises.entry_id` to each stable occurrence.
`exercise_id` remains only the catalogue identity and may occur more than once
in a session. The v6 → v7 migration rebuilds the obsolete uniqueness
constraint while retaining rows, sets, continuous activities, weights and
equipment associations.

Version 8 adds local `custom_equipment` definitions. The v7 → v8 migration is
additive: it retains all historic occurrences and their `equipment_id` values.
Supplied definitions continue to be generated from `catalog/equipment-v1.json`;
custom definitions exist only in the desktop database.

Version 9 adds the one-to-one `max_results` table. The v8 → v9 migration
converts a legacy `max_test` occurrence only when it contains exactly one
performed set with `reps = 1`, no duration, and a positive weight. The stable
session, occurrence, exercise, position, and equipment identities are retained.
Multiple attempts and every other ambiguous shape remain as historical sets.

Version 10 rebuilds only `performed_sets`. Its explicit projection preserves
every row ID, owning occurrence, position, repetitions-or-duration, and
existing `NULL` or positive `weight_kg` value unchanged; it permits a new
explicit zero actual load. The migration is transactional. Regression coverage
checks lossless migration, rollback after an injected rebuild-name collision,
`PRAGMA integrity_check`, `PRAGMA foreign_key_check`, restored foreign-key
enforcement, and rejection of negative loads or invalid metric shapes.

Version 11 is additive. It creates `exercise_body_zones` and the private
`exercise_body_zone_sync` comparison baseline, then inserts only the exact
stable-ID mappings declared with evidence in `catalog/body-zones-v1.json`.
There is no exercise-ID, occurrence-ID, session, performed-set, MAX, equipment
or body-observation rewrite. The migration is one transaction and uncertain
historical exercises remain valid with no relation.

Version 12 is additive. It creates `exercise_aliases`, whose unique source ID
points to one live canonical `exercises.exercise_id`. Valid writes keep this
mapping collapsed: a merge repoints occurrence ownership transactionally,
preserves all stable occurrence and child-row identities, unions compatible
direct zones, and rejects profile or primary-zone conflicts before mutation.

A schema fixture must represent the real historical structure. Rewriting only
`user_version` is not an acceptable migration test.

Version 18 is additive and transactional. It creates `ai_session_drafts`,
`ai_session_draft_entries`, and `ai_session_draft_imports` for the strict
`TRAINLOG_AI_SESSION_DRAFT_V1` Drive inbox. A desktop row retains the source
`aid_<uuid-v4>` ID, canonical payload SHA-256 digest, source exercise identity,
archive status/error, and nullable successful-publication timestamp separately
from target-only plan values. Entry rows do not store notes. The
`(draft_id, digest)` identity makes an exact replay a skip and makes a changed
payload under the same identity an explicit conflict. A failed child insert
rolls back header, entries, and identity together. No completed session,
performed set, MAX, feedback, active draft, or frozen exchange artifact is
rewritten.

Android schema v17 is likewise additive but has different local ownership:
its `ai_session_drafts` pending collection and entries coexist with—not replace—
the exactly-one `active_session_draft`. `started` and `deleted` state rows are
durable tombstones. Starting or deleting a pending proposal atomically removes
its entries while retaining that state row, so companion replay cannot recreate
it.

Schema v14 adds `exercise_feedback` and `session_followups` without rewriting
v13 facts. Both use stable immutable IDs, exact timestamps, non-empty bounded
text at API boundaries, and parent-owned cascading foreign keys to
occurrence/session owners. There is no direct feedback delete operation;
parent cascade preserves existing correction/deletion semantics. Android
additionally gains nullable `sessions.ended_at`; migration does
not populate it for old rows.
Desktop occurrence replacement preserves feedback for logical occurrences
whose `entry_id` remains in the same session: it snapshots and remaps the rows
inside the replacement transaction. Removed entries cascade, new entries do
not inherit observations, and the stable `sessions` row—and therefore every
session follow-up—remains unchanged.
On Android, v14 additionally creates `draft_exercise_feedback`, owned by
`draft_session_exercises` with `ON DELETE CASCADE`. Its stable `feedback_id`,
timestamp and raw text transfer to the completed table by occurrence `entry_id`
inside the existing finalization transaction. It is never synchronized while
the parent session remains a draft.

Schema v15 is additive. `exercise_feedback_revisions`, Android-only
`draft_exercise_feedback_revisions`, and `session_followup_revisions` retain
immutable wording revisions. Every v14 root receives the deterministic initial
identity `fr0_<root-id>` with `created_at=observed_at`; corrections use
`fr_<uuid-v4>`. Root `raw_text` remains only a transactionally updated
current-text cache. The authoritative current revision is selected by parsed
`created_at` instant, then bytewise `revision_id` on ties.

Desktop schema v16 transactionally rebuilds only `session_exercises` to add
`tracking_mode TEXT NOT NULL CHECK(tracking_mode IN ('reps','duration'))`.
Each v15 row is backfilled exactly once from its linked exercise before the
version is published. Row counts, foreign keys and source/catalog agreement are
checked before commit; `id`, `entry_id` and every child row remain unchanged.
Desktop schema v17 and Android schema v16 then add `exercise_profile_state`, a
one-row-per-exercise current pointer, plus `exercise_profile_revisions`, the
durable causal chain for mutable current capture profiles. Both migrations seed
`pr_legacy_v1` without rewriting exercises or occurrence snapshots. Local edits
append deterministic revisions transactionally. Each exercise is limited to 32
revision records; a 33rd edit/import is rejected explicitly, never pruned or
ordered by a wall clock.
Already-versioned prototype databases are repaired on open without a version
bump: an exact `pr1|recording_mode|tracking_mode|data_fields` current token is
replaced atomically by `pr_legacy_v1` and its canonical first `pr2_` child.
Malformed, impossible, orphaned, or mixed prototype lineage aborts the open and
rolls back the repair.

## 3. Connection invariants

Every connection enables:

```sql
PRAGMA foreign_keys = ON;
```

A bounded SQLite busy timeout is configured by the core.

## 4. Tables

### `exercises`

Canonical desktop exercise catalog.

```text
id
exercise_id          UNIQUE stable identity
name
normalized_name      UNIQUE normalized display form
tracking_mode        reps | duration
recording_mode       sets | continuous
data_fields          bounded bit mask
```

Rules include:

- continuous implies duration tracking;
- unknown supplemental field bits are rejected;
- normalized names remain unique.

### `exercise_aliases`

```text
source_exercise_id       PRIMARY KEY retired creator identity
canonical_exercise_id    foreign key -> exercises(exercise_id), restrict delete
```

Sources and targets differ. Targets are always live catalog identities, and a
target is never another alias source; this makes resolution bounded and rejects
chains/cycles in imported companion artifacts.

### `exercise_body_zones`

Direct exercise-to-zone relations:

```text
exercise_row_id      foreign key -> exercises(id), cascade delete
zone_id              stable ID from body-zones-v1.json
role                 primary | secondary
PRIMARY KEY          exercise_row_id, zone_id
partial UNIQUE       one role=primary row per exercise
```

The composite key prevents one zone from being both primary and secondary.
Public writers validate that every ID exists and is not a group. An exercise
with no rows is explicitly unclassified; a secondary-only state is rejected as
corruption. Parent relations are derived from the manifest during filtering
and are not stored here.

`exercise_body_zone_sync(exercise_row_id, synced_state)` is internal sync
metadata, not domain data. Its deterministic state records a nullable primary
and byte-sorted secondary IDs so one-sided edits can be distinguished from a
simultaneous conflict. It is never exposed as a translated value or used to
merge secondary sets by union. The publishing peer records the exact snapshot
as its baseline only after the companion has been published successfully.

### `sessions`

```text
id
session_id           UNIQUE stable identity
started_at
ended_at             nullable
session_type         training | max_test
notes                nullable
```

### `session_exercises`

Ordered exercise occurrence inside one session.

```text
session_row_id
exercise_row_id
entry_id             UNIQUE stable occurrence identity
recording_mode
tracking_mode         immutable occurrence unit snapshot
data_fields
position
load_mode
rest_seconds
target_sets
target_reps
target_duration_seconds
target_weight_kg
equipment_id         nullable equipment identity
notes
```

Desktop history snapshots `recording_mode`, `tracking_mode` and `data_fields`
in the occurrence row. Catalogue profile edits supply defaults only to future
occurrences and never update these snapshots.

An occurrence `equipment_id` resolves to either a supplied manifest definition
or a desktop-local `custom_equipment` definition. An ID that cannot be
resolved is retained as historic data and is explicitly visible to the user;
it is never silently converted to `NULL`.

### `custom_equipment`

Desktop-local custom equipment definitions. They supplement, but do not alter
or replace, the supplied catalogue generated from `catalog/equipment-v1.json`.
They synchronize through the separate `trainlog-equipment-definitions` v1
artifact; session/mobile and association V2 artifacts retain their existing
shapes. Definition reconciliation is additive: omission does not delete, an
equal same-ID row is idempotent, a divergent same-ID row conflicts, and IDs in
the supplied manifest are reserved.

`SETS` rows support two target shapes in schema v5:

```text
explicit planned target
    target_sets + exactly one target metric

actual-only mobile observation
    target_sets = NULL
    target_reps = NULL
    target_duration_seconds = NULL
```

This v5 rule is what permits heterogeneous mobile performed sets without
inventing a fake uniform target.

`CONTINUOUS` rows are targetless and require:

```text
load_mode = none
rest_seconds = 0
target_weight_kg = NULL
```

### `performed_sets`

Ordered actual set records.

```text
session_exercise_row_id
position
reps                 nullable
duration_seconds     nullable
weight_kg            nullable
```

Exactly one primary actual metric is present:

```text
reps
or
duration_seconds
```

Actual repetitions may be zero.

Each row is independent; heterogeneous repetition sequences are first-class
data. Its nullable `weight_kg` is likewise occurrence-set data: blank is
distinct from an explicit zero and from a planned target weight. When present,
an actual weight is finite and `>= 0`.

Planned `target_weight_kg` remains distinct planning metadata and, when
present, is finite and `> 0`; it is never copied into an actual set.

### `continuous_activity`

One-to-one actual record for a continuous session exercise.

```text
session_exercise_row_id  UNIQUE
duration_seconds
speed_kmh                nullable
distance_km              nullable
```

Continuous activity never creates a fake performed set.

### `max_results`

One-to-one explicit result for an occurrence in a `max_test` session.

```text
session_exercise_row_id  PRIMARY KEY, foreign key
max_weight_kg            finite positive weight
```

The occurrence has no `performed_sets` or `continuous_activity` row. The
referenced exercise owns the performance identity; nullable
`session_exercises.equipment_id` remains contextual metadata.

### `body_observations`

```text
observation_id       UNIQUE
observed_at
session_row_id       optional UNIQUE link
body_weight_kg
neck_cm
shoulders_cm
chest_cm
waist_cm
hips_cm
left_arm_cm
right_arm_cm
left_forearm_cm
right_forearm_cm
left_thigh_cm
right_thigh_cm
left_calf_cm
right_calf_cm
notes
```

At least one body metric must be present.

## 5. Identifier generation

Official desktop creator prefixes:

```text
ex_   exercise
se_   session
bo_   body observation
sy_   synchronization run
sxe_  session-exercise occurrence
```

All use random UUIDv4 values.

Exchange parsers may accept other schema-valid opaque identities where their
contract explicitly permits it.

## 6. Transactions

Multi-row user operations are atomic.

Persisted session correction replaces session child rows transactionally while
preserving the parent:

```text
session_id
started_at
ended_at
session_type
session notes
linked body observation
```

Removing an exercise from a persisted session is therefore a transactional
replacement of the remaining child set.

A failed replacement rolls back to the previously persisted session.

Body-observation editing preserves its stable identity, timestamp, and optional
session link.

Exercise creation/editing and replacement of all direct body-zone relations is
one transaction. Invalid or duplicate IDs leave both exercise metadata and the
prior relation set unchanged.

## 7. Mobile import semantics

`tools/import_mobile_export.py` validates the complete mobile snapshot before
committing database changes.

Properties:

```text
schema-v5-through-v11 aware
transactional
idempotent by stable IDs
profile-aware catalog reconciliation
heterogeneous performed sets preserved
no fake target generated
continuous activity kept separate
```

For active V2 reconciliation, an identical normalized name never suffices by
itself. Different exercise identities may coalesce only when recording and
tracking modes match, other represented invariants remain compatible, and
their bounded `data_fields` masks are comparable by inclusion. The existing
desktop identity is deterministic canonical ownership. The mask union retains
the richer capability, while `session_exercises.data_fields` and all child
values remain unchanged. A missing historic optional value stays `NULL`.
Incomparable masks or modes reject the entire mobile-import transaction.
On v11, a safe duplicate-row merge also preserves the only non-empty body-zone
state. Different non-empty states conflict; no relation list is unioned.

## 8. Units

Canonical desktop persistence:

```text
weight/load          kg
body circumference   cm
duration/rest         seconds
speed                 km/h
distance              km
```

## 9. Android database

The Android SQLite database is independent.

Current Android-local version: **17**. The explicit migration chain adds the
durable draft in v4, equipment references in v5, per-set load in v6, occurrence
identity/multi-occurrence support in v7, and the widened custom-equipment
definition graph in v8. Version 9 adds completed/draft explicit max rows, raw
max and load form text, and the optional stable source session used to resume a
completed Test max.

Version 10 adds `exercise_body_zones` and `exercise_body_zone_sync`, validates
IDs against the shared manifest asset, and seeds the same stable-ID mappings as
desktop v11 without modifying completed or draft work.

| Table | Ownership |
| --- | --- |
| `active_session_draft` | Single `id = 1` row, session type, selected catalog row, raw form text including MAX, optional resumed source session, update time |
| `draft_session_exercises` | Ordered draft exercises and profile snapshots |
| `draft_performed_sets` | Ordered heterogeneous repetition or duration actuals |
| `draft_continuous_activity` | Duration and configured speed/distance without synthetic sets |
| `draft_max_results` | Positive explicit max weight, one-to-one with a draft occurrence |
| `max_results` | Positive explicit max weight, one-to-one with a completed occurrence |
| `equipment`, `equipment_aliases` | Supplied and user-created definitions used by selectors and occurrence FKs |
| `exercise_equipment`, `catalog_exercise_equipment` | Persisted manifest relationship metadata retained across migrations and identity reconciliation |
| `exercise_body_zones` | Direct primary/secondary stable zone IDs; no derived parent rows |
| `exercise_body_zone_sync` | Private common-state baseline for explicit conflict detection |

Foreign keys remain enabled. Draft deletion cascades only through draft child
tables; it cannot delete catalog entries or completed history. The repository
commits completed-session insertion and draft removal together, rolling back
both on failure. Repeating finalization after success cannot create another
completed session. Completed `started_at` semantics are unchanged by this repair.

Migration tests cover historical v4 and v7 shapes rather than changing only
`user_version`. The physical Samsung and desktop v9 migrations preserved every
existing domain row after fresh coherent backups, with successful integrity and
foreign-key checks. Reconciliation validation also uses coherent Android and
desktop v9 copies before applying migrations to the real stores. SQLite files
are never synchronization artifacts.

Desktop schema v22 adds `sync_generation_archives` as an append-only archive
ledger. A row retains the generation identity, verified manifest digest,
deterministic archive digest, durable location, timestamp and canonical audit
record. Generation, artifact, ACK, consumption and causal-publication rows are
not deleted or rewritten when a payload becomes archived.

The schemas deliberately differ where ownership differs. Desktop sessions own
`ended_at`, notes, planned targets and session-specific load semantics; the
Android capture schema does not. Desktop body observations may link to a
session and carry notes; Android body observations contain capture metrics only.
Android occurrences snapshot `tracking_mode` as well as recording mode and
fields, whereas the current desktop schema resolves tracking mode through the
catalog exercise. Desktop stores occurrence `equipment_id` as stable text so
unknown historic IDs remain visible; Android stores a foreign key to its local
equipment definition.

Audit limitation: Android persists supplied exercise/equipment relationship
tables, but the current session selector searches the complete equipment list
instead of filtering or ranking it through those relations. The tables are
retained because migration and identity reconciliation already preserve them;
making them authoritative UI policy is future catalog work, not a v9 cleanup.

Desktop and Android schema versions are not required to match.

Do not synchronize SQLite database files.

## 10. Session-generation planning metadata

Desktop schema v11 and Android schema v11 add, through their additive
v10 -> v11 migration, `load_mode`, `rest_seconds`, `target_sets`,
`target_reps`, `target_duration_seconds`, and `target_weight_kg` to both normal
completed and durable-draft occurrences. Existing rows receive mode `none`,
zero rest and NULL targets; no historical plan is reconstructed. Row IDs,
identities, positions, equipment, actual sets, continuous rows, MAX results and
raw partial draft input are preserved.

Targets are nullable planning metadata separate from actual rows. A plan has
positive sets and exactly one positive repetitions or duration value; a target
weight is finite and positive when present. An absent target has no target shape.
Continuous and explicit-MAX occurrences are targetless. Plan persistence does
not weaken normal completion's requirement for actual work.

## 11. Validation

```bash
meson compile -C build
meson test -C build --print-errorlogs
```

Migration-specific regression coverage includes:

```text
schema_v5_migration
schema_v7_migration
body_zones
body_zone_catalog_validation
body_zone_sync
exercise_reconciliation
max_results
max_sync
```

The current normal desktop suite contains 47 tests.

## APP_SHELL_V1 read-only equipment paging

`trainlog_database_list_custom_equipment_page()` is a desktop read-only page
reader for the equipment catalogue UI; it is not a migration or a schema
change. It accepts an offset, caller-owned output buffer, and a capacity of
1 through 128. It reads at most `capacity + 1` rows in deterministic
`display_name COLLATE NOCASE, equipment_id` order. `output_more` is true only
when that one extra ordered row exists. Every output is valid only on `OK`;
invalid arguments, invalid/overflow offsets, and corrupt non-text or
embedded-NUL values fail explicitly. The offset is a refreshable presentation
position while data is unchanged, never a durable cursor or idempotency token.

## 11. Explicit and legacy measured maxima

Schema v9 persists a weight maximum in `max_results`, separate from
`performed_sets`. A new max occurrence therefore contains no repetition or set
count. Only `sessions.session_type = max_test` may own this row.

Exercise performance points carry the originating session type so the
measured-max layer can distinguish explicit tests from ordinary training.

The performance reader also preserves the prior measured-max interpretation for
ambiguous legacy max-test sets that the migration deliberately did not convert.
Rules for those legacy rows remain:

```text
training session
    never becomes measured max implicitly

max_test + external
    greatest successful actual load
    tie -> greatest reps/duration

max_test + assistance
    lowest successful assistance
    tie -> greatest reps/duration

max_test + no load
    greatest successful reps/duration
```

A zero-repetition failed legacy attempt is not a successful measurement.

The current measured result is the newest successful max-test point. The record
is the best max-test point using the same load mode.

For explicit rows, `max_weight_kg` is the canonical result. Latest-max history
groups by `exercise_id`, then reports date and occurrence equipment context;
it never groups by machine.

## 12. Equipment and occurrence migration

Desktop schema v6 added nullable `session_exercises.equipment_id`. Schema v7
adds the non-null stable `entry_id` and removes the obsolete
`UNIQUE(session_row_id, exercise_row_id)` constraint. The v6 -> v7 rebuild
preserves primary keys, completed sessions, ordered sets, continuous activities,
per-set weights and equipment values.

Schema v8 adds local `custom_equipment` definitions through an additive v7 ->
v8 migration. Existing occurrence links remain unchanged. They resolve through
the supplied manifest or the local custom table; unknown historic references
remain explicit rather than being discarded.

Schema v9 adds explicit max results through the bounded v8 -> v9 conversion
described above. It never chooses among multiple one-repetition attempts and
never deletes an ambiguous source row.

Android schema v6 added nullable `weight_kg` to completed and durable draft
set rows. Schema v7 assigns stable `entry_id` values to completed and draft
occurrences. Actual per-set weights remain independent values, so heterogeneous
sets and weights survive edit, finalization, reopen and V2 exchange.

Android schema v8 non-destructively migrates the v7 equipment reference graph
so custom definitions may use `load_semantics = none`. Historic completed
occurrences, draft occurrences, and their equipment references remain intact.

Android schema v9 applies the same bounded legacy conversion to completed and
durable-draft occurrences. A resumed max-test draft preserves its source
`session_id`; atomic finalization replaces that session's ordered children
instead of generating a second session.

Desktop schema v11 and Android schema v10 then add only body-zone relation and
sync-baseline tables. Both seed exact manifest mappings by stable exercise ID;
neither migration changes the occurrence/equipment/MAX graph described above.

Desktop and Android schema v12 added only the durable flattened
`exercise_aliases` mapping. It resolves retired creator IDs to a live canonical
exercise during catalog and companion reconciliation without changing any
session, set, continuous, MAX, equipment, body-zone or planning wire shape.

## 13. Body analytics persistence rule

Body analytics still require no dedicated schema change; schemas v9 through
v12 do not alter their measurement storage.

Canonical persistence continues to contain only measurements actually entered
by the user.

These values remain derived at display time and are not persisted:

```text
body-fat estimate
estimated fat mass
estimated lean mass
waist/hip ratio
shoulder/waist ratio
chest/waist ratio
left/right asymmetry percentages
```

The optional estimation profile is desktop configuration, not database
history.

## 14. Training knowledge read boundary

Training Knowledge V1 adds no table, migration, seed data or synchronization
artifact. Desktop schema v13 retains it unchanged. Read-only context assembly
joins an exact existing exercise with its persisted BODY ZONE relations,
occurrence history, raw sets, actual equipment and latest explicit MAX, then
optionally attaches immutable catalog knowledge. Missing catalog knowledge is
valid and never causes a database mutation. The scientific catalog's BODY ZONE
projection is not stored in `exercise_body_zones` and does not replace user
classification. See [Training knowledge system V1](domain/knowledge_system.md).
Occurrence pagination and latest explicit MAX use an exact derived instant
comparison, then bytewise session/entry ID ties. They scan all matching
metadata and retain only bounded candidates before hydration, inside a read
snapshot. Original timestamp text stays unchanged. Malformed storage and a
selected timestamp beyond the existing 40-character C output capacity produce
explicit errors. The [temporal contract](reviews/training_knowledge_v1_temporal_contract.md)
distinguishes the accepted profile from accidental Python ISO extensions.
The temporal contract has independent PASS evidence. The initial audit's stale
temporal documentation, Android loader, Meson input, and C role-only query
findings were repaired, independently verified, and final-validated;
`TRAINING_KNOWLEDGE_V1=PASS`.

Exercise naming changes update only `exercises.name` and `normalized_name` for
explicitly mapped stable IDs; no schema migration or historical-row rewrite is
required.

## 15. Profile-revision SQLite connection functions

Schema v17 profile-state insert/update triggers use the deterministic
`trainlog_profile_revision(parent, recording_mode, tracking_mode, data_fields)`
UUIDv5 function. The migration-only `trainlog_pr1_valid(...)` recognizer shares
the same connection boundary. These are process-local SQLite functions even
though the triggers that reference them are persisted.

Every production connection capable of preparing Trainlog schema statements
must therefore install the complete function set before executing schema or
data SQL. Native connections do so in
`trainlog_database_open_with_diagnostic()`; Python import/export tools use the
single `tools/trainlog_sqlite.py::connect_database()` factory. SQLite resolves
trigger functions while preparing a statement even when a trigger `WHEN`
condition will suppress its body, so read-mostly importers are not exempt if
they can update or insert an exercise. Raw SQLite connections are limited to
test-fixture construction and read-only inspection.
