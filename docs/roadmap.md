# Roadmap

This file is the canonical product roadmap for Trainlog.

Historical implementation detail belongs in Git history and `docs/reviews`.
The published Trainlog JSON v1 compatibility boundary remains frozen unless a
future feature explicitly introduces a new version.

## Completed foundation

```text
GATE_0=PASS
GATE_1=PASS
GATE_2=PASS

TRAINLOG_FORMAT_V1=FROZEN
DESKTOP_SCHEMA_V11=PASS
ANDROID_LOCAL_DATABASE_V10=PASS
ANDROID_LOCAL_DATABASE_V11=PASS
SESSION_GENERATOR_V1=PASS

DIRECT_MTP_TRANSPORT=PASS
BIDIRECTIONAL_SYNC_V1=PASS

VARIABLE_REPETITION_SETS=PASS
MEASURED_MAX_V1=PASS
EXPLICIT_MAX_RESULTS_V1=PASS
MAX_TEST_RESUME_STABLE_ID=PASS
BODY_ANALYTICS_V1=PASS
EXERCISE_EDIT_V1=PASS
ANDROID_BANNER_PARITY_V1=PASS
BODY_ZONES_V1=PASS
BODY_ZONE_SYNC_V1=PASS
BODY_ZONES_ANDROID_DEVICE_VALIDATION=PASS

DESKTOP_TESTS=45/45 PASS (latest validated checkpoint)
TUI_NOTCURSES_V1=PASS
NCURSESW_REMOVED_FROM_ACTIVE_TUI=PASS
NOTCURSES_TRUECOLOR_THEME=PASS
```

The current product baseline includes:

- SQLite persistence and explicit migrations;
- usable Notcurses desktop TUI;
- native Android capture client;
- exercise catalog;
- canonical hierarchical body zones, primary/secondary relations and filters;
- profile-aware set and continuous activity;
- heterogeneous repetition sets;
- session history and editing;
- body measurements and body history;
- measured-max sessions and measured-max analysis;
- explicit max-weight capture without synthetic sets, with stable-ID continuation;
- desktop body analytics;
- direct USB/MTP transport;
- bidirectional Android/PC synchronization;
- safe V2 exercise-identity reconciliation with richer compatible profiles;
- supplied and custom equipment definitions with occurrence-level links;
- shared synchronization engine and `trainlog-syncd`.

`BODY_ZONES_V1` is complete infrastructure for session generation: one shared
manifest, desktop v11/Android v10 relations, Android/TUI edit and display,
descendant-aware filters, unclassified history and one explicit-conflict sync
companion. The implemented generator uses this infrastructure; custom zones are
still outside this checkpoint and proposed loads remain editable plans rather
than actual work.

`TRAINING_KNOWLEDGE_V1=PASS` is read-only infrastructure. Scientific review,
independent temporal review, final engineering review, repair verification, and
final executable validation passed. The temporal contract preserves source text
and exact C/Android chronological pagination; one bounded repair chain closed
the audit's stale-documentation, Android-loader, Meson-input, and role-only C
query findings.
It provides catalog-backed scientific lookup and runtime context composition
without prescriptions, schema changes or catalog seeding. The implemented
separate session generator consumes that boundary; multi-session programming is
still not a roadmap gate. See
[Training knowledge system V1](domain/knowledge_system.md).

`EXERCISE_EDIT_V1` is a completed capture correction: Android permits
stable-ID renames, protects referenced profiles, and reconciles same-ID display
metadata without duplicates. `ANDROID_BANNER_PARITY_V1` is a presentation-only
completed checkpoint: all Android screens share the Notcurses-derived compact
header; it does not reorder the roadmap below.

## Product boundary

`ANDROID_SESSION_DRAFT_V1` is an implemented P0 capture-reliability correction:
one durable active draft, Home resume, explicit discard and atomic completion.
Host and device validation and the final tranche review pass. This repair
does not introduce planning/templates or reorder the product roadmap below.

The intended split remains:

```text
ANDROID
= fast capture during real use

DESKTOP TUI
= planning
+ catalog management
+ history
+ analytics
+ long-term decision support
```

Android must not become the main analytics surface unless this boundary is
explicitly revised later.

## Operational cursor — real data baseline

The immediate priority is not another implementation block.

Trainlog now needs real measurements and real training sessions so later
analytics are based on useful data rather than fixtures.

```text
CURRENT_OPERATIONAL_CURSOR=REAL_DATA_BASELINE_V1
```

Expected baseline:

- first complete real body observation;
- first real training sessions;
- Android used for capture;
- Android -> PC synchronization after training;
- measured max recorded only through explicit `max_test` results;
- no fictitious user data in the canonical databases.

Gate:

```text
REAL_DATA_BASELINE_V1=PASS
```

## Next feature — gym catalog v1

```text
NEXT_FEATURE=GYM_CATALOG_V1
```

The catalog must be based on the equipment actually available in the user's
gym rather than on a generic Internet exercise list.

The initial inventory will be built from a complete photo survey of the gym.

