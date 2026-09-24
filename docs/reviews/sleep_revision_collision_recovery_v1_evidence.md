# Sleep revision collision recovery V1 evidence

Date: 2026-09-24

## Collision

```text
COLLIDING_REVISION_ID=slr_d6cac4c6-dab7-4327-bc06-5cc95f7a2dfa
ANDROID_PARENT=slr_e98ad36c-94e2-452c-8068-a7e3d2ea4dd4
DESKTOP_PARENT=slr_2bbf573d-4262-49b0-896e-1bcabf3b002a
ANDROID_PUBLISHED=YES
DESKTOP_PUBLISHED=NO
ANDROID_REFERENCED_BY_IMMUTABLE_GENERATION=YES
DESKTOP_REFERENCED_BY_IMMUTABLE_GENERATION=NO
```

The two records had the same entry identity, night envelope, creation time,
notes, day form, and four event identities (`IDENTICAL`). Their causal parents
were different (`PARENT_CONFLICT`). Desktop contained two non-null quality
values where Android retained nulls (`NON_CONFLICTING_ADDITION`). Desktop also
contained five distinct medication occurrences absent from the older Android
branch (`MISSING_ON_ANDROID` / `NON_CONFLICTING_ADDITION`), with a total
quantity of six. No same occurrence ID had divergent content. Medication names,
unit doses, and notes are deliberately omitted from this repository record.

The root cause was the manual real-night causal repair recorded in
`docs/reviews/sleep_awake_selection_pdf_v1_evidence.md`: two independently
derived tips were assigned the same preselected new revision ID. No production
UUID collision or repository clone function produced the reuse. The faulty
transition was therefore that operational repair, not an Android/Desktop save
function. The production boundaries hardened by this tranche are
`sleep_diary_exchange.apply()` and
`TrainlogRepository.applySleepDiaryV1Json()`; database schemas v36/v33 also
make revision payloads append-only.

## Backups and isolated simulation

Immediate pre-recovery evidence is outside Git at
`/home/fy59/Downloads/trainlog-sleep-collision-recovery-20260924T140000+0200/`.
The Android application archive SHA-256 is
`7d0d95c9401df4baa66503658b8e309e4f6cb7359e5404517460a97afbce668a`,
the extracted Android database SHA-256 is
`6fb99e87a24041202a867e8487dd95ef088f8e189ebb6d6e60956f999bc01ae0`,
and the Desktop database SHA-256 is
`4a44a29a68683ddfaea4c20c121cd6f32309660d6b4141ca716c3fb20d89b225`.
Both databases reported `integrity_check=ok` and an empty foreign-key check.

Before canonical mutation, isolated copies retained both branches, replayed
the immutable generation, produced `consumed` / `sqlite-commit-full`, and
replayed the exact ACK byte-identically. The imported fixture had one session
ending `2026-09-24T11:16:00+02:00`, 7 exercises, 15 sets, 5 feedbacks,
7 exercise intervals, 4,919 BPM samples, 4,919 RR intervals, and one Program
execution. Sleep retained four events, five medication occurrences, and total
quantity six; both copied databases remained integrity/FK clean.

## Canonical recovery and replay

The unpublished Desktop branch was retained exactly under
`slr_42250548-5237-4225-a12d-7e68a0b57076`. The immutable Android branch kept
the collision identity and its original parent. The reconciliation successor
`slr_670c7516-f39e-4196-a861-8f3b657dbee1` descends from that published branch
and contains the non-conflicting union. The target manifest remains SHA-256
`ea3e7ed5622a04f514db069db29cccff1b4702c5cc0e4e253a578ffe8806fc70`;
no generation artifact was rewritten.

Generation `gen_76222040-50d7-4f5d-bcfb-ad9c1e7786f8` was consumed through the
production consumer with `sqlite-commit-full`, and Android accepted its exact
consumed ACK. Two subsequent normal Android generations were also consumed and
acknowledged during the recovery validation. The canonical database and Web
API expose the single original session identity at 11:16, all 7 exercises,
15 sets, 5 feedbacks, 4,919 BPM/RR values, one Program execution, and the
reconciled Sleep tip with four events, five medication occurrences, and total
quantity six.
