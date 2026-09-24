#!/usr/bin/env python3
"""Transactional Sleep revision collision recovery regressions."""

import copy
import sqlite3
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import recover_sleep_revision_collision as recovery
import sleep_diary_exchange as exchange


ENTRY = "sl_10000000-0000-4000-8000-000000000001"
REMOTE_PARENT = "slr_20000000-0000-4000-8000-000000000002"
LOCAL_PARENT = "slr_30000000-0000-4000-8000-000000000003"
COLLISION = "slr_40000000-0000-4000-8000-000000000004"
LOCAL_REIDENTIFIED = "slr_50000000-0000-4000-8000-000000000005"
RECONCILIATION = "slr_60000000-0000-4000-8000-000000000006"
RECOVERED_AT = "2026-10-25T18:30:00+01:00"
GENERATION = "gen_a0000000-0000-4000-8000-00000000000a"
RUN = "sy_b0000000-0000-4000-8000-00000000000b"
PRODUCER = "peer_c0000000-0000-4000-8000-00000000000c"
CONSUMER = "peer_d0000000-0000-4000-8000-00000000000d"
MANIFEST_DIGEST = "a" * 64


def incoming_entry() -> dict:
    return {
        "entry_id": ENTRY,
        "night_start_date": "2026-10-24",
        "night_end_date": "2026-10-25",
        "created_at": "2026-10-24T22:00:00+02:00",
        "updated_at": "2026-10-25T07:00:00+01:00",
        "revision_id": COLLISION,
        "parent_revision_id": REMOTE_PARENT,
        "ancestry": [COLLISION, REMOTE_PARENT],
        "deleted": False,
        "sleep_quality": None,
        "wake_quality": None,
        "day_form": None,
        "treatment_and_notes": "",
        "events": [{
            "event_id": "sle_70000000-0000-4000-8000-000000000007",
            "type": "sleep",
            "start_at": "2026-10-24T23:00:00+02:00",
            "end_at": "2026-10-25T06:00:00+01:00",
        }],
        "intakes": [],
    }


def document() -> dict:
    return {
        "format": exchange.FORMAT,
        "version": 2,
        "generated_at": "2026-10-25T18:00:00+01:00",
        "entries": [incoming_entry()],
        "medications": [],
    }


def manifest() -> dict:
    return {
        "generation_id": GENERATION,
        "run_id": RUN,
        "producer": {"peer_id": PRODUCER, "kind": "android"},
        "consumer_peer_id": CONSUMER,
        "generated_at": "2026-10-25T18:00:00+01:00",
        "parent_generation_id": None,
    }