For each useful equipment item, record where applicable:

```text
gym zone
equipment family
machine name
manufacturer/model if identifiable
useful duplicate count
supported exercise(s)
Trainlog recording profile
load semantics
primary body zone (BODY_ZONES_V1)
secondary body zones (BODY_ZONES_V1)
```

Important rule:

```text
one photographed machine != one exercise
```

A single piece of equipment may support multiple exercises. Equipment and
exercise identity must remain distinct concepts.
The primary/secondary body-zone dimension is already implemented by Body Zones
V1; gym inventory should link stable exercise identities to that model rather
than inventing another free-text body-region field.

Gate:

```text
GYM_CATALOG_V1=PASS
```

## Exercise metadata v1

After the real gym inventory is normalized, enrich the exercise catalog with
structured metadata needed by planning and analytics.

Remaining candidate metadata:

```text
equipment
movement_family
laterality
```

The final schema must be designed before implementation. Do not encode these
concepts into names or free-form notes as a substitute for a real model.
Body-zone metadata is no longer future scope here; its frozen V1 taxonomy and
direct relations must be reused.

Gate:

```text
EXERCISE_METADATA_V1=PASS
```

## Session generator v1

`SESSION_GENERATOR_V1=PASS`.
It provides
a bounded policy-driven generator in both UIs, then hands accepted nonempty
proposals to normal draft/editor flows where actual work is captured separately.
It does not create reusable templates, a planned-session sync product surface,
or a multi-session program. The one deep final audit found repairable gaps;
its bounded repairs, review, and final validation matrix passed.

## Session templates v1

Templates are built from the real exercise catalog and planning model.

Candidate examples:

```text
glutes + arms
back + shoulders
chest + arms
legs
full body
cardio
```

Templates are conveniences, not rigid training prescriptions.

Gate:

```text
SESSION_TEMPLATES_V1=PASS
```

## Training analytics v1

Implement only after enough real sessions exist to make the output meaningful.

Candidate views:

```text
7 days
30 days
90 days
since baseline
```

Candidate metrics:

- training frequency;
- sets;
- repetitions;
- duration;
- external load;
- assistance;
- measured-max progression;
- useful volume metrics where semantically valid;
- progression by exercise.

Use the existing Body Zones V1 read surface to aggregate by canonical body zone
and hierarchy when evaluating training balance. A separate movement-family
taxonomy, if ever needed, must be versioned rather than inferred from names.

Gate:

```text
TRAINING_ANALYTICS_V1=PASS
```

## Body analytics v2

Implement after multiple real body observations exist.

Possible additions:

- baseline comparison;
- previous-observation comparison;
- 7/30/90-day trends;
- moving-average weight trend;
- waist trend;
- anthropometric estimate trend;
- left/right asymmetry trend;
- obvious measurement-outlier warnings.

Permanent semantic rule:

```text
DIRECT_MEASUREMENT != ESTIMATE
```

Derived body-composition values remain explicitly labelled estimates.

Gate:

```text
BODY_ANALYTICS_V2=PASS
```

## Progression assist v1

Use accumulated history to produce conservative training suggestions.

Examples:

```text
external load:
stable completed work
-> suggest a small load increase

assistance:
stable completed work
-> suggest a small assistance decrease
```

The system proposes. It does not silently change planned training.

Measured max and estimated performance must remain distinct concepts.

Gate:

```text
PROGRESSION_ASSIST_V1=PASS
```

## Objectives v1

Support explicit user-defined objectives such as:

```text
body weight
waist circumference
plank duration
exercise load
repetitions
measured max
weekly training frequency
```

Display current value, target, remaining difference, and trend where the data
supports it.

Avoid mandatory gamification.

Gate:

```text
OBJECTIVES_V1=PASS
```

## Backup and export v1

SQLite remains canonical.

Provide durable user-controlled export and backup:

```text
SQLite
-> dated backup
-> complete JSON export
-> CSV export where useful
-> period-limited export
```

Gate:

```text
BACKUP_EXPORT_V1=PASS
```

## Canonical implementation order

```text
REAL_DATA_BASELINE_V1
        |
        v
GYM_CATALOG_V1
        |
        v
EXERCISE_METADATA_V1
        |
        v
SESSION_PLANNER_V1
        |
        v
SESSION_TEMPLATES_V1
        |
        v
TRAINING_ANALYTICS_V1
        |
        v
BODY_ANALYTICS_V2
        |
        v
PROGRESSION_ASSIST_V1
        |
        v
OBJECTIVES_V1
        |
        v
BACKUP_EXPORT_V1
```

This order is canonical until explicitly revised.

## Permanent constraints

Do not regress to:

```text
SQLite file synchronization
mandatory mounted Android filesystem
exercise-name identity heuristics
fake performed sets for continuous activity
fake uniform targets for heterogeneous actual sets
ordinary training promoted to measured max
assistance interpreted as external load
body-composition estimates presented as direct measurements
incompatible changes to Trainlog JSON v1 without a new format version
```
