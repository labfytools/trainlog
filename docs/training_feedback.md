# Training feedback V1/V2

`TRAINING_FEEDBACK_V1` collects subjective raw wording for later human review.
It never changes a session, load, MAX, plan, BODY ZONE, scientific profile, or
generator decision, and it performs no medical, sentiment, recovery, pain, or
anatomical inference.

## Ownership and data model

Android is the only creator. The desktop database stores synchronized rows
and the TUI displays them read-only; neither desktop surface offers add, edit,
delete, or dictation actions. Schema v14 adds only:

- `exercise_feedback(feedback_id, session_exercise_row_id, observed_at, raw_text)`;
- Android-only `draft_exercise_feedback(feedback_id, draft_session_exercise_row_id,
  observed_at, raw_text)`;
- `session_followups(followup_id, session_row_id, observed_at, raw_text)`.

The foreign keys point at real occurrence/session rows and cascade only when
an existing parent-deletion workflow removes that owner; there is no direct
feedback deletion action. Public exercise
feedback derives `session_id`, `entry_id`, and the current canonical
`exercise_id` from the occurrence. IDs are UUIDv4-backed `fb_…` and `fu_…`.
Schema v15 keeps each root and adds immutable wording revisions. A correction
keeps the root ID, parent and original `observed_at`; it is not a new
physiological observation. Android exposes **Modifier** with current text
prefilled and resumed dictation appending to it. Cancel creates nothing.
Blank text and text exceeding 8192 UTF-8 bytes are rejected, never truncated.
Only synthetic wording belongs in source fixtures; real feedback is private
runtime data and must not be committed.

`trainlog-training-feedback-v2.json` transports roots plus complete
`revisions[]`. Merge is an append-only union by revision ID; omission never
deletes and immutable disagreement is a hard conflict. V1 remains readable:
its `raw_text` becomes `fr0_<root-id>` only if absent and can never erase a
later V2 revision. Draft revisions survive restart/reconstruction and transfer
unchanged on finalization. Retained `session_id` + `entry_id` corrections keep
all revisions; confirmed parent removal may cascade them. No direct feedback
delete action exists, and feedback never drives automatic training adaptation.

## Dictation

The Android editor exposes **Démarrer**, live partial text while listening,
**Stop**, editable text, **Reprendre**, **Enregistrer**, and **Annuler**.
Stopping never saves. Resume appends recognition to the manually corrected
committed text. Errors retain captured text. French `fr-FR` is the V1 default;
on-device recognition is preferred when supported, otherwise Android's normal
service is used. Permission denial or unavailable recognition leaves manual
entry fully usable. The adapter and persistence transport text only: Trainlog
does not create or retain PCM, WAV, compressed audio, service audio, or a
microphone temporary file. Destroying the UI releases recognizer callbacks and
resources.

## Time presentation

`observed_at` and `sessions.ended_at` are exact offset-aware Trainlog
timestamps. New Android completions persist their explicit finalization instant
as `ended_at`; old NULL anchors are not backfilled or derived from `started_at`.
Session follow-up uses `H+floor((observed-end)/3600)` including after 24 hours,
or `H+?` when the anchor is missing/invalid. Exercise feedback observed before
the end displays `Pendant la séance`; at/after end it uses H+, and without an
anchor it displays the neutral `Ressenti`. No negative H+ is rendered. Ordering
parses instants, then compares the stable ID bytewise; it never
uses lexical timestamp order or machine-local timezone.

## Synchronization

`trainlog-training-feedback-v1.json` has format
`trainlog-training-feedback`, version 1, `generated_at`, and bounded
`exercise_feedback`/`session_followups` arrays. It is a separate,
direction-neutral companion and does not alter mobile export V3,
`TRAINLOG_FORMAT_V1`, BODY ZONES, equipment, or alias companions.

Import occurs after exercise definitions, aliases, machine identities, sessions,
entries, and equipment reconciliation. Missing parents fail; no phantom object
is created. Exercise claims resolve aliases and must equal the occurrence's
canonical identity. A missing stable record is inserted, an identical stable
record is an idempotent success, and any differing immutable field is a hard
conflict. Omission never deletes local rows. Consequently an Android follow-up
added after an earlier sync is unioned on the next run and the complete PC set
can converge back to Android without duplication.

Exercise feedback entered from the active-session occurrence UI is committed
immediately to `draft_exercise_feedback`; it does not wait for session
completion. It survives navigation, activity/process recreation and repository
reopen. Ordinary draft rewrites preserve it by stable `entry_id`. Explicitly
discarding the occurrence or whole draft removes this parent-owned local data
through `ON DELETE CASCADE`; there is no independent delete UI.

Finalization copies every draft observation to `exercise_feedback` in the same
transaction as the completed session, matching the completed occurrence by
`entry_id` and preserving `feedback_id`, `observed_at`, and `raw_text` exactly.
The subsequent draft deletion removes only draft storage. Any transfer failure
rolls the entire finalization back.

Draft feedback is Android-local and absent from all V1 companions because it
does not yet own a completed `session_id`. After finalization, the same stable
records enter the normal bidirectional Training Feedback companion.

Completed-session correction may reconstruct internal occurrence rows. Desktop
therefore snapshots feedback inside the correction transaction and reattaches
it only when the same `entry_id` remains in the same `session_id`; IDs,
timestamps and raw text are unchanged. Removed occurrences cascade their owned
feedback and new occurrences inherit nothing. The stable session row is not
recreated, so follow-ups remain untouched. Android has no general completed
training-session editor, but its bounded resumed-MAX correction path applies
the same rule before reconstructing occurrences. Any reattachment failure
rolls the complete correction back.
