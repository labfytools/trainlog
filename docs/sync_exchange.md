# Synchronization exchange

## Sleep Diary companion

`trainlog-sleep-diary` version 1 is finalized in stable v0.1.4 with both the
medication catalog and the entry-owned intake snapshots. It remains frozen and
readable historical input. The active Sleep exchange requires the separate
`sleep-diary-v2` capability and `sleep-diary-v2.json` with
`format=trainlog-sleep-diary`, `version=2`.
The artifact preserves catalog and diary causal parents, rejects concurrent
siblings and resurrection, and treats an exact revision replay as idempotent.
This completion happened before publication or freezing of the companion;
`TRAINLOG_FORMAT_V1` remains unchanged and frozen.

Every field edit commits locally as a new immutable diary revision. Explicit
day validation marks only the guarded current tip publishable and starts no
transport. V2 also permits the current revision of a quick-captured pending
night to be marked quick-publishable: this is a distinct exact-revision marker,
not day validation, and it does not validate an incomplete night. Generation
capture exports either eligible current tip, records each exact
generation/revision relation, and only a correlated consumed ACK advances the
acknowledged marker. A correction after ACK therefore remains local and
appears as modified until it is revalidated or quick-published again and
acknowledged by a later generation. Imported tips are already validated and
acknowledged because their containing generation was durably consumed. Draft
and synchronized state are not inferred from wall-clock time.

`trainlog-sleep-diary`, version 2, is the active full-generation companion.
It carries bounded complete entry snapshots with
stable identities, the current immutable revision and causal parent,
offset-bearing timestamps, appreciations, notes, logical deletion, and an
integer `quantity` in `1..99` on every medication intake. `dose_value` and
`dose_unit` remain the per-unit snapshot; quantity is never encoded in notes.
V1 input omits `quantity` and is normalized to one unit. Exact revision replay
is unchanged; sibling revisions and resurrection after deletion are rejected
in the generation consumption transaction.

This does not change `TRAINLOG_FORMAT_V1`, manifests, ACKs or causal
ownership.

## Bluetooth-priority transport and Android AI publication

The private 0.1.4 installation now adds Bluetooth Classic RFCOMM as the primary
byte transport without changing the full-generation business protocol. The
desktop registers the Trainlog-specific service UUID
`f0d1c0de-7a11-4f62-9b7c-545241494e4c` through BlueZ and accepts only the
configured bonded Android device address. Android connects as the RFCOMM
client, then proves the already persisted Trainlog `peer_id` in the first
bounded frame. Bluetooth name strings are never authority.

Automatic transport selection is `Bluetooth -> MTP -> unavailable` for the
current target configuration. MTP remains the wired recovery transport. The
business `sync.lock` remains common while Bluetooth and MTP have independent
physical transport locks, so an idle discovery probe cannot reject a Web
synchronization.

Entering Bluetooth range establishes one connection and publishes one bounded
Android full-generation request. Reconnect noise is rate-limited; a persistent
connection is not a timer that creates generations repeatedly. The existing
background generation coordinator still owns capture, validation, causal
consumption and durable ACKs.

After both generation directions are durably acknowledged, desktop generates
the canonical `trainlog_ai_export_v1.json` with the existing read-only AI
exporter and best-effort delivers that file to Android over the selected local
transport. Android stores it in `Documents/Trainlog`, then a separate SAF
publisher replaces the same file in the user-selected `Trainlog/AI` Google
Drive folder and verifies the provider bytes by size and SHA-256. Drive
publication failure is retryable and never rolls back or fails an already
completed synchronization. Desktop rclone is not part of this target path.

The detailed framing, trust and fallback contract is
[Bluetooth synchronization transport V1](design/sync_bluetooth_transport_v1.md).

## Legacy USB-priority and private Drive transport

Full-generation synchronization has one business engine and two byte
transports. Automatic desktop admission acquires the existing global lock,
probes the configured Android MTP peer, selects USB when that peer is actually
usable, and otherwise selects the configured private Drive namespace. A cable
or retained peer advertisement is not availability evidence.

After a successful USB conversation, desktop attempts to mirror the exact
current coordination objects and referenced immutable generations to the
separate configured `Trainlog/Sync/v1` namespace. Artifacts are uploaded and
read back for size/SHA-256 verification; a generation manifest is uploaded
after its artifacts and its mutable reference last. Mirror failure is reported
but never rolls back the already committed USB import or ACK. With Drive
disabled, historical USB behavior remains independent of rclone.

Drive carries only existing versioned artifacts, manifests, references and
ACKs. Desktop and Android SQLite files are never eligible transport objects.
`Trainlog/Sync` is independent from `Trainlog/AI`; the rclone remote is trusted
installation configuration, not a product constant or an HTTP parameter.

Android uses the system document provider: the user explicitly selects the
private `Trainlog/Sync/v1` folder, Trainlog persists access to that folder only,
and Disconnect revokes it. No rclone password, OAuth token or authorization
header is stored or logged by Trainlog. WorkManager checks Drive immediately
after connection and then under Android scheduling constraints; periodic work
has a 15-minute minimum and is not an instant push channel.

The separately enabled USB background mode uses a visible `dataSync`
foreground-service notification and the same `SyncGenerationCoordinator` as
SyncScreen. A process-wide guard rejects concurrent UI/service conversations.
Android 15+ limits `dataSync` foreground-service time and forbids starting it
from `BOOT_COMPLETED`; after reboot the user must reopen Trainlog and reactivate
the listener. A filesystem/provider event is only a prompt to validate the
full run, generation, peer, hash and ACK envelope and never starts a workout.

## Manual session preparations V2

`trainlog-session-preparations` V2 is a separate optional desktop-to-Android
generation artifact. It carries immutable ready revision snapshots and stable
delivery, preparation, occurrence and reserved execution identities, plus
immutable preparation-withdrawal operations. Program-owned deliveries also
carry the paired stable Program and Program-session identities. Android keeps
reading legacy deliveries that omitted this pair and accepts a later enrichment
only when all legacy fields and the reserved execution identity are identical.
It never
overloads mobile export, AI proposals or execution-drafts V1. Android validates
the complete artifact in the generation transaction and stores it as pending;
start remains an explicit foreground action. Desktop records the exact
generation relation at capture and changes delivery state to acknowledged only
after accepting the correlated durable consumption ACK. It similarly records
the exact withdrawal-to-generation relation and acknowledges a withdrawal only
after Android's whole-generation commit.

V1 remains readable for delivery replay compatibility but cannot transport a
withdrawal. Both peers advertise `session-preparations-v2`; generation mode
refuses an older capability set instead of omitting the causal operation. A V2
withdrawal lists every desktop-known delivery for its preparation. Android
cancels only exact pending matches, preserves exact started matches, stores the
result as a tombstone, and refuses later resurrection from an old V1/V2
delivery. Neither result deletes an active draft or completed workout.

