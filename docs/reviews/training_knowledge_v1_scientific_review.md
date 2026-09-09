# TRAINING KNOWLEDGE V1 — final scientific review

Date: 2026-09-09. Result: **PASS within the documented scope and uncertainty**.
No blocking scientific correction or confidence downgrade is required.

This bounded review assessed the six canonical scientific catalogs against the
previously researched source corpus, reviewed staging interpretations and
`schema_supplement.json`. It also inspected the generated BODY ZONE audit,
`docs/domain/` synthesis and Android's resolved-versus-conditional query guard.
It is a scientific review, not a build, persistence or UI validation report.
No production code, canonical catalog, database or persisted relation was edited
by this review.

## Coverage and fidelity

| Canonical collection | Count | Finding |
|---|---:|---|
| Scientific references | 23 | Bibliography, identifiers, source types, limitations and claims preserved |
| Functional muscle entities | 53 | Muscles, regions and groups remain distinct; membership overlap documented |
| Joint actions | 35 | Actions, joint complexes, nominal planes and non-exhaustive contributors preserved |
| Movement patterns | 26 | Authored conventions, French names, zones and anti-motion distinctions preserved |
| Exercise identities | 23 | 17 resolved families, 4 conditional candidates, 2 unresolved warm-ups |
| Equipment identities | 41 | 38 supplied and 3 custom; manufacturer and model remain null throughout |
| BODY ZONE audit rows | 23 | 16 confirmed, 4 questionable, 3 unresolved; no proposed mutation |

Scientific fields compare equal to the settled staging data plus the reviewed
supplement after ignoring array order. References compare equal under the
canonical field renaming. The rotary-torso compatibility representation follows
`representation-decisions.md`: a symmetric UUID association marked
`catalog_compatible_not_observed_occurrence` does not assert observed history.

The exercise confidence distribution is 6 `high`, 11 `moderate` and 6
`uncertain`. The equipment scientific-status distribution is 21
`scientifically_documented`, 17 `mechanically_identified_anatomy_incomplete`
and 3 `equipment_identity_uncertain`. Scientific documentation of a generic
machine family does not certify its local model, trajectory or calibration.

## All six high-confidence exercise mappings

Here `high` applies to the explicitly limited movement-family interpretation:
the relevant action, plausible muscle roles and coarse BODY ZONE projection.
It does not assert measured local force shares, a known manufacturer, exact
resistance curves, equal adaptation or transferable MAX. The source hierarchy
supports these bounded claims without requiring an EMG study for every simple
anatomical action.

| Stable exercise ID | Identified variant and equipment | Actions, roles and zones | Evidence assessment |
|---|---|---|---|
| `ex_01ff06dd-ad00-46ee-9b46-ed32cabedbef` | Hip adduction; recorded `hip_adduction` context | Resisted hip adduction; adductor group; `thighs` | High retained. Established adductor function supports the family mapping; individual contributions depend on hip/knee position. |
| `ex_1872246a-39ae-44dc-b58d-f87e90ca49ab` | Leg extension; recorded `leg_extension` context | Knee extension; quadriceps; `thighs` | High retained. Rectus femoris/vasti distinction and alignment/hip-angle limitations are explicit. |
| `ex_617007f9-7420-4408-91b9-8ffb77900f13` | Seated Leg; recorded `seated_leg_curl` establishes curl interpretation beyond the ambiguous label | Knee flexion; hamstrings with possible gastrocnemius assistance; `thighs` | High retained. Equipment provenance identifies the variant; no name heuristic or identity merge. |
| `ex_a1ef5047-b44b-4c64-a6ed-c7a3bc13b163` | Seated leg curl; recorded `seated_leg_curl` | Knee flexion; hamstrings with gastrocnemius assistance; `thighs` | High retained. Hip-flexion length effect is confined to biarticular hamstrings; no outcome equivalence claimed. |
| `ex_d7398d9f-d928-4d2e-94e9-74e201da55c5` | Prone leg curl; recorded `prone_leg_curl` | Knee flexion; hamstrings with position-dependent gastrocnemius assistance; `thighs` | High retained. Prone/seated context remains distinct, without an unverified exact hip angle. |
| `ex_ec619fc2-4685-4044-873c-86764bd4a0fe` | Arm curl; recorded `arm_curl` | Elbow flexion; biceps/brachialis primary, brachioradialis secondary, plausible forearm stabilization; `arms` | High retained for qualitative family roles. Grip/shoulder support limitations prevent a universal quantitative ranking. |

