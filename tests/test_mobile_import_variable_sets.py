#!/usr/bin/env python3
"""Regression test for heterogeneous mobile set import."""

from __future__ import annotations

import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IMPORTER = ROOT / "tools" / "import_mobile_export.py"
EXPORTER = ROOT / "tools" / "export_pc_mobile.py"

EXPECTED_REPS = [
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    9,
    8,
    7,
    6,
    5,
    4,
]


SCHEMA = """
PRAGMA foreign_keys=ON;

CREATE TABLE exercises (
    id INTEGER PRIMARY KEY,
    exercise_id TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    normalized_name TEXT NOT NULL UNIQUE,
    tracking_mode TEXT NOT NULL,
    recording_mode TEXT NOT NULL,
    data_fields INTEGER NOT NULL
);

CREATE TABLE sessions (
    id INTEGER PRIMARY KEY,
    session_id TEXT NOT NULL UNIQUE,
    started_at TEXT NOT NULL,
    ended_at TEXT,
    session_type TEXT NOT NULL,
    notes TEXT
);

CREATE TABLE session_exercises (
    id INTEGER PRIMARY KEY,
    entry_id TEXT NOT NULL UNIQUE,
    session_row_id INTEGER NOT NULL
        REFERENCES sessions(id) ON DELETE CASCADE,
    exercise_row_id INTEGER NOT NULL
        REFERENCES exercises(id),
    recording_mode TEXT NOT NULL,
    data_fields INTEGER NOT NULL,
    position INTEGER NOT NULL,
    load_mode TEXT NOT NULL,
    rest_seconds INTEGER NOT NULL,
    target_sets INTEGER,
    target_reps INTEGER,
    target_duration_seconds INTEGER,
    target_weight_kg REAL,
    notes TEXT,
    equipment_id TEXT
);

CREATE TABLE performed_sets (
    id INTEGER PRIMARY KEY,
    session_exercise_row_id INTEGER NOT NULL
        REFERENCES session_exercises(id) ON DELETE CASCADE,
    position INTEGER NOT NULL,
    reps INTEGER,
    duration_seconds INTEGER,
    weight_kg REAL CHECK(weight_kg >= 0.0)
);

CREATE TABLE continuous_activity (
    id INTEGER PRIMARY KEY,
    session_exercise_row_id INTEGER NOT NULL UNIQUE
        REFERENCES session_exercises(id) ON DELETE CASCADE,
    duration_seconds INTEGER NOT NULL,
    speed_kmh REAL,
    distance_km REAL
);

CREATE TABLE body_observations (
    id INTEGER PRIMARY KEY,
    observation_id TEXT NOT NULL UNIQUE,
    observed_at TEXT NOT NULL,
    session_row_id INTEGER,
    body_weight_kg REAL,
    neck_cm REAL,
    shoulders_cm REAL,
    chest_cm REAL,
    waist_cm REAL,
    hips_cm REAL,
    left_arm_cm REAL,
    right_arm_cm REAL,
    left_forearm_cm REAL,
    right_forearm_cm REAL,
    left_thigh_cm REAL,
    right_thigh_cm REAL,
    left_calf_cm REAL,
    right_calf_cm REAL,
    notes TEXT
);

CREATE TABLE max_results (
    session_exercise_row_id INTEGER PRIMARY KEY,
    max_weight_kg REAL NOT NULL CHECK(max_weight_kg > 0)
);

CREATE TABLE custom_equipment (
    equipment_id TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    label_name TEXT NOT NULL,
    equipment_type TEXT NOT NULL,
    load_semantics TEXT NOT NULL
);

PRAGMA user_version=10;
"""

EXPECTED_SETS = [
    (
        reps,
        32.5 if index == 0 else
        0.0 if index == 1 else
        None if index in (2, 6) else
        40.25 + index * 1.5,
    )
    for index, reps in enumerate(EXPECTED_REPS)
]