## Causal deletion safety gate

Android and desktop expose staged producers and consumers for the independent
`trainlog-causal-deletions` V1 artifact; it is not in the active MTP bundle. A
protected store refuses legacy mobile snapshot export, and legacy mobile import
rejects protected session, observation, or exercise identities before mutation.
Deleted execution-draft replay is stale. Tombstone-free V1/V2/V3 behavior is
unchanged.

Legacy feedback, BODY ZONES and custom-equipment companions are also refused
when their domain has deleted causal state. Retired exercises and custom
equipment remain resolvable for history but are excluded from new-work lists;
withdrawn feedback keeps its immutable revisions while disappearing from the
current feedback view. Profile and alias state cannot make a retired exercise
available because availability is independently guarded by causal state.

## Machine-exercise Phase 1 compatibility

Desktop schema v26 and Android schema v24 retain mobile export V3, readable V1/V2 imports,
equipment definitions V1, equipment associations V2, exercise aliases V1, and
the BODY ZONES companion without wire-format changes. Machine metadata is not
silently added to a frozen artifact: stable exercise IDs and canonical names
travel through the existing catalog, while v13 metadata stays local until a
dedicated versioned companion is required. Legacy equipment IDs may still be
written for fixed supplied machines as compatibility provenance.

V3 already carries occurrence `tracking_mode`; desktop v16 imports it directly
into `session_exercises` and exports the same snapshot. Catalogue
`tracking_mode` remains the default for future occurrences only, so no wire
format bump is required.

Android and the current desktop importer treat the exercise catalogue link of
a completed V2/V3 entry as an identity constraint only. The entry's own
`recording_mode`, `tracking_mode`, and `data_fields` validate its actual payload
and remain authoritative even after the current catalogue profile evolves.
Import never rewrites historical sets or continuous values to fit the current
profile. Legacy desktop schemas that cannot persist the occurrence tracking
mode retain their explicit compatibility rejection instead of losing its unit.

## 1. Status

```text
TRAINLOG_SYNC_BLUETOOTH_TRANSPORT_V1=PASS/FROZEN
DIRECT_MTP_TRANSPORT=PASS
ANDROID_TO_PC_IMPORT=PASS
PC_TO_ANDROID_CATALOG=PASS
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
EXPLICIT_MAX_RESULTS_V2=PASS
SESSION_GENERATOR_V1=PASS
BODY_ZONE_SYNC_V1=PASS
BODY_ZONE_SYNC_V1_LIVE_DEVICE=PASS
TRAINLOG_AI_SESSION_DRAFT_V1=VALIDATION_PENDING
TRAINLOG_SYNC_DATA_LIFECYCLE_V1=PASS/FROZEN

TRAINLOG_FORMAT_V1=FROZEN_UNCHANGED
```

Mobile export V3 also carries completed-session corrections. For an existing
session with the same immutable header, peers reconcile the complete occurrence
and planning state atomically. A retained `(session_id, entry_id)` cannot be
rebound to another exercise and retains feedback roots/revisions; omitted
occurrences follow the confirmed parent-owned cascade. An identical second
import is a skip. V2 has no complete planning authority and cannot apply a
general correction; its older resumed-MAX compatibility remains bounded.

Synchronization artifacts are separate from the frozen Trainlog session JSON
v1 format.

V4 history and execution-draft V1 are staged codecs with explicit repository
and command-line entry points. They remain deliberately absent from
`trainlog_sync_run()`: configured Android/daemon requests now route around that
strict legacy V3 engine into the existing full-generation orchestrator. The
request artifact remains only a correlated admission signal. Deployment must
stop the old user worker, replace the installed
binary/scripts as one unit, migrate only through the new binary, and restart
only after compatibility checks; this ticket performs none of those steps.

## 2. Exchange directory

Canonical Android shared-storage directory:

```text
Documents/Trainlog
```

Desktop accesses this directory through direct MTP.

Android accesses this exact public directory directly after the user enables
`MANAGE_EXTERNAL_STORAGE` in Android settings. It checks
`Environment.isExternalStorageManager()`, creates the directory when necessary,
and never uses a SAF tree URI, directory picker, `DocumentFile`, or
`DocumentsContract` for Trainlog artifacts.

The desktop resolves `Documents` and its direct `Trainlog` child for the whole
run. All outbound publications target that folder exclusively. A historical
`Download/Trainlog` tree may remain on the device for manual recovery and may
be inspected by explicitly bounded legacy tooling, but the active sync engine
does not publish to it or let it override an artifact in the new endpoint.

## 3. Artifact table

| Direction | File | Format |
| --- | --- | --- |
| Android -> PC | `trainlog-mobile-export-v1.json` | `trainlog-mobile-export` v1 |
| Android -> PC | `trainlog-mobile-export-v3.json` | `trainlog-mobile-export` v3 (active) |
| Android -> PC | `trainlog-mobile-equipment-definitions-v1.json` | `trainlog-equipment-definitions` v1 |
| Android -> PC | `trainlog-equipment-associations-v2.json` | `trainlog-equipment-associations` v2 |
| Android -> PC | `trainlog-exercise-body-zones-v1.json` | `trainlog-exercise-body-zones` v1 |
| Android -> PC | `trainlog-exercise-aliases-v1.json` | `trainlog-exercise-aliases` v1 |
| PC -> Android | `trainlog-pc-catalog-v1.json` | `trainlog-pc-catalog` v1 |
| PC -> Android | `trainlog-pc-equipment-definitions-v1.json` | `trainlog-equipment-definitions` v1 |
| PC -> Android | `trainlog-pc-mobile-export-v3.json` | `trainlog-mobile-export` v3 (active) |
| PC -> Android | `trainlog-equipment-associations-v2.json` | `trainlog-equipment-associations` v2 |
| PC -> Android | `trainlog-exercise-body-zones-v1.json` | `trainlog-exercise-body-zones` v1 |
| PC -> Android | `trainlog-exercise-aliases-v1.json` | `trainlog-exercise-aliases` v1 |
| PC -> Android | `trainlog-ai-session-drafts-v1.json` | `trainlog-ai-session-drafts` v1/v2 companion; v2 adds proposal withdrawals |
| PC -> Android | `programs-v1.json` | optional staged `trainlog-programs` v1 companion: full nondeleted snapshot plus unacknowledged deletion tombstones |
| Android -> PC | `program-executions-v1.json` | optional `trainlog-program-executions` v1 companion: stable in-progress/completed provenance |
| Android -> PC agent | `trainlog-sync-request-v1.json` | `trainlog-sync-request` v1 |
| Android -> full-generation daemon | `trainlog-sync-full-generation-request-v1.json` | `trainlog-sync-request` v1 |
| PC agent -> Android | `trainlog-sync-receipt-v1.json` | `trainlog-sync-receipt` v1 |

