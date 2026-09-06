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
DESKTOP_SCHEMA_V5=PASS
ANDROID_LOCAL_DATABASE_V3=PASS

DIRECT_MTP_TRANSPORT=PASS
BIDIRECTIONAL_SYNC_V1=PASS

VARIABLE_REPETITION_SETS=PASS
MEASURED_MAX_V1=PASS
BODY_ANALYTICS_V1=PASS

DESKTOP_TESTS=21/21 PASS
```

The current product baseline includes:

- SQLite persistence and explicit migrations;
- usable ncurses desktop TUI;
- native Android capture client;
- exercise catalog;
- profile-aware set and continuous activity;
- heterogeneous repetition sets;
- session history and editing;
- body measurements and body history;
- measured-max sessions and measured-max analysis;
- desktop body analytics;
- direct USB/MTP transport;
- bidirectional Android/PC synchronization;
- shared synchronization engine and `trainlog-syncd`.

## Product boundary

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
- measured max recorded only through explicit `max_test` sessions;
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
primary muscles
secondary muscles
```

Important rule:

```text
one photographed machine != one exercise
```

A single piece of equipment may support multiple exercises. Equipment and
exercise identity must remain distinct concepts.

Gate:

```text
GYM_CATALOG_V1=PASS
```

## Exercise metadata v1

After the real gym inventory is normalized, enrich the exercise catalog with
structured metadata needed by planning and analytics.

Candidate metadata:

```text
equipment
primary_muscles
secondary_muscles
movement_family
body_region
laterality
```

The final schema must be designed before implementation. Do not encode these
concepts into names or free-form notes as a substitute for a real model.

Gate:

```text
EXERCISE_METADATA_V1=PASS
```

## Session planner v1

The desktop TUI becomes the canonical session-planning surface.

A planned session should allow:

- selecting exercises from the real gym catalog;
- ordering exercises;
- planned sets/repetitions or duration;
- planned rest;
- optional planned load where semantically valid;
- synchronization to Android.

Android should then open a prepared session and require only actual performance
entry during training.

Target workflow:

```text
Desktop TUI
-> prepare session
-> sync

Android
-> open planned session
-> enter actual values
-> save
-> sync

Desktop TUI
-> history and analytics
```

The feature should also make later support possible for:

```text
duplicate previous session
reuse a planned session
session templates
```

Gates:

```text
SESSION_PLANNER_V1=PASS
ANDROID_PLANNED_SESSION_ENTRY=PASS
```

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

Once exercise metadata exists, aggregate by muscle group or movement family to
help evaluate training balance.

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
