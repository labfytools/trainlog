# SESSION_GENERATOR_V1 final scientific delta review

Date: 2026-09-09. Baseline: `fd955315ccc9bb835a13eb94d206f1e01d893dfb`.
Decision: **SESSION_GENERATOR_V1_SCIENTIFIC_REVIEW=PASS**.
Reviewer: configured anatomie / Astra-high specialist, bounded final policy
review. This clears the scientific policy gate for implementation; it does not
declare implementation, executable validation or `SESSION_GENERATOR_V1=PASS`.

## Scientific question

Does the reconciled final policy support an editable single-session suggestion
without deriving prescription from repetition-less MAX, overstating observed
performance, or interpreting authored exposure/selection rules as physiology?

Yes. No substantive scientific ambiguity remains that requires new research
before encoding this bounded policy. Individual suitability and heuristic
effectiveness remain explicitly uncertain; passing the review does not resolve
those uncertainties through an unsupported efficacy claim.

Read the anatomy skill, originating scientific review, final domain and policy,
approved architecture record, programming foundations and relevant existing
reference records. Compared the domain and policy against retained original
bytes in `/tmp/trainlog-session-generator-v1/original-proposal/`. Parsed policy
comparison found changes only in `numeric_rule_status`, `eligibility` and
`load`; goals, ranges, exposure, selection, duration, guidance and citations
are unchanged. Architecture/storage/exchange choices were accepted as inputs,
not independently re-reviewed here.

## Sources

Existing evidence is reused; no new scientific reference or frozen catalog
change is needed. Source verification was limited to the already cited papers.