No SQLite file is transferred.

## Optional Programs companion V1

`trainlog-programs` V1 is a staged controlled-generation companion. It is
selected only when the peer capability set includes `programs-v1`. Its snapshot
contains the current nondeleted desktop programs, their planning definitions,
sessions and entries, and the unacknowledged terminal program-deletion
tombstones. Android schema v24 stores a read-only synchronized projection and
returns a durable correlated ACK. Desktop schema v26 records the exact
generation and only then sets `program_deletions.acknowledged_at`.

This companion is independent of `trainlog-program` V1 import, mobile export
V3, `trainlog-pc-catalog` V1, and `trainlog-session-preparations` V2. It does
not change `TRAINLOG_FORMAT_V1`, active default V3 transport, or preparation
semantics.

## Optional Program executions companion V1

`trainlog-program-executions` V1 is an Android-to-desktop full-generation
companion. Android schema v25 emits at most one stable execution fact per
Program session. Desktop schema v27 validates that the Program/session pair
exists, imports completed history before accepting a completed fact, and
permits only the monotonic `in_progress` to `completed` transition. Exact replay
is idempotent; identity reuse or regression is a hard conflict. Because the
descriptor is optional, peers predating this companion keep exchanging all
existing required domains without a capability failure.

For Program-owned manual preparations, an acknowledged delivery is republished
while its matching canonical execution evidence is absent on the PC. This
bounded recovery lets Android enrich legacy deliveries with Program provenance
and stops automatically when the execution returns through this companion.
The recovery emission retains the delivery's original acknowledged generation
relation and requires exact delivery, revision, execution and Program
identities. A current-state Sleep Diary snapshot may initialize a fresh peer
from a non-root current revision while retaining the unavailable parent
identity for subsequent causal advances.

Desktop schema v28 adds terminal `deleted` provenance for a completed session
removed through causal history deletion. A retained older
`program-executions-v1` fact is skipped only when the matching session has a
durable deleted causal state; absence of that tombstone remains a hard error.
This dominance rule preserves provenance while preventing old generations from
resurrecting active history.

`trainlog-exercise-aliases` v1 is the separate EXERCISE_MERGE_V1 identity
companion. Its root contains exactly `format`, `version`, and `aliases`; every
entry contains exactly `source_exercise_id` and `canonical_exercise_id` using
lowercase UUIDv4 creator IDs. Entries are bytewise source-sorted, sources are
unique, mappings are collapsed (no target may also be a source), and the
artifact is bounded to 4096 entries and 1 MiB. It changes no mobile-export V3
field semantics. Import occurs before snapshot reconciliation; exports contain
only live canonical exercise IDs.

Android scoped storage can preserve a prior MTP-created object and create a
new artifact with the provider collision suffix, for example
`trainlog-mobile-export-v3 (N).json` or
`trainlog-mobile-equipment-definitions-v1 (N).json`, or
`trainlog-equipment-associations-v2 (N).json`, or
`trainlog-exercise-body-zones-v1 (N).json`, or
`trainlog-sync-request-v1 (N).json`. For Android -> PC, the engine
accepts only the canonical name and this exact suffix form, selects the newest
MTP modification time (then the greatest suffix and a deterministic object-ID
tie break), and validates that selected artifact normally. It never silently
falls back to an older candidate when the newest one is malformed. Definitions
reconcile before the V2 snapshot, so a valid custom `equipment_id` is known
before a session may reference it. This prevents an older canonical object from
being mistaken for Android's current data while preserving every file in
the historical `Download/Trainlog` tree without selecting it as the active
publication endpoint.

V2 is a separate format: every session entry has an `entry_id`, `position`,
metrics, actual loads and optional equipment identity. This permits two
occurrences of the same exercise without fusion. V1 remains readable with its
frozen contract. A V1 historical session is reconciled with V2 only when the
exercise/order correspondence is unambiguous; otherwise the importer reports
a conflict rather than silently overwriting data.

V3 is a separate, strict session artifact. It retains V2 identity, position,
equipment and actual-data shapes and additionally requires `load_mode`,
`rest_seconds`, and `target`. `target` is null or an object with positive
`sets`, exactly one positive `reps` or `duration_seconds`, and optional finite
positive `weight_kg`; bounds are position 0..100000, sets 1..64, reps 1..10000,
duration 1..86400 seconds and rest 0..86400 seconds. Continuous rows require target null, mode none
and zero rest; MAX remains actual-data-exclusive and targetless. Mode/target
contradictions reject before persistence.

Current publication uses V3 and never emits a lossy V2 rewrite of planned
history. V1/V2 readers retain their contracts. Selected V3 takes priority:
malformed or conflicting V3 fails explicitly with no V2 fallback; only V3
absence permits legacy selection. Equal stable-ID V3 replay skips idempotently;
divergent content conflicts and rolls back. A legacy replay cannot erase local
nondefault planning data. Existing definition-first, companion reconciliation
and filename-suffix selection rules continue to apply.

## 4. Equipment definitions V1

User-created equipment definitions use the separate, directional
`trainlog-equipment-definitions` v1 artifact. The Android-to-PC filename is
`trainlog-mobile-equipment-definitions-v1.json`; the PC-to-Android filename is
`trainlog-pc-equipment-definitions-v1.json`. The filenames identify direction;
the JSON format and version are the same.

The strict root has exactly `format`, `version`, `generated_at`, and
`equipment`. Each `equipment` item has exactly:

```text
equipment_id
display_name
label_name
equipment_type
load_semantics       none | external | assistance
```

Definitions are additive snapshots, not deletion instructions: absence never
deletes a local definition. A same-ID, field-for-field equal definition is an
idempotent skip. A same-ID divergent definition is a conflict. IDs reserved by
the supplied equipment manifest cannot appear in this artifact. These rules
preserve the definition identity that V2 equipment associations reference;
they do not change either V2 shape.

## 4A. Exercise body zones V1

`trainlog-exercise-body-zones-v1.json` is the sole body-zone exchange source
and uses the same filename and format in both directions. Taxonomy definitions
are not copied into each exchange; both applications validate stable IDs from
`catalog/body-zones-v1.json`.

The strict root contains exactly:

```text
format = trainlog-exercise-body-zones
version = 1
generated_at
exercises[]
```

Each exercise row contains exactly:

```text
exercise_id
primary_zone_id       canonical assignable ID or null
secondary_zone_ids    ordered distinct canonical assignable IDs
```

