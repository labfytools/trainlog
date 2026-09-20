# Web Exercises V1

`TRAINLOG_WEB_EXERCISES_V1=PASS` for Trainlog 0.1.3 development. The
loopback-only `/exercices` route is the desktop exercise-catalogue
administration surface. Android remains the field capture client and receives
catalogue changes only through a later normal synchronization.

## Ownership and history

Trainlog Core and the canonical desktop SQLite database own catalogue truth.
React never generates an `exercise_id`, normalizes a name as business truth, or
writes SQL. Creation generates the existing `ex_<uuid-v4>` identity in Core;
updates preserve that identity.

Changing name, profile, fields, or BODY ZONES affects future uses only.
Occurrence-owned recording/tracking/data-field snapshots, actual sets or
continuous activity, targets, equipment, feedback and MAX results in past
sessions are immutable under catalogue editing. No compatibility operation
silently rewrites history or existing drafts.

## Read and command surface

The versioned local API provides bounded catalogue pages, detail, assignable
BODY ZONES, creation, update, and retirement. Search uses Core canonical name
normalization. Profile and BODY ZONES validation remains authoritative in
Core: `CONTINUOUS + REPS`, unknown data-field bits, group zones, duplicate
roles, secondary-only assignment, and interactive SETS creation without a
primary zone are rejected.

Detail exposes the persisted profile, stable ID, zones, a deterministic
revision token, whether causal retirement is allowed, and resolved equipment
associations. Equipment is read-only in V1.

All mutations retain the Web loopback, trusted-Origin, CSRF, exact JSON content
type, bounded body, request-ID and strict-JSON protections. PUT and DELETE
require the current quoted revision. The token covers name, profile,
`data_fields`, zones and causal state under one immediate transaction; stale
writes fail rather than overwrite concurrent changes.

"Delete" is causal retirement, never physical exercise deletion. Built-in
Trainlog identities are protected. A retired custom exercise is excluded from
new-work catalogue reads and the next PC catalogue export, while its source row
and all historical references remain. Existing Programs, preparations,
proposals and drafts are not silently changed; their occurrence/planning
snapshots remain readable, and new-work selectors apply the established active
catalogue rule.

## Synchronization boundary

Create, update and retirement first commit only to the desktop database. They
do not call synchronization and do not require a phone. The next user-triggered
normal synchronization reuses the existing PC catalogue, exercise profile
state, BODY ZONES, equipment companions and causal deletion artifacts through
the existing USB-priority engine with Drive as mirror/fallback. No Web
Exercises artifact or protocol version was introduced.
