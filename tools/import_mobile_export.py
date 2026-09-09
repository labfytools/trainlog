#!/usr/bin/env python3
import argparse
import json
import math
import os
import sqlite3
import sys
import unicodedata
from pathlib import Path


FORMAT = "trainlog-mobile-export"
VERSION = 1
KNOWN_DATA_FIELDS = 3
DEFAULT_EQUIPMENT_CATALOG = (
    Path(__file__).resolve().parents[1]
    / "catalog"
    / "equipment-v1.json"
)

TOP_LEVEL_KEYS = {
    "format",
    "version",
    "generated_at",
    "exercises",
    "sessions",
    "body_observations",
}

EXERCISE_KEYS = {
    "exercise_id",
    "name",
    "recording_mode",
    "tracking_mode",
    "data_fields",
}

SESSION_KEYS = {
    "session_id",
    "started_at",
    "session_type",
    "exercises",
}

SESSION_EXERCISE_KEYS = {
    "exercise_id",
    "name",
    "recording_mode",
    "tracking_mode",
    "data_fields",
    "load_mode",
    "rest_seconds",
    "sets",
    "continuous",
}

V2_SESSION_EXERCISE_KEYS = SESSION_EXERCISE_KEYS | {
    "entry_id", "position", "equipment_id", "max_weight_kg"
}

BODY_BASE_KEYS = {
    "observation_id",
    "observed_at",
}

BODY_METRIC_KEYS = {
    "body_weight_kg",
    "neck_cm",
    "shoulders_cm",
    "chest_cm",
    "waist_cm",
    "hips_cm",
    "left_arm_cm",
    "right_arm_cm",
    "left_forearm_cm",
    "right_forearm_cm",
    "left_thigh_cm",
    "right_thigh_cm",
    "left_calf_cm",
    "right_calf_cm",
}


class ImportFailure(RuntimeError):
    pass


def default_database_path():
    data_home = os.environ.get("XDG_DATA_HOME")
    if data_home:
        return Path(data_home) / "trainlog" / "trainlog.db"

    return (
        Path.home()
        / ".local"
        / "share"
        / "trainlog"
        / "trainlog.db"
    )


def require_exact_keys(value, allowed, required, label):
    if not isinstance(value, dict):
        raise ImportFailure(f"{label}: objet JSON attendu")

    actual = set(value.keys())
    unknown = actual - allowed
    missing = required - actual

    if unknown:
        names = ", ".join(sorted(unknown))
        raise ImportFailure(
            f"{label}: champ(s) inconnu(s): {names}"
        )

    if missing:
        names = ", ".join(sorted(missing))
        raise ImportFailure(
            f"{label}: champ(s) manquant(s): {names}"
        )


def require_nonempty_string(value, label):
    if not isinstance(value, str) or not value:
        raise ImportFailure(
            f"{label}: chaîne non vide attendue"
        )

    return value


def require_int(value, minimum, maximum, label):
    if isinstance(value, bool) or not isinstance(value, int):
        raise ImportFailure(
            f"{label}: entier attendu"
        )

    if value < minimum or value > maximum:
        raise ImportFailure(
            f"{label}: hors bornes"
        )

    return value


def require_positive_number(value, label):
    if isinstance(value, bool) or not isinstance(
        value,
        (int, float),
    ):
        raise ImportFailure(
            f"{label}: nombre attendu"
        )

    parsed = float(value)

    if not math.isfinite(parsed) or parsed <= 0.0:
        raise ImportFailure(
            f"{label}: nombre positif attendu"
        )

    return parsed


def require_nonnegative_number(value, label):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ImportFailure(f"{label}: nombre attendu")

    parsed = float(value)
    if not math.isfinite(parsed) or parsed < 0.0:
        raise ImportFailure(f"{label}: nombre non négatif attendu")

    return parsed


def normalize_name(value):
    folded = unicodedata.normalize(
        "NFC",
        value.casefold(),
    )

    output = []
    pending_space = False
    wrote_content = False

    for char in folded:
        if char.isspace():
            if wrote_content:
                pending_space = True
            continue

        if pending_space:
            output.append(" ")
            pending_space = False

        output.append(char)
        wrote_content = True

    normalized = "".join(output)

    if not normalized:
        raise ImportFailure(
            "nom d'exercice vide après normalisation"
        )

    return normalized


def load_payload(path):
    try:
        with path.open(
            "r",
            encoding="utf-8",
        ) as handle:
            payload = json.load(handle)
    except (OSError, json.JSONDecodeError) as error:
        raise ImportFailure(
            f"lecture JSON impossible: {error}"
        ) from error

    require_exact_keys(
        payload,
        TOP_LEVEL_KEYS,
        TOP_LEVEL_KEYS,
        "racine",
    )

    if payload["format"] != FORMAT:
        raise ImportFailure(
            "format mobile export invalide"
        )

    if payload["version"] not in (1, 2):
        raise ImportFailure(
            "version mobile export non supportée"
        )

    require_nonempty_string(
        payload["generated_at"],
        "generated_at",
    )

    for key in (
        "exercises",
        "sessions",
        "body_observations",
    ):
        if not isinstance(payload[key], list):
            raise ImportFailure(
                f"{key}: tableau attendu"
            )

    return payload


