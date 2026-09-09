# Exercise and equipment interpretation

Scientific knowledge is keyed to existing stable exercise and equipment
identities. Display labels are identification clues. They are not anatomical
proof, runtime classification rules or grounds for merging identities.
`catalog/exercise-knowledge-v1.json` separates reviewed interpretations from
conditional candidates; `catalog/equipment-knowledge-v1.json` describes
physical apparatus and its exercise capabilities.

The reviewed inventory contains 23 exercise identities and 41 equipment
identities: 38 supplied definitions and three durable custom entries. No
manufacturer or model has been confirmed for the local apparatus. Available
manufacturer examples remain examples; their ratios, trajectories and outcome
claims are not transferred to the local inventory.

## Reviewed exercise families

The following are biomechanical interpretations using the stated execution.
They are qualitative classifications, not measurements of individual force
contributions. Anatomy and the references linked below support the rationale.

| Actual catalog exercise | Interpretation | Existing primary / secondary BODY ZONES | Main qualification |
|---|---|---|---|
| Abdominal crunch | Resisted spinal flexion; rectus and obliques | core | Confirm that movement is not predominantly hip flexion |
| Arm curl | Elbow flexion; biceps, brachialis and brachioradialis | arms | Grip and shoulder support change participation |
| Back extension | Spinal extension; erector spinae and multifidus | back / core | Pelvic restraint determines hip involvement |
| Converging Shoulder Press | Overhead push; deltoids with elbow extension | shoulders / arms | Plane, seat support and scapular freedom unresolved |
| Diverged seated row; Seated row | Horizontal pull; humeral extension and scapular retraction | back / arms | Elbow path determines lat/scapular/posterior-deltoid emphasis |
| Diverging lat pulldown; Lat pull | Vertical pull; shoulder adduction/extension and elbow flexion | back / arms | Grip and linkage do not establish regional isolation |
| Hip abduction | Abduction involving medius/minimus and other abductors | glutes | Tensor fasciae latae and superior maximus contributions vary |
| Hip adduction | Resisted thigh approximation by hip adductors | thighs | Hip and knee angles affect individual muscles |
| Leg extension | Resisted knee extension by quadriceps | thighs | Rectus femoris also crosses hip |
| Leg press | Combined knee/hip extension | thighs / glutes | Machine, depth and foot placement alter moments |
| Prone leg curl | Knee flexion with prone support | thighs | Distinct length context from seated curl |
| Seated Leg; Seated leg curl | Knee flexion with seated support | thighs | Two existing identities retained; common family does not merge IDs |
| Rear Delt | Reverse fly: humeral horizontal abduction with scapular contribution | shoulders / back | Distinct from Pec Fly on the same device |
| Rotary torso | Relative thorax-pelvis rotation; paired oblique action | core | Compatible supplied equipment is not proven occurrence linkage |

Chest Press priority is addressed through a complete **conditional** anterior
press interpretation: pectoralis major as a primary mover, anterior deltoid and
triceps as synergists, shoulder horizontal adduction with elbow extension,
and `horizontal_push`. The custom `Chest press` and `Converting chest press`
identities have not independently established that execution. Their retained
chest/shoulders/arms relations are plausible historical mappings; the scientific
candidate stays uncertain until apparatus and motion are confirmed.
[NIH pectoralis anatomy](https://www.ncbi.nlm.nih.gov/books/NBK525991/),
[OpenStax upper-limb anatomy](https://openstax.org/books/anatomy-and-physiology-2e/pages/11-5-muscles-of-the-pectoral-girdle-and-upper-limbs)

The custom `Abdominal` entry similarly retains a conditional crunch candidate,
not an asserted execution. Both generic warm-up identities remain unresolved.
`Marche` has a conditional walking interpretation; a continuous/duration profile
and an aggregate treadmill inventory do not establish all execution details or
a specific occurrence association. Unknown new custom exercises remain
unclassified until evidence is added explicitly.

Seated versus prone curl has direct longitudinal evidence: the studied seated
condition produced greater growth of biarticular hamstrings. This supports
retaining length context while avoiding a universal machine or outcome ranking.
[Maeo and colleagues](https://pubmed.ncbi.nlm.nih.gov/33009197/)

The leg-press review concerns EMG. It supports quadriceps involvement but does
not establish a robust universal foot-placement recipe or prove that hamstring
coactivation replaces knee-flexion training.
[Martín-Fuentes and colleagues](https://pubmed.ncbi.nlm.nih.gov/32605065/)

Reverse pec-deck and row/pulldown investigations concern selected EMG tasks.
They corroborate plausible posterior-deltoid and scapular contributions without
ranking long-term growth. Divergent/convergent machine labels add no independent
outcome evidence.
[Franke and colleagues](https://pubmed.ncbi.nlm.nih.gov/24947920/),
[Lehman and colleagues](https://pubmed.ncbi.nlm.nih.gov/15228624/)

Pelvic stabilization changes lumbar-extensor excitation. A Back Extension
execution dominated by hip motion may deserve different muscle roles from the
spinal-extension interpretation; that decision requires actual geometry and
execution evidence.
[Lee](https://pubmed.ncbi.nlm.nih.gov/26730390/)

## Multifunction and unrestricted equipment

A Rear Delt / Pec Fly device supports at least two distinct tasks. Rear Delt
uses shoulder horizontal abduction and a shoulder/back projection. Pec Fly
uses horizontal adduction, with a chest emphasis and possible anterior-deltoid
assistance. Pec Fly has no verified local exercise ID. It remains an unlinked
capability, not a fabricated catalog exercise.

The assistance device likewise supports separate assisted dip and assisted
chin/pull-up capabilities. Neither has a verified local exercise ID. The
legacy `assisted_dip` and `assisted_chin` strings in the equipment manifest are
not creator exercise identities. Chin-up and pronated pull-up also require
specific grip/execution context. Manufacturer catalogs demonstrate that such
combination products exist, not that these examples identify the local model.
[Life Fitness equipment catalogue](https://www.lifefitness.com.au/wp-content/uploads/2024/02/Life-Fitness-Catalogue_2024_web.pdf)

Functional trainers, adjustable pulleys, dumbbells, kettlebells, bags,
medicine balls and suspension straps require the actual exercise. Their
presence cannot establish a primary BODY ZONE. Cable height and routing,
attachment, stance and line of pull determine the task. A documented example
with a 4:1 cable ratio illustrates why local ratios must be verified separately.
[Precor RUD0915](https://www.precor.com/en-US/products/RUD0915)

Benches and guided squat machines require support and movement details. Cardio
apparatus requires mode, speed/cadence, resistance and support context. A rowing
ergometer includes a leg/trunk/arm cycle and is not the same exercise as a
seated resistance row. Battle ropes have mass and inertia: their existing
`bodyweight` catalog category is retained as legacy metadata, not a calibrated
physical load model. Any future vocabulary change needs a separate explicit
compatibility decision.

## BODY ZONE audit result

No reviewed finding justifies an automatic persisted-zone change. Existing
assignments are compatible with the identified families or remain conditional
where observation is missing. Additional potential synergists are documented
at muscle level. A future correction must state its evidence, affected stable
identities and explicit migration separately; enrichment never silently
rewrites historical mappings.
