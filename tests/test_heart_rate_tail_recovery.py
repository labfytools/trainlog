#!/usr/bin/env python3
"""Causal recovery coverage for delayed session-end heart-rate tails."""

import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import heart_rate_exchange
import causal_delete_exchange
import heart_rate_correction_exchange
import recover_heart_rate_tail as recovery


SESSION = "se_22222222-2222-4222-8222-222222222222"
OTHER_SESSION = "se_99999999-9999-4999-8999-999999999999"
CAPTURE = "hrc_11111111-1111-4111-8111-111111111111"
OTHER_CAPTURE = "hrc_88888888-8888-4888-8888-888888888888"
CUTOFF = "2026-09-24T11:16:00+02:00"


def create_schema(db: sqlite3.Connection) -> None:
    db.executescript(
        """
        PRAGMA foreign_keys=ON;
        CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,
          started_at TEXT,ended_at TEXT,session_type TEXT,notes TEXT);
        CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,session_row_id INTEGER);
        CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER);
        CREATE TABLE continuous_activity(session_exercise_row_id INTEGER PRIMARY KEY);
        CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY);
        CREATE TABLE session_exercise_timeline(session_id TEXT,entry_id TEXT,
          exercise_id TEXT,started_at TEXT,ended_at TEXT,imported_at TEXT);
        CREATE TABLE heart_rate_captures(capture_id TEXT PRIMARY KEY,context_kind TEXT,
          context_id TEXT,started_at TEXT,ended_at TEXT,sensor_name TEXT,imported_at TEXT);
        CREATE TABLE heart_rate_samples(capture_id TEXT,sequence INTEGER,observed_at TEXT,
          bpm INTEGER,exercise_entry_id TEXT,sensor_contact_detected INTEGER,
          energy_expended INTEGER,PRIMARY KEY(capture_id,sequence),
          FOREIGN KEY(capture_id) REFERENCES heart_rate_captures(capture_id) ON DELETE CASCADE);
        CREATE TABLE heart_rate_rr_intervals(capture_id TEXT,sample_sequence INTEGER,
          rr_index INTEGER,value_1024 INTEGER,
          PRIMARY KEY(capture_id,sample_sequence,rr_index),
          FOREIGN KEY(capture_id,sample_sequence)
            REFERENCES heart_rate_samples(capture_id,sequence) ON DELETE CASCADE);
        CREATE TABLE sync_causal_operations(operation_id TEXT PRIMARY KEY,target_kind TEXT,
          target_id TEXT,creator_id TEXT,predecessor_revision_id TEXT,created_at TEXT,
          payload_sha256 TEXT,publication_context TEXT);
        CREATE TABLE sync_causal_state(target_kind TEXT,target_id TEXT,
          current_revision_id TEXT,deleted INTEGER,operation_id TEXT,
          PRIMARY KEY(target_kind,target_id));
        PRAGMA user_version=36;
        """
    )


def seed(db: sqlite3.Connection, ended_at: str = CUTOFF,
         include_tail: bool = True) -> dict:
    db.execute(
        "INSERT INTO sessions VALUES(1,?,?,?,?,?)",
        (SESSION, "2026-09-24T10:00:00+02:00", ended_at, "training", "keep"),
    )
    db.execute("INSERT INTO session_exercises VALUES(1,1)")
    db.execute("INSERT INTO performed_sets VALUES(1,1)")
    db.execute(
        "INSERT INTO session_exercise_timeline VALUES(?,?,?,?,?,?)",
        (SESSION, "sxe_33333333-3333-4333-8333-333333333333",
         "ex_44444444-4444-4444-8444-444444444444",
         "2026-09-24T10:15:00+02:00", "2026-09-24T11:15:00+02:00", CUTOFF),
    )
    capture_end = "2026-09-24T11:16:00.000001+02:00" if include_tail else CUTOFF
    db.execute(
        "INSERT INTO heart_rate_captures VALUES(?,?,?,?,?,?,?)",
        (CAPTURE, "session", SESSION, "2026-09-24T10:00:00+02:00", capture_end,
         "Synthetic", CUTOFF),
    )
    samples = [
        (CAPTURE, 0, "2026-09-24T11:15:59.999999+02:00", 100, None, 1, 1),
        (CAPTURE, 1, CUTOFF, 101, None, 1, 2),
    ]
    if include_tail:
        samples.append((CAPTURE, 2, "2026-09-24T11:16:00.000001+02:00",
                        40, None, 0, 3))
    db.executemany("INSERT INTO heart_rate_samples VALUES(?,?,?,?,?,?,?)", samples)
    db.executemany(
        "INSERT INTO heart_rate_rr_intervals VALUES(?,?,0,?)",
        [(CAPTURE, row[1], 900 + row[1]) for row in samples],
    )
    db.execute(
        "INSERT INTO heart_rate_captures VALUES(?,?,?,?,?,?,?)",
        (OTHER_CAPTURE, "session", OTHER_SESSION, "2026-09-24T09:00:00+02:00",
         "2026-09-24T09:01:00+02:00", "Other", CUTOFF),
    )
    db.execute(
        "INSERT INTO heart_rate_samples VALUES(?,?,?,?,?,?,?)",
        (OTHER_CAPTURE, 0, "2026-09-24T09:00:30+02:00", 77, None, 1, None),
    )
    db.commit()
    return {
        "format": "trainlog-heart-rate",
        "version": 1,
        "generated_at": CUTOFF,
        "captures": [{
            "capture_id": CAPTURE,
            "context_kind": "session",
            "context_id": SESSION,
            "started_at": "2026-09-24T10:00:00+02:00",
            "ended_at": capture_end,
            "sensor_name": "Synthetic",
            "samples": [{
                "sequence": row[1], "observed_at": row[2], "bpm": row[3],
                "exercise_entry_id": None, "sensor_contact_detected": bool(row[5]),
                "energy_expended": row[6], "rr_intervals_1024": [900 + row[1]],
            } for row in samples],
        }],
    }


class HeartRateTailRecoveryTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / "trainlog.sqlite"
        self.db = sqlite3.connect(self.path)
        create_schema(self.db)

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def apply(self, operation=None, fail=False):
        return recovery.recover(
            self.db, SESSION, CAPTURE, CUTOFF, "peer_test", True,
            operation=operation, fail_after_effect=fail,
        )

    def test_exact_cutoff_rr_stats_and_old_generation_replay(self):
        old_generation = seed(self.db)
        report, operation = self.apply()
        self.assertEqual("applied", report["outcome"])
        self.assertEqual(1, report["before"]["bpm_remove"])
        self.assertEqual(1, report["before"]["rr_remove"])
        self.assertEqual(0, report["after"]["bpm_remove"])
        self.assertEqual(100.5, report["after"]["current_stats"]["average"])
        self.assertEqual(
            [(0, "2026-09-24T11:15:59.999999+02:00"), (1, CUTOFF)],
            self.db.execute(
                "SELECT sequence,observed_at FROM heart_rate_samples "
                "WHERE capture_id=? ORDER BY sequence", (CAPTURE,),
            ).fetchall(),
        )
        self.assertEqual(2, self.db.execute(
            "SELECT COUNT(*) FROM heart_rate_rr_intervals WHERE capture_id=?", (CAPTURE,),
        ).fetchone()[0])
        correction_document = heart_rate_correction_exchange.export_document(self.db)
        self.assertEqual([operation], correction_document["corrections"])
        self.assertEqual([], causal_delete_exchange.export_document(self.db)["operations"])
        self.assertEqual((0, 1), heart_rate_exchange.apply(
            self.db, heart_rate_exchange.validate(old_generation),
        ))
        second, _ = self.apply(operation)
        self.assertEqual("unchanged", second["outcome"])
        self.assertEqual(2, second["after"]["bpm_total"])

    def test_dry_run_and_injected_failure_roll_back_every_table(self):
        seed(self.db)
        before = list(self.db.iterdump())
        report, _ = recovery.recover(
            self.db, SESSION, CAPTURE, CUTOFF, "peer_test", False,
        )
        self.assertEqual("dry-run", report["mode"])
        self.assertEqual(before, list(self.db.iterdump()))
        with self.assertRaisesRegex(recovery.RecoveryError, "injected failure"):
            self.apply(fail=True)
        self.assertEqual(before, list(self.db.iterdump()))

    def test_wrong_id_and_invalid_temporal_bounds_are_rejected(self):
        seed(self.db)
        with self.assertRaisesRegex(recovery.RecoveryError, "session not found"):
            recovery.recover(self.db, OTHER_SESSION, CAPTURE, CUTOFF, "peer", False)
        with self.assertRaisesRegex(recovery.RecoveryError, "capture"):
            recovery.recover(
                self.db, SESSION,
                "hrc_77777777-7777-4777-8777-777777777777", CUTOFF, "peer", False,
            )
        self.db.execute(
            "UPDATE sessions SET ended_at='2026-09-24T09:59:59+02:00' WHERE session_id=?",
            (SESSION,),
        )
        self.db.commit()
        with self.assertRaisesRegex(Exception, "outside capture"):
            recovery.recover(
                self.db, SESSION, CAPTURE, "2026-09-24T09:59:59+02:00", "peer", False,
            )
        self.db.execute("UPDATE sessions SET ended_at=? WHERE session_id=?", (CUTOFF, SESSION))
        self.db.execute(
            "UPDATE session_exercise_timeline SET ended_at='2026-09-24T11:16:01+02:00'",
        )
        self.db.commit()
        with self.assertRaisesRegex(Exception, "precedes the last exercise"):
            self.apply()

    def test_already_bounded_capture_is_noop(self):
        seed(self.db, include_tail=False)
        report, operation = self.apply()
        self.assertEqual("unchanged", report["outcome"])
        self.assertIsNone(operation)
        self.assertEqual(0, self.db.execute(
            "SELECT COUNT(*) FROM sync_causal_operations",
        ).fetchone()[0])


if __name__ == "__main__":
    unittest.main()
