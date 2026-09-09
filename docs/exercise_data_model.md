# Exercise data model

## Status

```text
EXERCISE_DATA_MODEL_V1=PASS
PROFILE_AWARE_DESKTOP=PASS
PROFILE_AWARE_ANDROID=PASS
CONTINUOUS_ACTIVITY=PASS
VARIABLE_REPETITION_SETS=PASS
TRAINLOG_FORMAT_V1=FROZEN
BODY_ZONES_V1=PASS
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

## 3. Body-zone metadata

Exercise behavior and body-zone classification are independent. The sole V1
taxonomy is `catalog/body-zones-v1.json`:

```text
full_body                      Corps entier
upper_body                     Membres supérieurs (group)
  chest                        Pectoraux
  back                         Dos
  shoulders                    Épaules
  arms                         Bras
core                           Abdominaux / tronc
lower_body                     Membres inférieurs (group)
  glutes                       Fessiers
  thighs                       Cuisses
  calves                       Mollets
```

An exercise stores at most one direct `primary` relation and any number of
distinct `secondary` relations. Secondary relations require that primary; an
unclassified exercise has no relation. The same zone cannot have both roles.
Group nodes are not assignable: a direct `chest` relation is sufficient for an
`upper_body` descendant query. `full_body` is not a synonym for all zones, and
`core` is not implicitly upper or lower body. Cardio is an activity profile,
not a body zone.

New interactive `SETS` creation asks for a primary zone. Historical or
objectively ambiguous exercises may remain without relations and are displayed
and filterable as **Non renseignés**. Initial migration decisions use exact
stable `exercise_id` values and recorded equipment/catalog evidence, never a
general name rule.

The read surface supports zone lookup, children, ancestors, direct relations,
primary/secondary selection and exercises for a zone with optional descendants
or primary-only participation. Those exercise IDs compose with existing
performance/MAX history readers, so a future session generator needs no new
duplicated MAX or history storage. No generator or load proposal exists yet.

## 4. Load semantics

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

## 5. Set-based work

`SETS + REPS` stores one performed-set row per actual set.

Each ordered performed set independently owns its repetitions and an optional
`weight_kg`. Actual repetitions and actual loads can therefore differ from one
set to the next. A missing load is not a zero load and is not filled from a
planned target. When supplied, an actual load is finite and `>= 0`; an explicit
zero is preserved as an observed value.

Each performed row is the source of truth for actual work.

New normal desktop set work is created only as explicit actual rows. Compact
performed-repetition expressions are not an active desktop entry form; Android
legacy-draft decoding is a separate compatibility behavior documented in
`docs/android.md`.

Existing historical rows retain their stored repetitions, optional loads and
order unchanged. Editing or exchanging a session never normalizes heterogeneous
actual values into a uniform prescription.

This per-set capture contract does not introduce volume/tonnage, estimated 1RM,
or progression calculations.

`SETS + DURATION` likewise stores one actual duration per performed set.

## 6. Planned versus actual

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

## 7. Continuous work

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

## 8. Historical interpretation

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

## 9. Android/TUI parity

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
Body zones use their separate direction-neutral V1 companion rather than
changing this catalog artifact or frozen session V1.

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

## 10. Exchange boundaries

Frozen Trainlog session JSON v1 remains unchanged.

Synchronization data that exceeds frozen v1 uses separate versioned artifacts.

Continuous activity must never be:

- hidden in notes;
- converted into a fake set;
- silently discarded.

## 11. Measured max semantics

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
