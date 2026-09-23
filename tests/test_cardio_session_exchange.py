#!/usr/bin/env python3

import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import cardio_session_exchange as cardio
from trainlog_sqlite import connect_database
import import_equipment_definitions


SESSION = "se_11111111-1111-4111-8111-111111111111"
EXERCISE = "ex_22222222-2222-4222-8222-222222222222"
ENTRY = "sxe_33333333-3333-4333-8333-333333333333"


def document() -> dict:
    return {
        "format": "trainlog-cardio-sessions",
        "version": 1,
        "generated_at": "2026-09-23T10:00:00+02:00",
        "exercises": [
            {
                "exercise_id": EXERCISE,
                "name": "Cardio exchange exercise",
                "recording_mode": "sets",
                "tracking_mode": "reps",
                "data_fields": 0,
            }
        ],
        "sessions": [
            {
                "session_id": SESSION,
                "started_at": "2026-09-23T09:00:00+02:00",
                "ended_at": "2026-09-23T09:30:00+02:00",
                "session_type": "cardio",
                "note": None,
                "exercises": [
                    {
                        "exercise_id": EXERCISE,
                        "name": "Cardio exchange exercise",
                        "recording_mode": "sets",
                        "tracking_mode": "reps",
                        "data_fields": 0,
                        "load_mode": "none",
                        "rest_seconds": 0,
                        "entry_id": ENTRY,
                        "position": 0,
                        "equipment_id": None,
                        "target": None,
                        "note": None,
                        "sets": [{"reps": 12}],
                    }
                ],
            }
        ],
    }


class CardioSessionExchangeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp.name) / "trainlog.sqlite"
        fixture = ROOT / "build/tui/sync-generation-fixture"
        subprocess.run([str(fixture), str(self.db_path)], check=True, capture_output=True)
        self.db = connect_database(self.db_path)
        self.known_equipment = import_equipment_definitions.supplied_ids(
            ROOT / "catalog/equipment-v1.json"
        )

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def test_import_and_exact_replay_keep_legacy_type_frozen(self):
        payload = cardio.validate(document(), self.known_equipment)
        first = cardio.apply(self.db, payload, complete_causal_envelope=True)
        second = cardio.apply(self.db, payload, complete_causal_envelope=True)
        self.assertEqual(1, first["sessions_imported"])
        self.assertEqual(0, second["sessions_imported"])
        self.assertEqual(
            ("training", "cardio"),
            tuple(
                self.db.execute(
                    "SELECT session_type,session_kind FROM sessions WHERE session_id=?",
                    (SESSION,),
                ).fetchone()
            ),
        )
        self.assertEqual(
            (ENTRY, 12),
            tuple(
                self.db.execute(
                    "SELECT se.entry_id,ps.reps FROM session_exercises se "
                    "JOIN performed_sets ps ON ps.session_exercise_row_id=se.id "
                    "JOIN sessions s ON s.id=se.session_row_id WHERE s.session_id=?",
                    (SESSION,),
                ).fetchone()
            ),
        )

    def test_rejects_non_cardio_session_type(self):
        changed = document()
        changed["sessions"][0]["session_type"] = "training"
        with self.assertRaisesRegex(ValueError, "must be cardio"):
            cardio.validate(changed, self.known_equipment)

    def test_rejects_identity_collision_with_non_cardio_history(self):
        payload = cardio.validate(document(), self.known_equipment)
        cardio.apply(self.db, payload, complete_causal_envelope=True)
        self.db.execute(
            "UPDATE sessions SET session_kind='training' WHERE session_id=?",
            (SESSION,),
        )
        with self.assertRaisesRegex(ValueError, "collides with non-cardio"):
            cardio.apply(self.db, payload, complete_causal_envelope=True)


if __name__ == "__main__":
    unittest.main()
