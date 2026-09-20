#!/usr/bin/env python3
"""Production-service regressions for staged causal deletion."""

import importlib.util
import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
SPEC = importlib.util.spec_from_file_location("causal", ROOT / "tools/causal_delete_exchange.py")
causal = importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(causal)

SCHEMA = """
PRAGMA foreign_keys=ON;
CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER);
CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,started_at TEXT,ended_at TEXT,session_type TEXT,notes TEXT);
CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,session_row_id INTEGER REFERENCES sessions(id) ON DELETE CASCADE,exercise_row_id INTEGER REFERENCES exercises(id),entry_id TEXT UNIQUE,position INTEGER,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER,equipment_id TEXT,load_mode TEXT,rest_seconds INTEGER,target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,notes TEXT);
CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER REFERENCES session_exercises(id) ON DELETE CASCADE,position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL);
CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER REFERENCES session_exercises(id) ON DELETE CASCADE,duration_seconds INTEGER,speed_kmh REAL,distance_km REAL);
CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY REFERENCES session_exercises(id) ON DELETE CASCADE,max_weight_kg REAL);
CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT UNIQUE,observed_at TEXT,body_weight_kg REAL,neck_cm REAL,shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL,notes TEXT,session_row_id INTEGER REFERENCES sessions(id) ON DELETE SET NULL);
CREATE TABLE custom_equipment(equipment_id TEXT PRIMARY KEY,display_name TEXT,label_name TEXT,equipment_type TEXT,load_semantics TEXT);
CREATE TABLE equipment(equipment_id TEXT PRIMARY KEY,equipment_type TEXT);
CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY,canonical_exercise_id TEXT);
CREATE TABLE exercise_feedback(id INTEGER PRIMARY KEY,feedback_id TEXT UNIQUE,session_exercise_row_id INTEGER REFERENCES session_exercises(id) ON DELETE CASCADE,observed_at TEXT,raw_text TEXT);
CREATE TABLE exercise_feedback_revisions(revision_id TEXT PRIMARY KEY,feedback_id TEXT REFERENCES exercise_feedback(feedback_id) ON DELETE CASCADE,created_at TEXT,raw_text TEXT);
CREATE TABLE exercise_body_zones(exercise_row_id INTEGER REFERENCES exercises(id),zone_id TEXT,role TEXT,PRIMARY KEY(exercise_row_id,zone_id));
CREATE TABLE execution_drafts(session_id TEXT PRIMARY KEY,revision_id TEXT,parent_revision_id TEXT,payload_json TEXT);
CREATE TABLE execution_draft_finalizations(session_id TEXT PRIMARY KEY,final_revision_id TEXT,finalized_at TEXT);
CREATE TABLE sync_note_revisions(owner_kind TEXT,owner_id TEXT,revision_id TEXT,parent_revision_id TEXT,note TEXT,PRIMARY KEY(owner_kind,owner_id,revision_id));
CREATE TABLE sync_note_state(owner_kind TEXT,owner_id TEXT,revision_id TEXT,PRIMARY KEY(owner_kind,owner_id));
CREATE TABLE sync_causal_operations(operation_id TEXT PRIMARY KEY,target_kind TEXT,target_id TEXT,creator_id TEXT,predecessor_revision_id TEXT,created_at TEXT,payload_sha256 TEXT,publication_context TEXT);
CREATE TABLE sync_causal_state(target_kind TEXT,target_id TEXT,current_revision_id TEXT,deleted INTEGER,operation_id TEXT,PRIMARY KEY(target_kind,target_id));
PRAGMA user_version=20;
"""


class CausalDeleteTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.path = Path(self.tmp.name) / "db.sqlite"
        self.db = sqlite3.connect(self.path); self.db.executescript(SCHEMA)
        self.db.execute("INSERT INTO exercises VALUES(1,'ex_user','User','sets','reps',0)")
        self.db.execute("INSERT INTO sessions VALUES(1,'se_one','2030-01-01T10:00:00Z',NULL,'training','note')")
        self.db.execute("INSERT INTO session_exercises VALUES(1,1,1,'sxe_one',0,'sets','reps',0,NULL,'external',60,1,8,NULL,32.5,'occurrence')")
        self.db.execute("INSERT INTO performed_sets VALUES(1,1,0,8,NULL,32.5)")
        self.db.execute("INSERT INTO body_observations VALUES(1,'bo_one','2030-01-01T11:00:00Z',80.0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'body',1)")
        self.db.execute("INSERT INTO custom_equipment VALUES('eq_one','Custom','','custom','external')")
        self.db.execute("INSERT INTO exercise_feedback VALUES(1,'fb_one',1,'2030-01-01T10:30:00Z','root')")
        self.db.execute("INSERT INTO exercise_feedback_revisions VALUES('fr_one','fb_one','2030-01-01T10:30:00Z','root')")
        self.db.execute("INSERT INTO exercise_body_zones VALUES(1,'arms','primary')")
        self.db.execute("INSERT INTO execution_drafts VALUES('se_draft','dr_one',NULL,'{}')")
        self.db.commit()

    def tearDown(self): self.db.close(); self.tmp.cleanup()

    def delete(self, kind, target):
        self.db.execute("BEGIN IMMEDIATE")
        operation, outcome = causal.local_delete(self.db, kind, target, "peer_desktop_test")
        self.db.commit(); self.assertEqual("applied", outcome); return operation

    def test_domain_effects_replay_conflict_and_parent_safety(self):
        feedback = self.delete("feedback", "fb_one")
        self.assertEqual("root", self.db.execute("SELECT raw_text FROM exercise_feedback").fetchone()[0])
        self.assertEqual(1, self.db.execute("SELECT count(*) FROM exercise_feedback_revisions").fetchone()[0])
        self.delete("body_zone_relation", "ex_user|arms|primary")
        self.assertEqual(0, self.db.execute("SELECT count(*) FROM exercise_body_zones").fetchone()[0])
        self.delete("exercise", "ex_user"); self.delete("custom_equipment", "eq_one")
        self.assertEqual(1, self.db.execute("SELECT count(*) FROM exercises").fetchone()[0])
        self.assertEqual(1, self.db.execute("SELECT count(*) FROM custom_equipment").fetchone()[0])
        self.delete("execution_draft", "se_draft")
        self.assertEqual(0, self.db.execute("SELECT count(*) FROM execution_drafts").fetchone()[0])
        session = self.delete("session", "se_one")
        self.assertEqual(0, self.db.execute("SELECT count(*) FROM sessions").fetchone()[0])
        self.assertEqual((1, None), self.db.execute("SELECT count(*),session_row_id FROM body_observations").fetchone())
        self.assertIsNotNone(self.db.execute("SELECT 1 FROM execution_draft_finalizations WHERE session_id='se_one'").fetchone())
        self.assertEqual("unchanged", causal.apply_operation(self.db, session))
        conflicting = dict(session); conflicting["operation_id"] = "del_" + "1" * 36
        conflicting["payload_sha256"] = causal.digest({k:v for k,v in conflicting.items() if k != "payload_sha256"})
        with self.assertRaisesRegex(causal.CausalError, "concurrent deletion"):
            causal.apply_operation(self.db, conflicting)
        divergent = dict(feedback); divergent["creator_id"] = "other"
        divergent["payload_sha256"] = causal.digest({k:v for k,v in divergent.items() if k != "payload_sha256"})
        with self.assertRaisesRegex(causal.CausalError, "reused"):
            causal.apply_operation(self.db, divergent)

    def test_observation_delete_and_finalized_draft_conflict_roll_back(self):
        self.delete("body_observation", "bo_one")
        self.assertEqual(0, self.db.execute("SELECT count(*) FROM body_observations").fetchone()[0])
        self.db.execute("INSERT OR REPLACE INTO execution_draft_finalizations VALUES('se_draft','dr_final','2030-01-01T12:00:00Z')")
        self.db.commit(); before = self.db.execute("SELECT count(*) FROM sync_causal_operations").fetchone()[0]
        self.db.execute("BEGIN IMMEDIATE")
        with self.assertRaisesRegex(causal.CausalError, "finalization"):
            causal.local_delete(self.db, "execution_draft", "se_draft", "peer")
        self.db.rollback()
        self.assertEqual(before, self.db.execute("SELECT count(*) FROM sync_causal_operations").fetchone()[0])
        self.assertIsNotNone(self.db.execute("SELECT 1 FROM execution_drafts WHERE session_id='se_draft'").fetchone())

    def test_builtin_exercise_cannot_be_retired(self):
        built_in = next(iter(causal.load_exercise_names()))
        self.db.execute("INSERT INTO exercises VALUES(2,?,'Built in','sets','reps',0)", (built_in,))
        self.db.commit()
        with self.assertRaisesRegex(causal.CausalError, "built-in"):
            causal.local_delete(self.db, "exercise", built_in, "peer")

    def test_retired_exercise_is_omitted_from_next_pc_catalog(self):
        self.delete("exercise", "ex_user")
        output = Path(self.tmp.name) / "pc-catalog.json"
        result = subprocess.run(
            [sys.executable, str(ROOT / "tools/export_pc_catalog.py"),
             "--database", str(self.path), str(output)],
            check=False, capture_output=True, text=True,
        )
        self.assertEqual(0, result.returncode, result.stderr)
        catalog = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual([], catalog["exercises"])
        self.assertEqual(1, self.db.execute(
            "SELECT count(*) FROM exercises WHERE exercise_id='ex_user'").fetchone()[0])
        self.assertEqual(1, self.db.execute(
            "SELECT count(*) FROM session_exercises WHERE exercise_row_id=1").fetchone()[0])

    def hostile_operation(self, kind, target):
        operation = {"operation_id": "del_" + "a" * 36, "target_kind": kind,
                     "target_id": target, "creator_id": "hostile_fixture",
                     "predecessor_revision_id": causal.live_revision(self.db, kind, target) or "lv_unknown",
                     "created_at": "2030-01-01T12:00:00Z", "publication_context": None}
        operation["payload_sha256"] = causal.digest(operation)
        return operation

    def test_imported_builtin_and_alias_retirement_are_refused_without_writes(self):
        built_in = next(iter(causal.load_exercise_names()))
        self.db.execute("INSERT INTO exercises VALUES(2,?,'Built in','sets','reps',0)", (built_in,))
        self.db.execute("INSERT INTO equipment VALUES('barbell','barbell')")
        alias = "ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
        self.db.execute("INSERT INTO exercises VALUES(3,?,'Alias row','sets','reps',0)", (alias,))
        self.db.execute("INSERT INTO exercise_aliases VALUES(?,?)", (alias, built_in))
        self.db.commit()
        for operation in (self.hostile_operation("exercise", built_in),
                          self.hostile_operation("exercise", alias),
                          self.hostile_operation("custom_equipment", "barbell")):
            with self.assertRaisesRegex(causal.CausalError, "built-in"):
                causal.apply_operation(self.db, operation)
        self.assertEqual(0, self.db.execute("SELECT COUNT(*) FROM sync_causal_operations").fetchone()[0])
        self.assertEqual(0, self.db.execute("SELECT COUNT(*) FROM sync_causal_state").fetchone()[0])

    def test_child_mutations_invalidate_session_observation_and_feedback_deletes(self):
        session = self.hostile_operation("session", "se_one")
        self.db.execute("UPDATE performed_sets SET weight_kg=35.0 WHERE id=1")
        with self.assertRaisesRegex(causal.CausalError, "predecessor"):
            causal.apply_operation(self.db, session)
        self.assertEqual(35.0, self.db.execute("SELECT weight_kg FROM performed_sets WHERE id=1").fetchone()[0])
        observation = self.hostile_operation("body_observation", "bo_one")
        self.db.execute("UPDATE body_observations SET body_weight_kg=79.5,session_row_id=NULL WHERE observation_id='bo_one'")
        with self.assertRaisesRegex(causal.CausalError, "predecessor"):
            causal.apply_operation(self.db, observation)
        feedback = self.hostile_operation("feedback", "fb_one")
        self.db.execute("INSERT INTO exercise_feedback_revisions VALUES('fr_two','fb_one','2030-01-02T10:30:00Z','revised')")
        with self.assertRaisesRegex(causal.CausalError, "predecessor"):
            causal.apply_operation(self.db, feedback)
        self.assertEqual(0, self.db.execute("SELECT COUNT(*) FROM sync_causal_operations").fetchone()[0])

    def test_draft_identity_is_semantic_revision_not_json_serialization(self):
        first = causal.live_revision(self.db, "execution_draft", "se_draft")
        self.db.execute("UPDATE execution_drafts SET payload_json=? WHERE session_id='se_draft'", ('{ "b": 2, "a": 1 }',))
        self.assertEqual(first, causal.live_revision(self.db, "execution_draft", "se_draft"))
        self.db.execute("UPDATE execution_drafts SET revision_id='dr_two' WHERE session_id='se_draft'")
        self.assertNotEqual(first, causal.live_revision(self.db, "execution_draft", "se_draft"))

    def test_operation_count_and_input_byte_bounds_precede_effects(self):
        row = ("exercise", "x", "peer", "lv_x", "2030-01-01T12:00:00Z", "0" * 64, None)
        self.db.executemany("INSERT INTO sync_causal_operations VALUES(?,?,?,?,?,?,?,?)",
            [(f"op_{index:04d}", *row) for index in range(causal.MAX_OPERATIONS)])
        self.db.commit()
        document = causal.export_document(self.db)
        self.assertEqual(causal.MAX_OPERATIONS, len(document["operations"]))
        self.db.execute("INSERT INTO sync_causal_operations VALUES(?,?,?,?,?,?,?,?)", ("op_over", *row)); self.db.commit()
        with self.assertRaisesRegex(causal.CausalError, "artifact bound"):
            causal.export_document(self.db)
        oversized = Path(self.tmp.name) / "oversized.json"
        oversized.write_bytes(b" " * (causal.MAX_BYTES + 1))
        with self.assertRaisesRegex(causal.CausalError, "4 MiB"):
            causal.load(oversized)


if __name__ == "__main__": unittest.main()
