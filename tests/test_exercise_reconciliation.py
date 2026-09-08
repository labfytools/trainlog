#!/usr/bin/env python3
"""Exercise-identity reconciliation and complete V2 round-trip regressions."""

import copy
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IMPORT_MOBILE = ROOT / "tools/import_mobile_export.py"
IMPORT_DEFINITIONS = ROOT / "tools/import_equipment_definitions.py"
IMPORT_ASSOCIATIONS = ROOT / "tools/import_equipment_associations.py"
EXPORT_DEFINITIONS = ROOT / "tools/export_equipment_definitions.py"
EXPORT_CATALOG = ROOT / "tools/export_pc_catalog.py"
EXPORT_MOBILE = ROOT / "tools/export_pc_mobile.py"
EXPORT_ASSOCIATIONS = ROOT / "tools/export_equipment_associations.py"

DESKTOP_WALK_ID = "ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d"
ANDROID_WALK_ID = "ex_23212d79-52ce-4195-914d-dd983f133936"
ANDROID_SESSION_ID = "se_ac3908d6-8e3e-4ac6-81a6-62dc2c39075a"
ANDROID_WALK_ENTRY_ID = "sxe_f25142c8-455e-4346-9bfc-31d0989e275d"

SCHEMA = """
PRAGMA foreign_keys=ON;
CREATE TABLE exercises(
 id INTEGER PRIMARY KEY, exercise_id TEXT NOT NULL UNIQUE,
 name TEXT NOT NULL, normalized_name TEXT NOT NULL UNIQUE,
 tracking_mode TEXT NOT NULL, recording_mode TEXT NOT NULL,
 data_fields INTEGER NOT NULL
);
CREATE TABLE sessions(
 id INTEGER PRIMARY KEY, session_id TEXT NOT NULL UNIQUE,
 started_at TEXT NOT NULL, ended_at TEXT, session_type TEXT NOT NULL, notes TEXT
);
CREATE TABLE session_exercises(
 id INTEGER PRIMARY KEY, entry_id TEXT NOT NULL UNIQUE,
 session_row_id INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
 exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE RESTRICT,
 recording_mode TEXT NOT NULL, data_fields INTEGER NOT NULL, position INTEGER NOT NULL,
 load_mode TEXT NOT NULL, rest_seconds INTEGER NOT NULL, target_sets INTEGER,
 target_reps INTEGER, target_duration_seconds INTEGER, target_weight_kg REAL,
 equipment_id TEXT, notes TEXT, UNIQUE(session_row_id,position)
);
CREATE TABLE performed_sets(
 id INTEGER PRIMARY KEY,
 session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id) ON DELETE CASCADE,
 position INTEGER NOT NULL, reps INTEGER, duration_seconds INTEGER, weight_kg REAL
);
CREATE TABLE continuous_activity(
 id INTEGER PRIMARY KEY,
 session_exercise_row_id INTEGER NOT NULL UNIQUE REFERENCES session_exercises(id) ON DELETE CASCADE,
 duration_seconds INTEGER NOT NULL, speed_kmh REAL, distance_km REAL
);
CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY,max_weight_kg REAL NOT NULL);
CREATE TABLE body_observations(
 id INTEGER PRIMARY KEY, observation_id TEXT NOT NULL UNIQUE, observed_at TEXT NOT NULL,
 session_row_id INTEGER, body_weight_kg REAL, neck_cm REAL, shoulders_cm REAL,
 chest_cm REAL, waist_cm REAL, hips_cm REAL, left_arm_cm REAL, right_arm_cm REAL,
 left_forearm_cm REAL, right_forearm_cm REAL, left_thigh_cm REAL,
 right_thigh_cm REAL, left_calf_cm REAL, right_calf_cm REAL, notes TEXT
);
CREATE TABLE custom_equipment(
 equipment_id TEXT PRIMARY KEY, display_name TEXT NOT NULL, label_name TEXT NOT NULL,
 equipment_type TEXT NOT NULL, load_semantics TEXT NOT NULL
);
PRAGMA user_version=9;
"""


