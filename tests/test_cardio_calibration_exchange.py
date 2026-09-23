#!/usr/bin/env python3

import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import cardio_calibration_exchange as calibration
from trainlog_sqlite import connect_database


CAL = "cal_11111111-1111-4111-8111-111111111111"
SESSION = "se_22222222-2222-4222-8222-222222222222"
ENTRY = "sxe_33333333-3333-4333-8333-333333333333"
CAPTURE = "hrc_44444444-4444-4444-8444-444444444444"
EXERCISE = "ex_ca1b4a7e-1c2d-4f00-8a11-000000000001"


def document() -> dict:
    return {
        "format": "trainlog-cardio-calibrations",
        "version": 1,
        "generated_at": "2026-09-23T10:07:00+02:00",
        "calibrations": [
            {
                "calibration_id": CAL,
                "protocol_version": 1,
                "session_id": SESSION,
                "entry_id": ENTRY,
                "started_at": "2026-09-23T10:00:00+02:00",
                "effort_end_at": "2026-09-23T10:03:01+02:00",
                "ended_at": "2026-09-23T10:06:10+02:00",
                "heart_rate_capture_id": CAPTURE,
                "observed_peak_bpm": 160,
                "recovery": [
                    {
                        "target_offset_seconds": 60,
                        "observed_at": "2026-09-23T10:04:01+02:00",
                        "bpm": 140,
                    },
                    {
                        "target_offset_seconds": 120,
                        "observed_at": "2026-09-23T10:05:02+02:00",
                        "bpm": 120,
                    },
                    {
                        "target_offset_seconds": 180,
                        "observed_at": "2026-09-23T10:06:01+02:00",
                        "bpm": 100,
                    },
                ],
            }
        ],
    }


class CardioCalibrationExchangeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp.name) / "trainlog.sqlite"
        fixture = ROOT / "build/tui/sync-generation-fixture"
        subprocess.run([str(fixture), str(self.db_path)], check=True, capture_output=True)
        self.db = connect_database(self.db_path)
        self.db.execute(
            "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) "
            "VALUES(?,?,?,?,?,?)",
            (EXERCISE, "Calibration cardio", "calibration cardio", "continuous", "duration", 0),
        )
        exercise_row = self.db.execute(
            "SELECT id FROM exercises WHERE exercise_id=?", (EXERCISE,)
        ).fetchone()[0]
        self.db.execute(
            "INSERT INTO sessions(session_id,started_at,ended_at,session_type,session_kind) "
            "VALUES(?,?,?,?,?)",
            (
                SESSION,
                "2026-09-23T10:00:00+02:00",
                "2026-09-23T10:06:10+02:00",
                "training",
                "cardio",
            ),
        )
        session_row = self.db.execute(
            "SELECT id FROM sessions WHERE session_id=?", (SESSION,)
        ).fetchone()[0]
        self.db.execute(
            "INSERT INTO session_exercises("
            "entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,"
            "data_fields,position,load_mode,rest_seconds) VALUES(?,?,?,?,?,?,?,?,?)",
            (ENTRY, session_row, exercise_row, "continuous", "duration", 0, 0, "none", 0),
        )
        self.db.execute(
            "INSERT INTO heart_rate_captures("
            "capture_id,context_kind,context_id,started_at,ended_at,sensor_name,imported_at"
            ") VALUES(?,?,?,?,?,?,?)",
            (
                CAPTURE,
                "cardio",
                SESSION,
                "2026-09-23T10:00:00+02:00",
                "2026-09-23T10:06:10+02:00",
                "CYCPLUS H2",
                "2026-09-23T10:07:00+02:00",
            ),
        )
        samples = [
            ("2026-09-23T10:00:10+02:00", 80),
            ("2026-09-23T10:01:20+02:00", 112),
            ("2026-09-23T10:02:30+02:00", 150),
            ("2026-09-23T10:03:00+02:00", 160),
            ("2026-09-23T10:04:01+02:00", 140),
            ("2026-09-23T10:05:02+02:00", 120),
            ("2026-09-23T10:06:01+02:00", 100),
        ]
        self.db.executemany(
            "INSERT INTO heart_rate_samples("
            "capture_id,sequence,observed_at,bpm,exercise_entry_id,"
            "sensor_contact_detected,energy_expended) VALUES(?,?,?,?,?,?,?)",
            [
                (CAPTURE, index, observed_at, bpm, ENTRY, None, None)
                for index, (observed_at, bpm) in enumerate(samples)
            ],
        )
        self.db.commit()

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def test_import_and_exact_replay_are_factual_and_idempotent(self):
        payload = calibration.validate(document())
        self.assertEqual((1, 0), calibration.apply(self.db, payload))
        self.assertEqual((0, 1), calibration.apply(self.db, payload))
        self.assertEqual(
            (1, SESSION, CAPTURE, 160),
            tuple(
                self.db.execute(
                    "SELECT protocol_version,session_id,heart_rate_capture_id,observed_peak_bpm "
                    "FROM cardio_calibrations WHERE calibration_id=?",
                    (CAL,),
                ).fetchone()
            ),
        )
        self.assertEqual(
            [(60, 140), (120, 120), (180, 100)],
            [
                tuple(row)
                for row in self.db.execute(
                    "SELECT target_offset_seconds,bpm FROM cardio_calibration_recovery "
                    "WHERE calibration_id=? ORDER BY target_offset_seconds",
                    (CAL,),
                )
            ],
        )

    def test_rejects_peak_not_supported_by_raw_capture(self):
        changed = deepcopy(document())
        changed["calibrations"][0]["observed_peak_bpm"] = 159
        with self.assertRaisesRegex(ValueError, "peak BPM"):
            calibration.apply(self.db, calibration.validate(changed))

    def test_rejects_recovery_point_not_present_in_raw_capture(self):
        changed = deepcopy(document())
        changed["calibrations"][0]["recovery"][0]["bpm"] = 139
        with self.assertRaisesRegex(ValueError, "not a raw heart-rate sample"):
            calibration.apply(self.db, calibration.validate(changed))


if __name__ == "__main__":
    unittest.main()
