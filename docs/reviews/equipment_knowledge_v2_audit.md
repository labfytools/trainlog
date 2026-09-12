# EQUIPMENT_KNOWLEDGE_V2 audit

Date: 2026-09-11. Scope: all 41 records in `equipment-knowledge-v1.json`, checked
against the supplied manifest, current UUID exercise knowledge, and the canonical
V2 relations. `S` means supplied; `C` means custom. Sources are existing catalog
references only. An empty option is intentional and never implies anatomy.

| Equipment (ID — name) | Origin / load | Classification | Canonical exercise options; confidence; evidence | Missing question |
|---|---|---|---|---|
| abdominal — Abdominal / Crunch | S / external | RESOLVED_SINGLE_EXERCISE | Abdominal crunch; moderate; `nih_abdominal_wall`, `openstax_trunk` | Local linkage/trajectory |
| abdominal_bench — Banc à abdominaux | S / bodyweight | GENERIC_MULTI_PURPOSE | none | Crunch or sit-up execution? |
| arm_curl — Curl biceps | S / external | RESOLVED_SINGLE_EXERCISE | Arm curl; high; `openstax_upper` | Grip/cam/setup |
| assisted_dip_chin_machine — Chin / Dip Assist | S / assistance | PARTIALLY_RESOLVED | none: assisted dip and assisted chin are distinct capabilities, but neither has a resolved UUID exercise record | Create/identify each canonical exercise separately |
| back_extension — Extension lombaires | S / external | RESOLVED_SINGLE_EXERCISE | Back extension; moderate; `lee_2015`, `openstax_back`, `openstax_lower` | Local axis/ROM |
| battle_ropes — Cordes ondulatoires | S / bodyweight | GENERIC_MULTI_PURPOSE | none | Actual exercise protocol? |
| converging_shoulder_press — Presse épaules | S / external | RESOLVED_SINGLE_EXERCISE | Converging Shoulder Press; moderate; `nih_deltoid`, `openstax_upper` | Local handle/path |
| diverging_lat_pulldown — Tirage vertical divergent | S / external | RESOLVED_SINGLE_EXERCISE | Diverging lat pulldown; moderate; `lehman_2004`, `openstax_upper`, `vigotsky_2018` | Grip/path |
| diverging_seated_row — Tirage horizontal divergent | S / external | RESOLVED_SINGLE_EXERCISE | Diverged seated row; moderate; `franke_2015`, `lehman_2004`, `openstax_upper` | Grip/path |
| dual_adjustable_pulley — Double poulie | S / external | GENERIC_MULTI_PURPOSE | none | Actual exercise and configuration? |
| dumbbells — Haltères | S / external | GENERIC_MULTI_PURPOSE | none | Actual exercise? |
| eq_0a462c1e-9fb6-4fd7-b0a2-53a0e86f33c6 — custom chest press | C / runtime-owned | UNRESOLVED | none | Explicit verified canonical link unavailable in current schema |
| eq_3f987a36-bbb4-4e29-9c9f-1f201f1596e0 — custom abdominal | C / runtime-owned | UNRESOLVED | none | Explicit verified canonical link unavailable in current schema |
| eq_c660cd61-5b17-498e-8d51-9eafcf7e2731 — custom converting press | C / runtime-owned | UNRESOLVED | none | Explicit verified canonical link unavailable in current schema |
| functional_trainer — Functional Trainer | S / external | GENERIC_MULTI_PURPOSE | none | Actual exercise/configuration? |
| hip_abduction — Abducteurs | S / external | RESOLVED_SINGLE_EXERCISE | Hip abduction; moderate; `nih_gluteus_minimus`, `openstax_lower` | Local setup |
| hip_adduction — Adducteurs | S / external | RESOLVED_SINGLE_EXERCISE | Hip adduction; high; `openstax_lower` | Local setup |
| indoor_cycle — Vélo indoor | S / cardio | PARTIALLY_RESOLVED | none | Which canonical continuous exercise? |
| kettlebells — Kettlebells | S / external | GENERIC_MULTI_PURPOSE | none | Actual exercise? |
| lat_pull — Tirage vertical | S / external | RESOLVED_SINGLE_EXERCISE | Lat pull; moderate; `lehman_2004`, `openstax_upper`, `vigotsky_2018` | Grip/path |
| lat_pulldown — Lat Pulldown | S / external | PARTIALLY_RESOLVED | none | Distinguish from current Lat pull identity |
| leg_extension — Extension des jambes | S / external | RESOLVED_SINGLE_EXERCISE | Leg extension; high; `openstax_lower` | Local axis/setup |
| leg_press — Presse à cuisses | S / external | RESOLVED_SINGLE_EXERCISE | Leg press / Presse à cuisses; moderate; `martin_fuentes_2020`, `openstax_lower`, `vigotsky_2018` | Local geometry |
| low_row — Tirage horizontal bas | S / external | PARTIALLY_RESOLVED | none | Does an existing row exercise identity match exactly? |
| medicine_ball — Medicine Ball | S / external | GENERIC_MULTI_PURPOSE | none | Actual exercise? |
| perfect_squat — Squat guidé | S / external | PARTIALLY_RESOLVED | none | Canonical exercise identity and local mechanics? |
| plate_loaded_leg_press — Presse à cuisses à disques | S / external | RESOLVED_SINGLE_EXERCISE | Leg press / Presse à cuisses; moderate; same existing leg-press sources | Local geometry/load equivalence |
| prone_leg_curl — Leg curl allongé | S / external | RESOLVED_SINGLE_EXERCISE | Prone leg curl; high; `maeo_2021`, `openstax_lower` | Local setup |
| rear_delt_pec_fly — Oiseaux / Pec Fly | S / external | PARTIALLY_RESOLVED | Rear Delt only, configuration “Oiseaux / Rear Delt”; moderate; existing rear-delt sources | No canonical chest-fly UUID exists; do not fabricate one |
| recumbent_bike — Vélo semi-allongé | S / cardio | PARTIALLY_RESOLVED | none | Canonical continuous exercise? |
| rotary_torso — Rotation du buste | S / external | RESOLVED_SINGLE_EXERCISE | Rotary torso; moderate; `nih_abdominal_wall`, `openstax_back`, `openstax_trunk` | Local axis/ROM |
| rowing_machine — Rameur | S / cardio | PARTIALLY_RESOLVED | none | Canonical continuous rowing identity? |
| seated_dip — Dips assis | S / external | PARTIALLY_RESOLVED | none | Canonical seated-dip identity? |
| seated_leg_curl — Leg curl assis | S / external | RESOLVED_SINGLE_EXERCISE | Seated leg curl / Flexion de genou assise; high; `maeo_2021`, `openstax_lower` | Local setup |
| seated_row — Tirage horizontal assis | S / external | RESOLVED_SINGLE_EXERCISE | Seated row; moderate; `franke_2015`, `lehman_2004`, `openstax_upper` | Grip/path |
| stair_climber — Escalier | S / cardio | PARTIALLY_RESOLVED | none | Canonical continuous exercise? |
| suspension_trainer — Sangles | S / bodyweight | GENERIC_MULTI_PURPOSE | none | Actual exercise/setup? |
| treadmill — Tapis de course | S / cardio | PARTIALLY_RESOLVED | none | Walk, run, incline, and canonical identity? |
| upright_bike — Vélo droit | S / cardio | PARTIALLY_RESOLVED | none | Canonical continuous exercise? |
| vertical_chest_press — Presse pectoraux | S / external | PARTIALLY_RESOLVED | none | Existing chest-press records are conditional/custom, not this supplied identity |
| weighted_bag — Sac lesté | S / external | GENERIC_MULTI_PURPOSE | none | Actual exercise? |

`Seated Leg` (`ex_617007f9-7420-4408-91b9-8ffb77900f13`) remains explicitly
unresolved in V2 and receives no relation. It is not “Flexion de genou assise”
and receives no MAX, history, equipment, or anatomy transfer.

Future identification is documentation only: manufacturer/model/name plus an
optional photo, pictogram, and notes may create a proposal; the user verifies
the proposed identity before it is persisted. V2 performs no OCR or image
recognition and the current schema has no custom-equipment link field.
