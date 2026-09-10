#!/usr/bin/env python3
"""End-to-end V2 import regression: Walk, other movement, Walk again."""
import json
import copy
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPORTER = ROOT / "tools/import_mobile_export.py"
EQUIPMENT_IMPORTER = ROOT / "tools/import_equipment_associations.py"
DEFINITIONS_IMPORTER = ROOT / "tools/import_equipment_definitions.py"
MOBILE_EXPORTER = ROOT / "tools/export_pc_mobile.py"
EQUIPMENT_EXPORTER = ROOT / "tools/export_equipment_associations.py"

SCHEMA = """
CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,normalized_name TEXT UNIQUE,tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER);
CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,started_at TEXT,ended_at TEXT,session_type TEXT,notes TEXT);
CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT NOT NULL UNIQUE,session_row_id INTEGER,exercise_row_id INTEGER,recording_mode TEXT,data_fields INTEGER,position INTEGER,load_mode TEXT,rest_seconds INTEGER,target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,equipment_id TEXT,notes TEXT,UNIQUE(session_row_id,position));
CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER,position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL);
CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER UNIQUE,duration_seconds INTEGER,speed_kmh REAL,distance_km REAL);
CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY,max_weight_kg REAL NOT NULL);
CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT UNIQUE,observed_at TEXT,session_row_id INTEGER,body_weight_kg REAL,neck_cm REAL,shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL,notes TEXT);
CREATE TABLE custom_equipment(equipment_id TEXT PRIMARY KEY,display_name TEXT NOT NULL,label_name TEXT NOT NULL,equipment_type TEXT NOT NULL,load_semantics TEXT NOT NULL);
PRAGMA user_version=10;
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
    return {"format":"trainlog-mobile-export","version":2,"generated_at":"2026-09-07T10:00:00+02:00","exercises":catalog,"sessions":[{"session_id":"se_multi","started_at":"2026-09-07T10:00:00+02:00","session_type":"training","exercises":entries}],"body_observations":[{"observation_id":"bo_fixture","observed_at":"2026-03-01T08:00:00+01:00","body_weight_kg":72.5}]}

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


def invoke_definitions(path, db):
    result = subprocess.run(
        [sys.executable, str(DEFINITIONS_IMPORTER), str(path), "--database", str(db)],
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

        # Mobile v2 cannot transport custom definitions. Reject the complete
        # v8 input before catalog/session mutation, with occurrence context.
        unknown_db = root / "unknown.db"
        con = sqlite3.connect(unknown_db); con.executescript(SCHEMA); con.close()
        unknown_artifact = root / "unknown-mobile-v2.json"
        unknown_payload = copy.deepcopy(payload())
        unknown_payload["sessions"][0]["exercises"][1]["equipment_id"] = "custom_rack"
        unknown_artifact.write_text(json.dumps(unknown_payload), encoding="utf-8")
        rejected = subprocess.run(
            [sys.executable, str(IMPORTER), str(unknown_artifact), "--database", str(unknown_db)],
            text=True,
            capture_output=True,
        )
        assert rejected.returncode != 0
        assert "session_id=se_multi" in rejected.stderr
        assert "entry_id=sxe_push" in rejected.stderr
        assert "equipment_id=custom_rack" in rejected.stderr
        con = sqlite3.connect(unknown_db)
        assert con.execute("SELECT count(*) FROM sessions").fetchone()[0] == 0
        assert con.execute("SELECT count(*) FROM exercises").fetchone()[0] == 0
        con.close()

        # Android -> PC ordering regression: the companion definition is
        # imported through its real importer before the V2 snapshot validates
        # the entry reference.  A replay retains both stable identities and
        # never creates a duplicate custom machine or session occurrence.
        defined_db = root / "defined.db"
        con = sqlite3.connect(defined_db); con.executescript(SCHEMA); con.close()
        definitions = root / "mobile-definitions-v1.json"
        definitions.write_text(json.dumps({
            "format": "trainlog-equipment-definitions",
            "version": 1,
            "generated_at": "2026-09-07T10:00:00+02:00",
            "equipment": [{
                "equipment_id": "custom_rack",
                "display_name": "Rack Android",
                "label_name": "Rack",
                "equipment_type": "custom_machine",
                "load_semantics": "external",
            }],
        }), encoding="utf-8")
        assert "definitions_imported=1" in invoke_definitions(definitions, defined_db)
        defined_payload = copy.deepcopy(payload())
        defined_payload["sessions"][0]["exercises"][1]["equipment_id"] = "custom_rack"
        defined_artifact = root / "defined-mobile-v2.json"
        defined_artifact.write_text(json.dumps(defined_payload), encoding="utf-8")
        assert "sessions_imported=1" in invoke(defined_artifact, defined_db)
        con = sqlite3.connect(defined_db)
        assert con.execute(
            "SELECT display_name,label_name,equipment_type,load_semantics "
            "FROM custom_equipment WHERE equipment_id='custom_rack'"
        ).fetchone() == ("Rack Android", "Rack", "custom_machine", "external")
        assert con.execute(
            "SELECT equipment_id FROM session_exercises WHERE entry_id='sxe_push'"
        ).fetchone() == ("custom_rack",)
        con.close()
        assert "definitions_skipped=1" in invoke_definitions(definitions, defined_db)
        assert "sessions_skipped=1" in invoke(defined_artifact, defined_db)

        artifact.write_text(json.dumps(payload()), encoding="utf-8")
        assert "sessions_imported=1" in invoke(artifact, db)
        con = sqlite3.connect(db)
        rows = con.execute("SELECT e.exercise_id,se.entry_id,se.position,ca.duration_seconds FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN continuous_activity ca ON ca.session_exercise_row_id=se.id ORDER BY se.position").fetchall()
        assert rows == [("ex_walk","sxe_walk_10",0,600),("ex_push","sxe_push",1,None),("ex_walk","sxe_walk_15",2,900)], rows
        assert con.execute("SELECT weight_kg FROM performed_sets").fetchone()[0] == 20.0
        assert "sessions_skipped=1" in invoke(artifact, db)
        con.close()

        divergent = copy.deepcopy(payload())
        divergent["sessions"][0]["exercises"][1]["sets"][0]["reps"] = 13
        artifact.write_text(json.dumps(divergent), encoding="utf-8")
        rejected = subprocess.run([sys.executable, str(IMPORTER), str(artifact), "--database", str(db)], text=True, capture_output=True)
        assert rejected.returncode != 0 and "conflit de contenu" in rejected.stderr
        assert sqlite3.connect(db).execute("SELECT reps FROM performed_sets").fetchone()[0] == 12
        divergent = copy.deepcopy(payload())
        divergent["body_observations"][0]["body_weight_kg"] = 73.0
        artifact.write_text(json.dumps(divergent), encoding="utf-8")
        rejected = subprocess.run([sys.executable, str(IMPORTER), str(artifact), "--database", str(db)], text=True, capture_output=True)
        assert rejected.returncode != 0 and "conflit observation corporelle" in rejected.stderr
        artifact.write_text(json.dumps(payload()), encoding="utf-8")

        equipment.write_text(json.dumps({
            "format": "trainlog-equipment-associations",
            "version": 2,
            "generated_at": "2026-09-07T10:00:00+02:00",
            "associations": [
                {"session_id": "se_multi", "entry_id": "sxe_walk_10", "exercise_id": "ex_walk", "state": "cleared"},
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
            ("sxe_walk_10", None),
            ("sxe_push", "leg_press"),
            ("sxe_walk_15", None),
        ], rows
        con.close()

        assert "EQUIPMENT_ASSOCIATIONS_IMPORT=PASS" in invoke_equipment(equipment, db)
        con = sqlite3.connect(db)
        assert con.execute("SELECT count(*) FROM session_exercises").fetchone()[0] == 3
        assert con.execute(
            "SELECT entry_id,equipment_id FROM session_exercises ORDER BY position"
        ).fetchall() == rows
        con.close()

        mobile_output = root / "pc-mobile-v2.json"
        equipment_output = root / "pc-equipment-v2.json"
        for tool, output, marker in (
            (MOBILE_EXPORTER, mobile_output, "PC_MOBILE_EXPORT=PASS"),
            (EQUIPMENT_EXPORTER, equipment_output, "EQUIPMENT_ASSOCIATIONS_EXPORT=PASS"),
        ):
            result = subprocess.run(
                [sys.executable, str(tool), str(output), "--database", str(db)] +
                (["--version", "2"] if tool == MOBILE_EXPORTER else []),
                text=True,
                capture_output=True,
            )
            assert result.returncode == 0, result.stdout + result.stderr
            assert marker in result.stdout
        assert json.loads(mobile_output.read_text())["body_observations"][0]["body_weight_kg"] == 72.5
        # The desktop serializer is also accepted as an idempotent V2 replay,
        # which is the PC endpoint of the Android importer/exporter round trip.
        assert "body_skipped=1" in invoke(mobile_output, db)

        # Definitions travel in their companion; unchanged v2 references may
        # therefore contain a locally-known custom stable ID.
        con = sqlite3.connect(db)
        con.execute("INSERT INTO custom_equipment VALUES('custom_rack','Rack','Rack','rack','none')")
        con.execute("UPDATE session_exercises SET equipment_id='custom_rack' WHERE entry_id='sxe_push'")
        con.commit()
        con.close()
        custom_output = root / "custom-mobile-v2.json"
        exported = subprocess.run(
            [sys.executable, str(MOBILE_EXPORTER), str(custom_output), "--database", str(db), "--version", "2"],
            text=True,
            capture_output=True,
        )
        assert exported.returncode == 0, exported.stdout + exported.stderr
        assert json.loads(custom_output.read_text())["sessions"][0]["exercises"][1]["equipment_id"] == "custom_rack"
    print("PASS mobile_import_multi_occurrence")

if __name__ == "__main__": main()
