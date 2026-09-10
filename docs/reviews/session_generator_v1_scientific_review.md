# SESSION_GENERATOR_V1 scientific policy review

Date: 2026-09-09.
Status: **HISTORICAL ORIGINATING PROPOSAL — superseded for implementation**.
The [architecture decision](session_generator_v1_architecture_review.md) and
[final bounded scientific review](session_generator_v1_scientific_delta_review.md)
settle the current policy. In particular, the 50% MAX fallback and its numeric
fallback test requirements below were rejected and MUST NOT be implemented.
The original proposal text is retained as review provenance.
This is the originating scientific specialist's policy review, not an
independent implementation audit or a `SESSION_GENERATOR_V1=PASS` declaration.

## Scientific question

Can Trainlog suggest one editable resistance session from existing reviewed
exercise identities, requested BODY ZONE, goal, duration and completed history,
without claiming measured recovery or inventing 1RM meaning?

Finding: yes, with the explicit knowledge, uncertainty and target/history
boundaries in [the policy](../../catalog/session-generation-policy-v1.json) and
[domain specification](../domain/session_generation.md).

## Sources and evidence separation

Reviewed the project anatomy skill, domain anatomy, movement, equipment,
programming foundations and knowledge contracts, plus the exercise, equipment,
movement, BODY ZONE and central scientific reference catalogs. Followed PubMed
records for the existing adaptation, failure/fatigue and variation references
and added the current ACSM stand, a rest review and a recovery experiment.

