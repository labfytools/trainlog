# Bluetooth synchronization transport V1

## Status

`TRAINLOG_SYNC_BLUETOOTH_TRANSPORT_V1=LIVE_DEVICE_PASS_DRIVE_SETUP_PENDING`

This transport is additive. It does not change generation manifests, artifact
formats, causal deletion, revision ancestry, acknowledgement semantics, SQLite
schemas, or the frozen Trainlog session format.

## Product behavior

Normal synchronization is automatic when the authorized Android phone and
desktop are within Bluetooth range.

1. Android's already user-enabled sync foreground service maintains a bonded
   Bluetooth Classic RFCOMM connection to the selected desktop.
2. The desktop advertises one Trainlog-specific RFCOMM profile.
3. On a fresh Bluetooth connection Android publishes one bounded
   full-generation request. Reconnect noise is rate-limited.
4. The existing full-generation orchestrator performs the same bidirectional
   generation/ACK conversation through the Bluetooth byte transport.
5. If Bluetooth is unavailable, the existing MTP transport remains the wired
   fallback.
6. Google Drive is not a synchronization transport in the target configuration.
   After convergence, the desktop AI export is transferred to Android and
   Android publishes it to the user-selected `Trainlog/AI` Drive folder.
   Drive publication failure never rolls back a completed sync.

The manual Web and Android Synchroniser actions remain as recovery/forced-sync
controls.

## Private live-device evidence — 2026-09-22

The authorized Samsung Android peer and ArchASP completed a physical Bluetooth
Classic/RFCOMM synchronization without MTP or Drive as a sync transport.

- request: `sr_86ccc07f-cc5b-4506-86c1-06c00bd8b06a`;
- run: `sy_df8f87fb-e4c1-44f5-b3f7-8fe87942d19e`;
- inbound Android generation:
  `gen_9609d163-4a3a-4187-a806-84379a1bcf4c`;
- outbound desktop generation:
  `gen_bc1d5e4b-4b16-49bc-b228-c3ff1f1a1705`;
- final transport: `bt`;
- final Bluetooth state: `success`;
- final phase/result: `completed/completed`;
- desktop SQLite integrity: `ok`;
- post-sync AI export: `delivered_to_android`;
- Android copy:
  `Documents/Trainlog/trainlog_ai_export_v1.json`, 66,971 bytes,
  SHA-256 `49f7f02fdeebd15699655e43fdb475bb20d9dd2d1bc59d2bdb4cd099ae50ecda`.

The initial legacy bond contained mismatched link-key state. The private device
was explicitly re-paired; Android then reported the bond ready and established
the Trainlog RFCOMM service. Android now also repairs a missing bond through the
normal platform pairing flow before attempting RFCOMM.

The one remaining manual setup step is selecting the Android SAF destination
`Trainlog/AI`. This is intentionally user-authorized provider access and is
not bypassed by ADB or Trainlog.

## Trust and pairing

Bluetooth device names are presentation only and are never authority.

Desktop trusted configuration pins:

- Android sync `peer_id`;
- bonded Android Bluetooth device address;
- Trainlog RFCOMM service UUID.

Android persists the selected bonded desktop address after an explicit one-time
selection. The RFCOMM service uses normal bonded-device authentication.

Current private deployment values are installation configuration, not protocol
constants.

## RFCOMM service

Trainlog service UUID:

`f0d1c0de-7a11-4f62-9b7c-545241494e4c`

The desktop registers an `org.bluez.Profile1` server with BlueZ. Android is
the RFCOMM client. This direction lets Android resolve the advertised service
through SDP without hidden Android channel APIs.

The desktop Bluetooth agent is persistent and owns the BlueZ profile. Local
sync workers never talk to BlueZ directly; a bounded local adapter talks to the
agent over a mode-0600 Unix socket.

## Bluetooth byte envelope

Each Bluetooth frame is:

- 4-byte unsigned big-endian JSON header length;
- UTF-8 JSON header (maximum 64 KiB);
- optional payload whose length and SHA-256 are declared in the header.

Payload limit is 256 MiB and individual exchange files remain subject to their
existing generation bounds. A receiver validates length and SHA-256 before
making received bytes visible.

Initial frame:

```json
{
  "format": "trainlog-bt-frame",
  "version": 1,
  "type": "hello",
  "peer_id": "peer_<uuid>",
  "protocol": "trainlog-bt-files-v1"
}
```

The local adapter exposes only `pull` and `push`, matching the existing MTP
adapter boundary. Transfers use a bounded ZIP archive containing relative
exchange paths. Extraction rejects absolute paths, `..`, symbolic links,
duplicates and excess file/byte counts. Each destination is replaced from a
same-directory temporary after complete bytes are durable.

## Pull ownership

Android pull snapshots include only Android-owned/current coordination objects:

- `android-peer-v1.json`;
- `trainlog-sync-full-generation-request-v1.json`;
- `trainlog-sync-request-v1.json`;
- `android-generation-v1.json`;
- `android-consumption-ack-v1.json`;
- `android-generation-error-v1.json`;
- the immutable generation referenced by `android-generation-v1.json`.

Desktop-owned coordination files are never treated as Android source
authority.

## Push ownership

Desktop workers already construct phase-specific disposable outboxes. Bluetooth
push accepts exactly that bounded outbox and atomically publishes those files
under Android's canonical `Documents/Trainlog` directory.

## Concurrency

Bluetooth and MTP have independent physical transport locks. The common
business `sync.lock` still serializes synchronization conversations.

A background discovery probe must never own `sync.lock`. A Web-triggered
conversation must never fail merely because an idle transport probe is in
progress.

## Automatic arrival trigger

A successful RFCOMM connection is an arrival event. Android publishes at most
one automatic request per connection and persists a short reconnect cooldown.
The same connection is not a repeating timer; no connection means no automatic
request.

A later optimization may compare explicit dirty revisions before generating a
request, but V1 correctness does not depend on such an optimization.

## MTP fallback

MTP remains supported and keeps its current fail-closed semantics. Automatic
desktop selection is:

`Bluetooth -> MTP -> device_unavailable`

Drive is never an implicit fallback in the target configuration.

## AI export publication

The desktop remains the canonical producer of
`trainlog_ai_export_v1.json`. After a completed bidirectional sync it sends
the exact export bytes to Android. Android publishes/replaces the export in the
persisted SAF tree selected for `Trainlog/AI`.

The export has its own pending/success state. A Drive provider error is not a
sync error and is retried later by Android.