def load_supplied_equipment_ids(path):
    try:
        with path.open("r", encoding="utf-8") as handle:
            catalog = json.load(handle)
    except (OSError, json.JSONDecodeError) as error:
        raise ImportFailure(
            f"lecture catalogue équipement impossible: {error}"
        ) from error

    if (
        not isinstance(catalog, dict)
        or catalog.get("format") != "trainlog-equipment-catalog"
        or catalog.get("version") != 1
        or not isinstance(catalog.get("equipment"), list)
    ):
        raise ImportFailure("catalogue équipement v1 invalide")

    known = set()
    for index, item in enumerate(catalog["equipment"]):
        if not isinstance(item, dict):
            raise ImportFailure(
                f"catalogue équipement v1 invalide: equipment[{index}]"
            )
        known.add(
            require_nonempty_string(
                item.get("id"),
                f"catalogue.equipment[{index}].id",
            )
        )
    return known


def validate_profile(
    recording_mode,
    tracking_mode,
    data_fields,
    label,
):
    if recording_mode not in (
        "sets",
        "continuous",
    ):
        raise ImportFailure(
            f"{label}.recording_mode invalide"
        )

    if tracking_mode not in (
        "reps",
        "duration",
    ):
        raise ImportFailure(
            f"{label}.tracking_mode invalide"
        )

    require_int(
        data_fields,
        0,
        KNOWN_DATA_FIELDS,
        f"{label}.data_fields",
    )

    if data_fields & ~KNOWN_DATA_FIELDS:
        raise ImportFailure(
            f"{label}.data_fields inconnu"
        )

    if (
        recording_mode == "continuous"
        and tracking_mode != "duration"
    ):
        raise ImportFailure(
            f"{label}: continuous exige duration"
        )


def validate_exercises(payload):
    seen_ids = set()
    profiles = {}

    for index, item in enumerate(
        payload["exercises"]
    ):
        label = f"exercises[{index}]"

        require_exact_keys(
            item,
            EXERCISE_KEYS,
            EXERCISE_KEYS,
            label,
        )

        exercise_id = require_nonempty_string(
            item["exercise_id"],
            f"{label}.exercise_id",
        )

        if exercise_id in seen_ids:
            raise ImportFailure(
                f"{label}: exercise_id dupliqué"
            )

        seen_ids.add(exercise_id)

        require_nonempty_string(
            item["name"],
            f"{label}.name",
        )

        validate_profile(
            item["recording_mode"],
            item["tracking_mode"],
            item["data_fields"],
            label,
        )

        profiles[exercise_id] = item

    return profiles


def validate_set_item(
    value,
    tracking_mode,
    label,
):
    allowed_weight = {"weight_kg"}
    # CONTRACT: absent actual load is omitted/SQL NULL; when present it is a
    # finite nonnegative observation, including an explicit zero.
    if "weight_kg" in value:
        require_nonnegative_number(
            value["weight_kg"],
            f"{label}.weight_kg",
        )
    if tracking_mode == "reps":
        require_exact_keys(
            value,
            {"reps"} | allowed_weight,
            {"reps"},
            label,
        )

        reps = require_int(
            value["reps"],
            0,
            10000,
            f"{label}.reps",
        )

        return ("reps", reps)

    require_exact_keys(
        value,
        {"duration_seconds"} | allowed_weight,
        {"duration_seconds"},
        label,
    )

    duration = require_int(
        value["duration_seconds"],
        1,
        86400,
        f"{label}.duration_seconds",
    )

    return ("duration", duration)


def validate_session_exercise(
    item,
    label,
    known_exercise_ids,
    session_type,
):
    is_v2 = "entry_id" in item or "position" in item or "equipment_id" in item
    require_exact_keys(
        item,
        V2_SESSION_EXERCISE_KEYS if is_v2 else SESSION_EXERCISE_KEYS,
        (V2_SESSION_EXERCISE_KEYS if is_v2 else SESSION_EXERCISE_KEYS)
        - {"sets", "continuous", "max_weight_kg"},
        label,
    )

    if is_v2:
        require_nonempty_string(item["entry_id"], f"{label}.entry_id")
        require_int(item["position"], 0, 100000, f"{label}.position")
        if item["equipment_id"] is not None:
            require_nonempty_string(item["equipment_id"], f"{label}.equipment_id")

    exercise_id = require_nonempty_string(
        item["exercise_id"],
        f"{label}.exercise_id",
    )

    if exercise_id not in known_exercise_ids:
        raise ImportFailure(
            f"{label}: exercice absent du snapshot"
        )

    require_nonempty_string(
        item["name"],
        f"{label}.name",
    )

    recording_mode = item["recording_mode"]
    tracking_mode = item["tracking_mode"]

    validate_profile(
        recording_mode,
        tracking_mode,
        item["data_fields"],
        label,
    )

    catalog_profile = known_exercise_ids[exercise_id]
    if (
        recording_mode != catalog_profile["recording_mode"]
        or tracking_mode != catalog_profile["tracking_mode"]
        or item["data_fields"] & ~catalog_profile["data_fields"]
    ):
        # CONTRACT: an occurrence snapshots the fields that actually existed
        # when it was recorded.  It may omit later optional catalog fields, but
        # it may never claim a field absent from the catalog profile.
        raise ImportFailure(
            f"{label}: snapshot incompatible avec le profil catalogue"
        )

    if item["load_mode"] != "none":
        raise ImportFailure(
            f"{label}: mobile export v1 exige load_mode=none"
        )

    if item["rest_seconds"] != 0:
        raise ImportFailure(
            f"{label}: mobile export v1 exige rest_seconds=0"
        )

    if "max_weight_kg" in item:
        if not is_v2 or session_type != "max_test":
            raise ImportFailure(
                f"{label}: max_weight_kg exige mobile V2 et session max_test"
            )
        if "sets" in item or "continuous" in item:
            raise ImportFailure(
                f"{label}: max_weight_kg exclut sets et continuous"
            )
        require_positive_number(
            item["max_weight_kg"],
            f"{label}.max_weight_kg",
        )
        return

    if recording_mode == "continuous":
        if "sets" in item:
            raise ImportFailure(
                f"{label}: continuous ne doit pas avoir sets"
            )

        if "continuous" not in item:
            raise ImportFailure(
                f"{label}: continuous manquant"
            )

        continuous = item["continuous"]

        allowed = {
            "duration_seconds",
            "speed_kmh",
            "distance_km",
        }

        require_exact_keys(
            continuous,
            allowed,
            {"duration_seconds"},
            f"{label}.continuous",
        )

        require_int(
            continuous["duration_seconds"],
            1,
            86400,
            f"{label}.continuous.duration_seconds",
        )

        wants_speed = (
            item["data_fields"] & 1
        ) != 0

        wants_distance = (
            item["data_fields"] & 2
        ) != 0

        has_speed = "speed_kmh" in continuous
        has_distance = (
            "distance_km" in continuous
        )

        if wants_speed != has_speed:
            raise ImportFailure(
                f"{label}: présence speed_kmh incohérente"
            )

        if wants_distance != has_distance:
            raise ImportFailure(
                f"{label}: présence distance_km incohérente"
            )

        if has_speed:
            require_positive_number(
                continuous["speed_kmh"],
                f"{label}.continuous.speed_kmh",
            )

        if has_distance:
            require_positive_number(
                continuous["distance_km"],
                f"{label}.continuous.distance_km",
            )

        return

    if "continuous" in item:
        raise ImportFailure(
            f"{label}: sets ne doit pas avoir continuous"
        )

    if "sets" not in item:
        raise ImportFailure(
            f"{label}: sets manquant"
        )

    sets = item["sets"]

    if not isinstance(sets, list) or not sets:
        raise ImportFailure(
            f"{label}.sets: tableau non vide attendu"
        )

    for set_index, set_item in enumerate(sets):
        if not is_v2 and "weight_kg" in set_item:
            raise ImportFailure(f"{label}.sets[{set_index}]: poids interdit en v1")
        validate_set_item(
            set_item,
            tracking_mode,
            f"{label}.sets[{set_index}]",
        )


