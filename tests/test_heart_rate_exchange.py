#!/usr/bin/env python3
"""Heart Rate V1 companion validation and idempotent desktop import tests."""

import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import heart_rate_exchange as heart_rate


CAPTURE = "hrc_11111111-1111-4111-8111-111111111111"
SESSION = "se_22222222-2222-4222-8222-222222222222"
ENTRY = "sxe_33333333-3333-4333-8333-333333333333"


def schema(db: sqlite3.Connection) -> None:
    db.executescript(
        """
        PRAGMA foreign_keys=ON;
        CREATE TABLE heart_rate_captures(
          capture_id TEXT PRIMARY KEY,context_kind TEXT,context_id TEXT,
          started_at TEXT,ended_at TEXT,sensor_name TEXT,imported_at TEXT);
        CREATE TABLE heart_rate_samples(
          capture_id TEXT,sequence INTEGER,observed_at TEXT,bpm INTEGER,
          exercise_entry_id TEXT,sensor_contact_detected INTEGER,
          energy_expended INTEGER,PRIMARY KEY(capture_id,sequence),
          FOREIGN KEY(capture_id) REFERENCES heart_rate_captures(capture_id));
        CREATE TABLE heart_rate_rr_intervals(
          capture_id TEXT,sample_sequence INTEGER,rr_index INTEGER,value_1024 INTEGER,
          PRIMARY KEY(capture_id,sample_sequence,rr_index),
          FOREIGN KEY(capture_id,sample_sequence)
            REFERENCES heart_rate_samples(capture_id,sequence));
        PRAGMA user_version=30;
        """
    )


def document() -> dict:
    return {
        "format": "trainlog-heart-rate",
        "version": 1,
        "generated_at": "2026-09-22T19:00:00+02:00",
        "captures": [{
            "capture_id": CAPTURE,
            "context_kind": "session",
            "context_id": SESSION,
            "started_at": "2026-09-22T18:00:00+02:00",
            "ended_at": "2026-09-22T18:30:00+02:00",
            "sensor_name": "Synthetic HR",
            "samples": [{
                "sequence": 0,
                "observed_at": "2026-09-22T18:01:00+02:00",
                "bpm": 92,
                "exercise_entry_id": ENTRY,
                "sensor_contact_detected": True,
                "energy_expended": 12,
                "rr_intervals_1024": [700, 710],
            }],
        }],
    }


class HeartRateExchangeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db = sqlite3.connect(Path(self.temp.name) / "heart.sqlite")
        schema(self.db)

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def test_import_exact_replay_and_raw_rr(self):
        payload = heart_rate.validate(document())
        self.assertEqual((1, 0), heart_rate.apply(self.db, payload))
        self.assertEqual((0, 1), heart_rate.apply(self.db, payload))
        self.assertEqual(
            (92, ENTRY, 1, 12),
            self.db.execute(
                "SELECT bpm,exercise_entry_id,sensor_contact_detected,energy_expended "
                "FROM heart_rate_samples"
            ).fetchone(),
        )
        self.assertEqual(
            [(700,), (710,)],
            self.db.execute(
                "SELECT value_1024 FROM heart_rate_rr_intervals ORDER BY rr_index"
            ).fetchall(),
        )

    def test_same_identity_with_different_measurement_is_rejected(self):
        payload = heart_rate.validate(document())
        heart_rate.apply(self.db, payload)
        changed = document()
        changed["captures"][0]["samples"][0]["bpm"] = 93
        with self.assertRaisesRegex(ValueError, "different content"):
            heart_rate.apply(self.db, heart_rate.validate(changed))

    def test_cardio_context_accepts_session_identity_and_exercise_marker(self):
        cardio = document()
        cardio["captures"][0]["context_kind"] = "cardio"
        validated = heart_rate.validate(cardio)
        self.assertEqual("cardio", validated["captures"][0]["context_kind"])
        self.assertEqual(ENTRY, validated["captures"][0]["samples"][0]["exercise_entry_id"])

    def test_rejects_sample_outside_capture_and_sleep_exercise_marker(self):
        outside = document()
        outside["captures"][0]["samples"][0]["observed_at"] = "2026-09-22T17:59:59+02:00"
        with self.assertRaisesRegex(ValueError, "outside capture"):
            heart_rate.validate(outside)

        sleep = document()
        capture = sleep["captures"][0]
        capture["context_kind"] = "sleep"
        capture["context_id"] = "sl_44444444-4444-4444-8444-444444444444"
        with self.assertRaisesRegex(ValueError, "exercise context"):
            heart_rate.validate(sleep)


if __name__ == "__main__":
    unittest.main()