class SleepRevisionCollisionRecoveryTest(unittest.TestCase):
    def setUp(self) -> None:
        self.db = sqlite3.connect(":memory:")
        self.db.execute("PRAGMA foreign_keys=ON")
        self.db.executescript("""
          CREATE TABLE sleep_diary_entries(entry_id TEXT PRIMARY KEY,night_start_date TEXT,night_end_date TEXT,created_at TEXT,updated_at TEXT,current_revision_id TEXT,deleted INTEGER);
          CREATE TABLE sleep_diary_revisions(revision_id TEXT PRIMARY KEY,entry_id TEXT REFERENCES sleep_diary_entries(entry_id),parent_revision_id TEXT,created_at TEXT,sleep_quality TEXT,wake_quality TEXT,day_form TEXT,treatment_and_notes TEXT);
          CREATE TABLE sleep_diary_events(revision_id TEXT REFERENCES sleep_diary_revisions(revision_id),event_id TEXT,event_type TEXT,start_at TEXT,end_at TEXT,PRIMARY KEY(revision_id,event_id));
          CREATE TABLE sleep_medication_intakes(revision_id TEXT REFERENCES sleep_diary_revisions(revision_id),intake_id TEXT,medication_id TEXT,medication_name TEXT,taken_at TEXT,dose_value REAL,dose_unit TEXT,quantity INTEGER,note TEXT,created_at TEXT,PRIMARY KEY(revision_id,intake_id));
          CREATE TABLE sleep_medications(medication_id TEXT PRIMARY KEY,created_at TEXT,updated_at TEXT,current_revision_id TEXT,deleted INTEGER);
          CREATE TABLE sleep_medication_revisions(revision_id TEXT PRIMARY KEY,medication_id TEXT,parent_revision_id TEXT,created_at TEXT,name TEXT,default_dose_value REAL,default_dose_unit TEXT,form TEXT,note TEXT,active INTEGER);
          CREATE TABLE sleep_diary_publication_state(entry_id TEXT PRIMARY KEY,validated_revision_id TEXT,validated_at TEXT,acknowledged_revision_id TEXT,acknowledged_at TEXT);
          CREATE TABLE sleep_diary_generation_entries(generation_id TEXT,entry_id TEXT,revision_id TEXT,PRIMARY KEY(generation_id,entry_id));
          CREATE TABLE sync_consumed_generations(generation_id TEXT PRIMARY KEY,run_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,parent_generation_id TEXT,manifest_sha256 TEXT,consumed_at TEXT,result TEXT,durability TEXT,diagnostic TEXT,ack_json TEXT);
          CREATE TABLE sync_acknowledgements(ack_id TEXT PRIMARY KEY,generation_id TEXT,run_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,manifest_sha256 TEXT,result TEXT,durability TEXT,created_at TEXT,diagnostic TEXT,payload_sha256 TEXT);
          CREATE TABLE sync_generations(generation_id TEXT PRIMARY KEY,run_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,producer_kind TEXT,generated_at TEXT,parent_generation_id TEXT,manifest_sha256 TEXT,status TEXT,manifest_json TEXT,staging_path TEXT,acknowledged_at TEXT);
        """)
        item = incoming_entry()
        self.db.execute(
            "INSERT INTO sleep_diary_entries VALUES(?,?,?,?,?,?,0)",
            (ENTRY, item["night_start_date"], item["night_end_date"], item["created_at"],
             item["updated_at"], COLLISION),
        )
        # The fixture intentionally reproduces two parents and one reused id.
        for revision_id, parent in ((REMOTE_PARENT, None), (LOCAL_PARENT, REMOTE_PARENT)):
            self.db.execute(
                "INSERT INTO sleep_diary_revisions VALUES(?,?,?,?,?,?,?,?)",
                (revision_id, ENTRY, parent, item["updated_at"], None, None, None, ""),
            )
        self.db.execute(
            "INSERT INTO sleep_diary_revisions VALUES(?,?,?,?,?,?,?,?)",
            (COLLISION, ENTRY, LOCAL_PARENT, item["updated_at"], "TB", "B", None, ""),
        )
        self.db.execute(
            "INSERT INTO sleep_diary_events VALUES(?,?,?,?,?)",
            (COLLISION, item["events"][0]["event_id"], "sleep",
             item["events"][0]["start_at"], item["events"][0]["end_at"]),
        )
        for suffix, quantity in (("8", 1), ("9", 2)):
            self.db.execute(
                "INSERT INTO sleep_medication_intakes VALUES(?,?,?,?,?,?,?,?,?,?)",
                (COLLISION, f"mdi_80000000-0000-4000-8000-00000000000{suffix}",
                 "med_90000000-0000-4000-8000-000000000009", "Fixture medication",
                 "2026-10-24T22:30:00+02:00", 10.0, "mg", quantity, "",
                 "2026-10-24T22:30:00+02:00"),
            )
        self.db.execute(
            "INSERT INTO sleep_diary_publication_state VALUES(?,?,?,?,?)",
            (ENTRY, REMOTE_PARENT, item["updated_at"], REMOTE_PARENT, item["updated_at"]),
        )
        self.db.commit()

    def tearDown(self) -> None:
        self.db.close()

    def ids(self):
        values = iter((LOCAL_REIDENTIFIED, RECONCILIATION))
        return lambda: next(values)

    def state(self):
        return {
            "entry": self.db.execute("SELECT * FROM sleep_diary_entries").fetchall(),
            "revisions": self.db.execute(
                "SELECT * FROM sleep_diary_revisions ORDER BY revision_id"
            ).fetchall(),
            "events": self.db.execute(
                "SELECT * FROM sleep_diary_events ORDER BY revision_id,event_id"
            ).fetchall(),
            "intakes": self.db.execute(
                "SELECT * FROM sleep_medication_intakes ORDER BY revision_id,intake_id"
            ).fetchall(),
        }

    def test_unpublished_branch_is_preserved_and_replay_is_idempotent(self):
        report = recovery.recover(
            self.db, document(), COLLISION, RECOVERED_AT, self.ids()
        )
        self.assertEqual(LOCAL_REIDENTIFIED, report["local_reidentified_revision_id"])
        self.assertEqual(RECONCILIATION, report["reconciliation_revision_id"])
        self.assertEqual((2, 1), (report["intake_count"], report["event_count"]))
        self.assertEqual(
            RECONCILIATION,
            self.db.execute("SELECT current_revision_id FROM sleep_diary_entries").fetchone()[0],
        )
        self.assertEqual(
            (REMOTE_PARENT, None, None),
            self.db.execute(
                "SELECT parent_revision_id,sleep_quality,wake_quality FROM sleep_diary_revisions "
                "WHERE revision_id=?", (COLLISION,),
            ).fetchone(),
        )
        self.assertEqual(
            (COLLISION, "TB", "B"),
            self.db.execute(
                "SELECT parent_revision_id,sleep_quality,wake_quality FROM sleep_diary_revisions "
                "WHERE revision_id=?", (RECONCILIATION,),
            ).fetchone(),
        )
        self.assertEqual(
            [(1,), (2,)],
            self.db.execute(
                "SELECT quantity FROM sleep_medication_intakes WHERE revision_id=? "
                "ORDER BY intake_id", (RECONCILIATION,),
            ).fetchall(),
        )
        self.assertEqual((0, 1), exchange.apply(self.db, document()))
        self.assertEqual([], self.db.execute("PRAGMA foreign_key_check").fetchall())

    def test_historical_identity_mismatch_is_rejected_after_recovery(self):
        recovery.recover(self.db, document(), COLLISION, RECOVERED_AT, self.ids())
        conflicting = copy.deepcopy(document())
        conflicting["entries"][0]["sleep_quality"] = "M"
        with self.assertRaisesRegex(ValueError, "identity reused"):
            exchange.apply(self.db, conflicting)

    def test_failure_rolls_back_every_reidentification_write(self):
        before = self.state()
        with self.assertRaisesRegex(recovery.RecoveryError, "injected"):
            recovery.recover(
                self.db, document(), COLLISION, RECOVERED_AT, self.ids(),
                fail_after_reidentification=True,
            )
        self.assertEqual(before, self.state())

    def test_published_local_branch_is_never_reidentified(self):
        self.db.execute(
            "UPDATE sleep_diary_publication_state SET validated_revision_id=? WHERE entry_id=?",
            (COLLISION, ENTRY),
        )
        self.db.commit()
        with self.assertRaisesRegex(recovery.RecoveryError, "publication state"):
            recovery.recover(self.db, document(), COLLISION, RECOVERED_AT, self.ids())

    def test_two_non_null_scalar_values_require_human_decision(self):
        conflicting = document()
        conflicting["entries"][0]["sleep_quality"] = "M"
        with self.assertRaisesRegex(recovery.RecoveryError, "human decision"):
            recovery.recover(self.db, conflicting, COLLISION, RECOVERED_AT, self.ids())

    def test_same_intake_identity_with_different_quantity_is_rejected(self):
        conflicting = document()
        conflicting["entries"][0]["intakes"] = [{
            "intake_id": "mdi_80000000-0000-4000-8000-000000000008",
            "medication_id": "med_90000000-0000-4000-8000-000000000009",
            "medication_name": "Fixture medication",
            "taken_at": "2026-10-24T22:30:00+02:00",
            "dose_value": 10.0,
            "dose_unit": "mg",
            "quantity": 2,
            "note": "",
            "created_at": "2026-10-24T22:30:00+02:00",
        }]
        with self.assertRaisesRegex(recovery.RecoveryError, "identity reused"):
            recovery.recover(self.db, conflicting, COLLISION, RECOVERED_AT, self.ids())

    def insert_rejection_ack(self) -> None:
        ack_id = "ack_" + GENERATION.removeprefix("gen_")
        self.db.execute(
            "INSERT INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)",
            (ack_id, GENERATION, RUN, PRODUCER, CONSUMER, MANIFEST_DIGEST, "rejected",
             "sqlite-commit-rejection", RECOVERED_AT, "", "b" * 64),
        )

    def test_desktop_rejection_is_rearmed_with_recovery_transaction(self):
        self.db.execute(
            "INSERT INTO sync_consumed_generations VALUES(?,?,?,?,?,?,?,?,?,?,?)",
            (GENERATION, RUN, PRODUCER, CONSUMER, None, MANIFEST_DIGEST, RECOVERED_AT,
             "rejected", "sqlite-commit-rejection", "", "{}"),
        )
        self.insert_rejection_ack()
        self.db.commit()
        report = recovery.recover(
            self.db, document(), COLLISION, RECOVERED_AT, self.ids(),
            rejected_generation=(manifest(), MANIFEST_DIGEST),
        )
        self.assertTrue(report["desktop_rejection_rearmed"])
        self.assertEqual(0, self.db.execute(
            "SELECT COUNT(*) FROM sync_consumed_generations WHERE generation_id=?", (GENERATION,)
        ).fetchone()[0])
        self.assertEqual(0, self.db.execute(
            "SELECT COUNT(*) FROM sync_acknowledgements WHERE generation_id=?", (GENERATION,)
        ).fetchone()[0])

    def test_post_recovery_schema_rejection_can_be_rearmed_exactly(self):
        recovery.recover(self.db, document(), COLLISION, RECOVERED_AT, self.ids())
        self.db.execute(
            "INSERT INTO sync_consumed_generations VALUES(?,?,?,?,?,?,?,?,?,?,?)",
            (GENERATION, RUN, PRODUCER, CONSUMER, None, MANIFEST_DIGEST, RECOVERED_AT,
             "rejected", "sqlite-commit-rejection", "schema desktop v8 à v35 requis", "{}"),
        )
        self.insert_rejection_ack()
        self.db.commit()
        report = recovery.rearm_desktop_consumer(
            self.db, document(), manifest(), MANIFEST_DIGEST, COLLISION
        )
        self.assertEqual("ready_for_retry", report["status"])
        self.assertFalse(report["immutable_artifacts_rewritten"])
        self.assertEqual(0, self.db.execute(
            "SELECT COUNT(*) FROM sync_consumed_generations WHERE generation_id=?", (GENERATION,)
        ).fetchone()[0])
        self.assertEqual(0, self.db.execute(
            "SELECT COUNT(*) FROM sync_acknowledgements WHERE generation_id=?", (GENERATION,)
        ).fetchone()[0])

    def test_android_rearm_preserves_manifest_and_sleep_mapping(self):
        self.db.execute(
            "INSERT INTO sync_generations VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
            (GENERATION, RUN, PRODUCER, CONSUMER, "android", manifest()["generated_at"],
             None, MANIFEST_DIGEST, "rejected", "{}", "/immutable", RECOVERED_AT),
        )
        self.db.execute(
            "INSERT INTO sleep_diary_generation_entries VALUES(?,?,?)",
            (GENERATION, ENTRY, COLLISION),
        )
        self.insert_rejection_ack()
        self.db.commit()
        report = recovery.rearm_android_producer(
            self.db, manifest(), MANIFEST_DIGEST, COLLISION
        )
        self.assertFalse(report["immutable_artifacts_rewritten"])
        self.assertEqual(
            ("waiting_acknowledgement", None, "{}", "/immutable"),
            self.db.execute(
                "SELECT status,acknowledged_at,manifest_json,staging_path FROM sync_generations "
                "WHERE generation_id=?", (GENERATION,),
            ).fetchone(),
        )
        self.assertEqual(0, self.db.execute(
            "SELECT COUNT(*) FROM sync_acknowledgements WHERE generation_id=?", (GENERATION,)
        ).fetchone()[0])


if __name__ == "__main__":
    unittest.main()