def payload() -> dict:
    return {
        "format":
            "trainlog-mobile-export",
        "version":
            2,
        "generated_at":
            "2026-09-06T16:00:00+02:00",
        "exercises": [
            {
                "exercise_id":
                    "ex_mobile_pyramid",
                "name":
                    "Pompes pyramide",
                "recording_mode":
                    "sets",
                "tracking_mode":
                    "reps",
                "data_fields":
                    0,
            }
        ],
        "sessions": [
            {
                "session_id":
                    "se_mobile_pyramid",
                "started_at":
                    "2026-09-06T16:01:00+02:00",
                "session_type":
                    "training",
                "exercises": [
                    {
                        "entry_id":
                            "sxe_mobile_pyramid",
                        "position":
                            0,
                        "exercise_id":
                            "ex_mobile_pyramid",
                        "name":
                            "Pompes pyramide",
                        "recording_mode":
                            "sets",
                        "tracking_mode":
                            "reps",
                        "data_fields":
                            0,
                        "load_mode":
                            "none",
                        "rest_seconds":
                            0,
                        "equipment_id":
                            None,
                        "sets": [
                            ({"reps": reps} if weight is None else
                             {"reps": reps, "weight_kg": weight})
                            for reps, weight in EXPECTED_SETS
                        ],
                    }
                ],
            }
        ],
        "body_observations": [],
    }


