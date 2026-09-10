# SESSION_GENERATOR_V1 — one deep final audit

Date: 2026-09-10. Baseline: `fd955315ccc9bb835a13eb94d206f1e01d893dfb`.
Configured final-reviewer / Terra-high, isolated and read-only.
Initial decision: **FINAL_REVIEW=FAIL**. One bounded repair chain is authorized;
do not run a second broad final audit or mark the tranche PASS before repair
verification, documentation synchronization and final validation.

## Blocking findings

1. **V3 timestamp validation.** Python `validate_payload()` accepted the shared
   V3 fixture after replacing a session start with `not-a-time`. Android's
   common V2/V3 validator likewise checked only nonempty session/body timestamps.
   Accepted malformed history subsequently fails the generator's correct exact
   timestamp reader. Repair V3 import and export in both languages to use the
   existing exact Trainlog parser for session and body instants, rejecting before
   persistence/publication. Preserve V1/V2 semantics. Test rejection without
   mutation and malformed-present-V3 precedence over V2.
2. **Current documentation contradictions.** Architecture and Android docs
   retained active-mobile-V2 statements despite V3 implementation, and the V3
   section of `sync_exchange.md` stated duration/rest 1..86400. Rest is actually
   0..86400. Correct current-state references, retaining explicitly historical
   V1/V2 and equipment-associations V2 without semantic changes.
3. **Incomplete golden output comparisons.** Shared C/Kotlin fixtures checked
   subsets of ordinary and selection outputs. Expand one shared corpus to full
   normalized expected results: every selected identity/context/plan/load/source,
   rationale/source list, recency and zone/pattern metadata, complete exposure
   windows/latest/unclassified state, duration, insufficiency and shortage codes.
   Exercise exact ID/equipment-anchor ties and every named coverage/score rule.
   Assert the same complete results through both production engines.

## Positive evidence and scope

No additional material API/ABI, lifetime, transaction/snapshot cleanup,
scientific identity, or deterministic-selection defect was established in this
audit. Owned bounded C analyzer input copies, complete snapshot history scans,
observed-only external-load anchors, absent numeric MAX, Android normal-draft
acceptance and TUI zero-actual initial plans were reviewed positively.

Before repair: Meson 45/45, focused ASan/UBSan 4/4, Android 73 tests with zero
failures/errors and one known external real-v9 fixture skip, strict headers,
builds and validators passed. Those passes did not establish the missing full
output assertions or V3 malformed-time rejection.

Live MTP/device validation remains explicitly outside the automated evidence;
no device installation or real-data migration was performed. This limitation
is not a blocker and does not expand the bounded repair.

## Repair verification

The bounded V3 temporal repair is implemented. It validates V3-only root,
session and body timestamps with the settled exact parser before import,
publication or persistence, preserves V1/V2 nonempty-string behavior, and
retains malformed-present-V3 priority over V2. Focused Python and Android
production-path tests passed.

The bounded full-parity repair is implemented. One shared 16-case corpus now
asserts complete normalized output through both engines, including all named
score/group/tie rules, plans, load provenance, rationale/source lists, exposure,
recency, shortages and duration. Focused C and Android runs passed; production
engines and the frozen policy were not changed.

Documentation correction for finding 2 records V3 as the active mobile snapshot
and occurrence-aware exchange, retains historical V1/V2 and
equipment-associations V2, and states rest 0..86400 separately from duration
1..86400. The bounded final repair review subsequently verified that paragraph
and returned `BOUNDED_FINAL_REPAIR_REVIEW=PASS`. The one deep audit remains
historically `FINAL_REVIEW=FAIL`; it was not rerun. Its authorized bounded
repair chain, independent verification, and final validation matrix closed all
three findings.

## Closure

`BOUNDED_FINAL_REPAIR_REVIEW=PASS` closed the authorized repair chain. The
post-repair validation matrix passed: Meson 45/45, named ASan/UBSan 4/4, Android
75 with zero failures/errors and one unavailable external-v9 fixture skip, the
executed structural Android v10 migration, validators, strict headers,
deterministic regeneration and APK asset comparisons. Therefore
`SESSION_GENERATOR_V1=PASS`. This closure preserves the initial
`FINAL_REVIEW=FAIL` as historical evidence and does not represent a second broad
audit.