def run(tool, *arguments, succeeds=True):
    result = subprocess.run(
        [sys.executable, str(tool), *(str(value) for value in arguments)],
        text=True,
        capture_output=True,
    )
    if succeeds:
        assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert result.returncode != 0, result.stdout + result.stderr
    return result.stdout + result.stderr


def create_database(path, fields=1, with_history=False):
    connection = sqlite3.connect(path)
    connection.executescript(SCHEMA)
    connection.execute(
        "INSERT INTO exercises(exercise_id,name,normalized_name,tracking_mode,recording_mode,data_fields) "
        "VALUES(?,?,?,?,?,?);",
        (DESKTOP_WALK_ID, "Marche", "marche", "duration", "continuous", fields),
    )
    if with_history:
        connection.execute(
            "INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,?);",
            ("se_c0d07454-b58b-403e-8b79-744619815fc5", "2026-09-07T19:27:35+02:00", "training"),
        )
        session_row = connection.execute("SELECT id FROM sessions").fetchone()[0]
        exercise_row = connection.execute("SELECT id FROM exercises").fetchone()[0]
        for position, entry_id, duration, speed, equipment in (
            (0, "sxe_draft_legacy_1", 900, 6.6, None),
            (8, "sxe_093c1331-beaa-4b69-91b3-240292709be6", 300, 4.0, "treadmill"),
        ):
            cursor = connection.execute(
                "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,"
                "recording_mode,data_fields,position,load_mode,rest_seconds,equipment_id) "
                "VALUES(?,?,?,'continuous',1,?,'none',0,?);",
                (entry_id, session_row, exercise_row, position, equipment),
            )
            connection.execute(
                "INSERT INTO continuous_activity(session_exercise_row_id,duration_seconds,speed_kmh) "
                "VALUES(?,?,?);",
                (cursor.lastrowid, duration, speed),
            )
    connection.commit()
    connection.close()


def mobile_payload(fields=3):
    return {
        "format": "trainlog-mobile-export",
        "version": 2,
        "generated_at": "2026-09-08T15:21:26+02:00",
        "exercises": [
            {
                "exercise_id": ANDROID_WALK_ID,
                "name": "Marche",
                "recording_mode": "continuous",
                "tracking_mode": "duration",
                "data_fields": fields,
            },
            {
                "exercise_id": "ex_android_strength",
                "name": "Machine Android",
                "recording_mode": "sets",
                "tracking_mode": "reps",
                "data_fields": 0,
            },
        ],
        "sessions": [
            {
                "session_id": ANDROID_SESSION_ID,
                "started_at": "2026-09-08T11:25:30+02:00",
                "session_type": "max_test",
                "exercises": [
                    {
                        "entry_id": ANDROID_WALK_ENTRY_ID,
                        "position": 0,
                        "exercise_id": ANDROID_WALK_ID,
                        "name": "Marche",
                        "recording_mode": "continuous",
                        "tracking_mode": "duration",
                        "data_fields": fields,
                        "load_mode": "none",
                        "rest_seconds": 0,
                        "equipment_id": "treadmill",
                        "continuous": {
                            "duration_seconds": 900,
                            **({"speed_kmh": 5.5} if fields & 1 else {}),
                            **({"distance_km": 1.2} if fields & 2 else {}),
                        },
                    },
                    {
                        "entry_id": "sxe_android_strength",
                        "position": 1,
                        "exercise_id": "ex_android_strength",
                        "name": "Machine Android",
                        "recording_mode": "sets",
                        "tracking_mode": "reps",
                        "data_fields": 0,
                        "load_mode": "none",
                        "rest_seconds": 0,
                        "equipment_id": "eq_android_custom",
                        "sets": [{"reps": 9, "weight_kg": 42.5}],
                    },
                ],
            },
        ],
        "body_observations": [
            {
                "observation_id": "bo_android_reconciliation",
                "observed_at": "2026-09-08T08:00:00+02:00",
                "body_weight_kg": 83.7,
                "neck_cm": 37.1,
                "shoulders_cm": 111.2,
                "chest_cm": 99.3,
                "waist_cm": 84.4,
                "hips_cm": 96.5,
                "left_arm_cm": 31.6,
                "right_arm_cm": 31.7,
                "left_forearm_cm": 27.8,
                "right_forearm_cm": 27.9,
                "left_thigh_cm": 56.1,
                "right_thigh_cm": 56.2,
                "left_calf_cm": 37.3,
                "right_calf_cm": 37.4,
            },
        ],
    }