def run_import(
    json_path: Path,
    database_path: Path,
) -> str:
    result = subprocess.run(
        [
            sys.executable,
            str(IMPORTER),
            str(json_path),
            "--database",
            str(database_path),
        ],
        check=False,
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        raise AssertionError(
            "import failed:\n"
            + result.stdout
            + result.stderr
        )

    return result.stdout


def run_export(output_path: Path, database_path: Path) -> None:
    result = subprocess.run(
        [sys.executable, str(EXPORTER), str(output_path),
         "--database", str(database_path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError("export failed:\n" + result.stdout + result.stderr)


def database_contents(database_path: Path) -> tuple:
    """Capture durable rows so rejected payloads prove transaction atomicity."""
    with sqlite3.connect(database_path) as connection:
        return tuple(
            tuple(connection.execute(
                f"SELECT * FROM {table} ORDER BY rowid"
            ).fetchall())
            for table in (
                "exercises", "sessions", "session_exercises",
                "performed_sets", "continuous_activity",
                "body_observations", "max_results",
            )
        )


def require_weight_rejection(
    base: Path,
    database_path: Path,
    value,
    suffix: str,
) -> None:
    rejected = payload()
    rejected["sessions"][0]["session_id"] = f"se_rejected_{suffix}"
    rejected["sessions"][0]["exercises"][0]["sets"][0]["weight_kg"] = value
    rejected_path = base / f"invalid-weight-{suffix}.json"
    rejected_path.write_text(
        json.dumps(rejected, ensure_ascii=False, allow_nan=True),
        encoding="utf-8",
    )
    before = database_contents(database_path)
    result = subprocess.run(
        [sys.executable, str(IMPORTER), str(rejected_path),
         "--database", str(database_path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode == 0 or "weight_kg" not in result.stderr:
        raise AssertionError(
            f"invalid set weight accepted ({suffix}):\n"
            + result.stdout + result.stderr
        )
    if database_contents(database_path) != before:
        raise AssertionError(f"invalid set weight mutated database ({suffix})")


def main() -> int:
    with tempfile.TemporaryDirectory(
        prefix="trainlog-mobile-variable-"
    ) as directory:
        base = Path(directory)
        database_path = base / "trainlog.db"
        json_path = base / "mobile.json"

        connection = sqlite3.connect(
            database_path
        )

        try:
            connection.executescript(
                SCHEMA
            )

            connection.commit()
        finally:
            connection.close()

        json_path.write_text(
            json.dumps(
                payload(),
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )

        first = run_import(
            json_path,
            database_path,
        )

        if (
            "MOBILE_IMPORT=PASS"
            not in first
            or "sessions_imported=1"
            not in first
        ):
            raise AssertionError(
                "first import report invalid:\n"
                + first
            )

        second = run_import(
            json_path,
            database_path,
        )

        if (
            "MOBILE_IMPORT=PASS"
            not in second
            or "sessions_skipped=1"
            not in second
        ):
            raise AssertionError(
                "second import is not idempotent:\n"
                + second
            )

        renamed_payload = payload()
        renamed_payload["exercises"][0]["name"] = "Pompes corrigées"
        renamed_payload["sessions"][0]["exercises"][0]["name"] = "Pompes corrigées"
        json_path.write_text(
            json.dumps(renamed_payload, ensure_ascii=False),
            encoding="utf-8",
        )
        renamed = run_import(json_path, database_path)
        if "exercises_reconciled=1" not in renamed:
            raise AssertionError(
                "stable-ID rename was not reconciled:\n" + renamed
            )

        connection = sqlite3.connect(
            database_path
        )

        try:
            rows = connection.execute(
                """
                SELECT ps.reps, ps.weight_kg
                FROM performed_sets ps
                JOIN session_exercises se
                  ON se.id =
                     ps.session_exercise_row_id
                JOIN sessions s
                  ON s.id = se.session_row_id
                WHERE s.session_id =
                      'se_mobile_pyramid'
                ORDER BY ps.position;
                """
            ).fetchall()

            if rows != EXPECTED_SETS:
                raise AssertionError(
                    f"ordered sets mismatch: {rows!r}"
                )

            target = connection.execute(
                """
                SELECT
                    target_sets,
                    target_reps,
                    target_duration_seconds
                FROM session_exercises se
                JOIN sessions s
                  ON s.id = se.session_row_id
                WHERE s.session_id =
                      'se_mobile_pyramid';
                """
            ).fetchone()

            if target != (
                None,
                None,
                None,
            ):
                raise AssertionError(
                    f"fake target persisted: {target!r}"
                )

            catalog = connection.execute(
                """
                SELECT exercise_id, name, normalized_name
                FROM exercises;
                """
            ).fetchall()
            if catalog != [
                (
                    "ex_mobile_pyramid",
                    "Pompes corrigées",
                    "pompes corrigées",
                )
            ]:
                raise AssertionError(
                    f"stable-ID rename created or lost catalog row: {catalog!r}"
                )
        finally:
            connection.close()

        exported_path = base / "desktop-v2.json"
        roundtrip_db = base / "roundtrip.db"
        run_export(exported_path, database_path)
        exported = json.loads(exported_path.read_text(encoding="utf-8"))
        exported_sets = exported["sessions"][0]["exercises"][0]["sets"]
        expected_json_sets = [
            ({"reps": reps} if weight is None else
             {"reps": reps, "weight_kg": weight})
            for reps, weight in EXPECTED_SETS
        ]
        if exported_sets != expected_json_sets:
            raise AssertionError(f"V2 export changed sets: {exported_sets!r}")

        with sqlite3.connect(roundtrip_db) as connection:
            connection.executescript(SCHEMA)
        imported = run_import(exported_path, roundtrip_db)
        if "sessions_imported=1" not in imported:
            raise AssertionError("V2 reimport failed:\n" + imported)
        replay = run_import(exported_path, roundtrip_db)
        if "sessions_skipped=1" not in replay:
            raise AssertionError("V2 replay not idempotent:\n" + replay)
        with sqlite3.connect(roundtrip_db) as connection:
            roundtrip_sets = connection.execute(
                "SELECT reps,weight_kg FROM performed_sets ORDER BY position"
            ).fetchall()
        if roundtrip_sets != EXPECTED_SETS:
            raise AssertionError(f"V2 reimport changed sets: {roundtrip_sets!r}")

        # INVARIANT: all validation precedes mutation; every invalid optional
        # set weight therefore rejects the complete artifact atomically.
        for invalid_weight, suffix in (
            (-1.5, "negative"),
            (True, "bool"),
            ("20", "string"),
            (float("nan"), "nan"),
            (float("inf"), "infinity"),
        ):
            require_weight_rejection(
                base, database_path, invalid_weight, suffix
            )

        # A migration-era v9 database cannot represent explicit zero. Reject
        # before beginning the import instead of collapsing zero into NULL or
        # leaving a partially inserted graph.
        v9_database = base / "unmigrated-v9.db"
        with sqlite3.connect(v9_database) as connection:
            connection.executescript(
                SCHEMA.replace(
                    "weight_kg REAL CHECK(weight_kg >= 0.0)",
                    "weight_kg REAL CHECK(weight_kg > 0.0)",
                ).replace("PRAGMA user_version=10", "PRAGMA user_version=9")
            )
        before_v9 = database_contents(v9_database)
        rejected_v9 = subprocess.run(
            [sys.executable, str(IMPORTER), str(exported_path),
             "--database", str(v9_database)],
            check=False,
            capture_output=True,
            text=True,
        )
        if rejected_v9.returncode == 0 or "schéma desktop v10" not in rejected_v9.stderr:
            raise AssertionError(
                "zero-bearing V2 artifact did not fail explicitly on v9:\n" +
                rejected_v9.stdout + rejected_v9.stderr
            )
        if database_contents(v9_database) != before_v9:
            raise AssertionError("zero-bearing V2 artifact mutated v9 database")

    print(
        "PASS mobile_import_variable_sets"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