def validate_sessions(
    payload,
    known_exercise_ids,
    known_equipment_ids,
):
    seen_ids = set()

    for index, session in enumerate(
        payload["sessions"]
    ):
        label = f"sessions[{index}]"

        require_exact_keys(
            session,
            SESSION_KEYS,
            SESSION_KEYS,
            label,
        )

        session_id = require_nonempty_string(
            session["session_id"],
            f"{label}.session_id",
        )

        if session_id in seen_ids:
            raise ImportFailure(
                f"{label}: session_id dupliqué"
            )

        seen_ids.add(session_id)

        require_nonempty_string(
            session["started_at"],
            f"{label}.started_at",
        )

        if session["session_type"] not in (
            "training",
            "max_test",
        ):
            raise ImportFailure(
                f"{label}.session_type invalide"
            )

        exercises = session["exercises"]

        if (
            not isinstance(exercises, list)
            or not exercises
        ):
            raise ImportFailure(
                f"{label}.exercises: tableau non vide attendu"
            )

        seen_session_exercises = set()
        seen_positions = set()

        for exercise_index, exercise in enumerate(
            exercises
        ):
            exercise_label = (
                f"{label}.exercises[{exercise_index}]"
            )

            validate_session_exercise(
                exercise,
                exercise_label,
                known_exercise_ids,
                session["session_type"],
            )

            equipment_id = exercise.get("equipment_id")
            # CONTRACT: mobile export v2 carries only references whose supplied
            # or custom definition is already known. Validate before run_import
            # opens a transaction so an unknown ID cannot create partial history.
            if (
                payload["version"] == 2
                and equipment_id is not None
                and equipment_id not in known_equipment_ids
            ):
                raise ImportFailure(
                    "équipement inconnu "
                    f"session_id={session_id} "
                    f"entry_id={exercise['entry_id']} "
                    f"equipment_id={equipment_id}"
                )

            exercise_id = exercise["exercise_id"]
            identity = exercise.get("entry_id", exercise_id)

            if identity in seen_session_exercises:
                raise ImportFailure(
                    f"{exercise_label}: identité d'entrée dupliquée dans la séance"
                )

            seen_session_exercises.add(
                identity
            )
            if "position" in exercise:
                if exercise["position"] in seen_positions:
                    raise ImportFailure(f"{exercise_label}: position dupliquée")
                seen_positions.add(exercise["position"])


def validate_body(payload):
    seen_ids = set()
    allowed = BODY_BASE_KEYS | BODY_METRIC_KEYS

    for index, observation in enumerate(
        payload["body_observations"]
    ):
        label = f"body_observations[{index}]"

        require_exact_keys(
            observation,
            allowed,
            BODY_BASE_KEYS,
            label,
        )

        observation_id = require_nonempty_string(
            observation["observation_id"],
            f"{label}.observation_id",
        )

        if observation_id in seen_ids:
            raise ImportFailure(
                f"{label}: observation_id dupliqué"
            )

        seen_ids.add(observation_id)

        require_nonempty_string(
            observation["observed_at"],
            f"{label}.observed_at",
        )

        present_metrics = (
            set(observation.keys())
            & BODY_METRIC_KEYS
        )

        if not present_metrics:
            raise ImportFailure(
                f"{label}: au moins une mesure requise"
            )

        for metric in present_metrics:
            require_positive_number(
                observation[metric],
                f"{label}.{metric}",
            )


