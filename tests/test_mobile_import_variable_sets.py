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
    notes TEXT
);

CREATE TABLE performed_sets (
    id INTEGER PRIMARY KEY,
    session_exercise_row_id INTEGER NOT NULL
        REFERENCES session_exercises(id) ON DELETE CASCADE,
    position INTEGER NOT NULL,
    reps INTEGER,
    duration_seconds INTEGER,
    weight_kg REAL
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

PRAGMA user_version=5;
"""


def payload() -> dict:
    return {
        "format":
            "trainlog-mobile-export",
        "version":
            1,
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
                        "sets": [
                            {
                                "reps": reps
                            }
                            for reps
                            in EXPECTED_REPS
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
                SELECT ps.reps
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

            reps = [
                row[0]
                for row in rows
            ]

            if reps != EXPECTED_REPS:
                raise AssertionError(
                    f"reps mismatch: {reps!r}"
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

    print(
        "PASS mobile_import_variable_sets"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
