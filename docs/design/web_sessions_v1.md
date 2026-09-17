# Trainlog Web Sessions V1 contract

Status: implementation contract for `TRAINLOG_WEB_SESSIONS_V1`.

## Scope and domain boundaries

The Sessions route has three independent views:

- Preparation lists manual preparations and immutable imported AI proposals.
- Resume lists lifecycle-backed execution drafts that are not finalized or causally deleted.
- History lists completed sessions. A missing `ended_at` is rendered as an unknown end time; it
  does not make a historical row an active draft.

An AI proposal is never mutated by viewing it or by deriving a manual preparation. Derivation is
an explicit command which creates a new `sp_<uuid-v4>` preparation and records the proposal ID and
payload fingerprint used. A manual preparation is not an execution draft or completed session.

## Manual preparation persistence

Desktop schema v23 adds:

- `session_preparations`, the mutable head and delivery state;
- `session_preparation_revisions`, immutable revision metadata;
- `session_preparation_entries`, immutable ordered occurrence snapshots;
- `session_preparation_requests`, durable HTTP idempotency results;
- `session_preparation_deliveries`, the preparation-revision to execution identity relation.

Preparation IDs use `sp_<uuid-v4>`, occurrence IDs use `spe_<uuid-v4>`, revision IDs use
`spr_<uuid-v4>`, and execution IDs use the existing `se_<uuid-v4>` namespace. Revision content is
immutable. Updating a preparation inserts a complete successor revision and atomically advances
the head. Occurrence IDs survive reordering and compatible edits. Two occurrences of one exercise
remain distinct.

`editing_state` (`draft` or `ready`) and `delivery_state` (`local`, `pending`, `acknowledged`,
`remote_unknown`, or `cancelled`) are independent. Marking a complete revision ready changes no
Android state. Preparing it for Android creates one durable delivery and deterministic execution
identity per explicit command. Retrying the command with its idempotency key returns that same
identity. Later preparation edits never alter a published revision or execution.

## Validation

Titles and notes are UTF-8 and bounded by the existing name/note limits. A preparation may be
saved with no occurrences while it is a draft. Ready revisions require a non-empty title and at
least one occurrence. Every occurrence resolves a non-retired exercise and a compatible equipment
choice at commit time. `SETS/REPS`, `SETS/DURATION`, and `CONTINUOUS/DURATION` are the only legal
profiles. Continuous work has a duration target and never a target set. Assistance, external load,
and no load remain distinct. JSON `null`, zero, and absence retain distinct meanings; non-finite
numbers are rejected by the JSON parser and Core validation.

Every update supplies the expected revision and is checked in the write transaction. A mismatch
returns a conflict without partial mutation. Request idempotency is persistent, so a lost HTTP
response can be replayed without creating another preparation.

## Read API

Lists use bounded offset pages with deterministic stable-ID tie breaking. The response returns a
snapshot revision. If a write changes that revision between page requests, the client discards its
accumulated pages and explicitly reloads. Search matches the title or an exercise display name.
Dates mean planned calendar dates for preparations and occurrence dates for history. Zones are
offered only where the persisted exercise relations provide them.

Stable details are addressed by kind and identity under `/api/v1/sessions/...`. Unknown API routes
remain JSON 404 responses; recognized non-API paths use the embedded SPA fallback.

## Synchronization and Android

Ready deliveries use a separate `trainlog-session-preparations` V1 generation participant. It does
not change `TRAINLOG_FORMAT_V1`, mobile V3/V4, or execution-drafts V1. The artifact contains complete
immutable revision snapshots and explicit cancellation state. Android consumes a delivery into a
pending prepared-session store. Starting it is an explicit Android action: an occupied active-draft
singleton is never overwritten. Consumption and start are idempotent by delivery, preparation,
revision, execution, and occurrence IDs.

Delivery is shown as acknowledged only when the correlated generation ACK proves that the exact
artifact containing that revision was consumed. Publication time or a global successful sync is
not sufficient evidence. Old peers which do not advertise this participant are rejected before a
generation containing it is published.

## Explicit exclusions

This V1 does not implement Programs, Analysis, or the standalone Exercises route. It does not add
live Web workout capture, historical correction, automatic training prescriptions, proposal
acceptance, or automatic proposal-to-execution conversion.