def validate_payload(payload, known_equipment_ids):
    exercise_ids = validate_exercises(
        payload
    )

    validate_sessions(
        payload,
        exercise_ids,
        known_equipment_ids,
    )

    validate_body(payload)


def require_supported_schema(connection):
    version = connection.execute(
        "PRAGMA user_version;"
    ).fetchone()[0]

    # CONTRACT: v9 owns explicit max_results; earlier supported schemas remain
    # readable for legacy artifacts and are never made to fake that table.
    if version not in (5, 6, 7, 8, 9, 10):
        raise ImportFailure(
            f"base desktop schema v5 à v10 attendue, version trouvée: {version}"
        )


def lookup_exercise_by_id(
    connection,
    exercise_id,
):
    return connection.execute(
        """
        SELECT
            id,
            exercise_id,
            name,
            normalized_name,
            tracking_mode,
            recording_mode,
            data_fields
        FROM exercises
        WHERE exercise_id = ?;
        """,
        (exercise_id,),
    ).fetchone()


def lookup_exercise_by_normalized(
    connection,
    normalized,
):
    return connection.execute(
        """
        SELECT
            id,
            exercise_id,
            name,
            normalized_name,
            tracking_mode,
            recording_mode,
            data_fields
        FROM exercises
        WHERE normalized_name = ?;
        """,
        (normalized,),
    ).fetchone()


def data_fields_are_comparable(left, right):
    """Return true only when either bounded field mask contains the other."""
    return (left & ~right) == 0 or (right & ~left) == 0


def trace_exercise_decision(enabled, exercise, lookup, decision):
    """Emit an opt-in structured diagnostic without changing import state."""
    if not enabled:
        return
    print(
        "EXERCISE_DECISION=" + json.dumps(
            {
                "exercise_id": exercise["exercise_id"],
                "name": exercise["name"],
                "recording_mode": exercise["recording_mode"],
                "tracking_mode": exercise["tracking_mode"],
                "data_fields": exercise["data_fields"],
                "lookup": lookup,
                "decision": decision,
            },
            ensure_ascii=False,
            sort_keys=True,
        ),
        file=sys.stderr,
    )


def profiles_are_reconcilable(row, exercise):
    """Check the complete exercise-profile boundary carried by mobile V2.

    Name equality is deliberately checked by the caller: it is a collision
    precondition, never sufficient identity evidence by itself.
    """
    return (
        row["tracking_mode"] == exercise["tracking_mode"]
        and row["recording_mode"] == exercise["recording_mode"]
        and data_fields_are_comparable(
            row["data_fields"], exercise["data_fields"]
        )
    )


def profile_conflict(row, exercise):
    return (
        "profil incompatible entre identités "
        f"{row['exercise_id']} et {exercise['exercise_id']}: "
        f"desktop={row['recording_mode']}/{row['tracking_mode']}/"
        f"data_fields={row['data_fields']}, "
        f"entrant={exercise['recording_mode']}/{exercise['tracking_mode']}/"
        f"data_fields={exercise['data_fields']}"
    )


def enrich_desktop_profile(connection, row, exercise):
    """Retain the richer compatible capability without rewriting history."""
    # CONTRACT: field values are a bit mask, not an ordered enumeration. The
    # prior comparability check proves that this union is one existing superset.
    richer_fields = row["data_fields"] | exercise["data_fields"]
    if richer_fields != row["data_fields"]:
        # INVARIANT: session_exercises.data_fields remains untouched.  Missing
        # optional values therefore remain NULL instead of being invented.
        connection.execute(
            "UPDATE exercises SET data_fields=? WHERE id=?;",
            (richer_fields, row["id"]),
        )
    return richer_fields


def merge_desktop_exercise_rows(connection, canonical, retired):
    """Move the complete current desktop FK graph before deleting a duplicate."""
    # WHY: the current v8 desktop schema has exactly one exercise-row FK owner.
    # Keeping this operation explicit makes a future schema addition fail its
    # reconciliation tests instead of silently leaving a dangling identity.
    connection.execute(
        "UPDATE session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
        (canonical["id"], retired["id"]),
    )
    remaining = connection.execute(
        "SELECT COUNT(*) FROM session_exercises WHERE exercise_row_id=?;",
        (retired["id"],),
    ).fetchone()[0]
    if remaining != 0:
        raise ImportFailure(
            f"références résiduelles vers {retired['exercise_id']}"
        )
    connection.execute(
        "DELETE FROM exercises WHERE id=?;",
        (retired["id"],),
    )