`exercise_id` must be the stable lowercase `ex_<uuid-v4>` creator identity and
`generated_at` must carry an explicit UTC offset. Group IDs are rejected as
direct relations. An empty primary and empty list is the explicit unclassified
state; secondary relations without a primary are invalid. Equal state is an
idempotent skip. Successful publication records that exact snapshot as the
publisher's last shared baseline, including for a newly created custom
exercise. When only one side differs from that baseline, the complete incoming
or local state wins explicitly. If both sides differ, import reports a conflict
and rolls back; secondary lists are never unioned because that would invent
user intent. A mobile creator ID already reconciled by the immediately
preceding V2 exercise import is accepted only with that retained V2 definition
as proof, never from a name-only guess.

## 5. Android -> PC mobile snapshot and retained V2 entry shape

Header:

```json
{
  "format": "trainlog-mobile-export",
  "version": 2
}
```

The artifact is a complete idempotent mobile snapshot containing:

```text
exercises
sessions
body_observations
```

For current publication, Android captures its custom-definition V1 companion,
the mobile V3 snapshot, body zones V1, and the equipment-associations V2
companion before it publishes any of them. It publishes definitions V1, mobile
V3, body zones V1, then associations V2. A malformed persisted custom
definition aborts publication before a mobile V3 file can advertise its
reference; bundled manifest equipment is never copied into definitions V1.

The following V2 entry-shape description is retained for legacy-reader
compatibility only; it does not describe current publication.

V2 session entries additionally carry:

```text
entry_id             stable occurrence identity
position             stable order within session
equipment_id         optional canonical equipment identity
weight_kg            optional actual value on each set
max_weight_kg        optional explicit max-test result
```

The desktop imports sessions first, preserving `entry_id` and their equipment,
then validates the equipment companion only after all referenced entries exist.
The companion corroborates explicit `set`/`cleared` state; it does not overwrite
a divergent occurrence. Reimporting either artifact reconciles stable
identities; it neither duplicates sessions nor regenerates occurrence IDs.

Within an ordered `sets[]` array, `weight_kg` belongs to that individual set,
not to the occurrence or its planned target. Each set therefore replays its own
repetitions-or-duration and nullable load in its original order. A replay is
idempotent: it preserves historic heterogeneous values and does not replace
blank loads with zero, a target value, or another set's load.

An omitted `weight_kg` remains a null/absent actual load. When present, V2
requires a finite value `>= 0`, including explicit zero. This does not alter
the separate strictly-positive `max_weight_kg`/`max_results` contract, V2's
version number, or frozen `TRAINLOG_FORMAT_V1`.

Exercise profile fields:

```text
exercise_id
name
recording_mode
tracking_mode
data_fields
```

Set-based session exercise actuals use ordered:

```text
sets[]
```

with either:

```text
reps
```

or:

```text
duration_seconds
```

Heterogeneous repetition values are valid.

Continuous actuals use:

```text
continuous
    duration_seconds
    speed_kmh optional
    distance_km optional
```

No synthetic set is created for continuous work.

An explicit weight result uses the mutually exclusive shape:

```text
max_weight_kg        finite and > 0
```

It is valid only when the containing session has `session_type = max_test` and
the entry has neither `sets` nor `continuous`. Android, desktop import, desktop
export and the strict validator preserve `session_id`, `entry_id`,
`exercise_id`, `position`, optional `equipment_id`, and the weight. Frozen V1
is unchanged; Android refuses a V1 export containing explicit max data instead
of inventing a `1 × 1` set or dropping the result.

Android-local active-session drafts are excluded from this snapshot and remain
local during synchronization. Only successful atomic finalization makes a draft
a completed exportable session. The v1 artifact has no draft fields or tables;
catalog reconciliation preserves active draft references.
Same-ID catalog entries may update display-name metadata in their existing
catalog row; a rename never creates a second exercise identity.

## 6. Desktop mobile importer

Reference importer:

```text
tools/import_mobile_export.py
```

Properties:

```text
strict full-snapshot validation
transactional import
stable-ID idempotence
catalog reconciliation
profile conflict rejection
heterogeneous performed-set preservation
targetless schema-v5 import when no true target exists
continuous activity kept separate
explicit max result kept separate from sets
```

The importer never invents a uniform target merely to fit desktop persistence.

### Exercise identity reconciliation retained from V2

The retained V2/catalog reconciliation path may reconcile distinct `exercise_id` values sharing
one normalized name only when recording mode and tracking mode are equal, all
other represented invariants are compatible, and one bounded `data_fields`
mask contains the other. The existing desktop identity is deterministic
canonical ownership; Android adopts that PC identity when applying the outbound
catalog. The bitwise union keeps the richer compatible profile.

All references move inside the relevant SQLite transaction. Session and
occurrence IDs, position, sets, loads, continuous duration/speed/distance,
equipment and draft data remain unchanged. Historical occurrence masks remain
snapshots, so an optional field newly present in the catalog is not invented in
old work. Replaying either direction is idempotent. A different mode,
incomparable masks, conflicting overlapping equipment semantics, or any unsafe
reference condition produces an explicit conflict. A normalized-name match by
itself never authorizes a merge.

The current Marche case is the compatible `1` (speed) versus `3` (speed and
distance) subset/superset case. The canonical desktop definition is enriched
to `3`; speed-only history stays speed-only and the Android occurrence keeps
its real distance.

Exercise import reporting separates persistent insertions, persistent
reconciliation changes, and idempotent skips. Resolving a different incoming
ID to an already-compatible canonical desktop row without changing that row is
an idempotent lookup, not a new insertion or a repeated mutation. Accordingly,
the TUI/history summary's `+N exercice(s)` value is exactly the number of new
catalog rows inserted; detailed structured records retain the separate
`exercises_reconciled` and `exercises_skipped` values.

## 7. PC -> Android catalog

The desktop publishes:

```text
trainlog-pc-catalog-v1.json
```

It is a canonical exercise catalog snapshot containing stable profile metadata.

Android reconciles the received catalog into its local exercise catalog.
Applying the same catalog again leaves a previously re-keyed exercise on the
canonical PC identity and reports it as an identical skip. Session occurrences,
active-draft references, and equipment links keep their existing row targets.

This direction does not overload frozen Trainlog session JSON v1.

## 8. Android sync request

Android writes:

```text
trainlog-sync-request-v1.json
```

Header:

```json
{
  "format": "trainlog-sync-request",
  "version": 1
}
```

Required synchronization identity:

```text
request_id = sr_<uuid-v4>
```

The artifact also carries the request timestamp.

A new `request_id` represents a new synchronization request.

## 9. PC sync receipt

After processing an Android request, the PC publishes:

```text
trainlog-sync-receipt-v1.json
```

Header:

```json
{
  "format": "trainlog-sync-receipt",
  "version": 1
}
```

