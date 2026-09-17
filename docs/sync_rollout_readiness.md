# Full-generation sync rollout readiness

## Deployment-readiness candidate

The generation path remains disabled unless desktop configuration authorizes a
specific persisted Android peer and Android contains the strict opt-in document
`Documents/Trainlog/trainlog-sync-generation-opt-in-v1.json`. Absence preserves
V3; malformed opt-in fails explicitly. The Web coordinator owns a fixed worker,
bounded typed progress and its descendant process group. The foreground Android
coordinator advertises its real peer identity, publishes manifest-last bytes,
consumes the return generation and emits a durable correlated ACK.

The integrated proof uses the production generation adapter with only its
typed libudev/libmtp I/O callbacks replaced by an object-store double. This is
software evidence for selection, bounds, publication ordering and the complete
conversation, but not evidence for a particular phone's physical libmtp
behavior. Generation mode must remain disabled until that hardware gate and
the separately authorized rollout are complete.

### Candidate, backup and recovery

1. Build and validate one revision, then run `python3
   tools/package_sync_candidate.py --output <new-private-directory>`. Its
   inventory records hashes, schemas, protocols, entry points and dependencies.
   The relocatable bundle includes `trainlog`, `trainlog-sync-once`,
   `trainlog-syncd`, the production generation MTP adapter and their matching
   Python codecs. User units must point only into one versioned bundle; they
   must not combine a packaged binary with helpers from a live checkout.
2. `assembleDebug` is development evidence only. Rollout requires the existing
   release signing identity, verification of its public certificate and a valid
   increasing `versionCode`. A bounded `TRAINLOG_ANDROID_VERSION_CODE` build
   override may establish an update-only private rollout chain without changing
   the product version, schema or protocol. The bridge recipe also produces a
   signed release variant for installation; debug APKs remain test evidence.
3. Quiesce old writers only during an authorized rollout. Desktop backup uses
   `backup_trainlog_sqlite.py` and SQLite's backup API, then integrity and
   foreign-key checks.
4. V3 is not an Android backup. Current Android has a complete verified backup
   container. For the installed schema-v17 release, build the pinned bridge
   candidate, verify signing/version continuity separately, install only with
   explicit approval, create and verify the user backup, and retain it before
   installing current Android. None of those device/user operations occurred in
   this software mission.
5. Replacing a binary does not roll back a schema. Before any exchange, a
   mutually compatible captured pair may be restored. After causal writes or
   ACKs, never restore only one peer: stop synchronization, preserve both stores
   and all artifacts, and use an explicit paired recovery plan.
6. The eight-generation-per-peer ceiling is an admission gate. Exhaustion must
   preserve pending generations, tombstones, finalizations and ACK ledgers; it
   must never trigger hidden eviction.

Actual rollout needs one separate approval for the signed APK, desktop bundle,
verified user backups, coordinated service quiescence, pairing, synthetic smoke
and non-destructive hardware smoke. Drive remains separate and untested here.

This is a non-executed rollout checklist. The development branch implements the
path; the daily installation still uses V3.

The latest software-only candidate set was rebuilt under the private root
`/home/fy59/.cache/trainlog/sync-rollout-candidate-v1.NEoMDx` from source commit
`dd8c91a946a9c836965818d8cc35b8f8bce74658`. Its desktop inventory records every
packaged file digest plus schemas, protocols, entry points and runtime
dependencies; the inventory file SHA-256 is
`800034b409824a68cef45e5251dd4d94f00bd9e13b3387a1a302b8e2e1136839`. The current
debug APK SHA-256 is
`aca2f499ee9ff53ba5913a76a2a0117274e3e5545967ce361f6f126a6af497e1`; the
non-installed bridge APK SHA-256 is
`0a28d81cbb0818cdf2fee72d5024a4b706fcfda39f1182c682c12f000a026cb4`; and its
synthetic schema-v17 backup SHA-256 is
`06eb3999c6f3ce2fd8f551b38727a0a67fe6b48dde4c3244b3d50357afa7e6a3`.
These are unsigned development evidence, not deployment authorization.

## Compatible set

- desktop binary containing schema v21, the Web API and the generation worker;
- `sync_generation_exchange.py` and all matching transaction-neutral codecs;
- Android schema v20 with `SyncGenerationService`;
- peers advertising manifest V1, ACK V1, causal deletion V1, history V4 and
  execution draft V1 under their stable `peer_<uuid-v4>` identities;
- a trusted local orchestrator configuration referencing the packaged peer/MTP
  helper with fixed arguments.

## Controlled sequence

1. Back up both SQLite stores independently and record their schema versions.
2. Stop the user services only during the separately authorized deployment.
3. Install the mutually compatible desktop helper, binary and Android APK.
4. Migrate synthetic copies first; migration rollback means restoring a backup,
   not merely replacing the binary.
5. Verify peer identities and advertised capabilities without pairing by name.
6. Perform one non-destructive foreground Android publication and desktop run.
7. Verify both manifest digests, both durable ACK records, lineage and readers.
8. Keep V3 selectable until the controlled full-generation smoke is accepted.

## Recovery limits

Do not delete an unacknowledged generation, the last acknowledged generation,
its previous valid generation, or causal evidence. An ACK timeout is unresolved,
not rollback. Eight retained outgoing generations exhaust admission and require
operator diagnosis; this tranche adds no tombstone or evidence garbage
collection. A migrated database is recovered from its verified backup, never by
assuming an older binary reverses its migration.

Physical MTP, actual Drive configuration, service coordination, installed APK,
main merge and deployment are intentionally not performed here.
