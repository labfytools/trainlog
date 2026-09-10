# SESSION_GENERATOR_V1 architecture decision

Date: 2026-09-09. Baseline: `fd955315ccc9bb835a13eb94d206f1e01d893dfb`.
Independent configured advisor / Terra-high, isolated, read-only review:
`ADVISOR_DECISION=PASS`. Implementation and executable validation are pending.
This decision does not declare `SESSION_GENERATOR_V1=PASS`.

The review read the four retained proposals and the existing knowledge,
programming, persistence, session and exchange contracts. No new scientific
discovery or frozen-contract change is required. The original proposal bytes
are retained in `/tmp/trainlog-session-generator-v1/original-proposal/`.

## Shared policy and generator

`catalog/session-generation-policy-v1.json` is the sole authored policy source.
A strict Python validator checks duplicate keys, exact shape, versions, numeric
bounds and references to existing catalogs. Meson generates immutable C policy
data from it; Android strictly loads that same asset. Native C and Kotlin
algorithms use shared production golden fixtures for the complete normalized
output. No independently authored defaults, scores, windows, expansions or
movement groups are permitted. TRAINING_KNOWLEDGE_V1 remains read-only.

## Load decision

The proposed numeric `0.50 * MAX` fallback is rejected. The canonical
`docs/domain/programming_foundations.md` states that an explicit MAX lacks a
repetition count and establishes neither 1RM nor a percentage prescription.
Calling that percentage uncertain does not remove the contradiction.

The newest qualifying completed occurrence within an inclusive 28-day window
may supply an observed load, for the exact exercise and equipment and
compatible external-load semantics. At least the proposed number of actual
sets must have positive finite weight and repetitions at least equal to the
proposed repetitions. Use the minimum weight among qualifying rows, unchanged.
Preserve its source session, occurrence and timestamp. This is observed-dose
evidence, with uncertain applicability today; no progression or safety claim.

Without that evidence, numeric target weight is absent. MAX may be shown only
as context with `explicit_max_present_no_numeric_prescription`. Assistance,
bodyweight and unknown resistance context never yield an automatic numeric
target. Unknown increments do not authorize rounding an observed value.
Equipment selection uses the newest qualifying working anchor, then bytewise
equipment identity; MAX does not select or numerically prescribe a context.

## Android persistence

The authorized Android v10 to v11 migration adds `load_mode`, `rest_seconds`,
`target_sets`, `target_reps`, `target_duration_seconds`, `target_weight_kg` to
both normal completed occurrences and durable draft occurrences. Existing rows
receive `none`, `0` and NULL targets, with no historical reconstruction.
Preserve row IDs, stable identities, positions, equipment references, actual
sets, continuous data, MAX, and raw draft form input.

Planning metadata is separate from actual rows in Kotlin models and every
draft/detail/completion reader and writer. A planned set occurrence has positive
sets and exactly its profile's positive repetition or duration target. Optional
target weight is finite and positive. An absent target has NULL shape fields.
Continuous and explicit-MAX occurrences remain targetless. Completion retains
the existing atomic completed-session insertion and singleton-draft deletion.
The desktop remains schema v11; its existing occurrence model has plan fields.

## Session exchange V3

Use `format: trainlog-mobile-export`, `version: 3` and directional filenames
`trainlog-mobile-export-v3.json` / `trainlog-pc-mobile-export-v3.json`.
Targets are ordinary occurrence metadata and travel atomically with identity,
equipment, actual sets and MAX. No companion plan store is introduced.

V3 retains V2 identities, positions, equipment and actual-data shapes and
requires `load_mode`, `rest_seconds`, and `target`. The latter is null or an
object containing positive `sets`, exactly one positive `reps` or
`duration_seconds`, and optional positive finite `weight_kg`. Continuous rows
require null target, mode none and zero rest. MAX retains its actual-data
exclusivity and receives no invented target.

V1/V2 remain readable with their existing semantics. Never add V3 fields to a
V2 artifact or create a downgrade merely by rewriting its version. Current
publication uses V3 and must not silently produce lossy V2 for planned history.
V3 presence takes priority; invalid/conflicting V3 fails explicitly without a
V2 fallback. Only V3 absence permits legacy selection. Equal stable-ID V3
replays skip idempotently; divergent content conflicts and rolls back. Legacy
replay cannot erase local nondefault planning metadata. Preserve existing
definition-first and other companion reconciliation rules and suffix selection.
`TRAINLOG_FORMAT_V1` is unchanged.

## Preview, acceptance and exposure

