#!/usr/bin/env python3
"""Strict JSON validation regressions for exercise-alias imports."""

import importlib.util
import json
import sqlite3
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "import_exercise_aliases", ROOT / "tools/import_exercise_aliases.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_boolean_version_rejected():
    with tempfile.TemporaryDirectory() as directory:
        artifact = Path(directory) / "aliases.json"
        artifact.write_text(json.dumps({
            "format": "trainlog-exercise-aliases",
            "version": True,
            "aliases": [],
        }), encoding="utf-8")
        try:
            MODULE.load(artifact)
        except ValueError as error:
            assert str(error) == "artifact alias v1 invalide"
            return
    raise AssertionError("boolean alias artifact version was accepted")


def test_merge_preserves_same_session_child_graph():
    source = "ex_11111111-1111-4111-8111-111111111111"
    target = "ex_22222222-2222-4222-8222-222222222222"
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        database = root / "trainlog.db"
        artifact = root / "aliases.json"
        artifact.write_text(json.dumps({
            "format": "trainlog-exercise-aliases", "version": 1,
            "aliases": [{"source_exercise_id": source,
                         "canonical_exercise_id": target}],
        }), encoding="utf-8")
        connection = sqlite3.connect(database)
        connection.executescript(f"""
            PRAGMA foreign_keys=ON;
            CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,
              tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER);
            CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE);
            CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,
              session_row_id INTEGER REFERENCES sessions(id) ON DELETE CASCADE,
              exercise_row_id INTEGER REFERENCES exercises(id),entry_id TEXT UNIQUE,
              equipment_id TEXT);
            CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,
              session_exercise_row_id INTEGER REFERENCES session_exercises(id) ON DELETE CASCADE,
              duration_seconds INTEGER,weight_kg REAL);
            CREATE TABLE continuous_activity(session_exercise_row_id INTEGER PRIMARY KEY
              REFERENCES session_exercises(id) ON DELETE CASCADE,duration_seconds INTEGER);
            CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY
              REFERENCES session_exercises(id) ON DELETE CASCADE,max_weight_kg REAL);
            CREATE TABLE exercise_body_zones(exercise_row_id INTEGER REFERENCES exercises(id)
              ON DELETE CASCADE,zone_id TEXT,role TEXT,PRIMARY KEY(exercise_row_id,zone_id));
            CREATE TABLE exercise_body_zone_sync(exercise_row_id INTEGER PRIMARY KEY
              REFERENCES exercises(id) ON DELETE CASCADE,synced_state TEXT);
            CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY,
              canonical_exercise_id TEXT REFERENCES exercises(exercise_id) ON DELETE RESTRICT);
            INSERT INTO exercises VALUES(1,'{source}','duration','sets',0);
            INSERT INTO exercises VALUES(2,'{target}','duration','sets',0);
            INSERT INTO sessions VALUES(1,'se_same_session');
            INSERT INTO session_exercises VALUES(10,1,1,'sxe_source_set','eq_cable');
            INSERT INTO session_exercises VALUES(11,1,1,'sxe_source_continuous','eq_treadmill');
            INSERT INTO session_exercises VALUES(12,1,1,'sxe_source_max','eq_cable');
            INSERT INTO session_exercises VALUES(13,1,2,'sxe_target','eq_target');
            INSERT INTO performed_sets VALUES(20,10,45,37.5);
            INSERT INTO continuous_activity VALUES(11,900);
            INSERT INTO max_results VALUES(12,82.5);
            INSERT INTO performed_sets VALUES(21,13,30,20.0);
            INSERT INTO exercise_body_zones VALUES(1,'arms','primary');
            INSERT INTO exercise_body_zones VALUES(2,'arms','primary');
            INSERT INTO exercise_body_zone_sync VALUES(1,'arms|');
            INSERT INTO exercise_body_zone_sync VALUES(2,'arms|');
            PRAGMA user_version=12;
        """)
        connection.commit()
        connection.close()

        result = subprocess.run(
            ["python3", str(ROOT / "tools/import_exercise_aliases.py"),
             str(artifact), "--database", str(database)],
            cwd=ROOT, text=True, capture_output=True,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        connection = sqlite3.connect(database)
        try:
            assert connection.execute(
                "SELECT source_exercise_id,canonical_exercise_id FROM exercise_aliases"
            ).fetchall() == [(source, target)]
            assert connection.execute(
                "SELECT id,exercise_row_id,entry_id,equipment_id FROM session_exercises ORDER BY id"
            ).fetchall() == [
                (10, 2, "sxe_source_set", "eq_cable"),
                (11, 2, "sxe_source_continuous", "eq_treadmill"),
                (12, 2, "sxe_source_max", "eq_cable"),
                (13, 2, "sxe_target", "eq_target"),
            ]
            assert connection.execute("SELECT * FROM performed_sets ORDER BY id").fetchall() == [
                (20, 10, 45, 37.5), (21, 13, 30, 20.0),
            ]
            assert connection.execute("SELECT * FROM continuous_activity").fetchall() == [(11, 900)]
            assert connection.execute("SELECT * FROM max_results").fetchall() == [(12, 82.5)]
            assert connection.execute("PRAGMA foreign_key_check").fetchall() == []
        finally:
            connection.close()


if __name__ == "__main__":
    test_boolean_version_rejected()
    test_merge_preserves_same_session_child_graph()
