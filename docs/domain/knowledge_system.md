# Training knowledge system V1

`TRAINING_KNOWLEDGE_V1` is an implemented read-only, evidence-linked knowledge
layer with lifecycle status `TRAINING_KNOWLEDGE_V1=PASS`. Its bounded scientific
review, independent temporal review, final engineering audit, repair
verification, and final executable validation passed. It does not change the frozen
exchange formats, either SQLite schema, synchronization,
or the meaning of an exercise, BODY ZONE, occurrence, set, or measured maximum.

The authored scientific source is the six versioned JSON catalogs in
[`catalog/`](../../catalog/):

- `science-references-v1.json` (23 references);
- `muscles-v1.json` (53 muscles);
- `joint-actions-v1.json` (35 joint actions);
- `movement-patterns-v1.json` (26 movement patterns);
- `exercise-knowledge-v1.json` (23 exercise records);
- `equipment-knowledge-v1.json` (41 equipment records).

The catalog loader and validator enforce IDs, ordering and cross-references.
The C representation is generated from those assets; Android loads the same
assets through `TrainingKnowledgeCatalog`. Neither C nor Kotlin contains a
manually maintained duplicate scientific table. The generated knowledge audit
is an audit artifact and is not an authored source or a file to edit directly;
the retained review-oriented [knowledge audit](knowledge_audit.md) and
[`training-knowledge-audit-v1.json`](../../catalog/training-knowledge-audit-v1.json)
provide the current navigable audit records.

The catalog contains no runtime history and does not seed either database.
Unknown future exercise and equipment IDs remain valid runtime data. A display
name, legacy slug, equipment family, or catalog label never substitutes for an
actual `ex_<uuid-v4>` exercise ID. Consequently the documented capabilities
for Pec Fly, Assisted Dip and Assisted Chin do not create associations until a
real runtime exercise UUID exists. The two Leg press equipment variants remain
separate contexts; the unobserved Rotary compatibility never enters history.

## Scientific scope and uncertainty

Scientific mappings describe anatomy, mechanics, movement patterns and the
BODY ZONE projection used by the knowledge catalog. They do not overwrite the
persisted primary/secondary BODY ZONE relations selected for a runtime
exercise. Source type, notes, limitations, confidence and conditional
interpretation remain available through the APIs.

The current inventory has six high-confidence, eleven moderate-confidence and
six uncertain exercise records. It represents 20 initial zone-catalog exercise
IDs plus three further reviewed IDs, 38 supplied equipment IDs and three
observed custom IDs. The BODY ZONE audit has 16 confirmed, four questionable
and three unresolved records. It records no database mutation. `Marche` is a
conditional candidate and remains excluded by normal resolved-knowledge
filters; two warmups and the custom Chest press, Converting and Abdominal
entries remain unresolved until their execution is known. Across equipment,
21 records are scientifically documented, 17 are mechanically identified with
incomplete anatomy, and three have uncertain equipment identity. All 41 have
unknown manufacturer and model: a generic-family source does not prove a local
machine model.

The detailed evidence and limitations are in
[anatomy and movement](anatomy_and_movement.md),
[exercise and equipment interpretation](exercise_equipment_interpretation.md),
[programming foundations](programming_foundations.md), and the central
[`science-references-v1.json`](../../catalog/science-references-v1.json).
Project domain decisions follow the
[`trainlog-anatomy` guidance](../../.agents/skills/trainlog-anatomy/SKILL.md).

## Read-only application contracts

On desktop, [`training_knowledge.h`](../../tui/include/trainlog/training_knowledge.h)
provides immutable lookup, enumeration and resolved-knowledge query APIs. All
strings and records are borrowed generated storage with process-lifetime
validity; stable IDs, rather than labels, are keys. The query AND-combines its
optional scientific-zone, movement-pattern, muscle-role and available-equipment
filters. Conditional and unresolved entries cannot match it.

[`training_context.h`](../../tui/include/trainlog/training_context.h) composes
one exact runtime `exercise_id` with its persisted zones, optional scientific
record, compatible equipment, latest explicit MAX, and actual occurrence/set
history. Missing science is valid; it never fabricates history. One load uses a
nested-safe read snapshot. It accepts an occurrence limit of 1–32 and a set
preview limit of 1–64 per occurrence; empty history remains empty. Follow-up cursors order current data by the
exact represented instant, then bytewise session ID and occurrence ID; they are not a
snapshot across calls, including when equivalent instants use different
offset text.

The corrected C and Android readers use one explicit parser/comparator policy
for stored timestamps and exclusive cursors. They admit extended dates,
`T/t`, `Z/z`, offsets through `23:59`, arbitrary exact dot fractions and the
Android writer's omitted-seconds form. They scan all matching metadata and
retain bounded winners before hydration; no SQLite date parser selects the
page. Source text and identities remain unchanged. Malformed timestamps fail
explicitly, as does a selected value beyond C's existing output capacity.
The [temporal correction record](../reviews/training_knowledge_v1_temporal_contract.md)
documents parser-only extensions, limits and passing production regressions.

Android exposes the same scientific lookups through `TrainingKnowledgeCatalog`
and composes the same boundary through
`TrainlogRepository.getTrainingExerciseContext()`,
`listExerciseOccurrences()` and `listExerciseOccurrenceSets()`. Its cursors
have the same current-data, chronological contract. Returned runtime context
keeps persisted zone IDs separate from the scientific mapping, includes actual
equipment and the latest explicit MAX, and preserves raw per-set values.

No API infers range of motion, setup, actual force, or comparability of raw
kilogram labels. Equipment, resistance semantics and execution context remain
explicit. There are no prescriptions in V1.

## Documented future pipeline only

The following is an architectural boundary for later work, not a V1 generator,
scoring algorithm, proposal, database change, or user-interface behavior.

```text
Session inputs
  BODY ZONE, duration, goal, available equipment, recent history
    -> movement functions
    -> real resolved candidates
    -> explicit availability
    -> recent history and explicit-MAX context
    -> future fatigue/recent-coverage interpretation
    -> a future proposal
```

Candidate selection must seek diverse movement patterns rather than repeatedly
selecting the same muscle. A future multi-session program would additionally
take the goal, available days, functions/zones, recent frequency, recovery,
progression, equipment and history, then reason across sessions. Medical
constraints would be explicit inputs; no diagnosis or rehabilitation status is
to be inferred. V1 prescribes no exercises, weights, sets, fatigue scores,
progression or schedule.