- [ACSM 2009](https://pubmed.ncbi.nlm.nih.gov/19204579/): historical professional
  guidance supplies direction for repetition/rest conventions, not proof of the
  exact defaults or permission to substitute Trainlog MAX for a measured RM.
- [Singer 2024](https://pubmed.ncbi.nlm.nih.gov/39205815/): the rest synthesis
  reports uncertain, modest hypertrophy differences; it does not establish a
  universal 120-second optimum.
- [Grgic and colleagues](https://pubmed.ncbi.nlm.nih.gov/33497853/): reviewed
  comparisons do not generally require failure for adaptation. This does not
  establish equivalent stimulus from arbitrary easy sets.
- [Vieira and colleagues](https://pubmed.ncbi.nlm.nih.gov/34881412/): failure
  conditions produce greater acute fatigue in reviewed comparisons; logged
  repetitions and BODY ZONES do not measure those experimental outcomes.
- [Kassiano and colleagues](https://pubmed.ncbi.nlm.nih.gov/35438660/): limited
  intervention evidence supports considering systematic variation and cautions
  against excessive rotation. No exact Trainlog score or recency interval was
  validated by this review.

The five PubMed abstracts above were readable during this delta review. The
originating review's summaries of [ACSM 2026](https://pubmed.ncbi.nlm.nih.gov/41843416/),
[Currier 2023](https://pubmed.ncbi.nlm.nih.gov/37414459/) and
[Moran-Navarro 2017](https://pubmed.ncbi.nlm.nih.gov/28965198/) are reused within
their recorded limits: adult multiweek adaptation evidence and a small acute
recovery experiment do not validate this algorithm. Their direct PubMed opens
returned no readable body during this review; no fresh full-text verification
is claimed. No commercial or EMG evidence underlies this delta.

## Anatomical finding

No muscle-role or anatomical identity change is proposed. Existing resolved
exercise interpretations remain the eligibility authority. A BODY ZONE is a
projection of anatomy, not an interchangeable muscle dose or exercise identity.
No additional anatomy discovery or persisted mapping migration is required.

## Biomechanical finding

The retained exact exercise, exact non-null equipment, compatible external
resistance and no-known-execution-conflict conditions are appropriate minimum
boundaries for reusing a displayed load. Matching IDs do not establish identical
settings, range, technique or calibration. Assistance and bodyweight do not
become interchangeable external kilograms. Preserving an observed number
unchanged avoids inventing an available increment or calibrated resistance.
This applies the existing [MAX/context foundation](../domain/programming_foundations.md).

## Exercise interpretation

**Goal defaults/rest — accepted.** General `2x10/90s`, strength `3x6/180s`,
hypertrophy `3x10/120s` and local muscular endurance `2x16/60s`, with their
unchanged editable ranges, are defensible authored templates. They are not RM
tests, exclusive adaptation bands, weekly prescriptions or optimal individual
doses. A strength label without characterized relative load promises no
strength-specific intensity. Rest can be extended, affecting actual duration.

**Load anchor — accepted.** Within the inclusive 28-day window, select the
newest qualifying completed occurrence in the required context. At least the
proposed set count must contain positive finite actual weight and repetitions
at least equal to the proposed repetitions. Return the minimum weight across
all qualifying rows unchanged. Do not pool inadequate occurrences into an
invented qualifying session. An increased target dose must be requalified.

This is a historical performance anchor, not a demonstration of today's
capacity. Qualifying subsets may coexist with nonqualifying rows; warmups may
qualify. The minimum can produce an insufficiently challenging suggestion.
Actual rest, tempo, exercise order and accumulated fatigue are not established
by the qualifying fields. A newer nonqualifying performance does not erase an
older eligible observation, but the older observation cannot be described as
verified current performance or automatic progression. Source and uncertainty
must remain visible. These are limitations of the accepted rule, not new
qualification criteria or a request to infer missing measurements.

**MAX and equipment — accepted.** An explicit MAX without repetitions supplies
neither 1RM nor goal-specific repetition capacity. Without a qualifying actual
dose, numeric target weight is absent for every goal. MAX may provide context
only, using `explicit_max_present_no_numeric_prescription`; it cannot break
equipment ties or supply a percentage fallback. Context choice follows newest
qualifying actual anchor, then bytewise equipment identity. This review
supersedes the originating review's acceptance of 50% MAX and its obsolete
numeric-MAX-fallback test requirements.

**Legacy representation clarification — accepted.** The parent relayed the
advisor's final clarification: legacy Android/V2 actual-only occurrences store
plan `load_mode=none`. That value alone establishes no resistance semantics.
Such a row may supply an anchor only with all targets absent and rest zero,
when the exact known scientifically compatible exercise/equipment context
positively establishes external resistance and actual positive weights satisfy
every other anchor condition. Explicit external occurrences remain eligible;
assisted, unknown, missing or conflicting contexts remain excluded. This
bridge does not infer external resistance from a positive number alone, rewrite
history or weaken the scientific repeated-dose requirement. A generated plan
with absent weight may likewise use plan mode `none` while preserving distinct
equipment resistance semantics. Parent-owned architecture/policy wording will
record this representation clarification; no additional research is required.

**Scoring — accepted as practical inference.** Same-identity and same-pattern
penalties can favor an alternative even when the familiar exercise has a load
anchor. Their interaction with coverage priorities and the history bonus is
explicit and deterministic. Neither repeating within those windows nor choosing
an unanchored alternative is physiologically prohibited. Repeated use of the
generator is not a validated rotation schedule or longitudinal strength
program; specificity and familiar practice can be useful. No score is an
effect size, readiness measure or injury probability. The unchanged diversity
cap and duration estimate remain software conventions with explicit shortages.

## BODY ZONE mapping

No mapping changes. Preserve separate primary and secondary actual-row counts,
with primary precedence and per-row deduplication for grouped requests.
Thresholds `1/3` primary/secondary within 24 hours and `6/12` within 72 hours
are informational exposure conventions. Primary threshold gives `warning`;
secondary-only threshold gives `notice`; otherwise `none` only after successful
analysis. These labels are not clinical severity or measured recovery. Missing
or unclassified history remains insufficient information. MAX-only and
continuous activity exclusion means no comparable counted resistance-set dose,
not absence of fatigue from those activities. Invalid required history analysis
must not masquerade as no observed exposure.

## Confidence

- `high`: established anatomy/context distinctions and the conclusion that
  repetition-less MAX cannot establish 1RM or repetition capacity.
- `moderate`: broad goal-template interpretation as an editable suggestion.
- `uncertain`: today's load/effort/readiness, exact scoring effectiveness,
  optimal volume/rest, and outcomes from repeated generator use.

## Limitations

This is a final policy delta review, not new broad anatomy research, a clinical
prescription review, or an implementation audit. Retained literature concerns
population interventions or controlled acute protocols. None validates the
28-day freshness cutoff, exposure thresholds, scoring coefficients or duration
formula as personalized physiology. No frozen knowledge payload was changed.

## Implementation handoff

Implement the reconciled policy and architecture; no scientific numeric or
eligibility wording correction is required. Preserve the limitations above in
interpretation and explanations. Executable evidence should cover qualifying
and insufficient actual rows, target-dose edits, all-qualifying-row minimum,
newer nonqualifying versus older qualifying occurrence, exact context exclusion,
inclusive 28-day age, absent MAX-derived loads and no invented rounding.
Retain planned-versus-performed separation and existing exact temporal,
exposure/aggregation, deterministic selection and shortage checks.

Two documentation-only updates belong to the parent, outside this review's
write scope: update the stale policy `status` value that still says architecture
is pending, and link this delta decision from the current domain/status record.
Preserve the originating review as proposal history, with its MAX conclusion
clearly superseded; do not let its former numeric-MAX tests drive implementation.

Reviewed policy SHA-256 before the representation clarification above:
`912ba800d885916279b5f26a4923f8a39350de69c0c7ff021bfde3ec740d6fe2`.
Reviewed domain SHA-256 before the representation clarification above:
`beffc9a4ed97cd887a2ccd983e22a906dfa507b98c680083d41cee4bee5ecf50`.
Only this new review artifact was written. No production, policy, database,
device, staging, commit or push operation was performed.