def import_exercises(
    connection,
    payload,
    report,
    trace_exercises=False,
):
    mapping = {}

    for exercise in payload["exercises"]:
        exercise_id = exercise["exercise_id"]
        normalized = normalize_name(
            exercise["name"]
        )

        by_id = lookup_exercise_by_id(
            connection,
            exercise_id,
        )

        if by_id is not None:
            if not profiles_are_reconcilable(
                by_id,
                exercise,
            ):
                trace_exercise_decision(
                    trace_exercises,
                    exercise,
                    f"exercise_id:{by_id['exercise_id']}",
                    "conflict",
                )
                raise ImportFailure(
                    profile_conflict(by_id, exercise)
                )

            by_name = lookup_exercise_by_normalized(
                connection,
                normalized,
            )
            if (
                by_name is not None
                and by_name["id"] != by_id["id"]
            ):
                if not profiles_are_reconcilable(by_name, by_id):
                    trace_exercise_decision(
                        trace_exercises,
                        exercise,
                        "exercise_id:"
                        f"{by_id['exercise_id']};normalized_name:"
                        f"{by_name['exercise_id']}",
                        "conflict",
                    )
                    raise ImportFailure(profile_conflict(by_name, by_id))

                # CONTRACT: the row already owning the normalized desktop name
                # is the deterministic canonical identity.  All row references
                # move transactionally; entry_id/session_id and child values do
                # not change.
                richer_fields = (
                    by_name["data_fields"]
                    | by_id["data_fields"]
                    | exercise["data_fields"]
                )
                connection.execute(
                    "UPDATE exercises SET data_fields=? WHERE id=?;",
                    (richer_fields, by_name["id"]),
                )
                merge_desktop_exercise_rows(connection, by_name, by_id)
                mapping[exercise_id] = by_name["exercise_id"]
                report["exercises_reconciled"] += 1
                trace_exercise_decision(
                    trace_exercises,
                    exercise,
                    "exercise_id:"
                    f"{by_id['exercise_id']};normalized_name:"
                    f"{by_name['exercise_id']}",
                    "existing-reconciled",
                )
                continue

            richer_fields = enrich_desktop_profile(
                connection, by_id, exercise
            )

            # CONTRACT: exercise_id is the synchronization identity. A rename
            # updates metadata in place, retaining every historical and draft
            # foreign-key reference instead of creating a second exercise.
            if (
                by_id["name"] != exercise["name"]
                or by_id["normalized_name"] != normalized
            ):
                connection.execute(
                    """
                    UPDATE exercises
                    SET name = ?, normalized_name = ?, data_fields = ?
                    WHERE id = ?;
                    """,
                    (
                        exercise["name"],
                        normalized,
                        richer_fields,
                        by_id["id"],
                    ),
                )
                report["exercises_reconciled"] += 1
                decision = "existing-reconciled"
            elif richer_fields != by_id["data_fields"]:
                report["exercises_reconciled"] += 1
                decision = "existing-reconciled"
            else:
                report["exercises_skipped"] += 1
                decision = "existing-identical"

            mapping[exercise_id] = (
                by_id["exercise_id"]
            )
            trace_exercise_decision(
                trace_exercises,
                exercise,
                f"exercise_id:{by_id['exercise_id']}",
                decision,
            )
            continue

        by_name = lookup_exercise_by_normalized(
            connection,
            normalized,
        )

        if by_name is not None:
            if not profiles_are_reconcilable(by_name, exercise):
                # Name equality alone remains insufficient: incompatible modes
                # or incomparable optional-field masks are an explicit conflict.
                trace_exercise_decision(
                    trace_exercises,
                    exercise,
                    f"normalized_name:{by_name['exercise_id']}",
                    "conflict",
                )
                raise ImportFailure(profile_conflict(by_name, exercise))

            richer_fields = enrich_desktop_profile(connection, by_name, exercise)
            mapping[exercise_id] = by_name["exercise_id"]
            if richer_fields != by_name["data_fields"]:
                report["exercises_reconciled"] += 1
                decision = "existing-reconciled"
            else:
                # INVARIANT: resolving an already-compatible creator ID to the
                # persisted canonical row is idempotent lookup work, not a new
                # persistent reconciliation on every snapshot replay.
                report["exercises_skipped"] += 1
                decision = "existing-identical"
            trace_exercise_decision(
                trace_exercises,
                exercise,
                f"normalized_name:{by_name['exercise_id']}",
                decision,
            )
            continue

        connection.execute(
            """
            INSERT INTO exercises(
                exercise_id,
                name,
                normalized_name,
                tracking_mode,
                recording_mode,
                data_fields
            ) VALUES(?, ?, ?, ?, ?, ?);
            """,
            (
                exercise_id,
                exercise["name"],
                normalized,
                exercise["tracking_mode"],
                exercise["recording_mode"],
                exercise["data_fields"],
            ),
        )

        mapping[exercise_id] = exercise_id
        report["exercises_imported"] += 1
        trace_exercise_decision(
            trace_exercises,
            exercise,
            "none",
            "inserted",
        )

    return mapping


def exercise_row_id(
    connection,
    desktop_exercise_id,
):
    row = connection.execute(
        """
        SELECT id
        FROM exercises
        WHERE exercise_id = ?;
        """,
        (desktop_exercise_id,),
    ).fetchone()

    if row is None:
        raise ImportFailure(
            "exercice desktop introuvable après reconciliation"
        )

    return row[0]


def session_exists(
    connection,
    session_id,
):
    return (
        connection.execute(
            """
            SELECT 1
            FROM sessions
            WHERE session_id = ?;
            """,
            (session_id,),
        ).fetchone()
        is not None
    )


def import_set_session_exercise(
    connection,
    session_row_id,
    position,
    item,
    exercise_row,
):
    sets = item["sets"]
    tracking = item["tracking_mode"]

    entry_id = item.get("entry_id")
    schema_version = connection.execute("PRAGMA user_version;").fetchone()[0]
    if entry_id is None and schema_version >= 7:
        entry_id = "sxe_v1_" + str(session_row_id) + "_" + item["exercise_id"]
    columns = "entry_id, " if entry_id is not None else ""
    values = "?, " if entry_id is not None else ""
    equipment_columns = ", equipment_id" if schema_version >= 6 else ""
    equipment_values = ", ?" if schema_version >= 6 else ""
    arguments = ([entry_id] if entry_id is not None else []) + [session_row_id, exercise_row, item["data_fields"], position]
    if schema_version >= 6:
        arguments.append(item.get("equipment_id"))
    cursor = connection.execute(
        """
        INSERT INTO session_exercises(
            """ + columns + """session_row_id,
            exercise_row_id,
            recording_mode,
            data_fields,
            position,
            load_mode,
            rest_seconds,
            target_sets,
            target_reps,
            target_duration_seconds,
            target_weight_kg, notes""" + equipment_columns + """
        ) VALUES(
            """ + values + """?, ?, 'sets', ?, ?, 'none', 0,
            NULL, NULL, NULL, NULL, NULL""" + equipment_values + """
        );
        """,
        arguments,
    )

    session_exercise_row_id = (
        cursor.lastrowid
    )

    for set_index, set_item in enumerate(sets):
        if tracking == "reps":
            reps = set_item["reps"]
            duration = None
        else:
            reps = None
            duration = (
                set_item["duration_seconds"]
            )

        connection.execute(
            """
            INSERT INTO performed_sets(
                session_exercise_row_id,
                position,
                reps,
                duration_seconds,
                weight_kg
            ) VALUES(?, ?, ?, ?, ?);
            """,
            (
                session_exercise_row_id,
                set_index,
                reps,
                duration, set_item.get("weight_kg"),
            ),
        )

