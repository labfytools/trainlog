# Session generation policy V1

Status: **CANONICAL POLICY / FROZEN**. `SESSION_GENERATOR_V1=PASS` after its
one deep final review, bounded repairs, bounded repair review, and final matrix.
Architecture: [ADVISOR_DECISION=PASS](../reviews/session_generator_v1_architecture_review.md).
Bounded final science: [SESSION_GENERATOR_V1_SCIENTIFIC_REVIEW=PASS](../reviews/session_generator_v1_scientific_delta_review.md).
These decisions settle the separately frozen policy. The completed lifecycle is
recorded by the canonical current-state and review documents.
The originating proposal is retained as historical review evidence and is
superseded for its MAX-derived numeric fallback. The implementation adds Android
planning metadata through v10 -> v11 and a separate mobile-export V3; it does
not change runtime scientific mappings, the desktop schema v11, or
`TRAINLOG_FORMAT_V1`.

The authored policy is
[`session-generation-policy-v1.json`](../../catalog/session-generation-policy-v1.json).
It extends the application through a separate prescription policy; it does not
change the read-only scope or semantics of `TRAINING_KNOWLEDGE_V1`.
Existing scientific references remain in
[`science-references-v1.json`](../../catalog/science-references-v1.json).
Three additional bibliographic records are local to the new policy so that the
frozen knowledge catalogs and their generated assets remain unchanged.

## Evidence and intended interpretation

