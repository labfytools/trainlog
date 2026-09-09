# Functional anatomy and movement knowledge

TRAINING KNOWLEDGE V1 separates anatomy from the exercise, its execution and
its physical equipment. A muscle can move a joint, assist another mover or
stabilize a segment; its role changes with posture, resistance direction and
movement phase. The catalogs record qualitative roles, not force percentages,
effective-set fractions or physiological measurements.

The scientific references are in `catalog/science-references-v1.json`.
Functional entities, joint actions and authored movement conventions belong in
`catalog/muscles-v1.json`, `catalog/joint-actions-v1.json` and
`catalog/movement-patterns-v1.json`. A muscle region or muscle group is not a
new anatomical muscle. Parent groups and their component muscles must not be
summed as independent exposure.

## Evidence and confidence

Every interpretation distinguishes established anatomy, biomechanical
interpretation, EMG evidence, intervention evidence, manufacturer statements
and practical inference. Anatomy explains plausible function; longitudinal
training studies address adaptation. Surface EMG measures a signal affected
by recording and physiological conditions. Greater amplitude does not establish
greater muscle force, hypertrophy, strength improvement or universal primary
muscle status. [Vigotsky and colleagues](https://pubmed.ncbi.nlm.nih.gov/29354060/)

Confidence uses exactly `high`, `moderate` and `uncertain`. Established anatomy
can have high confidence while its application to an unobserved machine variant
remains uncertain. Moderate confidence is appropriate when the exercise family
is clear but geometry changes secondary or stabilizing roles. Missing evidence
is retained explicitly, not replaced with a commercial-name rule.

## Functional muscle coverage

The catalog covers these functional distinctions:

| Area | Functional distinctions |
|---|---|
| Chest | Pectoralis major, clavicular and sternocostal regions; pectoralis minor as a scapular muscle |
| Back and scapula | Latissimus dorsi, teres major, trapezius regions, rhomboids and serratus anterior |
| Shoulder | Anterior, middle and posterior deltoid; supraspinatus, infraspinatus, teres minor and subscapularis |
| Arms and forearms | Elbow flexors, triceps, grip/wrist flexors and extensors, pronation and supination |
| Trunk | Rectus abdominis, obliques, transversus abdominis, erector spinae, multifidus and quadratus lumborum |
| Hip | Gluteal muscles, tensor fasciae latae, iliopsoas and adductors |
| Thigh | Quadriceps with biarticular rectus femoris distinguished from vasti; biarticular hamstrings distinguished from biceps femoris short head |
| Lower leg | Gastrocnemius, soleus and tibialis anterior |

Upper-limb anatomy supports shoulder, scapular, elbow and forearm distinctions.
Scapular movement is not interchangeable with glenohumeral movement. Stabilizing
the humeral head is not the same task as dynamically rotating the shoulder.
[OpenStax upper-limb anatomy](https://openstax.org/books/anatomy-and-physiology-2e/pages/11-5-muscles-of-the-pectoral-girdle-and-upper-limbs)

Pectoral regions share actions but differ in orientation and contribution across
shoulder positions. Their existence does not establish separate isolatable
“upper” and “lower” chest muscles. Deltoid regions likewise have different
lines of action; the movement identifies the likely emphasis.
[NIH pectoralis anatomy](https://www.ncbi.nlm.nih.gov/books/NBK525991/),
[NIH deltoid anatomy](https://www.ncbi.nlm.nih.gov/books/NBK537056/)

The hip and knee distinctions are essential: knee extension and knee flexion
are different functions despite both mapping to `thighs`. Gastrocnemius crosses
the knee and ankle; soleus does not cross the knee. Hip flexion changes the
length of biarticular hamstrings but not biceps femoris short head.
[OpenStax lower-limb anatomy](https://openstax.org/books/anatomy-and-physiology-2e/pages/11-6-appendicular-muscles-of-the-pelvic-girdle-and-lower-limbs),
[Maeo and colleagues](https://pubmed.ncbi.nlm.nih.gov/33009197/)

Gluteus minimus abducts and stabilizes the hip. Some lower-limb textbook figure
alternative text describes direction inconsistently, including minimus and
adductor examples; those descriptions are not copied as anatomical truth.
The NIH account corroborates the minimus classification.
[NIH gluteus minimus anatomy](https://www.ncbi.nlm.nih.gov/books/NBK556144/)

Spinal flexion/extension and hip flexion/extension must remain separate.
Abdominal-wall muscles combine movement and tension/control functions, while
posterior spinal muscles contribute extension and segmental control.
[OpenStax spinal anatomy](https://openstax.org/books/anatomy-and-physiology-2e/pages/11-3-axial-muscles-of-the-head-neck-and-back),
[OpenStax trunk anatomy](https://openstax.org/books/anatomy-and-physiology-2e/pages/11-4-axial-muscles-of-the-abdominal-wall-and-thorax),
[NIH abdominal-wall anatomy](https://www.ncbi.nlm.nih.gov/books/NBK525975/)

## Actions, patterns and stabilization

Joint actions describe motion. Their definitions include shoulder and scapular
actions, elbow flexion/extension, forearm rotation, hip actions, knee actions,
ankle actions and trunk flexion/extension/rotation/lateral flexion.
A loaded return may reverse the visible joint motion while the same agonists
control it eccentrically.
[OpenStax movement terminology](https://openstax.org/books/anatomy-and-physiology-2e/pages/9-5-types-of-body-movements)

Horizontal/vertical push and pull, knee dominant, hip dominant and single-joint
patterns are Trainlog programming conventions grounded in those actions.
They are not universally standardized anatomical categories or exact torque
ratios. A single-joint exercise can involve many muscles and stabilizers.
A dip grouped as a vertical push is still mechanically different from an
overhead press.

Trunk stabilization describes resisting an external moment while limiting
motion. Anti-extension, anti-rotation and anti-lateral-flexion are task demands,
not invented joint movements. Locomotion, cyclic pedaling and cyclic rowing
remain distinct; a cardio profile alone does not identify the action sequence.

## BODY ZONES projection

`catalog/body-zones-v1.json` remains the authoritative UX taxonomy. Its zones
are coarser than functional anatomy. Forearm muscles fall under `arms`,
posterior trunk muscles can relate to both `back` and `core`, and the lateral
thorax/scapular function of serratus anterior does not fit a simple surface
location rule. Scientific muscle-level projections explain these conventions;
they do not alter persisted exercise relations.

A secondary BODY ZONE need not list every accessory or stabilizing muscle.
Absence of `shoulders` on a row does not deny posterior-deltoid participation.
Absence of `thighs` on hip abduction does not deny tensor fasciae latae
participation. `full_body` is not an automatic synonym for cardio or a command
to mark every zone.
