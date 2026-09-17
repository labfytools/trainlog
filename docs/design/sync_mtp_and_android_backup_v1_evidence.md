# Sync MTP and Android backup V1 evidence

Status: **SOFTWARE COMPLETE — hardware and user-operation gates pending**.

The production C adapter resolves the direct-MTP object tree and is selected
only by trusted `mode: mtp` configuration. Unit tests substitute its typed
libudev/libmtp callbacks, not its selection, bounds, download, retry or
manifest-last publication logic. The integrated headless-Firefox proof uses the
same algorithm over an object-store double and completes the real Android and
desktop generation/ACK conversation.

Android creates a SQLite-consistent, versioned plaintext archive containing the
database and owned preferences, verifies exact entries, bounds, hashes,
integrity, foreign keys, package and schema before replacement, and retains the
old database/preferences until restore verification succeeds. The pinned
schema-v17 bridge candidate builds without installation, passes its v17
round-trip test, and produces an archive that current Android restores and
migrates to schema v20.

Software evidence includes the Meson transport test, orchestrator process and
configuration tests, Android backup tests, bridge/current migration chain,
Android debug assembly, full isolated validation harness, and candidate
inventory verification. Physical MTP with an unlocked phone, release-signature
continuity, bridge/current APK installation, real user backup/restore, service
changes, main integration and deployment were not performed and require their
separate approvals.
