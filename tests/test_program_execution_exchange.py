#!/usr/bin/env python3
"""Program-execution companion validation and idempotence regressions."""

import json
import sqlite3
import sys
import tempfile
import unittest
from contextlib import closing
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import program_execution_exchange


class ProgramExecutionExchangeTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.database = Path(self.temporary.name) / "trainlog.sqlite"
        self.artifact = Path(self.temporary.name) / "program-executions-v1.json"
        with closing(sqlite3.connect(self.database)) as db:
            db.executescript("""
                PRAGMA foreign_keys=ON;
                CREATE TABLE programs(program_id TEXT PRIMARY KEY);
                CREATE TABLE program_sessions(
                    program_session_id TEXT PRIMARY KEY,
                    program_id TEXT NOT NULL REFERENCES programs(program_id));
                CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE);
                CREATE TABLE program_session_executions(
                    program_session_id TEXT PRIMARY KEY REFERENCES program_sessions(program_session_id),
                    program_id TEXT NOT NULL REFERENCES programs(program_id),
                    session_id TEXT NOT NULL UNIQUE,
                    state TEXT NOT NULL,
                    observed_at TEXT NOT NULL);
                PRAGMA user_version=27;
            """)
            db.execute("INSERT INTO programs VALUES(?)", (
                "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
            ))
            db.execute("INSERT INTO program_sessions VALUES(?,?)", (
                "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
            ))
            db.execute("INSERT INTO sessions(session_id) VALUES(?)", (
                "se_cccccccc-cccc-4ccc-8ccc-cccccccccccc",
            ))
            db.commit()

    def tearDown(self):
        self.temporary.cleanup()

    def document(self, state: str) -> dict:
        return {
            "format": "trainlog-program-executions",
            "version": 1,
            "generated_at": "2026-09-18T20:00:00Z",
            "executions": [{
                "program_id": "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                "program_session_id": "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                "session_id": "se_cccccccc-cccc-4ccc-8ccc-cccccccccccc",
                "state": state,
                "observed_at": "2026-09-18T19:30:00Z",
            }],
        }

    def apply(self, state: str):
        self.artifact.write_text(json.dumps(self.document(state)), encoding="utf-8")
        root = program_execution_exchange.load(self.artifact)
        with closing(sqlite3.connect(self.database)) as db:
            result = program_execution_exchange.apply_executions(db, root)
            db.commit()
            row = db.execute(
                "SELECT state,session_id FROM program_session_executions"
            ).fetchone()
        return result, row

    def test_progression_is_durable_and_replay_is_idempotent(self):
        self.assertEqual(((1, 0, 0), ("in_progress", "se_cccccccc-cccc-4ccc-8ccc-cccccccccccc")),
                         self.apply("in_progress"))
        self.assertEqual(((0, 0, 1), ("in_progress", "se_cccccccc-cccc-4ccc-8ccc-cccccccccccc")),
                         self.apply("in_progress"))
        self.assertEqual(((0, 1, 0), ("completed", "se_cccccccc-cccc-4ccc-8ccc-cccccccccccc")),
                         self.apply("completed"))
        self.assertEqual(((0, 0, 1), ("completed", "se_cccccccc-cccc-4ccc-8ccc-cccccccccccc")),
                         self.apply("completed"))

    def test_completed_requires_imported_history(self):
        with closing(sqlite3.connect(self.database)) as db:
            db.execute("DELETE FROM sessions")
            db.commit()
        self.artifact.write_text(json.dumps(self.document("completed")), encoding="utf-8")
        root = program_execution_exchange.load(self.artifact)
        with closing(sqlite3.connect(self.database)) as db:
            with self.assertRaisesRegex(ValueError, "missing history"):
                program_execution_exchange.apply_executions(db, root)


if __name__ == "__main__":
    unittest.main()
