# Web Sessions V1

Preparation list rows expose independent `editing_state` and `delivery_state`.
A local draft action saves one new ready revision on the same preparation and
then creates its delivery; a local ready revision is delivered directly.
Pending, acknowledged and remote-unknown preparations never create a second
delivery. The action is disabled while its request is running and every state
has visible text in addition to color.

Successful delivery admission immediately starts the configured common sync
orchestrator. If transport is unavailable, the durable pending delivery remains
retryable; the preparation is not reverted or recreated. When completed
history contains the delivery's exact `execution_session_id`, the preparation
leaves the active list while all preparation revisions, entries and delivery
provenance remain stored. Program completion continues to derive from the Core
`execution_state`, never from list disappearance.

The Sessions route implements four distinct views:

- Preparation: manual preparations and immutable AI proposals;
- Resume: lifecycle-backed execution drafts, continued only on Android;
- History: completed sessions, including explicit unknown end times.
- Programs: durable desktop planning definitions, import and explicit manual
  preparation creation.

Manual preparation supports a title, session type, optional calendar date,
note, catalogue selection, duplicate occurrences, keyboard reordering,
equipment/load semantics and profile-compatible targets. Draft saves may remain
incomplete. Marking ready validates executable content; preparing for Android
then creates a distinct immutable delivery and reserved execution identity.

Every view has one stable descending presentation order. Preparation merges
manual preparations and proposals by valid planned civil date, then factual
`sort_timestamp`, then kind and stable identity. Undated or invalidly dated
items remain visible after valid dates. Resume uses the execution-draft recency
timestamp supplied by Core, and History uses the real start instant; both use
stable identity to break exact ties. Search and filters operate on the complete
paged result and reapply the same order. Late results from an older view or
query cannot replace the current list.

Web presentation defaults to French civil dates (`DD/MM/YYYY`) and 24-hour
times. A local **Display settings** dialog can select ISO dates. The versioned
preference is stored atomically with private permissions in
`$XDG_CONFIG_HOME/trainlog/web/preferences-v1.json`, falling back to
`$HOME/.config`; it is neither business data nor synchronized state. Missing
or invalid storage uses the French default, and invalid storage is reported.
Civil `YYYY-MM-DD` values are validated and never timezone-shifted. Zoned
timestamps preserve their instant and are presented in the browser timezone;
the original machine value remains in API data and `<time datetime>`.

Stored states remain wire-level enums, while Sessions presents contextual
French labels. A preparation derived from a proposal exposes the persisted
source identity and title as a stable deep link; the source digest remains a
technical detail and no title-based inference is performed.

Opening or deriving from an AI proposal never accepts, starts, deletes or
rewrites the source. Derivation records the exact proposal identity and imported
payload fingerprint. The Android screen labels manual preparations separately
and refuses to overwrite an active draft.

**Delete preparation** is a durable logical withdrawal, not physical evidence
purging. The protected DELETE command requires Host/Origin/CSRF, a bounded
idempotency key and the exact current revision. One desktop transaction records
the immutable withdrawal and makes the preparation unavailable for editing,
new delivery, active Sessions lists and the Dashboard projection. Revisions,
delivery rows, ACK evidence, the source proposal and all execution identities
remain intact.

The separately versioned `trainlog-session-preparations` V2 artifact carries
unacknowledged withdrawals and the exact linked delivery identities. Android
schema v23 stores one permanent withdrawal result per preparation. Pending
deliveries become `cancelled`; started deliveries, the active draft and all
completed facts remain unchanged and are recorded as `execution_preserved`.
The tombstone rejects an older delivery replay, including after process restart.
Desktop marks a withdrawal acknowledged only after the correlated Android
generation ACK. Until then Web reports that Android synchronization is pending.

Every deletable row has a separate right-edge trash action with an accessible
target. It never opens the detail. A focus-managed `alertdialog` names the item,
kind, relevant date and consequence; Cancel and Escape write nothing. Proposal
deletion records a durable logical withdrawal and publishes a bounded V2
proposal tombstone without deleting derived preparations or performed work.
Inactive execution drafts and completed sessions use the existing causal
operation/state boundary. Active or stale drafts conflict, and completed-session
deletion records a finalization so older snapshots cannot recreate the session.

Programs are planning data owned by desktop schema v26. The Programs subtab
uses responsive cards and a timeline detail view backed by real imported
definitions, sessions, usage, and provenance data. Import first runs the
same strict Core validator in preview mode, then commits only after explicit
confirmation. Exact content replay is idempotent; same-ID divergent content is
a conflict. Archiving preserves definitions and derived preparations. Creating
a preparation from one active program session copies targets and provenance
into a new manual preparation but creates no delivery, execution or performed
data. Program imports preserve target weights expressed as either integer or
real JSON numbers. A Program row affected by the former integer-weight import
defect can still create an editable draft: its impossible zero weight becomes
an unspecified draft target without rewriting the imported Program. Archived
or deleted programs cannot be prepared.

Desktop schema v27 enriches each Program session with a persisted execution
state. Web displays `À faire`, `Préparée`, `En cours`, or `Effectuée`; completed
and in-progress states come from stable Android execution provenance, while
prepared comes from a live derived preparation. No title, date, position or
content heuristic is used.

The discrete right-edge Program trash action is accessible and opens a
confirmation dialog that names the program and its consequence. Focus moves
into that dialog, Escape and Cancel perform no mutation, and focus returns to
the initiating action. Deletion is terminal logical deletion: it is allowed for
active and archived programs, retains source and derived-preparation evidence,
and prevents resurrection and re-preparation. See
[Program format V1](program_format_v1.md).

This V1 does not provide live Web capture, historical correction, automatic
training generation, Analysis or the standalone Exercises route.
