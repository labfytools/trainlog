#!/usr/bin/env python3
"""Regression tests for the read-only TRAINLOG_AI_EXPORT_V1 artifact."""

import hashlib
import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "export_ai_history.py"
SPECIAL_TEXT = 'Très dur "aujourd\'hui".\nDos OK.\n\\ test UTF-8 : éàùç'


SCHEMA = """
PRAGMA user_version=17;
CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT,name TEXT,
 normalized_name TEXT,tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER);
CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT,started_at TEXT,
 ended_at TEXT,session_type TEXT,notes TEXT);
CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT,
 session_row_id INTEGER,exercise_row_id INTEGER,recording_mode TEXT,
 data_fields INTEGER,position INTEGER,load_mode TEXT,rest_seconds INTEGER,
 target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,
 target_weight_kg REAL,equipment_id TEXT,notes TEXT,tracking_mode TEXT);
CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER,
 position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL);
CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER,
 duration_seconds INTEGER,speed_kmh REAL,distance_km REAL);
CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY,max_weight_kg REAL);
CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT,
 observed_at TEXT,session_row_id INTEGER,body_weight_kg REAL,neck_cm REAL,
 shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,
 right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,
 right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL,notes TEXT);
CREATE TABLE exercise_feedback(id INTEGER PRIMARY KEY,feedback_id TEXT,
 session_exercise_row_id INTEGER,observed_at TEXT,raw_text TEXT);
CREATE TABLE session_followups(id INTEGER PRIMARY KEY,followup_id TEXT,
 session_row_id INTEGER,observed_at TEXT,raw_text TEXT);
CREATE TABLE exercise_feedback_revisions(revision_id TEXT PRIMARY KEY,
 feedback_id TEXT,created_at TEXT,raw_text TEXT);
CREATE TABLE session_followup_revisions(revision_id TEXT PRIMARY KEY,
 followup_id TEXT,created_at TEXT,raw_text TEXT);
"""


class AiHistoryExportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        self.database = self.directory / "trainlog.db"
        with sqlite3.connect(self.database) as connection:
            connection.executescript(SCHEMA)

    def tearDown(self):
        self.temporary.cleanup()

    def export(self, name="export.json"):
        output = self.directory / name
        subprocess.run(
            [sys.executable, str(TOOL), "--database", str(self.database),
             str(output)], check=True, capture_output=True, text=True,
        )
        return output, json.loads(output.read_text(encoding="utf-8"))

    def populate(self):
        with sqlite3.connect(self.database) as connection:
            connection.execute(
                "INSERT INTO exercises VALUES(1,'ex_same','Tirage dos','tirage dos',"
                "'reps','sets',0)"
            )
            connection.execute(
                "INSERT INTO sessions VALUES(1,'se_one','2026-09-12T10:00:00Z',"
                "'2026-09-12T11:00:00Z','training','Séance dos')"
            )
            for row_id, entry_id, position in ((1, "sxe_a", 0), (2, "sxe_b", 1)):
                connection.execute(
                    "INSERT INTO session_exercises VALUES(?,?,1,1,'sets',0,?,"
                    "'none',0,NULL,NULL,NULL,NULL,NULL,NULL,'reps')",
                    (row_id, entry_id, position),
                )
            connection.executemany(
                "INSERT INTO performed_sets VALUES(?,?,?,?,NULL,?)",
                ((1, 1, 0, 8, 48.0), (2, 1, 1, 7, 48.0),
                 (3, 2, 0, 10, 42.5)),
            )
            connection.execute(
                "INSERT INTO exercise_feedback VALUES(1,'fb_one',2,"
                "'2026-09-12T10:45:00Z',?)", (SPECIAL_TEXT,)
            )
            connection.execute(
                "INSERT INTO exercise_feedback_revisions VALUES("
                "'fr_one','fb_one','2026-09-12T10:45:00Z',?)", (SPECIAL_TEXT,)
            )
            connection.execute(
                "INSERT INTO session_followups VALUES(1,'fu_one',1,"
                "'2026-09-13T09:00:00Z','Courbatures dos')"
            )
            connection.execute(
                "INSERT INTO session_followup_revisions VALUES("
                "'fur_one','fu_one','2026-09-13T09:00:00Z','Courbatures dos')"
            )

    def test_empty_export_is_valid_and_has_required_root(self):
        _, payload = self.export()
        self.assertEqual("TRAINLOG_AI_EXPORT", payload["format"])
        self.assertEqual(1, payload["version"])
        self.assertRegex(payload["generated_at"], r"Z$")
        self.assertEqual([], payload["sessions"])
        self.assertEqual([], payload["max_history"])
        self.assertEqual([], payload["measurements"])

    def test_session_multi_occurrence_feedback_text_and_followup_ownership(self):
        self.populate()
        output, payload = self.export()
        # Parsing the actual file proves quotes, backslashes, newlines and UTF-8
        # are encoded as JSON rather than interpolated into it.
        json.loads(output.read_text(encoding="utf-8"))
        session = payload["sessions"][0]
        self.assertEqual(["sxe_a", "sxe_b"],
                         [entry["entry_id"] for entry in session["entries"]])
        self.assertEqual([8, 7],
                         [item["reps"] for item in session["entries"][0]["sets"]])
        self.assertEqual([], session["entries"][0]["immediate_feedback"])
        feedback = session["entries"][1]["immediate_feedback"][0]
        self.assertEqual(SPECIAL_TEXT, feedback["revisions"][0]["raw_text"])
        self.assertNotIn("session_followups", session["entries"][1])
        self.assertEqual("fu_one", session["session_followups"][0]["followup_id"])

    def test_max_measurements_determinism_and_read_only_database(self):
        self.populate()
        with sqlite3.connect(self.database) as connection:
            connection.execute("INSERT INTO max_results VALUES(1,62.5)")
            connection.execute(
                "INSERT INTO body_observations(id,observation_id,observed_at,"
                "body_weight_kg,waist_cm,notes) VALUES("
                "1,'bo_one','2026-09-13T08:00:00Z',75.2,82.0,'Matin')"
            )
        before = hashlib.sha256(self.database.read_bytes()).digest()
        _, first = self.export("first.json")
        after = hashlib.sha256(self.database.read_bytes()).digest()
        _, second = self.export("second.json")
        first.pop("generated_at")
        second.pop("generated_at")
        self.assertEqual(before, after)
        self.assertEqual(first, second)
        self.assertEqual("sxe_a", first["max_history"][0]["entry_id"])
        self.assertEqual(62.5, first["max_history"][0]["max_weight_kg"])
        self.assertEqual(75.2, first["measurements"][0]["body_weight_kg"])
        self.assertEqual(82.0, first["measurements"][0]["waist_cm"])


if __name__ == "__main__":
    unittest.main()
