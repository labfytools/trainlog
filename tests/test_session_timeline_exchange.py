#!/usr/bin/env python3

import sqlite3
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import session_timeline_exchange as timeline

SESSION = "se_11111111-1111-4111-8111-111111111111"
ENTRY = "sxe_22222222-2222-4222-8222-222222222222"
EXERCISE = "ex_33333333-3333-4333-8333-333333333333"


def schema(db: sqlite3.Connection) -> None:
    db.executescript(
        """
        CREATE TABLE sessions(
          id INTEGER PRIMARY KEY,
          session_id TEXT UNIQUE,
          started_at TEXT,
          ended_at TEXT
        );
        CREATE TABLE exercises(
          id INTEGER PRIMARY KEY,
          exercise_id TEXT UNIQUE
        );
        CREATE TABLE session_exercises(
          id INTEGER PRIMARY KEY,
          session_row_id INTEGER,
          exercise_row_id INTEGER,
          entry_id TEXT UNIQUE
        );
        CREATE TABLE session_exercise_timeline(
          session_id TEXT,
          entry_id TEXT,
          exercise_id TEXT,
          started_at TEXT,
          ended_at TEXT,
          imported_at TEXT,
          PRIMARY KEY(session_id,entry_id)
        );
        PRAGMA user_version=31;
        """
    )
    db.execute(
        "INSERT INTO sessions(id,session_id,started_at,ended_at) VALUES(1,?,?,?)",
        (SESSION, "2026-09-23T08:00:00+02:00", "2026-09-23T09:00:00+02:00"),
    )
    db.execute("INSERT INTO exercises(id,exercise_id) VALUES(1,?)", (EXERCISE,))
    db.execute(
        "INSERT INTO session_exercises(id,session_row_id,exercise_row_id,entry_id) "
        "VALUES(1,1,1,?)",
        (ENTRY,),
    )


def document() -> dict:
    return {
        "format": "trainlog-session-timeline",
        "version": 1,
        "generated_at": "2026-09-23T09:01:00+02:00",
        "sessions": [
            {
                "session_id": SESSION,
                "started_at": "2026-09-23T08:00:00+02:00",
                "ended_at": "2026-09-23T09:00:00+02:00",
                "exercises": [
                    {
                        "entry_id": ENTRY,
                        "exercise_id": EXERCISE,
                        "started_at": "2026-09-23T08:10:00+02:00",
                        "ended_at": "2026-09-23T08:20:00+02:00",
                    }
                ],
            }
        ],
    }


class SessionTimelineExchangeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db = sqlite3.connect(Path(self.temp.name) / "timeline.sqlite")
        schema(self.db)

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def test_import_and_exact_replay(self):
        payload = timeline.validate(document())
        self.assertEqual((1, 0), timeline.apply(self.db, payload))
        self.assertEqual((0, 1), timeline.apply(self.db, payload))
        self.assertEqual(
            (
                SESSION,
                ENTRY,
                EXERCISE,
                "2026-09-23T08:10:00+02:00",
                "2026-09-23T08:20:00+02:00",
            ),
            self.db.execute(
                "SELECT session_id,entry_id,exercise_id,started_at,ended_at "
                "FROM session_exercise_timeline"
            ).fetchone(),
        )

    def test_rejects_identity_reuse_with_different_content(self):
        payload = timeline.validate(document())
        timeline.apply(self.db, payload)
        changed = deepcopy(document())
        changed["sessions"][0]["exercises"][0]["ended_at"] = "2026-09-23T08:21:00+02:00"
        with self.assertRaisesRegex(ValueError, "different content"):
            timeline.apply(self.db, timeline.validate(changed))

    def test_rejects_occurrence_that_does_not_match_history(self):
        payload = document()
        payload["sessions"][0]["exercises"][0]["exercise_id"] = (
            "ex_44444444-4444-4444-8444-444444444444"
        )
        with self.assertRaisesRegex(ValueError, "does not match session history"):
            timeline.apply(self.db, timeline.validate(payload))

    def test_rejects_exercise_outside_session_bounds(self):
        payload = document()
        payload["sessions"][0]["exercises"][0]["started_at"] = "2026-09-23T07:59:59+02:00"
        with self.assertRaisesRegex(ValueError, "outside session"):
            timeline.validate(payload)


if __name__ == "__main__":
    unittest.main()