def definitions_payload():
    return {
        "format": "trainlog-equipment-definitions",
        "version": 1,
        "generated_at": "2026-09-08T15:21:26+02:00",
        "equipment": [
            {
                "equipment_id": "eq_android_custom",
                "display_name": "Machine Android custom",
                "label_name": "Machine custom",
                "equipment_type": "custom_machine",
                "load_semantics": "external",
            },
        ],
    }


def associations_payload():
    return {
        "format": "trainlog-equipment-associations",
        "version": 2,
        "generated_at": "2026-09-08T15:21:26+02:00",
        "associations": [
            {
                "session_id": ANDROID_SESSION_ID,
                "entry_id": ANDROID_WALK_ENTRY_ID,
                "exercise_id": ANDROID_WALK_ID,
                "state": "set",
                "equipment_id": "treadmill",
            },
            {
                "session_id": ANDROID_SESSION_ID,
                "entry_id": "sxe_android_strength",
                "exercise_id": "ex_android_strength",
                "state": "set",
                "equipment_id": "eq_android_custom",
            },
        ],
    }


def assert_no_integrity_error(connection):
    assert connection.execute("PRAGMA integrity_check;").fetchone()[0] == "ok"
    assert connection.execute("PRAGMA foreign_key_check;").fetchall() == []


def semantic_state(connection):
    """Capture exchanged business state without depending on local rowids."""
    return (
        connection.execute(
            "SELECT exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields "
            "FROM exercises ORDER BY exercise_id;"
        ).fetchall(),
        connection.execute(
            "SELECT session_id,started_at,ended_at,session_type,notes "
            "FROM sessions ORDER BY session_id;"
        ).fetchall(),
        connection.execute(
            "SELECT s.session_id,se.entry_id,e.exercise_id,se.recording_mode,se.data_fields,"
            "se.position,se.load_mode,se.rest_seconds,se.target_sets,se.target_reps,"
            "se.target_duration_seconds,se.target_weight_kg,se.equipment_id,se.notes "
            "FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id "
            "JOIN exercises e ON e.id=se.exercise_row_id "
            "ORDER BY s.session_id,se.position;"
        ).fetchall(),
        connection.execute(
            "SELECT se.entry_id,ps.position,ps.reps,ps.duration_seconds,ps.weight_kg "
            "FROM performed_sets ps JOIN session_exercises se "
            "ON se.id=ps.session_exercise_row_id ORDER BY se.entry_id,ps.position;"
        ).fetchall(),
        connection.execute(
            "SELECT se.entry_id,ca.duration_seconds,ca.speed_kmh,ca.distance_km "
            "FROM continuous_activity ca JOIN session_exercises se "
            "ON se.id=ca.session_exercise_row_id ORDER BY se.entry_id;"
        ).fetchall(),
        connection.execute(
            "SELECT observation_id,observed_at,body_weight_kg,neck_cm,shoulders_cm,"
            "chest_cm,waist_cm,hips_cm,left_arm_cm,right_arm_cm,left_forearm_cm,"
            "right_forearm_cm,left_thigh_cm,right_thigh_cm,left_calf_cm,right_calf_cm "
            "FROM body_observations ORDER BY observation_id;"
        ).fetchall(),
        connection.execute(
            "SELECT equipment_id,display_name,label_name,equipment_type,load_semantics "
            "FROM custom_equipment ORDER BY equipment_id;"
        ).fetchall(),
    )