def import_continuous_session_exercise(
    connection,
    session_row_id,
    position,
    item,
    exercise_row,
):
    entry_id = item.get("entry_id")
    schema_version = connection.execute("PRAGMA user_version;").fetchone()[0]
    if entry_id is None and schema_version >= 7:
        entry_id = "sxe_v1_" + str(session_row_id) + "_" + item["exercise_id"]
    columns = "entry_id, " if entry_id is not None else ""
    values = "?, " if entry_id is not None else ""
    equipment_columns = ", equipment_id" if schema_version >= 6 else ""
    equipment_values = ", ?" if schema_version >= 6 else ""
    arguments = ([entry_id] if entry_id is not None else []) + [session_row_id, exercise_row, item["data_fields"], position]
    if schema_version >= 6:
        arguments.append(item.get("equipment_id"))
    cursor = connection.execute(
        """
        INSERT INTO session_exercises(
            """ + columns + """session_row_id,
            exercise_row_id,
            recording_mode,
            data_fields,
            position,
            load_mode,
            rest_seconds,
            target_sets,
            target_reps,
            target_duration_seconds,
            target_weight_kg, notes""" + equipment_columns + """
        ) VALUES(
            """ + values + """?, ?, 'continuous', ?, ?, 'none', 0,
            NULL, NULL, NULL, NULL, NULL""" + equipment_values + """
        );
        """,
        arguments,
    )

    continuous = item["continuous"]

    connection.execute(
        """
        INSERT INTO continuous_activity(
            session_exercise_row_id,
            duration_seconds,
            speed_kmh,
            distance_km
        ) VALUES(?, ?, ?, ?);
        """,
        (
            cursor.lastrowid,
            continuous["duration_seconds"],
            continuous.get("speed_kmh"),
            continuous.get("distance_km"),
        ),
    )


def import_max_session_exercise(
    connection,
    session_row_id,
    position,
    item,
    exercise_row,
):
    """Persist a V2 max without manufacturing a performed set."""
    schema_version = connection.execute("PRAGMA user_version;").fetchone()[0]
    if schema_version < 9:
        raise ImportFailure(
            "max_weight_kg exige le schéma desktop v9"
        )
    cursor = connection.execute(
        """
        INSERT INTO session_exercises(
            entry_id, session_row_id, exercise_row_id, recording_mode,
            data_fields, position, load_mode, rest_seconds,
            target_sets, target_reps, target_duration_seconds,
            target_weight_kg, notes, equipment_id
        ) VALUES(?, ?, ?, ?, ?, ?, 'none', 0,
                 NULL, NULL, NULL, NULL, NULL, ?);
        """,
        (
            item["entry_id"],
            session_row_id,
            exercise_row,
            item["recording_mode"],
            item["data_fields"],
            position,
            item.get("equipment_id"),
        ),
    )
    connection.execute(
        "INSERT INTO max_results(session_exercise_row_id,max_weight_kg) "
        "VALUES(?,?);",
        (cursor.lastrowid, item["max_weight_kg"]),
    )