The receipt contains:

```text
request_id
sync_id
status
summary
Android -> PC counts
PC -> Android catalog count
```

Android accepts a receipt only when its `request_id` matches the pending
request.

For `TRAINLOG_AI_SESSION_DRAFT_V1`, Drive—not `Documents/Trainlog`—is the
single inbound source: `TrainLog Gdrive:Trainlog/AI/inbox/trainlog_ai_session_draft_v1.json`.
The desktop fetches it shell-free, parses the strict one-draft source, and
commits it idempotently before copying the exact validated local snapshot with
shell-free `rclone copyto` to
`TrainLog Gdrive:Trainlog/AI/archive/<aid>.json`. The mutable inbox object is
never moved or deleted, so same-name replacement cannot alter the archived
bytes or be removed by archiving. A missing source, transport failure, or
archive failure is reported explicitly; logical inbox replay retains a
retryable desktop status and never undoes the committed import. The desktop
outbound companion selects at most 256 not-yet-published proposals in stable
creation/identity order. Successful MTP publication transactionally marks only
that exact batch; failure leaves it unchanged for retry, while later normal
syncs drain further batches. V2 additionally carries at most 256 permanent
proposal withdrawals. Desktop admission keeps the complete withdrawal set
within that bound. Android stores each as a deleted tombstone before removing
pending proposal entries, so an older V1 replay cannot resurrect the proposal.
The permanent Drive import ledger is independent.
The inbound outcome (`NONE`, `IMPORTED`, `ALREADY_IMPORTED`, `REJECTED`,
`DRIVE_FAIL`, or `ARCHIVE_FAIL`) is retained in the successful sync report and
history; fetched-invalid, Drive, and archive outcomes do not fail an otherwise
successful Android/PC synchronization.

## 10. Shared desktop engine

Canonical implementation:

```text
trainlog_sync_run()
```

TUI path:

```text
TUI
-> shared engine
```

Android-triggered path:

```text
Android request
-> trainlog-syncd
-> shared engine
-> Drive AI draft fetch/import/archive attempt
-> PC -> Android AI draft companion publication
-> receipt
```

Only after either path has fully succeeded, the same desktop engine runs:

```text
SYNC=PASS
-> tools/export_ai_history.py
-> AI_EXPORT=PASS
-> rclone copyto trainlog_ai_export_v1.json
   "TrainLog Gdrive:Trainlog/AI/trainlog_ai_export_v1.json"
-> GDRIVE_UPLOAD=PASS|FAIL
```

`AI_EXPORT=FAIL` skips Drive publication so an older artifact cannot be
uploaded as the result of the new synchronization. Export and Drive are
best-effort post-sync operations: either may fail visibly while the completed
Trainlog synchronization remains successful. Android never executes rclone,
and Trainlog stores neither its configuration nor Google OAuth secrets.

The engine has three explicit modes:

```text
a   Android -> PC: definition V1 -> mobile V3 -> body zones V1 -> association V2
    -> Drive AI draft midpoint; no Android publish
p   PC -> Android: Drive AI draft midpoint -> AI-drafts companion -> profile
    state/aliases/feedback -> definition V1 -> catalog V1 -> body zones V1 ->
    mobile V3 (including bodies) -> association V2; no Android receive
b   bidirectional: complete inbound sequence -> Drive AI draft midpoint -> complete outbound sequence
```

Android consumes that PC publication by reconciling definitions, catalog,
flattened aliases and strict profile-state before body zones. Sessions,
equipment associations, AI drafts and feedback follow zone reconstruction, so
a later companion failure cannot expose a newly catalogued but accidentally
unclassified database as a valid outbound zone edit.

In both directions, definitions are reconciled before V2 artifacts that may
reference their IDs. A mode retains normal validation, transactions, conflict
reporting, and structured history for the work it performs.

## 11. Conflict reporting and preservation

Synchronization does not silently overwrite a session, body observation,
equipment association, body-zone relation, or equipment definition when stable-identity content
conflicts. The diagnostic identifies the affected stable identity and its
source artifact/direction, then records a concise source summary in the run
history. The conflicting persisted value remains preserved; resolution is an
explicit correction or reconciliation, not a side effect of synchronization.

The sole bounded session exception is continuation of the same `max_test`:
`session_id` and `started_at` must match; every existing `entry_id`, movement,
position and ordering prefix must remain; existing values may be corrected and
new ordered entries may be appended. Removal, reorder, exercise rebinding,
session-type change, or an unrelated same-ID divergence still conflicts. This
rule permits an Android-resumed max test to update the canonical desktop and a
subsequent PC snapshot to update Android without duplicating the session.

## 12. Concurrency and request consumption

Synchronization owns:

```text
$XDG_DATA_HOME/trainlog/sync.lock
```

The daemon uses non-blocking acquisition while polling.

The TUI manual action waits for the active synchronization lock.

After a request is completed and its receipt is published, the request ID is
recorded locally so the same request is not processed as a new request again.

## 13. Structured sync history

Every real run has:

```text
sy_<uuid-v4>
```

Each structured run records the selected synchronization direction as `a`,
`p`, or `b` together with its result summary. Thus every current local-history
entry has a known direction. `direction inconnue` applies only to a legacy row
whose historical representation did not record one; the direction is not
inferred.

Artifacts:

```text
$XDG_DATA_HOME/trainlog/sync_runs/sy_*.json
$XDG_DATA_HOME/trainlog/sync_runs/sy_*.txt
$XDG_DATA_HOME/trainlog/sync_history.log
```

The TUI presents newest runs in a selectable list and opens the detail file with
`Enter`.

Legacy history rows without a `sync_id` remain readable as list entries but
cannot have structured detail; a legacy row also has `direction inconnue` only
when its historical representation lacks a direction.

## 14. PC user service

Install or refresh:

```bash
bash tools/install_syncd_user.sh
```

Check:

```bash
systemctl --user is-active trainlog-syncd.service
systemctl --user --no-pager --full status trainlog-syncd.service
```

Daemon log:

```bash
tail -f ~/.local/state/trainlog/syncd.log
```

No root privilege is required.

## 15. Transport invariants

Do not regress to:

```text
SQLite database copying
mandatory GVFS/FUSE mounts
exercise-name identity heuristics
fake sets for continuous activity
fake uniform targets for heterogeneous actual sets
overloading frozen Trainlog JSON v1
```

## 16. Equipment associations V2 and legacy V1