- [ACSM 2026](https://pubmed.ncbi.nlm.nih.gov/41843416/): intervention synthesis
  and professional position, supporting broad prescription principles rather
  than this exact algorithm.
- [Currier 2023](https://pubmed.ncbi.nlm.nih.gov/37414459/) and
  [Grgic](https://pubmed.ncbi.nlm.nih.gov/33497853/): multiweek outcome evidence;
  no assumption that failure is required or arbitrary low effort equivalent.
- [ACSM 2009](https://pubmed.ncbi.nlm.nih.gov/19204579/): historical goal/rest
  conventions; not adopted progression or 1RM percentage rules.
- [Singer 2024](https://pubmed.ncbi.nlm.nih.gov/39205815/): rest-interval evidence
  with substantial uncertainty; no exact optimal rest claim.
- [Vieira](https://pubmed.ncbi.nlm.nih.gov/34881412/) and
  [Moran-Navarro](https://pubmed.ncbi.nlm.nih.gov/28965198/): acute fatigue and
  protocol-dependent recovery, not biological recovery measurement from logs.
- [Kassiano](https://pubmed.ncbi.nlm.nih.gov/35438660/): limited variation
  evidence, not randomized exercise selection or a validated diversity score.

No new EMG-derived primary roles, manufacturer identification, clinical claim
or anatomy mapping is introduced. The policy-local references avoid modifying
the frozen knowledge payload. PubMed abstracts/records provided the current
claims; the ACSM PMC full-text route returned an access challenge and was not
represented as read in full.

## Settled policy decisions

1. General `2x10/90s`, strength `3x6/180s`, hypertrophy `3x10/120s`, local
   muscular endurance `2x16/60s` are editable defaults, not RM tests or weekly
   recommendations. Broad inclusive guidance is respectively 1–3 sets / 8–12
   repetitions / 60–120 seconds; 2–3 / 5–8 / 180–300; 2–3 / 8–12 / 90–180;
   and 1–3 / 15–20 / 45–90. Exact endpoints are practical conventions, not
   optimal physiological boundaries. Dose edits recompute duration and load
   qualification. No progression.
2. Prefer recent repeated actual-dose evidence in the exact exercise and
   external equipment context, with an inclusive 28-day freshness window.
   At least the proposed set count must meet the proposed repetitions; use the
   minimum qualifying weight. "Working" means recorded completion only.
3. A compatible explicit MAX can support a numeric fallback at half the
   recorded load, rounded downward to 0.1 kg, in the same freshness window.
   This was explicitly examined because MAX lacks repetitions: it is legitimate
   only as an **uncertain, unvalidated starting-load convention**, not percent
   1RM, guaranteed safe weight or predicted rep capacity. Same coefficient for
   all goals avoids invented goal-specific precision. This distinction must
   survive implementation and user-facing presentation.
4. Assistance/bodyweight/unknown context leaves numeric weight absent.
5. Distinct primary and secondary actual-set counters use rolling 24/72-hour
   windows with exact timestamps. Thresholds are respectively 1/3 and 6/12
   primary/secondary sets. Any primary threshold produces `warning`, otherwise
   any secondary threshold produces `notice`, otherwise `none` after successful
   analysis. These are nonblocking targeting/exposure signals, not clinical
   severity. Secondary-only exposure never produces `warning`.
6. Primary-zone fit, function diversity, available equipment and existing
   history guide deterministic selection. Pattern duplication and missing
   science cannot be concealed by invented candidates.
7. Six-exercise cap and the `300 + sum(60 + sets*reps*4 + (sets-1)*rest)` time
   estimate are engineering conventions. Incomplete coverage/time is explicit.
8. Proposed targets never become performed rows. Preservation of targets in
   Android requires the architecture decision still pending.
9. Exact optional preferred UUIDs add +15 once. Excluded UUIDs or any intersecting
   excluded pattern IDs are hard exclusions. Availability, science, exclusion,
   diversity and time-fit rules win over preference. Unknown pattern IDs are
   request errors; unmatched exercise IDs never create candidates.
10. Actual same-exercise performance within `0 <= age < 259200` seconds adds
    -25; actual resolved same-pattern performance within `0 <= age < 604800`
    adds -15. Each applies once and adds to zone penalties. They are soft
    selection priorities, not biological cooldowns. Equipment context remains
    strict for numeric load despite identity recency across equipment.
11. Requested grouped-zone summaries deduplicate each actual row across all
    descendant matches; primary wins over secondary. Counts, distinct session
    IDs, sorted pattern union and latest exposure use the same eligible-row
    and exact temporal contracts; latest exposure is not limited to 24/72 hours.
12. Invalid stored timestamps fail required history analysis explicitly; no
    complete exposure or generation result is returned. Never silently discard
    malformed instants. Invalid reference time is a request validation error.

## Limitations and required implementation checks

- Confidence: `high` for established anatomy/context distinctions; `moderate`
  for population-level training-template interpretation; `uncertain` for
  today's load suitability and heuristic effectiveness. No universal recovery
  clock, clinical restriction or measured readiness is implied.
- Target-only, zero-set, draft, MAX-only and valid future rows cannot create
  performed exposure or qualify repeated working dose. Invalid stored time
  fails analysis explicitly, including load/recency readers. Missing history
  does not mean rested. Data-reader limits cannot silently undercount exposure.
- Load tests must include same-exercise different-equipment exclusion,
  assistance exclusion, newer ordinary-dose precedence over MAX, numeric
  qualified MAX fallback, old anchors, nonfinite values, and absent load.
- Boundary tests must cover exact 24-hour/72-hour exclusions, inclusive 28-day
  load age, exact 7-day pattern-recency exclusion, equivalent offset/fraction
  instants, invalid stored timestamp failure and deterministic identity ties.
- Test role-separated counts and shared-zone opposing functions; no duplicate
  patterns, no conditional-name guessing and explicit shortages.
- Test parent aggregation where one row matches both a primary and several
  secondary descendants: count once as primary, with one matching session;
  do not multiply the row through its pattern union. Verify secondary-only
  `notice` versus primary `warning` at equal qualifying counts, and no warning
  level standing in for an analysis error. Verify exact-ID preference,
  preferred-and-excluded conflict, unavailable preferences, excluded patterns
  and a valid alternative outranking recently repeated identity.
- Current resolved science has no chest or calves candidate. Sparse proposals
  are an honest result; fixing unresolved identities is separate research.
- The model lacks effort, RIR, technique/ROM verification, warmup classification,
  machine calibration and reliably selectable increments. Do not hide those
  limits behind the term "successful" or a confident precise kilogram number.

## Handoff and outstanding gate

Research artifacts are reviewable; production implementation has not been
authorized through this document. The parent reported that the intended
architecture advisor could not be spawned after repeated thread-limit failures.
The Android target/draft/storage architecture remains unresolved; the review
does not substitute for that advisor or authorize a schema/format migration.
No implementation, persistence, build or release success is claimed here.
