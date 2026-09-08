# Exercise data model

## Status

```text
EXERCISE_DATA_MODEL_V1=PASS
PROFILE_AWARE_DESKTOP=PASS
PROFILE_AWARE_ANDROID=PASS
CONTINUOUS_ACTIVITY=PASS
VARIABLE_REPETITION_SETS=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

## 1. Metadata axes

An exercise is described by independent metadata:

```text
recording_mode = SETS | CONTINUOUS
tracking_mode  = REPS | DURATION
data_fields    = supplemental field bit mask
```

Known supplemental fields:

```text
SPEED_KMH       bit 0, mask value 1
DISTANCE_KM     bit 1, mask value 2
```

Consequently `data_fields = 1` means speed only and `data_fields = 3`
means speed plus distance. These meanings come from the shared C/Kotlin model
constants and serializers; the masks are not ordinal profile numbers.

Valid model-v1 combinations:

```text
SETS + REPS
SETS + DURATION
CONTINUOUS + DURATION
```

Invalid:

```text
CONTINUOUS + REPS
```

UI behavior must never be inferred from an exercise display name.

## 2. Examples

```text
Presse à cuisses
    SETS + REPS

Gainage
    SETS + DURATION

Marche
    CONTINUOUS + DURATION + SPEED_KMH

Course
    CONTINUOUS + DURATION + SPEED_KMH

Vélo
    CONTINUOUS + DURATION + SPEED_KMH + DISTANCE_KM

Rameur
    CONTINUOUS + DURATION + DISTANCE_KM
```

## 3. Load semantics

Load mode is session-specific:

```text
none
external
assistance
```

`external` is added resistance.

`assistance` is help. Lower assistance represents less help and is therefore
better when comparing otherwise equivalent performance.

Machine-displayed kilograms are stored faithfully without claiming mechanical
equivalence across different machines.

## 4. Set-based work

`SETS + REPS` stores one performed-set row per actual set.

Actual repetitions can differ across sets.

Accepted compact repetition input includes:

```text
5x10
4,5,6,7,8,9,10,9,8,7,6,5,4
4..10..4
```

The pyramid shorthand:

```text
4..10..4
```

expands to:

```text
4,5,6,7,8,9,10,9,8,7,6,5,4
```

Each performed row is the source of truth for actual work.

`SETS + DURATION` likewise stores one actual duration per performed set.

## 5. Planned versus actual

Desktop-created set sessions may carry explicit planned targets.

Android mobile snapshots can represent actual-only heterogeneous work.

Desktop schema v5 therefore allows an imported set session to have no synthetic
uniform target:

```text
target_sets = NULL
target_reps = NULL
target_duration_seconds = NULL
```

Do not derive a fake target from heterogeneous actual sets.

## 6. Continuous work

Continuous activity does not ask for:

```text
set count
repetitions
per-set rest
load
```

Example:

```text
Marche

Durée      45 min
Vitesse    5.8 km/h
```

Actual persistence uses one `continuous_activity` record containing duration
and configured supplemental values.

No fake performed set is created.

## 7. Historical interpretation

Desktop `session_exercises` snapshot:

```text
recording_mode
data_fields
```

This prevents later catalog changes to those fields from rewriting the meaning
of historical rows.

Current desktop tracking mode remains tied to the referenced exercise identity.
Any future change to that historical contract requires an explicit schema
decision rather than an implicit name-based migration.

Catalog enrichment does not rewrite an occurrence snapshot. For example, a
historic `data_fields = 1` walk remains a speed-only occurrence after its
catalog definition becomes `data_fields = 3`. Its absent distance stays
absent/`NULL`; synchronization does not infer it from duration and speed.

## 8. Android/TUI parity

Both interfaces use the same metadata axes.

The PC -> Android catalog synchronization artifact carries:

```text
exercise_id
name
recording_mode
tracking_mode
data_fields
```

Creating an exercise inline on Android or desktop follows the same model rules.

### Safe V2 identity reconciliation

A normalized-name collision between distinct `exercise_id` values is eligible
for automatic V2/catalog reconciliation only when:

- the normalized name is identical under the receiving store's current rule;
- `recording_mode` and `tracking_mode` are identical;
- all other represented business invariants, including overlapping equipment
  load semantics on Android, are compatible;
- the `data_fields` masks are equal, or one is a bitwise subset of the other;
- moving references loses no occurrence, actual value or metadata.

The existing desktop identity is canonical. Android therefore adopts the PC
identity when applying a PC catalog. The catalog stores the bitwise union (the
compatible richer profile), while sessions retain their own `entry_id`, order,
profile snapshot, sets, loads, continuous values and equipment. The operation
is transactional and replay-safe. Different modes, incomparable masks, or any
other incompatible invariant produce an explicit conflict. Equal names alone
never establish identity.

This is a synchronization-V2 policy. It does not relax or redefine the frozen
Trainlog JSON v1 document rules.

## 9. Exchange boundaries

Frozen Trainlog session JSON v1 remains unchanged.

Synchronization data that exceeds frozen v1 uses separate versioned artifacts.

Continuous activity must never be:

- hidden in notes;
- converted into a fake set;
- silently discarded.

## 10. Measured max semantics

`session_type = max_test` is an explicit semantic boundary.

An explicit weight maximum is an occurrence-owned result:

```text
exercise_id
entry_id
equipment_id optional
max_weight_kg > 0
position
```

It is mutually exclusive with `performed_sets` and `continuous_activity` for
that occurrence. No `sets = 1` or `reps = 1` value is stored. This result mode
does not change the frozen catalogue combinations: a catalogue exercise keeps
its existing recording/tracking profile, while the containing `max_test`
session selects the explicit MAX capture form.

`exercise_id` owns the movement result. `equipment_id` identifies only the
physical context, so two movements performed on the same combined machine have
independent maxima.

Measured max v1 never equates an ordinary best set with a measured maximum.
Legacy `max_test` sets that cannot be migrated unambiguously remain available
under their historical semantics.

For those retained legacy set-based exercises:

```text
external
    max measured load = greatest successful actual load in a max_test

assistance
    best measured assistance = lowest successful assistance in a max_test

none
    measured max = greatest successful reps/duration in a max_test
```

Ties use greater repetitions/duration.

The newest successful explicit test is the current measured result. A separate
same-mode historical record may be older.

No estimated 1RM is mixed into this contract.