The first five rows use established lower-limb anatomy. OpenStax's main text
identifies the adductors, knee extensors and knee flexors and distinguishes
lower-leg plantarflexors. Inconsistent image alternative text is not used to
reverse an anatomical action.
[OpenStax lower-limb anatomy](https://openstax.org/books/anatomy-and-physiology-2e/pages/11-6-appendicular-muscles-of-the-pelvic-girdle-and-lower-limbs)

The seated/prone curl distinction also has an intervention source which
explicitly considers biarticular hamstrings and biceps femoris short head.
Its longitudinal finding is not generalized to all people, machines or
strength tasks. [Maeo and colleagues](https://pubmed.ncbi.nlm.nih.gov/33009197/)

The curl mapping is supported by the anatomical elbow-flexor and forearm
functions. Its primary/secondary labels are qualitative exercise interpretation,
not ratios established by the textbook.
[OpenStax upper-limb anatomy](https://openstax.org/books/anatomy-and-physiology-2e/pages/11-5-muscles-of-the-pectoral-girdle-and-upper-limbs)

## Representation checks

- Custom Chest press, Converting chest press and Abdominal retain
  `interpretation: null`. Their useful candidate interpretations remain
  explicitly conditional and uncertain. Chest candidates include the reviewed
  `pectoralis_major` / `horizontal_push` interpretation only under stated
  execution assumptions.
- Marche retains a conditional gait interpretation, without creating an
  occurrence-to-treadmill association or assigning a persisted full-body zone.
  Both generic warm-up identities have neither ordinary nor candidate mapping.
- Rear Delt is linked to its actual UUID on the combined apparatus. Pec Fly
  is a separate unlinked capability. Assisted dip and assisted chin/pull-up
  are separate unlinked capabilities with empty actual exercise-ID lists.
  No capability or legacy manifest slug becomes a phantom exercise identity.
- The four added aggregates preserve member and overlap semantics. The
  functional hip-flexor and spinal-stabilizer groups are non-exhaustive.
  Their action lists describe member capabilities, not actions shared by every
  member. Anatomical regions remain `muscle_region`.
- Action contributors are non-exhaustive; missing a possible contributor does
  not claim absence of participation. Nominal planes are not trajectory
  constraints. Anti-extension, anti-rotation and lateral stabilization have
  no invented dynamic joint action and remain authored task conventions.
- Manufacturer/model fields are null for every physical identity. Example
  manufacturer references remain mechanics/identification examples rather
  than anatomical outcome evidence or local equipment identification.
- EMG findings remain distinct from intervention evidence and do not establish
  force shares, hypertrophy rankings or universal primary-muscle status.
  [Vigotsky and colleagues](https://pubmed.ncbi.nlm.nih.gov/29354060/)
- BODY ZONE audit `confirmed` means the existing coarse projection is
  scientifically compatible under the stated interpretation. It does not
  certify observed technique. `questionable` preserves conditional geometry
  or identity; it is not a direction to mutate persistence.
- Android's inspected ordinary query path obtains only
  `resolved_family_variant_limited` interpretations. Conditional/null records
  cannot enter ordinary muscle, pattern or scientific-zone matches through
  that guard. Broader runtime behavior remains the engineering review's scope.

## Remaining uncertainty and handoff

Exact local manufacturer/model, resistance curves, range settings and detailed
trajectories remain unresolved. Custom apparatus require actual execution
confirmation. Back Extension requires pelvic-restraint and spinal-versus-hip
motion evidence. Row/pulldown contributions depend on arm path and support;
hip-abduction contributions depend on hip angle. Rotary torso compatibility
must not be presented as a historical occurrence. Missing Pec Fly and assisted
exercise identities require real catalog creation or identity evidence before
linkage. Generic warm-ups require their actual activity sequences.

The scientific foundation supports contextual MAX interpretation only.
Assistance is not external resistance; no conversion, estimated 1RM, percentage
prescription, universal recovery duration or exercise-equivalence guarantee is
justified by these catalogs.

These uncertainties require observation or future scoped research. They do
not require xhigh escalation solely because the catalog is large. Scientific
implementation handoff: preserve the reviewed distinctions and current
confidence levels, and complete engineering validation separately. No BODY
ZONE migration is recommended.

## Reviewed catalog content hashes

SHA-256 values identify the exact reviewed catalog bytes. A subsequent change
to scientific fields requires a bounded delta review.

| File | SHA-256 |
|---|---|
| `science-references-v1.json` | `ceef31455e0c1da1bc383d4b3a7dc63198cfb62ba0f80e26bc79bae77abb7836` |
| `muscles-v1.json` | `21cb08c9c2ca393d5c87cf73a607175b7c267061687767912a0c92769be5ff4b` |
| `joint-actions-v1.json` | `a66737f4419bcc4040d0abe5ca525b2a185e80f0bcf679091222eb540b3fd8a1` |
| `movement-patterns-v1.json` | `ddd4e3522ebcb4df4618ed2a952fdb5834c42e21e7220a0afd69ab8f3437f072` |
| `exercise-knowledge-v1.json` | `30ee4f400626af9a39c0547ae28360db94e76736462501e113f57721429ef43f` |
| `equipment-knowledge-v1.json` | `b96ca23026fa1f0eb94bc8c0b2a586951130919db5342ee5d60ff1353725acb2` |

### Subsequent representation verification

The authored BODY ZONE audit was subsequently embedded in each exercise record
so both platforms and the audit generator can read it from the canonical source.
An automated comparison verified that removing only `body_zone_audit` reproduces
the exact reviewed exercise-catalog SHA-256 above. All 23 embedded annotations
also equal the reviewed audit data after the documented status and array-order
normalization. No scientific assertion or confidence changed. The complete
exercise catalog now has SHA-256
`a2c72e4743dc5dc24d1a7bbf9e8234442adc2d5ef6882f15cadea1cb9629d647`.
