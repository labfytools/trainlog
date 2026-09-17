# Full-generation sync rollout readiness

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