def main():
    with tempfile.TemporaryDirectory(prefix="trainlog-reconcile-") as directory:
        root = Path(directory)

        # A: identical profiles with different IDs use the existing desktop ID.
        identical_db = root / "identical.db"
        create_database(identical_db, fields=1)
        identical = mobile_payload(fields=1)
        identical["sessions"] = []
        identical["body_observations"] = []
        identical["exercises"] = identical["exercises"][:1]
        identical_path = root / "identical.json"
        identical_path.write_text(json.dumps(identical), encoding="utf-8")
        first_identical = run(
            IMPORT_MOBILE, identical_path, "--database", identical_db,
            "--trace-exercises",
        )
        assert "exercises_reconciled=0" in first_identical
        assert "exercises_skipped=1" in first_identical
        assert f'"exercise_id": "{ANDROID_WALK_ID}"' in first_identical
        assert f'"lookup": "normalized_name:{DESKTOP_WALK_ID}"' in first_identical
        assert '"decision": "existing-identical"' in first_identical
        with sqlite3.connect(identical_db) as connection:
            assert connection.execute(
                "SELECT exercise_id,data_fields FROM exercises"
            ).fetchall() == [(DESKTOP_WALK_ID, 1)]
            identical_before_replay = semantic_state(connection)
        second_identical = run(
            IMPORT_MOBILE, identical_path, "--database", identical_db,
            "--trace-exercises",
        )
        assert "exercises_reconciled=0" in second_identical
        assert "exercises_skipped=1" in second_identical
        assert '"decision": "existing-identical"' in second_identical
        with sqlite3.connect(identical_db) as connection:
            assert semantic_state(connection) == identical_before_replay

        # C/G: equal text cannot merge incomparable masks or different modes.
        for label, mutate in (
            ("fields", lambda value: value["exercises"][0].update(data_fields=2)),
            ("mode", lambda value: value["exercises"][0].update(recording_mode="sets", tracking_mode="duration", data_fields=0)),
        ):
            conflict_db = root / f"conflict-{label}.db"
            create_database(conflict_db, fields=1)
            conflict = copy.deepcopy(identical)
            mutate(conflict)
            conflict_path = root / f"conflict-{label}.json"
            conflict_path.write_text(json.dumps(conflict), encoding="utf-8")
            output = run(
                IMPORT_MOBILE,
                conflict_path,
                "--database",
                conflict_db,
                succeeds=False,
            )
            assert "profil incompatible entre identités" in output
            with sqlite3.connect(conflict_db) as connection:
                assert connection.execute(
                    "SELECT exercise_id,data_fields FROM exercises"
                ).fetchall() == [(DESKTOP_WALK_ID, 1)]

        # B/D/E: run the real ordered Android -> PC tools on a v8 copy.
        complete_db = root / "complete.db"
        create_database(complete_db, fields=1, with_history=True)
        mobile_path = root / "trainlog-mobile-export-v2.json"
        definitions_path = root / "trainlog-mobile-equipment-definitions-v1.json"
        associations_path = root / "trainlog-equipment-associations-v2.json"
        mobile_path.write_text(json.dumps(mobile_payload()), encoding="utf-8")
        definitions_path.write_text(json.dumps(definitions_payload()), encoding="utf-8")
        associations_path.write_text(json.dumps(associations_payload()), encoding="utf-8")

        assert "definitions_imported=1" in run(
            IMPORT_DEFINITIONS, definitions_path, "--database", complete_db
        )
        assert "sessions_imported=1" in run(
            IMPORT_MOBILE, mobile_path, "--database", complete_db
        )
        assert "EQUIPMENT_ASSOCIATIONS_IMPORT=PASS" in run(
            IMPORT_ASSOCIATIONS,
            associations_path,
            "--database",
            complete_db,
            "--mobile-export",
            mobile_path,
        )

        with sqlite3.connect(complete_db) as connection:
            assert connection.execute(
                "SELECT exercise_id,data_fields FROM exercises WHERE normalized_name='marche';"
            ).fetchall() == [(DESKTOP_WALK_ID, 3)]
            assert connection.execute(
                "SELECT COUNT(*) FROM exercises WHERE exercise_id=?;",
                (ANDROID_WALK_ID,),
            ).fetchone()[0] == 0
            rows = connection.execute(
                "SELECT s.session_id,se.entry_id,se.position,e.exercise_id,se.data_fields,"
                "ca.duration_seconds,ca.speed_kmh,ca.distance_km,se.equipment_id "
                "FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id "
                "JOIN exercises e ON e.id=se.exercise_row_id "
                "LEFT JOIN continuous_activity ca ON ca.session_exercise_row_id=se.id "
                "WHERE e.normalized_name='marche' ORDER BY s.started_at,se.position;"
            ).fetchall()
            assert rows == [
                (
                    "se_c0d07454-b58b-403e-8b79-744619815fc5",
                    "sxe_draft_legacy_1", 0, DESKTOP_WALK_ID, 1,
                    900, 6.6, None, None,
                ),
                (
                    "se_c0d07454-b58b-403e-8b79-744619815fc5",
                    "sxe_093c1331-beaa-4b69-91b3-240292709be6", 8,
                    DESKTOP_WALK_ID, 1, 300, 4.0, None, "treadmill",
                ),
                (
                    ANDROID_SESSION_ID, ANDROID_WALK_ENTRY_ID, 0,
                    DESKTOP_WALK_ID, 3, 900, 5.5, 1.2, "treadmill",
                ),
            ]
            assert connection.execute(
                "SELECT reps,weight_kg FROM performed_sets"
            ).fetchall() == [(9, 42.5)]
            assert connection.execute(
                "SELECT body_weight_kg,neck_cm,shoulders_cm,chest_cm,waist_cm,hips_cm,"
                "left_arm_cm,right_arm_cm,left_forearm_cm,right_forearm_cm,"
                "left_thigh_cm,right_thigh_cm,left_calf_cm,right_calf_cm "
                "FROM body_observations "
                "WHERE observation_id='bo_android_reconciliation';"
            ).fetchone() == (
                83.7, 37.1, 111.2, 99.3, 84.4, 96.5, 31.6, 31.7,
                27.8, 27.9, 56.1, 56.2, 37.3, 37.4,
            )
            assert_no_integrity_error(connection)

        # F: replay Android input, publish every PC companion, then re-import
        # the merged PC snapshot.  The semantic database state must stay fixed.
        assert "definitions_skipped=1" in run(
            IMPORT_DEFINITIONS, definitions_path, "--database", complete_db
        )
        replay_output = run(
            IMPORT_MOBILE, mobile_path, "--database", complete_db
        )
        assert "sessions_skipped=1" in replay_output
        assert "exercises_reconciled=0" in replay_output
        assert "exercises_skipped=2" in replay_output
        run(
            IMPORT_ASSOCIATIONS,
            associations_path,
            "--database",
            complete_db,
            "--mobile-export",
            mobile_path,
        )

        pc_definitions = root / "trainlog-pc-equipment-definitions-v1.json"
        pc_catalog = root / "trainlog-pc-catalog-v1.json"
        pc_mobile = root / "trainlog-pc-mobile-export-v2.json"
        pc_associations = root / "trainlog-pc-equipment-associations-v2.json"
        for tool, output in (
            (EXPORT_DEFINITIONS, pc_definitions),
            (EXPORT_CATALOG, pc_catalog),
            (EXPORT_MOBILE, pc_mobile),
            (EXPORT_ASSOCIATIONS, pc_associations),
        ):
            run(tool, output, "--database", complete_db)

        exported = json.loads(pc_mobile.read_text(encoding="utf-8"))
        walk = next(
            item for item in exported["exercises"]
            if item["exercise_id"] == DESKTOP_WALK_ID
        )
        assert walk["data_fields"] == 3
        old_walks = [
            item
            for session in exported["sessions"]
            for item in session["exercises"]
            if item["entry_id"] in {
                "sxe_draft_legacy_1",
                "sxe_093c1331-beaa-4b69-91b3-240292709be6",
            }
        ]
        assert len(old_walks) == 2
        assert all(item["data_fields"] == 1 for item in old_walks)
        assert all("distance_km" not in item["continuous"] for item in old_walks)

        with sqlite3.connect(complete_db) as connection:
            before = semantic_state(connection)
        assert "sessions_skipped=2" in run(
            IMPORT_MOBILE, pc_mobile, "--database", complete_db
        )
        run(IMPORT_ASSOCIATIONS, pc_associations, "--database", complete_db)
        with sqlite3.connect(complete_db) as connection:
            after = semantic_state(connection)
            assert before == after
            assert_no_integrity_error(connection)

    print("PASS exercise_reconciliation")


if __name__ == "__main__":
    main()