def import_sessions(
    connection,
    payload,
    exercise_mapping,
    report,
):
    for session in payload["sessions"]:
        if session_exists(
            connection,
            session["session_id"],
        ):
            if payload["version"] == 1:
                report["sessions_skipped"] += 1
                continue
            existing = connection.execute(
                "SELECT s.id FROM sessions s WHERE s.session_id=?;",
                (session["session_id"],)).fetchone()
            session_row_id = existing[0]
            incoming = [
                (
                    item["entry_id"],
                    exercise_mapping.get(item["exercise_id"]),
                )
                for item in session["exercises"]
            ]
            if any(exercise_id is None for _, exercise_id in incoming):
                raise ImportFailure(
                    "mapping exercice absent pour " + session["session_id"]
                )
            rows = connection.execute(
                "SELECT se.entry_id,e.exercise_id FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id WHERE se.session_row_id=? ORDER BY se.position;",
                (session_row_id,)).fetchall()
            current = [(row[0], row[1]) for row in rows]
            legacy = all(value[0].startswith("sxe_legacy_") or value[0].startswith("sxe_v1_") for value in current)
            header = connection.execute(
                "SELECT started_at,session_type FROM sessions WHERE id=?;",
                (session_row_id,),
            ).fetchone()
            resumable_max = (
                header is not None
                and tuple(header) == (session["started_at"], "max_test")
                and session["session_type"] == "max_test"
                and len(incoming) >= len(current)
                and all(current[index] == incoming[index]
                        for index in range(len(current)))
            )
            # A v1 history may be upgraded only when exercise/order mapping is
            # unique. Any other identity disagreement is an explicit conflict.
            if current != incoming and not (
                (legacy and [x[1] for x in current] == [x[1] for x in incoming])
                or resumable_max
            ):
                raise ImportFailure("conflit d'identités d'entrées pour " + session["session_id"])
            if not legacy and session_semantically_matches(
                connection,
                session_row_id,
                session,
                exercise_mapping,
            ):
                report["sessions_skipped"] += 1
                continue
            if not legacy and not resumable_max:
                raise ImportFailure("conflit de contenu pour " + session["session_id"])
            # CONTRACT: a resumed max_test may edit existing max values and
            # append occurrences, but cannot remove/reorder/rebind any stable
            # entry. This bounded replacement makes tomorrow's continuation
            # idempotent without turning arbitrary session conflicts into wins.
            # Explicit child deletion makes reconciliation safe even for old
            # databases which were created without enforced foreign keys.
            connection.execute("DELETE FROM performed_sets WHERE session_exercise_row_id IN (SELECT id FROM session_exercises WHERE session_row_id=?);", (session_row_id,))
            connection.execute("DELETE FROM continuous_activity WHERE session_exercise_row_id IN (SELECT id FROM session_exercises WHERE session_row_id=?);", (session_row_id,))
            if connection.execute("PRAGMA user_version;").fetchone()[0] >= 9:
                connection.execute("DELETE FROM max_results WHERE session_exercise_row_id IN (SELECT id FROM session_exercises WHERE session_row_id=?);", (session_row_id,))
            connection.execute("DELETE FROM session_exercises WHERE session_row_id=?;", (session_row_id,))
            report["sessions_reconciled"] += 1
        else:
            cursor = connection.execute(
                """
                INSERT INTO sessions(
                    session_id,
                    started_at,
                    ended_at,
                    session_type,
                    notes
                ) VALUES(?, ?, NULL, ?, NULL);
                """,
                (
                    session["session_id"],
                    session["started_at"],
                    session["session_type"],
                ),
            )

            session_row_id = cursor.lastrowid

        for position, item in enumerate(
            session["exercises"]
        ):
            position = item.get("position", position)
            mobile_id = item["exercise_id"]

            desktop_id = exercise_mapping.get(
                mobile_id
            )

            if desktop_id is None:
                raise ImportFailure(
                    f"mapping exercice absent: {mobile_id}"
                )

            row = lookup_exercise_by_id(
                connection,
                desktop_id,
            )

            if row is None:
                raise ImportFailure(
                    f"exercice desktop absent: {desktop_id}"
                )

            if (
                row["tracking_mode"]
                != item["tracking_mode"]
                or row["recording_mode"]
                != item["recording_mode"]
                or item["data_fields"] & ~row["data_fields"]
            ):
                raise ImportFailure(
                    "snapshot de séance incompatible avec le catalogue desktop"
                )

            row_id = exercise_row_id(
                connection,
                desktop_id,
            )

            if "max_weight_kg" in item:
                import_max_session_exercise(
                    connection,
                    session_row_id,
                    position,
                    item,
                    row_id,
                )
            elif item["recording_mode"] == "continuous":
                import_continuous_session_exercise(
                    connection,
                    session_row_id,
                    position,
                    item,
                    row_id,
                )
            else:
                import_set_session_exercise(
                    connection,
                    session_row_id,
                    position,
                    item,
                    row_id,
                )

        report["sessions_imported"] += 1


def session_semantically_matches(
    connection,
    session_row_id,
    incoming,
    exercise_mapping,
):
    header = connection.execute(
        "SELECT started_at,session_type FROM sessions WHERE id=?", (session_row_id,)).fetchone()
    if header is None or (header[0], header[1]) != (incoming["started_at"], incoming["session_type"]):
        return False
    rows = connection.execute(
        "SELECT se.id,se.entry_id,se.position,e.exercise_id,se.recording_mode,e.tracking_mode,"
        "se.data_fields,se.equipment_id FROM session_exercises se JOIN exercises e "
        "ON e.id=se.exercise_row_id WHERE se.session_row_id=? ORDER BY se.position", (session_row_id,)).fetchall()
    items = sorted(incoming["exercises"], key=lambda item: item["position"])
    if len(rows) != len(items):
        return False
    for row, item in zip(rows, items):
        canonical_exercise_id = exercise_mapping.get(item["exercise_id"])
        if tuple(row[1:8]) != (item["entry_id"], item["position"], canonical_exercise_id,
                               item["recording_mode"], item["tracking_mode"],
                               item["data_fields"], item.get("equipment_id")):
            return False
        if "max_weight_kg" in item:
            current = connection.execute(
                "SELECT max_weight_kg FROM max_results "
                "WHERE session_exercise_row_id=?",
                (row[0],),
            ).fetchone()
            if current is None or current[0] != item["max_weight_kg"]:
                return False
        elif item["recording_mode"] == "continuous":
            current = connection.execute(
                "SELECT duration_seconds,speed_kmh,distance_km FROM continuous_activity "
                "WHERE session_exercise_row_id=?", (row[0],)).fetchone()
            expected = item["continuous"]
            if current is None or tuple(current) != (expected["duration_seconds"], expected.get("speed_kmh"), expected.get("distance_km")):
                return False
        else:
            current = connection.execute(
                "SELECT reps,duration_seconds,weight_kg FROM performed_sets "
                "WHERE session_exercise_row_id=? ORDER BY position", (row[0],)).fetchall()
            expected = [(value.get("reps"), value.get("duration_seconds"), value.get("weight_kg"))
                        for value in item["sets"]]
            if [tuple(value) for value in current] != expected:
                return False
    return True