`TRAINLOG_FORMAT_V1` remains frozen. The active companion is
`trainlog-equipment-associations-v2.json`, format
`trainlog-equipment-associations`, version `2`. Each row is identified by
`(session_id, entry_id)` and contains `exercise_id` as consistency metadata,
then either `state: set` with a canonical `equipment_id`, or `state: cleared`
for an intentional removal. In the V2 association contract, the mobile snapshot already
carries the occurrence equipment value and the companion validates it. A
missing companion conveys no equipment information and cannot clear a
previously known choice. Unknown canonical IDs, unknown entries, ambiguous
identities and divergent values reject processing explicitly; an unknown
equipment reference is never silently changed to null.

After `MACHINE_EXERCISE_MODEL_V1`, a pre-v13 association companion can outlive
the five approved completed-occurrence splits even though the active mobile V3
snapshot already carries the new identities. Desktop accepts that stale
corroborating `exercise_id` only for the frozen `(session_id, entry_id, old ID,
new ID)` split table and only when the mobile V3 snapshot imported in the same
run proves the exact new local identity. Equipment state must still be equal.
Every other exercise mismatch remains a conflict. The old Leg Press and Marche
IDs are not aliases for their machine-specific siblings, and draft occurrences
are outside this completed-history compatibility rule.

V2 association semantics are unchanged. A custom equipment ID is accepted only
after its definition V1 artifact has reconciled it on the receiving side.
Unknown IDs, unknown entries, ambiguous identities, and definition conflicts
reject processing explicitly; they are never converted to null or silently
overwritten.

The historical V1 companion remains readable only where its
`(session_id, exercise_id)` targeting is unambiguous. It cannot represent two
occurrences of the same exercise in one session and is not redefined to do so.

## 17. Hardware validation

The following is a prior hardware baseline; it does not claim device validation
of the definitions V1 or three-mode synchronization change:

```text
MTP device discovery PASS
storage access PASS
read/write/list/delete PASS
Android mobile snapshot download PASS
desktop idempotent import PASS
PC catalog publication PASS
Android request detection PASS
trainlog-syncd processing PASS
receipt publication/readback PASS
multiple distinct Android request IDs PASS
```

The current reconciliation checkpoint first ran the production definition, V2
mobile, association, body and outbound exporters on coherent Android and
desktop copies. The explicit-max migration was then applied to the real stores:
the identified session retained all stable identities, converted eight
unambiguous rows, retained its continuous warm-up, and passed integrity and
foreign-key checks. Two hardware bidirectional libmtp runs imported no duplicate
session, exercise, measurement or equipment; both Android and PC V2 artifacts
retained the eight explicit results without synthetic sets.

The PC-to-Android idempotence regression additionally feeds artifacts from all
four production PC exporters into the production Android repository importers.
Its first pass imports the missing fixture data; its second and third passes
report zero session, exercise, body-observation, and equipment additions and
leave exact snapshots of every Android business table unchanged. On the
Samsung SM-G990B, the Body Zones APK certificate matched the installed package
and established keystore before `adb install -r`. The installed database then
migrated v9 -> v10 without changing any pre-existing application row. Two live
bidirectional runs exchanged `trainlog-exercise-body-zones-v1.json`; the second
reported no additions/reconciliations, every Android application table stayed
equal to the first pass, and both peers retained the same 32 relations for 20
of 23 stable exercise IDs. The final companion states were semantically equal
between passes and the three explicit unclassified states remained intact.

## 18. Staged coherent generations and durable acknowledgement

`tools/sync_generation_exchange.py` and Android `SyncGenerationService` are
the production staged owners for manifest V1 and ACK V1. The trusted opt-in
orchestrator can carry their object tree through the production direct-MTP
adapter; automatic V3 selection remains unchanged. Both allocate stable installation peer IDs and
opaque UUIDv4 generation/run IDs only in the explicitly opened staged store.

Desktop capture uses one private SQLite backup and runs every established
exporter against that immutable snapshot. Android holds one repository read
transaction while calling the existing domain exporters. Both validate domain
and outer limits before recording a captured generation. Publication copies
verified immutable bytes into `generations/<generation_id>/`, resumes only
byte-identical partial publication, and writes `manifest.json` last. Capacity
admits at most eight active outgoing generations per consumer. Acknowledged
generations older than the two newest lineage members leave active admission
only after their exact consumed ACK, manifest, artifact sizes and digests have
been reconciled and a complete atomic archive copy has been verified and
journaled. Archive rows preserve identity, lineage, late-ACK and replay
protection; pending, rejected, ambiguous and recovery-tip generations remain
active. Archive directories are outside bounded MTP polling and are never
uploaded recursively. Failure or interruption leaves admission closed rather
than fabricating an ACK or discarding evidence.

Before Android captures for a negotiated `generation-archive-v1` conversation,
the desktop consumer publishes
`desktop-archive-acknowledgements-v1.json`. Its strictly correlated envelope
contains at most 32 exact terminal consumed or rejected ACK documents already
retained in the desktop consumption ledger. The bounded window selects the
newest terminal evidence first, because older acknowledged generations are
normally already archived and must not starve interrupted recent
conversations. This repairs the evidence gap left by older
Android producers that advanced generation status without retaining the
received ACK. Android revalidates every document against its immutable local
generation and manifest before storing it. A consumed ACK makes its exact
generation archive-eligible; a rejected ACK closes the exact producer row but
does not make it an acknowledged archive candidate. Missing, mismatched, or
ambiguous evidence cannot advance any generation. Rejected ACKs created before
the cross-platform diagnostic canonicalization fix remain durable desktop
evidence but are omitted from the recovery envelope when Android cannot verify
their digest. The object is a bounded coordination artifact, not a new ACK
source.

If Android still cannot admit a generation, it durably publishes the
run-correlated `android-generation-error-v1.json` with
`peer_capacity_exhausted`. The desktop worker and Web surface preserve that
cause and its archive/retry action instead of converting it to a later
transport timeout.

The required artifact order is causal deletions, catalog/profile identity,
custom-equipment definitions, V4 history, aliases and occurrence equipment,
BODY ZONES, feedback, execution drafts, then optional desktop AI proposals.
All files are validated and frozen before mutation. Transaction-neutral desktop
helpers and the Android repository's outer transaction apply every business
row, causal state and the consumption/ACK record together. A late semantic
failure rolls back earlier writes and commits a separate bounded `rejected`
ACK; missing/incomplete transport input remains retryable and produces no ACK.
Within a complete causal envelope, a history occurrence and its equipment
association companion are both dominated when their owning session already has
a durable deletion state. Standalone history and association imports remain
strict and still reject unknown sessions or occurrences.

Lineage is scoped to the producer/consumer pair. Exact replay returns the
stored result; an accepted successor must name the last consumed generation as
parent. UUIDs, clocks, mtimes, and directory scans never choose a successor.
An ACK timeout leaves `waiting_acknowledgement`; a late exact ACK resolves it
idempotently. Causal V1 operation bytes remain unchanged across first emission
and retransmission because generation membership lives only in
`sync_causal_publications`.

