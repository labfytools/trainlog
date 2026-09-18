#!/usr/bin/env python3
"""Programs V1 desktop projection export regressions."""

import sqlite3
import sys
import tempfile
import unittest
from contextlib import closing
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import export_programs


SCHEMA = """
CREATE TABLE programs(program_id TEXT PRIMARY KEY,title TEXT,note TEXT,state TEXT,
 start_date TEXT,end_date TEXT,created_at TEXT,updated_at TEXT,revision_id TEXT,
 source_format TEXT,source_version INTEGER,source_payload_sha256 TEXT,deleted_at TEXT);
CREATE TABLE program_sessions(program_session_id TEXT PRIMARY KEY,program_id TEXT,
 position INTEGER,title TEXT,session_type TEXT,planned_for TEXT,note TEXT);
CREATE TABLE program_session_entries(program_session_id TEXT,entry_id TEXT,position INTEGER,
 exercise_id TEXT,equipment_id TEXT,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER,
 load_mode TEXT,rest_seconds INTEGER,target_sets INTEGER,target_reps INTEGER,
 target_duration_seconds INTEGER,target_weight_kg REAL,notes TEXT);
CREATE TABLE program_deletions(program_id TEXT PRIMARY KEY,request_id TEXT,
 expected_revision TEXT,deleted_revision TEXT,deleted_at TEXT,response_json TEXT,
 generation_id TEXT,acknowledged_at TEXT);
PRAGMA user_version=26;
"""


class ProgramsExchangeTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.database = Path(self.temporary.name) / "trainlog.sqlite"
        with closing(sqlite3.connect(self.database)) as db:
            db.executescript(SCHEMA)
            db.execute("INSERT INTO programs VALUES(?,?,?,?,?,?,?,?,?,?,?,?,NULL)", (
                "pg_live", "Cycle", None, "active", "2026-09-21", None,
                "2026-09-18T10:00:00Z", "2026-09-18T11:00:00Z", "pgr_live",
                "trainlog-program", 1, "a" * 64,
            ))
            db.execute("INSERT INTO program_sessions VALUES(?,?,?,?,?,?,?)",
                       ("pgs_live", "pg_live", 0, "A", "training", None, None))
            db.execute("INSERT INTO program_session_entries VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                       ("pgs_live", "pge_live", 0, "ex_unknown", None, "sets", "reps", 0,
                        "none", 60, 3, 8, None, None, None))
            db.execute("INSERT INTO programs VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)", (
                "pg_deleted", "Deleted", None, "active", None, None,
                "2026-09-18T10:00:00Z", "2026-09-18T11:00:00Z", "pgr_old",
                "trainlog-program", 1, "b" * 64, "2026-09-18T12:00:00Z",
            ))
            db.execute("INSERT INTO program_deletions VALUES(?,?,?,?,?,?,NULL,NULL)",
                       ("pg_deleted", "pgd_delete", "pgr_old", "pgr_deleted",
                        "2026-09-18T12:00:00Z", "{}"))
            db.execute("INSERT INTO program_deletions VALUES(?,?,?,?,?,?,?,?)",
                       ("pg_ack", "pgd_ack", "pgr_ack_old", "pgr_ack_deleted",
                        "2026-09-18T12:00:00Z", "{}", "gen_old", "2026-09-18T13:00:00Z"))
            db.commit()

    def tearDown(self):
        self.temporary.cleanup()

    def test_full_live_snapshot_and_only_unacknowledged_deletions(self):
        payload = export_programs.export_programs(self.database)
        self.assertEqual("trainlog-programs", payload["format"])
        self.assertEqual(["pg_live"], [program["program_id"] for program in payload["programs"]])
        occurrence = payload["programs"][0]["sessions"][0]["occurrences"][0]
        self.assertEqual((3, 8, None), (
            occurrence["target_sets"], occurrence["target_reps"],
            occurrence["target_duration_seconds"],
        ))
        self.assertEqual([{
            "deletion_id": "pgd_delete",
            "program_id": "pg_deleted",
            "predecessor_revision_id": "pgr_old",
            "revision_id": "pgr_deleted",
            "requested_at": "2026-09-18T12:00:00Z",
        }], payload["deletions"])


if __name__ == "__main__":
    unittest.main()
