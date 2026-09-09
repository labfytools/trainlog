---
name: trainlog-anatomy
description: Use Trainlog's cited anatomy and biomechanics knowledge for exercise targeting, BODY ZONES, equipment interpretation, substitution, MAX context, and future workout or program design.
---

# Trainlog anatomy

Resolve domain decisions from scientific evidence before implementation.
Start at `docs/domain/` from the repository root and consult the relevant
canonical assets in `catalog/`:

- `science-references-v1.json`: evidence, bibliographic metadata and limitations;
- `muscles-v1.json` and `joint-actions-v1.json`: functional anatomy;
- `movement-patterns-v1.json`: explicitly defined programming abstractions;
- `exercise-knowledge-v1.json`: variant-specific roles and BODY ZONE projection;
- `equipment-knowledge-v1.json`: physical equipment and compatible exercises.

Follow `source_refs` to the cited references. Check that evidence supports the
actual variant and claim; structural validation is not scientific validation.
Use `anatomie` for substantive new research and `anatomie-xhigh` only when one
substantive investigation leaves material scientific ambiguity unresolved.
Scientific conclusions must be settled before implementation agents encode them.

Preserve these distinctions:

- Physical machine != exercise != movement pattern != joint action != muscle.
- BODY ZONE is a business/UX projection, not a complete anatomical taxonomy.
- Higher EMG amplitude does not establish better hypertrophy, strength outcomes,
  or universal primary-muscle status.
- Manufacturer documentation identifies mechanics; it does not establish
  anatomical outcomes. Do not invent a manufacturer, model or trajectory.

Separate established anatomy, biomechanical interpretation, EMG, intervention
evidence, manufacturer statements and practical inference. Retain `high`,
`moderate` or `uncertain` confidence; downgrade unsupported claims. Unknown
custom exercises stay unclassified until their actual execution is established.

Use existing exercise/equipment identities. Never infer runtime knowledge from
names, merge multifunction-machine exercises, overwrite persisted BODY ZONES,
or copy user history into scientific catalogs. MAX belongs to an exercise and
its equipment/resistance/execution context; assistance is not external load.

TRAINING KNOWLEDGE V1 provides read-only knowledge and history composition.
Workout/program prescription and clinical rehabilitation are outside its scope.
