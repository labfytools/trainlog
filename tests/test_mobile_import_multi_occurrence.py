#!/usr/bin/env python3
"""End-to-end V2 import regression: Walk, other movement, Walk again."""
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPORTER = ROOT / "tools/import_mobile_export.py"
EQUIPMENT_IMPORTER = ROOT / "tools/import_equipment_associations.py"
CATALOG_EXPORTER = ROOT / "tools/export_pc_catalog.py"
MOBILE_EXPORTER = ROOT / "tools/export_pc_mobile.py"
EQUIPMENT_EXPORTER = ROOT / "tools/export_equipment_associations.py"

SCHEMA = """
CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,normalized_name TEXT UNIQUE,tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER);
CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,started_at TEXT,ended_at TEXT,session_type TEXT,notes TEXT);
CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT NOT NULL UNIQUE,session_row_id INTEGER,exercise_row_id INTEGER,recording_mode TEXT,data_fields INTEGER,position INTEGER,load_mode TEXT,rest_seconds INTEGER,target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,equipment_id TEXT,notes TEXT,UNIQUE(session_row_id,position));
CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER,position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL);
CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER UNIQUE,duration_seconds INTEGER,speed_kmh REAL,distance_km REAL);
CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT UNIQUE,observed_at TEXT,session_row_id INTEGER,body_weight_kg REAL,neck_cm REAL,shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL,notes TEXT);
PRAGMA user_version=7;
"""

def payload():
    catalog = [
        {"exercise_id":"ex_walk","name":"Marche","recording_mode":"continuous","tracking_mode":"duration","data_fields":0},
        {"exercise_id":"ex_push","name":"Pompes","recording_mode":"sets","tracking_mode":"reps","data_fields":0},
    ]
    entries = [
        {"entry_id":"sxe_walk_10","position":0,"exercise_id":"ex_walk","name":"Marche","recording_mode":"continuous","tracking_mode":"duration","data_fields":0,"load_mode":"none","rest_seconds":0,"equipment_id":None,"continuous":{"duration_seconds":600}},
        {"entry_id":"sxe_push","position":1,"exercise_id":"ex_push","name":"Pompes","recording_mode":"sets","tracking_mode":"reps","data_fields":0,"load_mode":"none","rest_seconds":0,"equipment_id":"leg_press","sets":[{"reps":12,"weight_kg":20.0}]},
        {"entry_id":"sxe_walk_15","position":2,"exercise_id":"ex_walk","name":"Marche","recording_mode":"continuous","tracking_mode":"duration","data_fields":0,"load_mode":"none","rest_seconds":0,"equipment_id":None,"continuous":{"duration_seconds":900}},
    ]
    return {"format":"trainlog-mobile-export","version":2,"generated_at":"2026-09-07T10:00:00+02:00","exercises":catalog,"sessions":[{"session_id":"se_multi","started_at":"2026-09-07T10:00:00+02:00","session_type":"training","exercises":entries}],"body_observations":[]}

def invoke(path, db):
    result = subprocess.run([sys.executable, str(IMPORTER), str(path), "--database", str(db)], text=True, capture_output=True)
    if result.returncode: raise AssertionError(result.stdout + result.stderr)
    return result.stdout


def invoke_equipment(path, db):
    result = subprocess.run(
        [sys.executable, str(EQUIPMENT_IMPORTER), str(path), "--database", str(db)],
        text=True,
        capture_output=True,
    )
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout

def main():
    with tempfile.TemporaryDirectory(prefix="trainlog-multi-") as temp:
        root = Path(temp); db = root / "db.sqlite"; artifact = root / "mobile-v2.json"
        equipment = root / "equipment-v2.json"
        con = sqlite3.connect(db); con.executescript(SCHEMA); con.close()
        artifact.write_text(json.dumps(payload()), encoding="utf-8")
        assert "sessions_imported=1" in invoke(artifact, db)
        con = sqlite3.connect(db)
        rows = con.execute("SELECT e.exercise_id,se.entry_id,se.position,ca.duration_seconds FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN continuous_activity ca ON ca.session_exercise_row_id=se.id ORDER BY se.position").fetchall()
        assert rows == [("ex_walk","sxe_walk_10",0,600),("ex_push","sxe_push",1,None),("ex_walk","sxe_walk_15",2,900)], rows
        assert con.execute("SELECT weight_kg FROM performed_sets").fetchone()[0] == 20.0
        con.close()

        equipment.write_text(json.dumps({
            "format": "trainlog-equipment-associations",
            "version": 2,
            "generated_at": "2026-09-07T10:00:00+02:00",
            "associations": [
                {"session_id": "se_multi", "entry_id": "sxe_walk_10", "exercise_id": "ex_walk", "state": "set", "equipment_id": "treadmill"},
                {"session_id": "se_multi", "entry_id": "sxe_push", "exercise_id": "ex_push", "state": "set", "equipment_id": "leg_press"},
                {"session_id": "se_multi", "entry_id": "sxe_walk_15", "exercise_id": "ex_walk", "state": "cleared"},
            ],
        }), encoding="utf-8")

        # Regression for the real failure: the historic TUI invocation passed
        # only the artifact, while the importer requires the target database.
        rejected = subprocess.run(
            [sys.executable, str(EQUIPMENT_IMPORTER), str(equipment)],
            text=True,
            capture_output=True,
        )
        assert rejected.returncode != 0
        assert "the following arguments are required: --database" in rejected.stderr

        assert "EQUIPMENT_ASSOCIATIONS_IMPORT=PASS" in invoke_equipment(equipment, db)
        con = sqlite3.connect(db)
        rows = con.execute(
            "SELECT entry_id,equipment_id FROM session_exercises ORDER BY position"
        ).fetchall()
        assert rows == [
            ("sxe_walk_10", "treadmill"),
            ("sxe_push", "leg_press"),
            ("sxe_walk_15", None),
        ], rows
        con.close()

        assert "sessions_reconciled=1" in invoke(artifact, db)
        assert "EQUIPMENT_ASSOCIATIONS_IMPORT=PASS" in invoke_equipment(equipment, db)
        con = sqlite3.connect(db)
        assert con.execute("SELECT count(*) FROM session_exercises").fetchone()[0] == 3
        assert con.execute(
            "SELECT entry_id,equipment_id FROM session_exercises ORDER BY position"
        ).fetchall() == rows
        con.close()

        catalog_output = root / "pc-catalog.json"
        mobile_output = root / "pc-mobile-v2.json"
        equipment_output = root / "pc-equipment-v2.json"
        for tool, output, marker in (
            (CATALOG_EXPORTER, catalog_output, "PC_CATALOG_EXPORT=PASS"),
            (MOBILE_EXPORTER, mobile_output, "PC_MOBILE_EXPORT=PASS"),
            (EQUIPMENT_EXPORTER, equipment_output, "EQUIPMENT_ASSOCIATIONS_EXPORT=PASS"),
        ):
            result = subprocess.run(
                [sys.executable, str(tool), str(output), "--database", str(db)],
                text=True,
                capture_output=True,
            )
            assert result.returncode == 0, result.stdout + result.stderr
            assert marker in result.stdout
    print("PASS mobile_import_multi_occurrence")

if __name__ == "__main__": main()