Preview writes nothing. Android explicit acceptance atomically creates its
normal singleton draft with targets and no performed sets. Any existing draft
causes a non-mutating `existing_active_draft` conflict. Resume/discard uses the
existing explicit flows. Desktop acceptance passes its in-memory normal draft
to the existing session editor/insertion path. No generated-history silo.

Reusable read-only exposure and recency services consume complete required
history on one nested-safe read snapshot, with bounded accumulator memory.
The existing context preview page limits cannot bound scientific history.
Count actual positive-repetition SETS+REPS rows only; drafts, targets, MAX-only,
continuous, empty and future occurrences do not create set exposure. Use the
settled exact timestamp parser/comparator. Invalid stored time fails the whole
required analysis; invalid reference time fails request validation.

Parent-zone counting evaluates each actual row once: any requested descendant
primary match takes precedence over all secondary matches. Preserve separate
primary/secondary counters, distinct session counts, sorted pattern unions and
latest nonfuture exposure across full history. Unresolved science is explicitly
unclassified. Warning thresholds and selection/duration conventions otherwise
remain the retained scientific proposal, subject to bounded final delta review.

## C17 ownership and bounds

A focused `session_generation.h` API accepts borrowed call-duration requests
with zone, goal, minutes, explicit reference time and bounded optional ID arrays.
Results own allocations with one release function, or fixed capacities with
explicit failure before any complete-success claim. No retained request pointers
or global state. Output includes selected stable identities/context, targets,
duration, exposure, recency, confidence, stable rationale codes and source IDs.

Malformed/unknown request zone or pattern/count/reference is INVALID_ARGUMENT;
stored corruption, invalid timestamps, SQL failure or unrepresentable selected
output is DATABASE_ERROR; unsupported schema fails explicitly. Complete sparse
or empty candidate coverage is OK with explicit insufficiency, never an invented
exercise. Preserve existing public ID and timestamp capacities: compare valid
long stored instants in full, but fail if selected text cannot fit. Check all
counts, arithmetic, narrowing and capacities. Required WHY/CONTRACT/INVARIANT
comments belong in the implementation patch. Run early strict standalone C17.

## Required proof before completion

Shared C/Android fixtures must cover exact temporal boundaries, malformed time,
deduplicated primary/secondary exposure and session/pattern summaries, selection
ties/diversity/equipment/exclusions/preferences/recency, sparse zones, observed
load anchors and absent MAX/assistance loads. Exercise actual production paths.
Use a structurally real Android v10 fixture; prove migration/restart/completion
preservation and acceptance conflicts. Test V3 round trips in both directions,
plan/actual/identity equality, legacy readability, truthful downgrade rejection,
idempotence/conflict rollback and malformed V3 precedence. Validate previews do
not write, both UI flows, all builds/tests/validators, strict C17, sanitizer paths,
deterministic asset regeneration, bounded scientific review, engineering review,
documentation, one deep final audit and final evidence capture.

No application database mutation, installed-app operation, Git staging, commit
or push was performed by the advisor review.

## Bounded representation clarification

`ADVISOR_CLARIFICATION=PASS` after inspecting existing importers and desktop
schema checks. This clarification resolves planned versus actual load metadata;
it does not change historical values or scientific equipment semantics.

V2 actual-only history always has planned `load_mode=none`, even with positive
actual weights on known external equipment. A qualifying actual anchor may use
explicit occurrence mode external, or the narrow actual-only shape
`none / rest 0 / all target fields absent` with exact known scientifically
compatible external equipment. Apply the same durable-shape rule to V3; never
infer from the exchange version, display name, family, or unknown equipment.
An explicit assistance/conflicting context cannot qualify. This is transient
actual-load eligibility, not a persisted mode rewrite.

The desktop existing schema requires a non-null target weight for planned
external/assistance mode and forbids target weight for mode none. Therefore:

- `target:null` requires mode none and zero rest, including MAX and continuous.
- A non-null target without weight uses mode none and may retain planned rest.
- A non-null target with weight uses external or assistance.
- Generated absent-weight suggestions use planned mode none while retaining
  the separate selected equipment's resistance-context information.

V3 explicit bounds are position 0..100000, target sets 1..64, repetitions
1..10000, duration 1..86400 seconds, rest 0..86400 seconds, target weight
finite and strictly positive. Actual weight retains V2 finite nonnegative
semantics. Reject absent/present mode contradictions before persistence.
These new V3 validations do not change desktop schema or V1/V2 semantics.

Tests must include the actual-only external-equipment bridge and its missing,
unknown, different and assisted equipment rejections; valid absent-weight
planned mode none with positive rest; rejected targetless non-none/rest shapes;
MAX contradictions; mode/weight contradictions; and every range/nonfinite edge.
