# EXERCISE_NAMING_V1 audit

The mapping in `catalog/exercise-names-v1.json` is a presentation-metadata
mapping keyed solely by stable `exercise_id`. It does not create IDs, merge
rows, or rewrite session observations. Its evidence is the resolved movement
interpretation in `catalog/exercise-knowledge-v1.json`.

| id | current | proposed | status / confidence | action |
| --- | --- | --- | --- | --- |
| ex_01ff06dd-ad00-46ee-9b46-ed32cabedbef | Hip adduction | Adduction de hanche assise | resolved/high | SAFE_RENAME |
| ex_1872246a-39ae-44dc-b58d-f87e90ca49ab | Leg extension | Extension de genou assise | resolved/high | SAFE_RENAME |
| ex_1a34814c-2e46-40fc-b1f4-6d60b8e5a3e0 | Rotary torso | Rotation du tronc à la machine | resolved/moderate | SAFE_RENAME |
| ex_1b0c6b8b-b05e-4e6f-8809-5f7d85d668de | Diverged seated row | Tirage horizontal divergent assis | resolved/moderate | SAFE_RENAME |
| ex_33f79331-871c-4eed-babe-346e53a99070 | Seated row | Tirage horizontal assis | resolved/moderate | SAFE_RENAME |
| ex_474ec393-3efa-4aaa-8e08-1a0245ed7835 | Back extension | Extension du tronc | resolved/moderate | SAFE_RENAME |
| ex_4cd2433e-80b1-478a-b8df-73fc6ef80962 | Rear Delt | Écarté inversé à la machine | resolved/moderate | SAFE_RENAME |
| ex_6dfc7ffd-8891-464e-a995-808baf1b0d7b | Converging Shoulder Press | Développé épaules convergent | resolved/moderate | SAFE_RENAME |
| ex_7e7cf906-2214-4066-bcb7-c16382d83b3b | Hip abduction | Abduction de hanche assise | resolved/moderate | SAFE_RENAME |
| ex_9adf7566-f10c-443c-b63b-681d37665693 | Abdominal crunch | Crunch à la machine | resolved/moderate | SAFE_RENAME |
| ex_a1ef5047-b44b-4c64-a6ed-c7a3bc13b163 | Seated leg curl | Flexion de genou assise | resolved/high | SAFE_RENAME |
| ex_a72fa713-4b0e-431d-95e2-42d95beb77b1 | Lat pull | Tirage vertical à la poulie | resolved/moderate | SAFE_RENAME |
| ex_b432623f-bfe9-4daf-a653-60ec7fdffbde | Leg press | Presse à cuisses | resolved/moderate | SAFE_RENAME |
| ex_b4d1daf1-de4a-4016-abdf-487bf6014ce6 | Diverging lat pulldown | Tirage vertical divergent | resolved/moderate | SAFE_RENAME |
| ex_d7398d9f-d928-4d2e-94e9-74e201da55c5 | Prone leg curl | Flexion de genou couchée | resolved/high | SAFE_RENAME |
| ex_ec619fc2-4685-4044-873c-86764bd4a0fe | Arm curl | Flexion de coude à la machine | resolved/high | SAFE_RENAME |

`Abdominal`, `Chest press`, and `Converting chest press` are conditional with
uncertain identity and are intentionally unchanged. `Gym échauffement`,
`Marche`, `Planche`, and custom floor variants are human-readable or have no
resolved catalog mapping and remain unchanged. The user explicitly confirmed
that `Seated Leg` (`ex_617007f9-7420-4408-91b9-8ffb77900f13`) and `Seated leg
curl` (`ex_a1ef5047-b44b-4c64-a6ed-c7a3bc13b163`) are distinct exercises on
different physical machines. They are **NOT_DUPLICATE**: no merge, shared MAX,
shared performance context, or history collapse is permitted. `Seated Leg`
therefore remains unchanged until its exact identity has independent evidence;
only the independently named `Seated leg curl` receives the resolved seated
knee-flexion label.