def body_exists(
    connection,
    observation_id,
):
    return (
        connection.execute(
            """
            SELECT 1
            FROM body_observations
            WHERE observation_id = ?;
            """,
            (observation_id,),
        ).fetchone()
        is not None
    )


def import_body(
    connection,
    payload,
    report,
):
    metric_order = [
        "body_weight_kg",
        "neck_cm",
        "shoulders_cm",
        "chest_cm",
        "waist_cm",
        "hips_cm",
        "left_arm_cm",
        "right_arm_cm",
        "left_forearm_cm",
        "right_forearm_cm",
        "left_thigh_cm",
        "right_thigh_cm",
        "left_calf_cm",
        "right_calf_cm",
    ]

    placeholders = ", ".join(
        "?" for _ in range(
            2 + len(metric_order)
        )
    )

    columns = (
        "observation_id, observed_at, "
        + ", ".join(metric_order)
    )

    sql = (
        f"INSERT INTO body_observations("
        f"{columns}"
        f") VALUES({placeholders});"
    )

    for observation in payload[
        "body_observations"
    ]:
        existing = connection.execute(
            f"SELECT observed_at,{','.join(metric_order)} FROM body_observations WHERE observation_id=?",
            (observation["observation_id"],)).fetchone()
        if existing is not None:
            expected = [observation["observed_at"]] + [observation.get(metric) for metric in metric_order]
            if list(existing) != expected:
                raise ImportFailure("conflit observation corporelle: " + observation["observation_id"])
            report["body_skipped"] += 1
            continue

        values = [
            observation["observation_id"],
            observation["observed_at"],
        ]

        values.extend(
            observation.get(metric)
            for metric in metric_order
        )

        connection.execute(
            sql,
            values,
        )

        report["body_imported"] += 1


def run_import(
    payload,
    database_path,
    dry_run,
    trace_exercises=False,
):
    if not database_path.exists():
        raise ImportFailure(
            f"base desktop introuvable: {database_path}"
        )

    connection = sqlite3.connect(
        database_path
    )

    connection.row_factory = sqlite3.Row

    report = {
        "exercises_imported": 0,
        "exercises_reconciled": 0,
        "exercises_skipped": 0,
        "sessions_imported": 0,
        "sessions_reconciled": 0,
        "sessions_skipped": 0,
        "body_imported": 0,
        "body_skipped": 0,
    }

    try:
        connection.execute(
            "PRAGMA foreign_keys = ON;"
        )

        require_supported_schema(
            connection
        )

        schema_version = connection.execute("PRAGMA user_version;").fetchone()[0]
        has_explicit_max = any(
            "max_weight_kg" in entry
            for session in payload["sessions"]
            for entry in session["exercises"]
        )
        if has_explicit_max and schema_version < 9:
            raise ImportFailure("max_weight_kg exige le schéma desktop v9")
        has_explicit_zero_set_weight = any(
            set_item.get("weight_kg") == 0
            for session in payload["sessions"]
            for entry in session["exercises"]
            for set_item in entry.get("sets", [])
            if "weight_kg" in set_item
        )
        if has_explicit_zero_set_weight and schema_version < 10:
            raise ImportFailure(
                "weight_kg=0 exige le schéma desktop v10; import annulé"
            )

        connection.execute(
            "BEGIN IMMEDIATE;"
        )

        mapping = import_exercises(
            connection,
            payload,
            report,
            trace_exercises,
        )

        import_sessions(
            connection,
            payload,
            mapping,
            report,
        )

        import_body(
            connection,
            payload,
            report,
        )

        if dry_run:
            connection.rollback()
        else:
            connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()

    return report


def print_report(
    report,
    dry_run,
):
    prefix = (
        "MOBILE_IMPORT_DRY_RUN=PASS"
        if dry_run
        else "MOBILE_IMPORT=PASS"
    )

    print(prefix)

    for key in (
        "exercises_imported",
        "exercises_reconciled",
        "exercises_skipped",
        "sessions_imported",
        "sessions_reconciled",
        "sessions_skipped",
        "body_imported",
        "body_skipped",
    ):
        print(
            f"{key}={report[key]}"
        )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Import idempotent d'un snapshot "
            "Trainlog Android dans la DB desktop."
        )
    )

    parser.add_argument(
        "json_path",
        type=Path,
    )

    parser.add_argument(
        "--database",
        type=Path,
        default=default_database_path(),
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
    )

    parser.add_argument(
        "--catalog",
        type=Path,
        default=DEFAULT_EQUIPMENT_CATALOG,
    )

    parser.add_argument(
        "--trace-exercises",
        action="store_true",
        help="journaliser les lookups et décisions catalogue sur stderr",
    )

    args = parser.parse_args()

    try:
        payload = load_payload(
            args.json_path
        )

        known_equipment_ids = load_supplied_equipment_ids(args.catalog)
        if args.database.exists():
            with sqlite3.connect(args.database) as connection:
                if connection.execute("PRAGMA user_version").fetchone()[0] >= 8:
                    known_equipment_ids.update(row[0] for row in connection.execute(
                        "SELECT equipment_id FROM custom_equipment"))

        validate_payload(payload, known_equipment_ids)

        report = run_import(
            payload,
            args.database,
            args.dry_run,
            args.trace_exercises,
        )

        print_report(
            report,
            args.dry_run,
        )
    except ImportFailure as error:
        print(
            f"MOBILE_IMPORT=FAIL: {error}",
            file=sys.stderr,
        )

        raise SystemExit(1)
    except sqlite3.Error as error:
        print(
            f"MOBILE_IMPORT=FAIL: SQLite: {error}",
            file=sys.stderr,
        )

        raise SystemExit(1)


if __name__ == "__main__":
    main()
