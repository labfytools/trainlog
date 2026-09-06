#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
import sys
import unicodedata
from pathlib import Path


FORMAT = "trainlog-mobile-export"
VERSION = 1
KNOWN_DATA_FIELDS = 3

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

    if parsed <= 0.0:
        raise ImportFailure(
            f"{label}: nombre positif attendu"
        )

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

    if payload["version"] != VERSION:
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

    return seen_ids


def validate_set_item(
    value,
    tracking_mode,
    label,
):
    if tracking_mode == "reps":
        require_exact_keys(
            value,
            {"reps"},
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
        {"duration_seconds"},
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
):
    require_exact_keys(
        item,
        SESSION_EXERCISE_KEYS,
        SESSION_EXERCISE_KEYS
        - {"sets", "continuous"},
        label,
    )

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

    if item["load_mode"] != "none":
        raise ImportFailure(
            f"{label}: mobile export v1 exige load_mode=none"
        )

    if item["rest_seconds"] != 0:
        raise ImportFailure(
            f"{label}: mobile export v1 exige rest_seconds=0"
        )

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

    metric_values = []

    for set_index, set_item in enumerate(sets):
        _, metric = validate_set_item(
            set_item,
            tracking_mode,
            f"{label}.sets[{set_index}]",
        )

        metric_values.append(metric)

    if len(set(metric_values)) != 1:
        raise ImportFailure(
            f"{label}: mobile export v1 exige des séries uniformes"
        )

    if (
        tracking_mode == "reps"
        and metric_values[0] < 1
    ):
        raise ImportFailure(
            f"{label}: mobile export v1 ne peut pas dériver une cible depuis 0 reps"
        )


def validate_sessions(
    payload,
    known_exercise_ids,
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
            )

            exercise_id = exercise["exercise_id"]

            if exercise_id in seen_session_exercises:
                raise ImportFailure(
                    f"{exercise_label}: exercice dupliqué dans la séance"
                )

            seen_session_exercises.add(
                exercise_id
            )


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


def validate_payload(payload):
    exercise_ids = validate_exercises(
        payload
    )

    validate_sessions(
        payload,
        exercise_ids,
    )

    validate_body(payload)


def require_schema_v4(connection):
    version = connection.execute(
        "PRAGMA user_version;"
    ).fetchone()[0]

    if version != 4:
        raise ImportFailure(
            f"base desktop schema v4 attendue, version trouvée: {version}"
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


def profile_matches(
    row,
    exercise,
):
    return (
        row["tracking_mode"]
        == exercise["tracking_mode"]
        and row["recording_mode"]
        == exercise["recording_mode"]
        and row["data_fields"]
        == exercise["data_fields"]
    )


def import_exercises(
    connection,
    payload,
    report,
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
            if not profile_matches(
                by_id,
                exercise,
            ):
                raise ImportFailure(
                    f"profil incompatible pour {exercise_id}"
                )

            mapping[exercise_id] = (
                by_id["exercise_id"]
            )

            report["exercises_skipped"] += 1
            continue

        by_name = lookup_exercise_by_normalized(
            connection,
            normalized,
        )

        if by_name is not None:
            if not profile_matches(
                by_name,
                exercise,
            ):
                raise ImportFailure(
                    "conflit de profil pour le nom "
                    + exercise["name"]
                )

            mapping[exercise_id] = (
                by_name["exercise_id"]
            )

            report["exercises_reconciled"] += 1
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
    metric_values = []

    for set_item in sets:
        if tracking == "reps":
            metric_values.append(
                set_item["reps"]
            )
        else:
            metric_values.append(
                set_item["duration_seconds"]
            )

    target_metric = metric_values[0]

    if tracking == "reps":
        target_reps = target_metric
        target_duration = None
    else:
        target_reps = None
        target_duration = target_metric

    cursor = connection.execute(
        """
        INSERT INTO session_exercises(
            session_row_id,
            exercise_row_id,
            recording_mode,
            data_fields,
            position,
            load_mode,
            rest_seconds,
            target_sets,
            target_reps,
            target_duration_seconds,
            target_weight_kg,
            notes
        ) VALUES(
            ?, ?, 'sets', ?, ?, 'none', 0,
            ?, ?, ?, NULL, NULL
        );
        """,
        (
            session_row_id,
            exercise_row,
            item["data_fields"],
            position,
            len(sets),
            target_reps,
            target_duration,
        ),
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
            ) VALUES(?, ?, ?, ?, NULL);
            """,
            (
                session_exercise_row_id,
                set_index,
                reps,
                duration,
            ),
        )


def import_continuous_session_exercise(
    connection,
    session_row_id,
    position,
    item,
    exercise_row,
):
    cursor = connection.execute(
        """
        INSERT INTO session_exercises(
            session_row_id,
            exercise_row_id,
            recording_mode,
            data_fields,
            position,
            load_mode,
            rest_seconds,
            target_sets,
            target_reps,
            target_duration_seconds,
            target_weight_kg,
            notes
        ) VALUES(
            ?, ?, 'continuous', ?, ?, 'none', 0,
            NULL, NULL, NULL, NULL, NULL
        );
        """,
        (
            session_row_id,
            exercise_row,
            item["data_fields"],
            position,
        ),
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
            report["sessions_skipped"] += 1
            continue

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
                or row["data_fields"]
                != item["data_fields"]
            ):
                raise ImportFailure(
                    "snapshot de séance incompatible avec le catalogue desktop"
                )

            row_id = exercise_row_id(
                connection,
                desktop_id,
            )

            if item["recording_mode"] == "continuous":
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
        if body_exists(
            connection,
            observation["observation_id"],
        ):
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
        "sessions_skipped": 0,
        "body_imported": 0,
        "body_skipped": 0,
    }

    try:
        connection.execute(
            "PRAGMA foreign_keys = ON;"
        )

        require_schema_v4(
            connection
        )

        connection.execute(
            "BEGIN IMMEDIATE;"
        )

        mapping = import_exercises(
            connection,
            payload,
            report,
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

    args = parser.parse_args()

    try:
        payload = load_payload(
            args.json_path
        )

        validate_payload(
            payload
        )

        report = run_import(
            payload,
            args.database,
            args.dry_run,
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
