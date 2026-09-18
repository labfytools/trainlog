# Trainlog Program format V1

`trainlog-program` V1 is a strict desktop import format for planning data. It
does not change `TRAINLOG_FORMAT_V1` and is not an Android synchronization
artifact.

## Identity and ownership

A program, each ordered session definition and each ordered exercise occurrence
has a non-empty stable import identity of at most 128 UTF-8 bytes. The `pg_`,
`pgs_`, and `pge_` UUIDv4 forms are recommended creator forms, while imported
identities remain opaque. Display titles are not identities. A program is not a
reusable session template, AI proposal, manual preparation, execution draft or
completed session.

Desktop schema v25 owns the imported program and its exact source SHA-256.
Exact same-ID content replay is idempotent. The same ID with a different byte
payload is an explicit conflict. A different ID may reuse a title because titles
have no identity semantics.

## Document shape

The root has exactly `format`, `version`, and `program`, with format
`trainlog-program` and integer version `1`. The program object has exactly:

- `program_id`, `title`, nullable `note`, `state`, nullable `start_date`,
  nullable `end_date`, and `sessions`;
- state `active` on import;
- 1–64 ordered sessions;
- title length 1–200 UTF-8 bytes and note length at most 4,000;
- canonical `YYYY-MM-DD` civil dates, with the end not before the start.

Each session has exactly `program_session_id`, `title`, `session_type`, nullable
`planned_for`, nullable `note`, and `occurrences`. Session type is `training` or
`max_test`; each session contains 1–64 occurrences.

Each occurrence has exactly `entry_id`, `exercise_id`, nullable `equipment_id`,
`load_mode`, `rest_seconds`, nullable `target_sets`, nullable `target_reps`,
nullable `target_duration_seconds`, nullable `target_weight_kg`, and nullable
`notes`. Exercise and equipment identities must already resolve in the desktop
catalog. Recording/tracking profiles and target/load combinations are validated
by the same Core rules used for manual preparations. No actual set, MAX result,
completion timestamp or other performed field is accepted.

The complete UTF-8 JSON document is limited to 512 KiB. Duplicate object keys,
unknown keys, non-finite numbers, invalid UTF-8, duplicate identities, excessive
collections and incompatible profiles are rejected before any row is written.

## Preview, commit and lifecycle

The Web file picker sends bytes to the Core validation endpoint first. Its
preview reports identity, title, dates, session count, unknown-exercise count
and warnings while leaving the database unchanged. Import occurs only after the
user confirms and commits all program/session/occurrence/source rows in one
transaction.

Archive is a revision-guarded lifecycle transition. It hides no derived data
and retains the source definition and digest. Creating a preparation from one
session is a separate idempotent command that copies the ordered plan and stores
program/session provenance on a new `sp_<uuid-v4>` manual preparation. It does
not create a delivery, Android object, execution draft, completed session or
performed measurement.
