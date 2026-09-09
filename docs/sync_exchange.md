# Synchronization exchange

## 1. Status

```text
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
BODY_ZONE_SYNC_V1=PASS
BODY_ZONE_SYNC_V1_LIVE_DEVICE=PASS

TRAINLOG_FORMAT_V1=FROZEN_UNCHANGED
```

Synchronization artifacts are separate from the frozen Trainlog session JSON
v1 format.

## 2. Exchange directory

Canonical Android shared-storage directory:

```text
Download/Trainlog
```

Desktop accesses this directory through direct MTP.

Android accesses PC-created artifacts through a persistent Storage Access
Framework folder grant.

## 3. Artifact table

| Direction | File | Format |
| --- | --- | --- |
| Android -> PC | `trainlog-mobile-export-v1.json` | `trainlog-mobile-export` v1 |
| Android -> PC | `trainlog-mobile-export-v2.json` | `trainlog-mobile-export` v2 (active) |
| Android -> PC | `trainlog-mobile-equipment-definitions-v1.json` | `trainlog-equipment-definitions` v1 |
| Android -> PC | `trainlog-equipment-associations-v2.json` | `trainlog-equipment-associations` v2 |
| Android -> PC | `trainlog-exercise-body-zones-v1.json` | `trainlog-exercise-body-zones` v1 |
| PC -> Android | `trainlog-pc-catalog-v1.json` | `trainlog-pc-catalog` v1 |
| PC -> Android | `trainlog-pc-equipment-definitions-v1.json` | `trainlog-equipment-definitions` v1 |
| PC -> Android | `trainlog-pc-mobile-export-v2.json` | `trainlog-mobile-export` v2 |
| PC -> Android | `trainlog-equipment-associations-v2.json` | `trainlog-equipment-associations` v2 |
| PC -> Android | `trainlog-exercise-body-zones-v1.json` | `trainlog-exercise-body-zones` v1 |
| Android -> PC agent | `trainlog-sync-request-v1.json` | `trainlog-sync-request` v1 |
| PC agent -> Android | `trainlog-sync-receipt-v1.json` | `trainlog-sync-receipt` v1 |

No SQLite file is transferred.

Android scoped storage can preserve a prior MTP-created object and create a
new artifact with the provider collision suffix, for example
`trainlog-mobile-export-v2 (N).json` or
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
`Download/Trainlog`.

V2 is a separate format: every session entry has an `entry_id`, `position`,
metrics, actual loads and optional equipment identity. This permits two
occurrences of the same exercise without fusion. V1 remains readable with its
frozen contract. A V1 historical session is reconciled with V2 only when the
exercise/order correspondence is unambiguous; otherwise the importer reports
a conflict rather than silently overwriting data.

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

## 5. Android -> PC mobile snapshot

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

Android captures its custom-definition V1 companion, this V2 snapshot, body
zones, and the equipment-association companion before it publishes any of
them. It then publishes in that order: definitions, V2 snapshot, body zones,
associations. A malformed
persisted custom definition aborts publication before a V2 file can advertise
its reference; bundled manifest equipment is never copied into definitions V1.

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

### Exercise identity reconciliation

The active V2/catalog path may reconcile distinct `exercise_id` values sharing
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
-> receipt
```

The engine has three explicit modes:

```text
a   Android -> PC: definition V1 -> mobile V2 -> body zones V1 -> association V2; no publish
p   PC -> Android: definition V1 -> catalog V1 -> body zones V1 -> mobile V2
    (including bodies) -> association V2; no receive
b   bidirectional: complete inbound sequence, then complete outbound sequence
```

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
for an intentional removal. In the current V2 flow, the mobile snapshot already
carries the occurrence equipment value and the companion validates it. A
missing companion conveys no equipment information and cannot clear a
previously known choice. Unknown canonical IDs, unknown entries, ambiguous
identities and divergent values reject processing explicitly; an unknown
equipment reference is never silently changed to null.

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

## 18. Audited protocol limitations

Each mutating importer validates strictly and owns a SQLite transaction. The
V2 association companion only corroborates equipment already imported in the
mobile snapshot and refuses divergent state. A complete
definitions/mobile/body-zones/associations batch nevertheless has no common generation ID
or cross-file transaction. Independent “newest artifact” selection can
therefore observe a partially published generation; validation stops on a
mismatch, but an earlier artifact may already have committed. Replay is
idempotent and no conflicting local value is overwritten. A future atomic-batch
design requires a new versioned manifest rather than a semantic change to any
published format.

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
