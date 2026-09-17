# Full-generation synchronization V1 private deployment record

Date: 2026-09-17

Status: `TRAINLOG_SYNC_FINALIZATION_AND_ROLLOUT_V1=PASS`

This record deliberately omits phone serials, peer IDs, generation IDs,
credential locations, user content, and private backup hashes. Complete causal
evidence is retained in the private rollout journal.

## Installed set

- Trainlog 0.1.2 development; desktop schema v21 and Android schema v20.
- Versioned relocatable desktop bundle behind stable user-bin symlinks.
- Android release-variant APK installed only through compatible signed updates;
  the final private rollout version code is 6.
- Production direct-libmtp generation adapter with one explicitly paired peer.
- Loopback Web service and the user-session request daemon are active from the
  same versioned installation. No live-checkout runtime helper is used.

## Data and migration evidence

- The original Android schema-v17 store was backed up through the signed bridge
  UI and verified for manifest, digests, SQLite integrity, foreign keys and an
  exact semantic match to the pre-update store. The real archive migrated on a
  copy before live installation.
- A post-migration Android backup was also verified. The real source contained
  no active draft, so draft restoration was correctly not applicable; no
  synthetic draft was inserted.
- The desktop schema-v18 store was copied with SQLite's backup API, validated,
  and migrated on an isolated copy before the live schema-v21 open. Historical
  table values were preserved.
- Backups, failed generations, rejection ACKs and successful generation/ACK
  evidence remain retained. No one-sided rollback was performed after causal
  writes began.

## Operational evidence

- The actual foreground Android advertisement exposed the required persistent
  identity and all five capabilities.
- Production libudev/libmtp discovery and object transfer completed on the
  selected unlocked phone.
- Three final bidirectional conversations completed with a durable consumed ACK
  on each peer and all eight required domains reported successful.
- A service/application restart and a second non-destructive exchange produced
  no duplicate business data. Logical hashes for 23 desktop business tables
  and 540 rows were identical before and after replay; integrity and foreign-key
  checks passed.
- Android visibly returned to an enabled action and displayed synchronization
  completion for both Android and PC.

## Rollout repairs

The real rollout exposed and closed four bounded defects: feedback idempotence
depended on the SQLite row representation; the desktop package omitted several
runtime exporters; Android accepted stale coordination filenames without
run/generation correlation; and its UI waited for a legacy receipt after the
generation coordinator had already completed. Regression tests cover each
affected boundary.

## Remaining limits

The phone must be connected, unlocked and in the foreground synchronization
flow. Retained outgoing generations remain subject to the explicit capacity of
eight per peer; evidence is never silently evicted. Drive was not configured or
tested. This private deployment does not publish 0.1.2, create a tag or release,
or alter the existing v0.1.1 release.
