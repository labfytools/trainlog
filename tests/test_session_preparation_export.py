#!/usr/bin/env python3
"""Versioned preparation delivery and withdrawal export regressions."""

import sqlite3
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import export_session_preparations


class SessionPreparationExportTest(unittest.TestCase):
    def setUp(self):
        self.database = sqlite3.connect(":memory:")
        self.database.executescript(
            """
            CREATE TABLE session_preparations(
                preparation_id TEXT PRIMARY KEY,
                current_revision_id TEXT NOT NULL,
                source_proposal_id TEXT,
                source_payload_sha256 TEXT,
                source_program_id TEXT,
                source_program_session_id TEXT,
                withdrawn_at TEXT
            );
            CREATE TABLE session_preparation_revisions(
                revision_id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                session_type TEXT NOT NULL,
                planned_for TEXT,
                notes TEXT
            );
            CREATE TABLE session_preparation_entries(
                entry_id TEXT PRIMARY KEY,
                revision_id TEXT NOT NULL,
                position INTEGER NOT NULL,
                exercise_id TEXT NOT NULL,
                equipment_id TEXT,
                recording_mode TEXT NOT NULL,
                tracking_mode TEXT NOT NULL,
                data_fields INTEGER NOT NULL,
                load_mode TEXT NOT NULL,
                rest_seconds INTEGER NOT NULL,
                target_sets INTEGER,
                target_reps INTEGER,
                target_duration_seconds INTEGER,
                target_weight_kg REAL,
                notes TEXT
            );
            CREATE TABLE session_preparation_deliveries(
                delivery_id TEXT PRIMARY KEY,
                preparation_id TEXT NOT NULL,
                revision_id TEXT NOT NULL,
                execution_session_id TEXT NOT NULL,
                state TEXT NOT NULL,
                created_at TEXT NOT NULL
            );
            CREATE TABLE program_session_executions(
                program_session_id TEXT PRIMARY KEY,
                program_id TEXT NOT NULL,
                session_id TEXT NOT NULL UNIQUE,
                state TEXT NOT NULL,
                observed_at TEXT NOT NULL
            );
            CREATE TABLE session_preparation_withdrawals(
                withdrawal_id TEXT PRIMARY KEY,
                preparation_id TEXT NOT NULL,
                revision_id TEXT NOT NULL,
                requested_at TEXT NOT NULL,
                generation_id TEXT,
                acknowledged_at TEXT
            );
            PRAGMA user_version=24;
            """
        )

    def tearDown(self):
        self.database.close()

    def add_preparation(self, suffix: str, withdrawn: bool) -> None:
        preparation = f"sp_{suffix}"
        revision = f"spr_{suffix}"
        delivery = f"spd_{suffix}"
        execution = f"se_{suffix}"
        self.database.execute(
            "INSERT INTO session_preparations VALUES(?,?,?,?,?,?,?)",
            (
                preparation,
                revision,
                "aid_11111111-1111-4111-8111-111111111111",
                "a" * 64,
                "pg_11111111-1111-4111-8111-111111111111",
                "pgs_22222222-2222-4222-8222-222222222222",
                "2026-09-18T12:00:00Z" if withdrawn else None,
            ),
        )
        self.database.execute(
            "INSERT INTO session_preparation_revisions VALUES(?,?,?,?,?)",
            (revision, "Préparation", "training", "2026-09-20", None),
        )
        self.database.execute(
            "INSERT INTO session_preparation_entries VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (
                f"spe_{suffix}", revision, 0,
                "ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", None,
                "sets", "reps", 0, "none", 60, 2, 8, None, None, None,
            ),
        )
        self.database.execute(
            "INSERT INTO session_preparation_deliveries VALUES(?,?,?,?,?,?)",
            (delivery, preparation, revision, execution, "pending", "2026-09-18T10:00:00Z"),
        )
        if withdrawn:
            self.database.execute(
                "INSERT INTO session_preparation_withdrawals VALUES(?,?,?,?,NULL,NULL)",
                (f"spw_{suffix}", preparation, revision, "2026-09-18T12:00:00Z"),
            )
        self.database.commit()

    def test_withdrawn_preparation_is_not_delivered_and_exports_exact_evidence(self):
        kept = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
        withdrawn = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"
        self.add_preparation(kept, withdrawn=False)
        self.add_preparation(withdrawn, withdrawn=True)

        payload = export_session_preparations.build_export(self.database)

        self.assertEqual("trainlog-session-preparations", payload["format"])
        self.assertEqual(2, payload["version"])
        self.assertEqual([f"spd_{kept}"], [row["delivery_id"] for row in payload["deliveries"]])
        self.assertEqual(
            "pg_11111111-1111-4111-8111-111111111111",
            payload["deliveries"][0]["source_program_id"],
        )
        self.assertEqual(
            "pgs_22222222-2222-4222-8222-222222222222",
            payload["deliveries"][0]["source_program_session_id"],
        )
        self.assertEqual(1, len(payload["withdrawals"]))
        removal = payload["withdrawals"][0]
        self.assertEqual(f"spw_{withdrawn}", removal["withdrawal_id"])
        self.assertEqual(f"sp_{withdrawn}", removal["preparation_id"])
        self.assertEqual(
            [{
                "delivery_id": f"spd_{withdrawn}",
                "revision_id": f"spr_{withdrawn}",
                "execution_session_id": f"se_{withdrawn}",
            }],
            removal["deliveries"],
        )

    def test_acknowledged_withdrawal_is_not_republished(self):
        suffix = "cccccccc-cccc-4ccc-8ccc-cccccccccccc"
        self.add_preparation(suffix, withdrawn=True)
        self.database.execute(
            "UPDATE session_preparation_withdrawals SET acknowledged_at=?",
            ("2026-09-18T12:05:00Z",),
        )
        self.database.commit()

        payload = export_session_preparations.build_export(self.database)

        self.assertEqual([], payload["deliveries"])
        self.assertEqual([], payload["withdrawals"])

    def test_acknowledged_program_delivery_is_republished_until_execution_returns(self):
        suffix = "dddddddd-dddd-4ddd-8ddd-dddddddddddd"
        self.add_preparation(suffix, withdrawn=False)
        self.database.execute(
            "UPDATE session_preparation_deliveries SET state='acknowledged'"
        )
        self.database.commit()

        payload = export_session_preparations.build_export(self.database)

        self.assertEqual([f"spd_{suffix}"], [row["delivery_id"] for row in payload["deliveries"]])
        self.assertEqual("remote_unknown", payload["deliveries"][0]["state"])

        self.database.execute(
            "INSERT INTO program_session_executions VALUES(?,?,?,?,?)",
            (
                "pgs_22222222-2222-4222-8222-222222222222",
                "pg_11111111-1111-4111-8111-111111111111",
                f"se_{suffix}",
                "completed",
                "2026-09-18T12:10:00Z",
            ),
        )
        self.database.commit()

        payload = export_session_preparations.build_export(self.database)

        self.assertEqual([], payload["deliveries"])


if __name__ == "__main__":
    unittest.main()