The foreground Android coordinator correlates both durable desktop ACKs and
desktop generation references by the active run (and the Android generation
for its ACK). Objects retained from an interrupted conversation are ignored
until atomically replaced by matching objects. After both peer ACKs, Android
reports completion directly; it does not enter the legacy receipt wait state.

The peer publication precedes one fresh
`trainlog-sync-full-generation-request-v1` signal on
every explicit Android attempt. This wakes the existing user-session daemon;
it is not an ACK and does not alter generation identity. A retry may resume an
exact `captured`, `published`, or `waiting_acknowledgement` generation. It may
also finish the inbound half of an acknowledged outbound generation when no
terminal consumed/rejected inbound ledger row exists for the run. It never
recaptures state under that run identity.

The 2026-09-17 private rollout exercised this path with the production libmtp
adapter on one paired Samsung phone. Three complete bidirectional conversations
passed, including service/application restart and an idempotent replay whose
23 desktop business tables and 540 rows retained identical logical hashes.
Rejected pre-fix generations and their ACKs were retained as causal evidence.
No Drive transport was configured or claimed by this validation.

The generation MTP adapter resolves `Documents/Trainlog` through libudev and
libmtp, selects one exact advertised peer/capability set across bounded devices
and storages, and mirrors only bounded regular object names. It downloads to
private temporary files before replacement, verifies reported and received
sizes, rejects ambiguous peers, traversal, excess depth/count/bytes and
truncation, and publishes generation manifests last. Production behavior is
tested by replacing only the typed libmtp/udev I/O callbacks with a filesystem
object double; the same compiled transport algorithm remains under test.

Each worker push is additionally scoped by a private disposable outbox. The
first push contains only the correlated request; the post-import push adds the
desktop consumption ACK. Publication uses two causally ordered bounded pushes:
the immutable current desktop generation directory is committed first, with
its manifest last, and only then is its run-correlated reference made visible.
Android therefore cannot consume a reference to an in-progress MTP directory. The
durable transport root remains the owner of retained generations, staging,
legacy compatibility artifacts, and evidence, but those unrelated objects are
not recursively sent on every phase. Constructing an outbox does not delete or
rewrite any source object. A bounded adapter timeout is an ambiguous transport
failure with stable code `transport_timeout`; it is not evidence of rollback
and does not authorize automatic mutation replay.

MTP polling follows the same bounded ownership rule in the opposite direction.
It reads the peer advertisement, current Android generation reference and
current Android consumption ACK, then downloads only the generation directory
named by that validated reference. It does not recursively mirror retained
Android generations, desktop publications, or unrelated exchange history.
Local correlation is checked every 50 ms without USB work; complete MTP pulls
are throttled to a three-second cadence. Each MTP operation has its own
30-second ceiling while the conversation retains its configured global
deadline. Exhausting the global deadline while waiting for a correlated
reference reports `timeout waiting for correlated android-generation-v1.json`;
only an adapter that exceeds its independent operation budget reports
`transport_timeout`.

Android direct-file publication has a separate MediaProvider visibility
barrier. Immutable generation files, including the manifest, become visible
before `android-generation-v1.json` is committed. The reference therefore
cannot advertise a generation whose files have not completed the Android MTP
visibility callbacks. No remote object is removed during polling or scanning.

`GET /api/v1/prepared-items` is a separate read-only Web projection. It does not
change the frozen Dashboard `next_session.available` field or any exchange
format. It lists bounded durable desktop AI proposals and relevant execution
drafts as distinct kinds. Completed sessions remain outside this projection,
and finalized or causally deleted execution drafts are excluded.

## 19. Heart-rate companion V1

`trainlog-heart-rate` V1 is an optional full-generation participant produced
only by Android. Its logical name is `heart-rate` and its immutable generation
file is `heart-rate-v1.json`. Desktop never produces sensor measurements and
never connects to the heart-rate device.

A normal Android generation includes only stopped captures that have not
already been acknowledged and are not already retained by an outstanding
captured/published/waiting-ACK generation. Capture, sample and RR identities
are respectively `capture_id`, `capture_id + sequence`, and
`capture_id + sequence + rr_index`.

Desktop schema v30 validates and imports the whole companion inside the
containing generation transaction. Exact replay is idempotent; reusing one
stable measurement identity with different content rejects the generation.

Android schema v27 records the exact `capture_id -> generation_id` relation
when the immutable generation is captured. Only a correlated `consumed` ACK
sets those captures' acknowledgement timestamps. Rejected generations do not
advance cardio acknowledgement. Acknowledged captures remain in Android
history but are omitted from normal later publication, preventing a full
night's samples from being retransmitted on every sync.

The companion changes neither the frozen Bluetooth Classic RFCOMM transport,
MTP recovery, manifest/ACK framing, Sleep Diary V1 nor mobile-export semantics.
Heart-rate BLE/GATT acquisition remains Android-only and is a separate
implementation layer.

## 20. Session timeline companion V1

`trainlog-session-timeline` V1 is an optional Android→desktop full-generation
participant. Android records the real execution interval of each stable
`entry_id`; session start/end reuse the same lifecycle identity and timestamps
that become canonical completed history.

Only completed timelines are published. The desktop v31 consumer first proves
that `session_id`, `started_at`, `ended_at`, `entry_id` and `exercise_id`
already match canonical history, then inserts the occurrence timing facts.
Exact replay is a no-op and conflicting reuse of an identity is rejected.

Heart-rate samples use the same active `entry_id` at acquisition time. Samples
between exercise end and the next exercise start retain no fabricated exercise
owner and therefore remain factual rest/transition data.

## 21. Cardio sessions companion V1

`trainlog-cardio-sessions` V1 is an optional Android→desktop
full-generation participant. It carries completed sessions whose canonical
Android `session_kind` is `cardio`. These sessions are deliberately absent
from the frozen `history-v4` artifact.

The companion reuses the same bounded exercise/occurrence/set/continuous fact
shape internally but owns its own envelope and requires every public
`session_type` to be exactly `cardio`. Desktop schema v32 imports those
facts transactionally, stores legacy `session_type=training` only for
compatibility, and stores canonical `session_kind=cardio`.

Exact replay is idempotent. Reusing a cardio `session_id` that already belongs
to non-cardio canonical history is rejected. The companion is consumed before
session-timeline and heart-rate participants in the same generation, so the
stable `session_id` and occurrence facts exist before their timing and sensor
facts are applied.

Programs, manual preparations, AI execution drafts, mobile V3/V4 and
`TRAINLOG_FORMAT_V1` are not widened by this companion.

## 22. Cardio calibration companion V1

