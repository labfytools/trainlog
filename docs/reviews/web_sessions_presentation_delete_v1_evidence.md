# Web Sessions presentation and preparation withdrawal V1 evidence

Date: 2026-09-18

This record covers the corrective presentation tranche, the durable
preparation-withdrawal implementation, and its controlled private deployment.
It does not open the Exercises route, publish a release, or change
`TRAINLOG_FORMAT_V1`.

## Delivered behavior

- Preparation merges manual preparations and AI proposals in descending valid
  planned-date order, followed by factual sort timestamp, kind and stable
  identity. Resume uses Core-owned draft recency; History uses the real session
  start instant. Undated or invalidly dated records remain visible after valid
  records.
- French civil dates and 24-hour times are the default. A persisted local Web
  preference selects ISO dates without entering business SQLite or
  synchronization artifacts. Civil dates are never timezone-shifted.
- Contextual French state labels preserve their distinct stored enums.
  Proposal provenance comes from the persisted source identity and title; no
  title heuristic is used.
- **Delete preparation** performs a revision-guarded, idempotent logical
  withdrawal. Desktop schema v24 retains revisions, deliveries and one
  immutable withdrawal. `trainlog-session-preparations` V2 transports the
  operation; Android schema v23 cancels only pending deliveries and permanently
  rejects older delivery replay.

## Controlled deployment

The desktop bundle installed from commit `4573fe9a4240b7ff233da29c1facd5666462659a`
contains one hashed 47-file inventory and coordinated Core, Web, sync daemon,
generation adapter and Python helpers. It was tested outside the checkout with
private HOME/XDG roots before installation. The prior versioned installation
was retained.

The desktop database was backed up consistently at schema v23, passed integrity
and foreign-key checks, and migrated on a copy before the live v24 migration.
All 39 historical tables and 784 rows compared exactly on the migration copy.
The Android `.tlbackup` archive was verified by the production backup service;
restoration of a test-owned copy preserved every historical value while
migrating schema v22 to v23.

The installed Android package advanced from private `versionCode` 13 to 14
with the exact certificate identity already installed. That existing private
identity is an Android Debug certificate despite older deployment prose that
described it as a release identity. A separately built APK with a different
release certificate was retained as evidence and was not installed. No
uninstall, data clear, downgrade or package-identity change occurred.

## Requested real withdrawal

Preflight resolved the exact target by stable identity, not title:

- preparation: `sp_0b6e748d-5acd-4b3a-b17b-90bf513fe3c1`;
- current revision: `spr_6aebc873-5214-4c37-914f-c8ac35aad2d6`;
- delivery: `spd_d0ddf01a-aa9c-4667-9fde-7804afc9c826`;
- source proposal retained: `aid_4b14b098-a567-4631-82af-cfb7dfb7b960`.

The Android delivery had changed since the earlier report: it was already
`started`. The protected Core/API command therefore created exactly one
withdrawal, removed the preparation from active desktop readers, and requested
Android propagation without deleting the delivery or any execution identity.
Android consumed that exact operation as `execution_preserved`. The linked
delivery remained `started`; no completed session exists under its reserved
session identity and no active draft or workout fact was removed.

The first full-generation attempt exposed a retained pre-upgrade peer
advertisement in the desktop transport cache. It failed before generation
consumption. The worker was corrected and regression-tested to require a fresh
successful MTP pull before peer validation. The retained failed run was not
deleted. The retry completed with a correlated Android ACK, and a second full
generation after Web, daemon and application restart also completed with zero
sessions reconciled.

After replay:

- desktop and Android each retain exactly one withdrawal with the same stable
  identity and `execution_preserved` result;
- the active Web preparation list is empty and its deep link reports
  `withdrawn` with the Android acknowledgement;
- the Android Sessions reader exposes neither the target nor a start action;
- the linked delivery remains `started` and its payload is unchanged;
- workout, draft, execution, proposal and delivery table groups on Android are
  byte-for-byte logically unchanged across the second exchange;
- desktop workout, preparation-content and delivery table groups match their
  pre-withdrawal digests;
- the source proposal keeps its ID, `archived` state, entries and
  `published_at`. The legacy V3 inbox re-copied the same Drive archive and
  refreshed only its operational `archived_at` timestamp, as designed by that
  existing helper.

No generation, archive, causal proof, old installation or backup was removed.

## Validation evidence

- Web TypeScript typecheck, production build, and 91 tests in 14 files passed.
- Civil-date tests passed under `Pacific/Honolulu` and `Pacific/Kiritimati`.
- The strict GCC Meson suite passed 86/86 tests.
- A strict Clang 22 ASan/UBSan build passed the nine directly affected native
  tests, including the C17 public-header syntax check.
- Android debug assembly and unit tests passed: 226 tests, zero failures or
  errors, with five optional fixture skips.
- JSON, import-contract and training-knowledge validators passed.
- Firefox exercised the production embedded C server and Core on a synthetic
  database, including a real delete click, V2 export, desktop layout, and an
  exact 390×844 viewport. The mobile footer overlap discovered by that run was
  fixed and the browser test then passed.
- The rollout-specific stale-peer regression and the generation/deployment
  suites passed before the corrected package was installed.

## Captures

- [Sessions list, desktop](evidence/web-sessions-list-desktop.png)
- [Preparation editor, 390×844](evidence/web-sessions-editor-mobile.png)
- [Preparation detail, desktop](evidence/web-sessions-detail-desktop.png)
- [Withdrawal confirmation, 390×844](evidence/web-sessions-delete-confirmation-mobile.png)
- [Withdrawn preparation detail, desktop](evidence/web-sessions-withdrawn-detail-desktop.png)

The captures use isolated synthetic data. The real withdrawal evidence is the
correlated persistent desktop/Android state described above; no private device
identifier, peer identifier, user-data payload or signing digest is committed.
