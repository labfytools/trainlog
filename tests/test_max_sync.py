#!/usr/bin/env python3
"""Explicit max V2 import/export and idempotence regression."""

import copy
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPORTER = ROOT / "tools/import_mobile_export.py"
EXPORTER = ROOT / "tools/export_pc_mobile.py"

SCHEMA = """
CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,normalized_name TEXT UNIQUE,tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER);
CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,started_at TEXT,ended_at TEXT,session_type TEXT,notes TEXT);
CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT NOT NULL UNIQUE,session_row_id INTEGER,exercise_row_id INTEGER,recording_mode TEXT,data_fields INTEGER,position INTEGER,load_mode TEXT,rest_seconds INTEGER,target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,equipment_id TEXT,notes TEXT,UNIQUE(session_row_id,position));
CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER,position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL);
CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER UNIQUE,duration_seconds INTEGER,speed_kmh REAL,distance_km REAL);
CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY,max_weight_kg REAL NOT NULL CHECK(max_weight_kg>0));
CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT UNIQUE,observed_at TEXT,session_row_id INTEGER,body_weight_kg REAL,neck_cm REAL,shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL,notes TEXT);
CREATE TABLE custom_equipment(equipment_id TEXT PRIMARY KEY,display_name TEXT NOT NULL,label_name TEXT NOT NULL,equipment_type TEXT NOT NULL,load_semantics TEXT NOT NULL);
PRAGMA user_version=9;
"""


def payload():
    catalog = [
        {"exercise_id": "ex_pec", "name": "Pec Fly", "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0},
        {"exercise_id": "ex_rear", "name": "Rear Delt Fly", "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0},
    ]
    entries = [
        {"entry_id": "sxe_pec", "position": 0, "exercise_id": "ex_pec", "name": "Pec Fly", "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0, "load_mode": "none", "rest_seconds": 0, "equipment_id": "rear_delt_pec_fly", "max_weight_kg": 100.0},
        {"entry_id": "sxe_rear", "position": 1, "exercise_id": "ex_rear", "name": "Rear Delt Fly", "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0, "load_mode": "none", "rest_seconds": 0, "equipment_id": "rear_delt_pec_fly", "max_weight_kg": 86.0},
    ]
    return {
        "format": "trainlog-mobile-export",
        "version": 2,
        "generated_at": "2031-02-03T09:00:00+01:00",
        "exercises": catalog,
        "sessions": [{"session_id": "se_max", "started_at": "2031-02-03T08:15:00+01:00", "session_type": "max_test", "exercises": entries}],
        "body_observations": [],
    }


def run(*arguments):
    return subprocess.run(
        [sys.executable, *map(str, arguments)],
        text=True,
        capture_output=True,
    )


def main():
    with tempfile.TemporaryDirectory(prefix="trainlog-max-sync-") as temp:
        root = Path(temp)
        database = root / "trainlog.db"
        artifact = root / "mobile-v2.json"
        exported = root / "pc-v2.json"
        connection = sqlite3.connect(database)
        connection.executescript(SCHEMA)
        connection.close()
        artifact.write_text(json.dumps(payload()), encoding="utf-8")

        first = run(IMPORTER, artifact, "--database", database)
        assert first.returncode == 0, first.stdout + first.stderr
        assert "sessions_imported=1" in first.stdout
        connection = sqlite3.connect(database)
        rows = connection.execute(
            "SELECT e.exercise_id,se.entry_id,se.equipment_id,mr.max_weight_kg "
            "FROM max_results mr JOIN session_exercises se ON se.id=mr.session_exercise_row_id "
            "JOIN exercises e ON e.id=se.exercise_row_id ORDER BY se.position"
        ).fetchall()
        assert rows == [
            ("ex_pec", "sxe_pec", "rear_delt_pec_fly", 100.0),
            ("ex_rear", "sxe_rear", "rear_delt_pec_fly", 86.0),
        ]
        assert connection.execute("SELECT count(*) FROM performed_sets").fetchone()[0] == 0
        connection.close()

        second = run(IMPORTER, artifact, "--database", database)
        assert second.returncode == 0, second.stdout + second.stderr
        assert "sessions_skipped=1" in second.stdout

        resumed = payload()
        resumed["sessions"][0]["exercises"][0]["max_weight_kg"] = 101.5
        artifact.write_text(json.dumps(resumed), encoding="utf-8")
        update = run(IMPORTER, artifact, "--database", database)
        assert update.returncode == 0, update.stdout + update.stderr
        assert "sessions_reconciled=1" in update.stdout
        assert sqlite3.connect(database).execute(
            "SELECT max_weight_kg FROM max_results mr JOIN session_exercises se "
            "ON se.id=mr.session_exercise_row_id WHERE se.entry_id='sxe_pec'"
        ).fetchone() == (101.5,)

        result = run(EXPORTER, exported, "--database", database)
        assert result.returncode == 0, result.stdout + result.stderr
        round_trip = json.loads(exported.read_text(encoding="utf-8"))
        values = round_trip["sessions"][0]["exercises"]
        assert [entry["max_weight_kg"] for entry in values] == [101.5, 86.0]
        assert all("sets" not in entry and "continuous" not in entry for entry in values)

        invalid = copy.deepcopy(payload())
        invalid["sessions"][0]["session_type"] = "training"
        rejected_path = root / "invalid.json"
        rejected_path.write_text(json.dumps(invalid), encoding="utf-8")
        rejected = run(IMPORTER, rejected_path, "--database", database)
        assert rejected.returncode != 0
        assert "max_weight_kg exige" in rejected.stderr
        assert sqlite3.connect(database).execute("SELECT count(*) FROM sessions").fetchone()[0] == 1

    print("max sync: PASS")


if __name__ == "__main__":
    main()
