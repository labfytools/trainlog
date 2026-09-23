#!/usr/bin/env python3

import sys
import tempfile
import subprocess
import unittest
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import cardio_guidance_exchange as guidance
from trainlog_sqlite import connect_database

RUN = "cgr_11111111-1111-4111-8111-111111111111"
PHASE = "cgp_22222222-2222-4222-8222-222222222222"
SESSION = "se_33333333-3333-4333-8333-333333333333"
ENTRY = "sxe_44444444-4444-4444-8444-444444444444"
EXERCISE = "ex_55555555-5555-4555-8555-555555555555"
CAPTURE = "hrc_66666666-6666-4666-8666-666666666666"
CAL = "cal_77777777-7777-4777-8777-777777777777"


def document(calibrated: bool = False) -> dict:
    target = {
        "minimum_bpm": 120,
        "maximum_bpm": 140,
        "calibration_id": CAL if calibrated else None,
        "calibration_observed_peak_bpm": 160 if calibrated else None,
        "minimum_percent": 75 if calibrated else None,
        "maximum_percent": 88 if calibrated else None,
    }
    return {
        "format": "trainlog-cardio-guidance",
        "version": 1,
        "generated_at": "2026-09-23T13:11:00+02:00",
        "runs": [
            {
                "run_id": RUN,
                "session_id": SESSION,
                "started_at": "2026-09-23T13:00:20+02:00",
                "ended_at": "2026-09-23T13:03:20+02:00",
                "phases": [
                    {
                        "phase_id": PHASE,
                        "entry_id": ENTRY,
                        "position": 0,
                        "kind": "work",
                        "target": target,
                        "exit_condition": {
                            "kind": "fixed_duration",
                            "seconds": 180,
                            "bpm": None,
                        },
                        "started_at": "2026-09-23T13:00:20+02:00",
                        "ended_at": "2026-09-23T13:03:20+02:00",
                        "final_instruction": "maintain",
                        "events": [
                            {
                                "sequence": 0,
                                "observed_at": "2026-09-23T13:00:23+02:00",
                                "instruction": "accelerate",
                                "bpm": 110,
                                "target_minimum_bpm": 120,
                                "target_maximum_bpm": 140,
                            },
                            {
                                "sequence": 1,
                                "observed_at": "2026-09-23T13:00:30+02:00",
                                "instruction": "suspended",
                                "bpm": None,
                                "target_minimum_bpm": 120,
                                "target_maximum_bpm": 140,
                            },
                            {
                                "sequence": 2,
                                "observed_at": "2026-09-23T13:00:40+02:00",
                                "instruction": "maintain",
                                "bpm": 130,
                                "target_minimum_bpm": 120,
                                "target_maximum_bpm": 140,
                            },
                        ],
                    }
                ],
            }
        ],
    }


class CardioGuidanceExchangeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp.name) / "trainlog.sqlite"
        fixture = ROOT / "build/tui/sync-generation-fixture"
        subprocess.run([str(fixture), str(self.db_path)], check=True, capture_output=True)
        self.db = connect_database(self.db_path)
        self.db.execute(
            "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) "
            "VALUES(?,?,?,?,?,?)",
            (EXERCISE, "Tapis guidé", "tapis guide", "continuous", "duration", 0),
        )
        exercise_row = self.db.execute(
            "SELECT id FROM exercises WHERE exercise_id=?", (EXERCISE,)
        ).fetchone()[0]
        self.db.execute(
            "INSERT INTO sessions(session_id,started_at,ended_at,session_type,session_kind) "
            "VALUES(?,?,?,?,?)",
            (
                SESSION,
                "2026-09-23T13:00:00+02:00",
                "2026-09-23T13:10:10+02:00",
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
            "INSERT INTO session_exercise_timeline("
            "session_id,entry_id,exercise_id,started_at,ended_at,imported_at"
            ") VALUES(?,?,?,?,?,?)",
            (
                SESSION,
                ENTRY,
                EXERCISE,
                "2026-09-23T13:00:10+02:00",
                "2026-09-23T13:10:10+02:00",
                "2026-09-23T13:11:00+02:00",
            ),
        )
        self.db.execute(
            "INSERT INTO heart_rate_captures("
            "capture_id,context_kind,context_id,started_at,ended_at,sensor_name,imported_at"
            ") VALUES(?,?,?,?,?,?,?)",
            (
                CAPTURE,
                "cardio",
                SESSION,
                "2026-09-23T13:00:00+02:00",
                "2026-09-23T13:10:10+02:00",
                "CYCPLUS H2",
                "2026-09-23T13:11:00+02:00",
            ),
        )
        self.db.executemany(
            "INSERT INTO heart_rate_samples("
            "capture_id,sequence,observed_at,bpm,exercise_entry_id,"
            "sensor_contact_detected,energy_expended) VALUES(?,?,?,?,?,?,?)",
            [
                (CAPTURE, 0, "2026-09-23T13:00:23+02:00", 110, ENTRY, None, None),
                (CAPTURE, 1, "2026-09-23T13:00:40+02:00", 130, ENTRY, None, None),
            ],
        )
        self.db.execute(
            "INSERT INTO cardio_calibrations("
            "calibration_id,protocol_version,session_id,entry_id,started_at,effort_end_at,"
            "ended_at,heart_rate_capture_id,observed_peak_bpm,imported_at"
            ") VALUES(?,?,?,?,?,?,?,?,?,?)",
            (
                CAL,
                1,
                SESSION,
                ENTRY,
                "2026-09-23T13:00:00+02:00",
                "2026-09-23T13:02:00+02:00",
                "2026-09-23T13:03:00+02:00",
                CAPTURE,
                160,
                "2026-09-23T13:11:00+02:00",
            ),
        )
        self.db.commit()

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def test_import_and_exact_replay_preserve_snapshot_and_events(self):
        payload = guidance.validate(document())
        self.assertEqual((1, 0), guidance.apply(self.db, payload))
        self.assertEqual((0, 1), guidance.apply(self.db, payload))
        self.assertEqual(
            (SESSION, "2026-09-23T13:00:20+02:00", "2026-09-23T13:03:20+02:00"),
            tuple(
                self.db.execute(
                    "SELECT session_id,started_at,ended_at FROM cardio_guidance_runs WHERE run_id=?",
                    (RUN,),
                ).fetchone()
            ),
        )
        self.assertEqual(
            [(0, "accelerate", 110), (1, "suspended", None), (2, "maintain", 130)],
            [
                tuple(row)
                for row in self.db.execute(
                    "SELECT sequence,instruction,bpm FROM cardio_guidance_events "
                    "WHERE run_id=? AND phase_id=? ORDER BY sequence",
                    (RUN, PHASE),
                )
            ],
        )

    def test_calibration_provenance_must_match_imported_profile(self):
        payload = guidance.validate(document(calibrated=True))
        self.assertEqual((1, 0), guidance.apply(self.db, payload))
        changed = deepcopy(document(calibrated=True))
        changed["runs"][0]["phases"][0]["target"]["calibration_observed_peak_bpm"] = 159
        with self.assertRaisesRegex(ValueError, "calibration provenance"):
            guidance.apply(self.db, guidance.validate(changed))

    def test_guidance_bpm_must_exist_in_raw_capture(self):
        changed = deepcopy(document())
        changed["runs"][0]["phases"][0]["events"][0]["bpm"] = 109
        with self.assertRaisesRegex(ValueError, "not a raw heart-rate sample"):
            guidance.apply(self.db, guidance.validate(changed))

    def test_identity_reuse_with_different_event_is_rejected(self):
        payload = guidance.validate(document())
        guidance.apply(self.db, payload)
        changed = deepcopy(document())
        changed["runs"][0]["phases"][0]["final_instruction"] = "slow_down"
        with self.assertRaisesRegex(ValueError, "different content"):
            guidance.apply(self.db, guidance.validate(changed))


if __name__ == "__main__":
    unittest.main()