`trainlog-cardio-calibrations` V1 is an optional Android→desktop full-generation participant containing only completed calibration profiles. It references an already synchronized cardio session, its reserved stable occurrence, and an immutable stopped Heart Rate V1 capture.

Before import, desktop proves that the referenced session/occurrence and HR capture already exist and match. The declared observed peak must equal a BPM actually present in that capture, and each recovery point must match an exact raw `(observed_at, bpm)` sample. Exact replay is idempotent; stable identity reuse with different metadata or recovery facts is rejected.

Android generation membership is retained until a correlated consumed ACK. Acknowledged profiles remain local history and are not ordinarily republished. No calibration artifact claims a physiological maximum: the persisted value is the highest BPM actually observed in that calibration capture before recovery.

## 23. Cardio guidance companion V1

`trainlog-cardio-guidance` V1 is an optional Android→desktop full-generation participant containing only completed guided-cardio runs. Each run owns ordered phases, the exact target snapshot used at execution time, its exit condition and only the chronological instruction changes (`accelerate`, `maintain`, `slow_down`, `suspended`). Raw BPM/RR remain exclusively in Heart Rate V1.

A calibration-derived target carries `calibration_id`, the observed peak snapshot and the percentages that resolved the BPM bounds. Desktop requires that reference to match an already imported completed calibration. Every non-suspended instruction event carrying BPM must match an exact raw heart-rate sample for the same cardio session and timestamp. Suspended events carry no BPM and therefore cannot silently reuse a stale measurement.

Exact replay is idempotent. Reusing a stable run/phase identity with different target, condition, interval or event history is rejected. Android records exact `run_id -> generation_id` membership; only a correlated consumed ACK marks the run acknowledged and excludes it from ordinary future publication.

## 24. Legacy active-protocol limitations

The Web end-to-end tranche adds an explicit opt-in application orchestrator.
`POST /api/v1/sync` admits one correlated run and `GET /api/v1/sync/status` is
passive. The worker owns blocking peer/transport work, shares `sync.lock` with
the TUI and daemon, and publishes bounded durable phases. Android foreground
participation is required for a fresh run-correlated publication. No filesystem
mtime or old generation can satisfy that freshness boundary.

The opt-in is `TRAINLOG_SYNC_GENERATION_CONFIG`, an absolute trusted local
configuration path. It is launcher-owned and cannot be supplied through HTTP.
Without it, the staged path reports disabled and the deployed V3 path remains
unchanged.

The complete-sync semantics remain frozen separately in
[`TRAINLOG_SYNC_GAP_CONTRACT_V1`](design/sync_gap_contract_v1.md). The staged
generation slice implements its batch boundary but does not activate it.

Each mutating importer validates strictly and owns a SQLite transaction. The
V2 association companion only corroborates equipment already imported in the
mobile snapshot and refuses divergent state. A complete
definitions/mobile/body-zones/associations batch nevertheless has no common generation ID
or cross-file transaction. Independent “newest artifact” selection can
therefore observe a partially published generation; validation stops on a
mismatch, but an earlier artifact may already have committed. Replay is
idempotent and no conflicting local value is overwritten. That active legacy
design remains non-atomic. Explicit manifest V1 generation consumption
provides the separate atomic path without changing any published companion.

Snapshots carry no exercise, session, body-observation, body-zone-relation, or equipment-definition
tombstones. Omission therefore never deletes one of those objects. The only
explicit removal operation is association V2 `state: cleared`, targeted to one
`(session_id, entry_id)`.

Custom equipment is reconciled by stable `equipment_id`, never by display
name. Different custom IDs may coexist even when their names are conceptually
similar; a name-only merge could corrupt load semantics or occurrence
references and is intentionally forbidden.

The Android PC-catalog V1 reader validates the required catalog identity and
exercise-profile fields, but does not enforce an exact root/item key set as
strictly as the newer mobile V2 and equipment companions. Tightening that
published V1 reader requires a compatibility review.

The historical V1 fallback never consumes a V2 equipment companion: V1 carries
no occurrence-level equipment signal, so a neighboring V2 companion belongs to
a different generation and is ignored. This preserves rather than clears
existing equipment.

Mapped canonical exercise names are metadata owned by `exercise_id`: PC exports
them and both import paths prevent stale mapped peer labels from restoring an
old display name. This does not change TRAINLOG_FORMAT_V1.
Training observations use the independent direction-neutral
`trainlog-training-feedback-v2.json`. Its append-only revision union, strict identity
checks, ordering, bounds, and conflicts are specified in
[Training feedback](training_feedback.md).
## Exercise current-profile state v1

`trainlog-exercise-profile-state-v1.json` is a separate, directional-neutral
companion. It does not alter `TRAINLOG_FORMAT_V1`, mobile session V3, PC catalog
v1, feedback V2, equipment, or BODY ZONES. Each record carries a required
oldest-to-current `history` of at most 32 revision records. Equal revisions are
idempotent, an incoming descendant is adopted when its chain proves the local
current revision, any known incoming ancestor is retained, and sibling or
otherwise incomparable descendants conflict. A 33rd revision is rejected; the
chain is never silently truncated. The shared legacy root lets the first
post-upgrade sync repair older profile differences. Machine/science fields are
strictly typed equality guards and remain immutable through this artifact.

For an Android-triggered bidirectional run, Android builds every current
outbound snapshot before publishing any of them and publishes the complete
bundle before writing the sync request. The request therefore signals that the
canonical mobile V3, definitions, associations, aliases, zones, feedback V2,
and profile-state V1 objects have all been refreshed from the same builder
phase. The desktop receives profile state and runs its causal pre-pass before
mobile V3, then runs the strict post-pass only when that companion was actually
received. A V3/V2 same-ID profile mismatch with no received companion reports
`PROFILE_STATE_COMPANION_MISSING`; absence alone remains accepted for older
peers whose mobile snapshot does not require causal reconciliation.

The desktop pre/post-pass importer opens its database through the canonical
Python Trainlog connection factory. That factory registers the same
deterministic profile-revision function as the native database opener before
the importer prepares `UPDATE exercises`; the persisted guarded profile trigger
therefore remains valid even when the authoritative incoming tip makes its body
a no-op. Replaying the same artifact neither changes the current revision nor
adds history rows.

Desktop Python companions, including the alias importer, receive the explicit
XDG-resolved database path. Their scripts resolve from the Meson-configured
repository tools directory rather than from the installed executable's parent
layout; `TRAINLOG_TOOLS_DIR` may explicitly override that directory for a
packaged user-session deployment. Before every invocation the bounded combined
stdout/stderr result is truncated. Resolution, launch, argparse, exception,
signal, and non-zero-exit diagnostics therefore describe only the current run
and are retained in the synchronization error/history when the run fails.