The 2026 ACSM position stand synthesizes 137 reviews of healthy adults. It
supports resistance training for several adaptations; heavier loads favor
strength and greater weekly volume favors hypertrophy. These findings concern
training over weeks and do not validate a one-session optimizer, an individual's
readiness, or Trainlog's numeric defaults.
[Currier and colleagues, ACSM 2026](https://pubmed.ncbi.nlm.nih.gov/41843416/)

A large network meta-analysis likewise supports many viable resistance-training
configurations, with higher loads favoring strength and multiple sets appearing
in better-ranked hypertrophy configurations. Rankings are population summaries,
not evidence that one prescription is optimal for everyone.
[Currier and colleagues, 2023](https://pubmed.ncbi.nlm.nih.gov/37414459/)

The historical ACSM stand described different repetition and rest conventions
for strength, hypertrophy and local muscular endurance. It supplies background
for the direction of the goal templates below, with current reviews taking
precedence. Its progression schedules and percentage-of-1RM prescriptions are
not imported into this generator.
[ACSM 2009](https://pubmed.ncbi.nlm.nih.gov/19204579/)

| Goal | Proposed sets | Repetitions per set | Inter-set rest |
|---|---:|---:|---:|
| `general` | 2 | 10 | 90 seconds |
| `strength` | 3 | 6 | 180 seconds |
| `hypertrophy` | 3 | 10 | 120 seconds |
| `endurance` | 2 | 16 | 60 seconds |

Broad editable guidance around those defaults is:

| Goal | Sets range | Repetitions range | Rest range |
|---|---:|---:|---:|
| `general` | 1–3 | 8–12 | 60–120 seconds |
| `strength` | 2–3 | 5–8 | 180–300 seconds |
| `hypertrophy` | 2–3 | 8–12 | 90–180 seconds |
| `endurance` | 1–3 | 15–20 | 45–90 seconds |

Endpoints are inclusive practical guidance, not validated optimal intervals or
hard physiological limits. The generator uses the single defaults unless values
are explicitly overridden. An override requires recalculating both duration and
load qualification using the proposed dose; it must not retain a load qualified
for fewer repetitions or fewer sets. Goal ranges overlap because adaptations
are not confined to mutually exclusive repetition bands. The historical
prescription principles above support their direction; these exact endpoints
remain authored conventions, including the conservative cap of three sets.

These are editable, moderate-volume defaults, **not RM tests**, exclusive
adaptation ranges, weekly targets or proof of effective effort. `endurance`
means local muscular endurance in this bounded resistance-session generator;
it does not prescribe aerobic conditioning. Three sets is not necessarily
better than two for an individual. A strength-labelled session without a
well-characterized challenging load cannot promise strength-specific loading.

The rest review suggests a small hypertrophy advantage to resting more than
60 seconds, with considerable uncertainty and little detected difference beyond
90 seconds. A 120-second hypertrophy default is therefore a convenient allowance,
not a scientifically exact optimum. Users may rest longer; the time estimate
then changes in practice.
[Singer and colleagues, 2024](https://pubmed.ncbi.nlm.nih.gov/39205815/)

Failure is not generally required for adaptation in reviewed comparisons, but
this does not mean arbitrarily easy sets provide equivalent stimulus. Trainlog
does not measure proximity to failure, technique or effort. Guidance is to use
a controllable load and finish with repetitions still possible, adjusting
downward or stopping if the intended repetitions and technique cannot be
maintained. No exact repetitions-in-reserve value is inferred or stored.
[Grgic and colleagues](https://pubmed.ncbi.nlm.nih.gov/33497853/)

## Candidate identity, movement and BODY ZONES

Candidates require an existing runtime exercise UUID, `SETS + REPS`, a resolved
scientific interpretation with `high` or `moderate` confidence, and an explicitly
compatible available equipment context. Conditional or unknown interpretations
remain excluded with a reason. Flexible equipment and unlinked capabilities
cannot create an exercise identity. No display-name matching is permitted.

`upper_body` expands to chest, back, shoulders and arms; `lower_body` to glutes,
thighs and calves; `full_body` to those seven leaf zones plus core. This is a
generator request expansion, not an alteration of the standalone `full_body`
catalog zone or a reason to relabel a cardio activity. A requested leaf admits
primary or explicit secondary scientific matches, preferring primary matches.
Scientific and persisted zone disagreements are explained, never silently fixed.

The existing anatomy and pattern catalogs remain authoritative. Knee extension
and knee flexion, for example, remain distinct functions despite the shared
`thighs` zone. Pattern IDs are programming abstractions grounded in joint actions;
they do not measure force distribution or equal training dose. See
[anatomy and movement](anatomy_and_movement.md) and
[equipment interpretation](exercise_equipment_interpretation.md).

Selection favors purposeful functional variety, not randomized novelty. The
variation review supports considering systematic variation but is small and
largely restricted to young men; it does not validate any exact rotation or
diversity score.
[Kassiano and colleagues](https://pubmed.ncbi.nlm.nih.gov/35438660/)

At most six exercises are selected, each exercise UUID once, with at most one
candidate carrying any already-selected exact pattern ID. Thus two rows or two
leg-curl identities do not fill a session with the same pattern. This deliberate
V1 cap may omit useful within-pattern variation; it is not a biological law.

Optional `preferred_exercise_ids` gives each matching exact UUID a **+15** soft
score bonus, once regardless of duplicate requested IDs. Optional
`excluded_exercise_ids` and `excluded_pattern_ids` are hard exclusions; exclude
a candidate if its UUID is excluded or any of its scientific pattern IDs is
excluded. Exclusion wins when an exercise is both preferred and excluded.
Availability, reviewed scientific eligibility, exclusions, duplicate/diversity
limits and time fit apply before coverage priorities and score. Preference
cannot create an unavailable, conditional or unknown candidate. Unknown exercise
IDs are reported as unavailable/unmatched; unknown pattern IDs are invalid
request input. None of these request options changes the catalog or history.

Before scoring, prefer candidates in an uncovered region for `full_body`
(upper, lower, core). Region membership uses the candidate's scientific primary
zone. For `upper_body`, prefer an uncovered push/pull class if a fitting candidate
exists. For `lower_body`, similarly prefer the uncovered extension/flexion
classes listed in the policy. If several priority classes remain uncovered,
compare all their fitting candidates by score; once covered or unavailable,
use the whole remaining eligible pool. Region/pattern coverage limitations must
remain visible; they do not certify a balanced program.

Score each eligible candidate as follows. Apply each boolean term once; the
two base match scores are mutually exclusive. Recompute after each selection.

| Condition | Points |
|---|---:|
| Scientific primary matches requested expansion | +100 |
| Only a scientific secondary matches | +60 |
| Primary zone not yet selected | +20 |
| Pattern not yet selected | +30 |
| Qualifying repeated-set load history | +5 |
| Explicitly preferred exercise UUID | +15 |
| Same exercise actually performed within 72 hours | -25 |
| Same scientific pattern actually performed within 7 days | -15 |
| Primary-zone primary count reaches 24-hour threshold | -30 |
| Primary-zone secondary count reaches 24-hour threshold | -10 |
| Primary-zone primary count reaches 72-hour threshold | -20 |
| Primary-zone secondary count reaches 72-hour threshold | -10 |
| Any candidate secondary zone has any exposure flag | -10 |

Take the highest score that fits the remaining time estimate, breaking ties by
bytewise exercise UUID and then equipment ID. Equipment choice prioritizes the
newest qualifying repeated-set anchor, then bytewise equipment ID; MAX does not choose the context. Occurrence recency ties use the shared exact instant
comparator and bytewise session/occurrence identity ordering. These scores and
priorities are deterministic software conventions; none is calibrated to an
adaptation effect size or injury probability.

Same-exercise recency requires at least one actual positive-repetition performed
row for the exact UUID in completed history with `0 <= age < 259200` seconds.
This selection penalty applies across equipment contexts, while numeric load
reuse remains restricted to the exact equipment context. Same-pattern recency
requires an actual row whose resolved scientific pattern IDs intersect the
candidate's pattern IDs, with `0 <= age < 604800` seconds. Apply each penalty
once, even if several rows or patterns match, and add both to the zone terms.
Targets, MAX-only records and empty occurrences satisfy neither condition.
Exactly 72 hours or 7 days is outside its respective window. These are soft
priorities, not prohibited intervals between exercises.

For example, otherwise equivalent same-pattern candidates A and B both receive
the pattern penalty, but only recently performed A receives the additional
25-point identity penalty. Even A's 5-point working-history bonus does not erase
that difference; B can rank higher. Purposeful repetition remains permissible
when alternatives are missing, coverage differs or other score terms prevail.

The current knowledge inventory has no resolved chest exercise and no linked
resolved calf exercise. The custom chest entries remain conditional. A chest
or calf request can therefore produce an explicit shortage; other requests may
be shorter or incomplete. Do not repair these gaps by guessing anatomy from
equipment names. Scientific identity work is a separate future task.

## Observed-load reuse, explicit MAX and missing load

Numeric load reuse requires the exact exercise UUID, a non-null exact equipment
ID, external-load semantics and no known conflict in execution context. Different
machines, pulley contexts, load modes and variants never share anchors. Unknown
settings, range of motion and technique remain limitations even when recorded
IDs match. All proposals carry their source occurrence, date, context and
confidence; no numeric result is labelled a verified safe working weight.

Use this order:

1. Search completed history within 28 elapsed days, inclusive, for the newest
   occurrence with at least the proposed number of actual sets having positive
   finite weight and actual repetitions greater than or equal to the proposed
   repetitions. Reuse the minimum weight among those qualifying rows. For three
   proposed sets of 10, actual sets `10x50, 12x55, 10x50` support 50 kg. Actual
   sets `10x50, 8x50, 6x50` do not establish three sets of 10. Targets never
   satisfy this condition. The minimum is a conservative choice, not a test of
   effort. Other nonqualifying rows remain history, not fabricated successes.
2. If no qualifying repeated dose exists, leave weight absent. A compatible
   explicit MAX may be displayed as historical context with
   `explicit_max_present_no_numeric_prescription`, but supplies no numeric target.
3. Do not substitute zero, an invented average, another exercise's result,
   a percentage of MAX, or a random default.

Observed-load reuse has confidence **`uncertain`** for today's prescription.
"Successful" or "working" here means only that a qualifying repeated dose was
recorded as performed. Trainlog cannot separate warmup from work reliably,
verify technique, know whether the set ended at failure, certify safety, or
detect all changes in readiness. Reusing observed load does not prove future
completion. An older qualifying occurrence is historical evidence, not automatic
progression over a more recent different performance. Its source must be visible.

An explicit MAX is an occurrence-owned observed maximum result. Its model does
not contain a repetition count and is mutually exclusive with performed sets.
It therefore cannot be qualified as a measured 1RM from this record alone.
**No percentage prescription follows from an unqualified explicit MAX.**
The original proposal's 50% fallback was rejected by the architecture review
because it contradicts this existing canonical boundary. The same absent-load
fallback applies to every goal. The 28-day actual-anchor cutoff remains a
freshness convention, not a physiological detraining boundary. A MAX-only
occurrence never creates a performed-set count.

External load and assistance are distinct. V1 omits numeric assistance and
bodyweight proposals. More assistance generally reduces unsupported demand;
neither halving assistance nor subtracting it from body mass is an external-load
prescription. Preserve an assistance explanation and equipment identity. Machine
increment availability is unknown: retain a qualifying observed value unchanged
as an indicative editable value, and invent no increment or rounding. See
[MAX context](programming_foundations.md).

Legacy Android/V2 actual-only occurrences store planned mode `none`, zero rest
and absent targets even when actual sets have positive weight. They may qualify
only with the exact known compatible external equipment and all repeated-dose
conditions above. This transient compatibility rule also applies to V3
actual-only rows; it never rewrites history or accepts assistance/unknown context.
Planning mode is separate: an absent-weight generated plan uses `none` under
the existing desktop invariant, retaining equipment resistance context separately.
A present observed target weight uses `external`.

There is no progression, estimated 1RM, future schedule or automatic load increase.
No automatic MAX-derived fallback or validated personalized intensity is claimed.

## Recorded exposure and recency

Use the settled exact timestamp policy of `TRAINING_KNOWLEDGE_V1`. Reference
time is an explicit input. Define age as elapsed seconds between reference and
occurrence instant. Exposure windows are `0 <= age < 86400` and
`0 <= age < 259200`; exactly 24 hours is outside the short window and exactly
72 hours outside the long window. Calendar dates and a universal 48-hour rule
play no role.

Count actual positive-repetition `SETS + REPS` rows from completed history,
deduplicated by occurrence/set identity. Weight need not be present. Each row
counts once for its resolved scientific primary zone and once for each distinct
explicit secondary zone, in **separate integer counters**. Never sum those
counters into equivalent effective sets or give secondary roles a claimed
fractional biological dose. Parent zones and muscle groups are not counted
again. Unknown or conditional history remains unclassified and visible as a
knowledge gap. Use neither exercise-name guesses nor target counts.

Requested-zone summaries require an additional aggregation rule: expand the
request, then classify each distinct actual row **once**. Count it as primary
if its scientific primary is inside the expansion; otherwise count it as
secondary if any scientific secondary is inside the expansion. Primary wins
when both primary and secondary descendants match. Never construct a parent
total by adding leaf-zone counters. A row with primary `back` and secondary
`arms` counts once as primary for `upper_body`, and once as secondary for an
`arms` request; the two requested summaries are separate views of the same row.

Within each 24/72-hour summary, `set_count = primary_count + secondary_count`
is the deduplicated count of actual rows, not equivalent physiological sets.
`session_count` counts distinct matching session IDs, and `pattern_ids` is the
sorted distinct union of scientific patterns from exactly those matching rows.
The latest exposure is the newest matching nonfuture actual-row occurrence in
the complete queried history, even if it is outside both windows. Preserve its
source session/occurrence identities and scientific patterns; use the existing
exact timestamp and identity tie policy. Last exposure is absent when none is
observed, not an inferred recovery date. Reader truncation cannot present an
incomplete latest-exposure or count summary as complete. The same eligible rows,
identity deduplication and temporal comparator underlie selection recency.

Drafts, targets without performance, zero-set occurrences, MAX-only results,
continuous activity and valid future occurrences contribute zero
to these resistance-set counters. This does not claim MAX attempts or aerobic
activity cause no fatigue; they simply have no comparable recorded set dose.
An occurrence with target three sets but one actual set contributes one.

| Informational flag | Primary count | Secondary count |
|---|---:|---:|
| `recent_exposure` within 24 hours | at least 1 | or at least 3 |
| `repeated_exposure` within 72 hours | at least 6 | or at least 12 |

Both flags may be present. Below the thresholds use `no_threshold_observed`,
never `recovered` or `safe`. Missing or unclassified history means insufficient
information. No arbitrary reader preview limit may silently undercount the
windows; paginate or expose incomplete history and withhold a complete-summary
claim. Thresholds are prioritization conventions and warning triggers only.

Expose exactly one level in addition to the flags: **`warning`** if either
primary threshold is met; otherwise **`notice`** if either secondary threshold
is met; otherwise **`none`** after successful analysis. Secondary-only exposure
never creates `warning`. For example three actual primary sets in 24 hours
produce `warning`; three secondary-only sets produce `notice`; one primary set
produces `warning`, while one secondary-only set reaches no threshold. These
levels express the strength of the recorded direct-versus-indirect targeting
signal, not clinical severity. All remain nonblocking. For grouped requests,
apply the thresholds to the deduplicated requested-zone primary/secondary counts
above, not sums of descendant warnings.

An invalid reference instant fails request validation. An invalid stored
timestamp encountered by a required history reader **fails analysis explicitly**
under the existing temporal contract: return the specific error and **no complete
exposure or generation result**. Never discard malformed instants as if they
were outside the window. This applies to load anchoring, latest-exposure lookup,
set summaries and recency penalties alike. Missing history and a reader error
are distinct outcomes; the error is not `none`, `notice` or `warning`.

Acute fatigue differs with failure and protocol. A systematic review found
greater fatigue after failure conditions; a small trained-men experiment found
different recovery courses even when total repetition volume was matched.
Neither permits recovery estimation from only logged set counts and body zones.
[Vieira and colleagues](https://pubmed.ncbi.nlm.nih.gov/34881412/),
[Moran-Navarro and colleagues](https://pubmed.ncbi.nlm.nih.gov/28965198/)

Explain recent recorded exposure and preserve the user's ability to continue,
modify the proposal or choose another zone. Do not convert flags into a clinical
restriction, recovered percentage or predicted injury risk. No score changes
the saved anatomy or history.

## Duration and incomplete coverage

Presets are 30, 45 and 60 minutes; custom input is an integer from 10 to 120
minutes. Bounds are interface conventions, not exercise-health thresholds.
Estimate 300 seconds for preparation and, per exercise:

```text
60 + sets * repetitions * 4 + (sets - 1) * rest_seconds
```

The 60 seconds allow setup/transition. Four seconds per repetition is a planning
estimate, not a compulsory tempo. Preparation does not create a fake exercise
or performed set and is not an individualized warmup prescription. Sum these
terms without counting rest after the final set. Select only whole exercise
blocks fitting the remaining budget; never shrink rest or add redundant work
to fill the requested duration. Actual equipment queues, setup and rest can
increase time. Show estimated selected duration and any unfilled request.

For example, one hypertrophy block costs 420 seconds; three blocks plus the
300-second preparation allowance cost 1560 seconds (26 minutes). This is a
duration estimate, not evidence that 26 minutes is an optimal session.

## Implementation boundary

Generation must remain read-only until explicit acceptance into the normal
capture flow. Proposed sets, repetitions, rest and load are targets, never
performed history. The accepted architecture adds nullable targets and explicit rest/load fields
to Android draft/completed occurrences through an additive v10-to-v11 migration.
A separate session exchange V3 preserves those normal fields atomically with
actual values and stable identities; V1/V2 remain readable and unchanged.
Preview performs no write. Explicit acceptance creates a normal active draft,
with a non-mutating conflict if another draft exists. See the
[architecture decision](../reviews/session_generator_v1_architecture_review.md).
No notes, fake performed sets, MAX reinterpretation or frozen exchange overload
is permitted. The initial final audit found repairable V3 timestamp,
documentation-bound, and shared-full-parity gaps; its one bounded repair chain
and final validation are complete.

Scientific confidence is `high` for the anatomy/context distinctions,
`moderate` for broad goal-template interpretation, and `uncertain` for individual
load, effort, recovery and exact heuristic effectiveness. The generator has no
clinical or individualized rehabilitation scope and makes no long-term outcome
guarantee from one session.
