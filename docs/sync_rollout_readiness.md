# Full-generation sync rollout readiness

## Deployment-readiness candidate

The generation path remains disabled unless desktop configuration authorizes a
specific persisted Android peer and Android contains the strict opt-in document
`Documents/Trainlog/trainlog-sync-generation-opt-in-v1.json`. Absence preserves
V3; malformed opt-in fails explicitly. The Web coordinator owns a fixed worker,
bounded typed progress and its descendant process group. The foreground Android
coordinator advertises its real peer identity, publishes manifest-last bytes,
consumes the return generation and emits a durable correlated ACK.

The integrated proof uses a directory object-I/O double. It is not evidence for
physical libmtp behavior. The shipped physical generation adapter is not yet
complete, so generation mode must not be enabled on services.

### Candidate, backup and recovery

1. Build and validate one revision, then run `python3
   tools/package_sync_candidate.py --output <new-private-directory>`. Its
   inventory records hashes, schemas, protocols, entry points and dependencies.
2. `assembleDebug` is development evidence only. Rollout requires the existing
   release signing identity, verification of its public certificate and a valid
   increasing `versionCode`; this mission reads no signing secret.
3. Quiesce old writers only during an authorized rollout. Desktop backup uses
   `backup_trainlog_sqlite.py` and SQLite's backup API, then integrity and
   foreign-key checks.
4. V3 is not an Android backup and omits active-draft and causal state. No safe
   backup path for the installed release was demonstrated without device access.
   This is a hard gate: an authorized procedure must capture the app database,
   WAL-consistent state and active draft before update.
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
