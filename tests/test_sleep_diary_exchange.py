#!/usr/bin/env python3
import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import sleep_diary_exchange as exchange


def entry(revision, parent=None, deleted=False):
    return {"entry_id": "sl_10000000-0000-4000-8000-000000000001",
            "night_start_date": "2026-10-24", "night_end_date": "2026-10-25",
            "created_at": "2026-10-24T22:00:00+02:00", "updated_at": "2026-10-25T18:00:00+01:00",
            "revision_id": revision, "parent_revision_id": parent, "deleted": deleted,
            "sleep_quality": "B", "wake_quality": "Moy", "day_form": "TB",
            "treatment_and_notes": "Synthetic fixture", "events": [
                {"event_id": "sle_30000000-0000-4000-8000-000000000003", "type": "sleep",
                 "start_at": "2026-10-24T23:30:00+02:00", "end_at": "2026-10-25T06:30:00+01:00"}],
            "intakes": [{"intake_id": "mdi_70000000-0000-4000-8000-000000000007",
                "medication_id": "med_80000000-0000-4000-8000-000000000008",
                "medication_name": "Synthetic medication", "taken_at": "2026-10-25T05:15:00+01:00",
                "dose_value": 10, "dose_unit": "mg", "note": "", "created_at": "2026-10-24T22:00:00+02:00"}]}


class SleepExchangeTest(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(":memory:")
        self.db.executescript("""
          CREATE TABLE sleep_diary_entries(entry_id TEXT PRIMARY KEY,night_start_date TEXT,night_end_date TEXT,created_at TEXT,updated_at TEXT,current_revision_id TEXT,deleted INTEGER);
          CREATE TABLE sleep_diary_revisions(revision_id TEXT PRIMARY KEY,entry_id TEXT,parent_revision_id TEXT,created_at TEXT,sleep_quality TEXT,wake_quality TEXT,day_form TEXT,treatment_and_notes TEXT);
          CREATE TABLE sleep_diary_events(revision_id TEXT,event_id TEXT,event_type TEXT,start_at TEXT,end_at TEXT,PRIMARY KEY(revision_id,event_id));
          CREATE TABLE sleep_medications(medication_id TEXT PRIMARY KEY,created_at TEXT,updated_at TEXT,current_revision_id TEXT,deleted INTEGER);
          CREATE TABLE sleep_medication_revisions(revision_id TEXT PRIMARY KEY,medication_id TEXT,parent_revision_id TEXT,created_at TEXT,name TEXT,default_dose_value REAL,default_dose_unit TEXT,form TEXT,note TEXT,active INTEGER);
          CREATE TABLE sleep_medication_intakes(revision_id TEXT,intake_id TEXT,medication_id TEXT,medication_name TEXT,taken_at TEXT,dose_value REAL,dose_unit TEXT,quantity INTEGER NOT NULL DEFAULT 1 CHECK(quantity BETWEEN 1 AND 99),note TEXT,created_at TEXT,PRIMARY KEY(revision_id,intake_id));
          CREATE TABLE sleep_diary_publication_state(entry_id TEXT PRIMARY KEY,validated_revision_id TEXT,validated_at TEXT,acknowledged_revision_id TEXT,acknowledged_at TEXT);
        """)

    def document(self, item):
        medication = {"medication_id": "med_80000000-0000-4000-8000-000000000008",
            "revision_id": "medr_90000000-0000-4000-8000-000000000009", "parent_revision_id": None,
            "created_at": "2026-10-24T18:00:00+02:00", "updated_at": "2026-10-24T18:00:00+02:00",
            "name": "Synthetic medication", "default_dose_value": 5, "default_dose_unit": "mg",
            "form": "tablet", "note": "", "active": True, "deleted": False}
        return {"format": exchange.FORMAT, "version": 1, "generated_at": "2026-10-25T18:00:00+01:00", "entries": [item], "medications": [medication]}

    def test_roundtrip_replay_conflict_and_non_resurrection(self):
        first = "slr_20000000-0000-4000-8000-000000000002"
        second = "slr_40000000-0000-4000-8000-000000000004"
        self.assertEqual((1, 0), exchange.apply(self.db, self.document(entry(first))))
        self.assertEqual((0, 1), exchange.apply(self.db, self.document(entry(first))))
        self.assertEqual((1, 0), exchange.apply(self.db, self.document(entry(second, first, True))))
        with self.assertRaisesRegex(ValueError, "resurrect"):
            exchange.apply(self.db, self.document(entry("slr_50000000-0000-4000-8000-000000000005", second, False)))
        exported = exchange.build(self.db)
        self.assertTrue(exported["entries"][0]["deleted"])

    def test_v1_hydrates_quantity_one_and_v2_preserves_quantity(self):
        first = "slr_20000000-0000-4000-8000-000000000002"
        legacy = self.document(entry(first))
        self.assertEqual((1, 0), exchange.apply(self.db, legacy))
        self.assertEqual(1, self.db.execute(
            "SELECT quantity FROM sleep_medication_intakes"
        ).fetchone()[0])

        second = "slr_40000000-0000-4000-8000-000000000004"
        evolved = self.document(entry(second, first))
        evolved["version"] = 2
        evolved["entries"][0]["intakes"][0]["quantity"] = 2
        self.assertEqual((1, 0), exchange.apply(self.db, evolved))
        self.assertEqual(2, self.db.execute(
            "SELECT quantity FROM sleep_medication_intakes WHERE revision_id=?",
            (second,),
        ).fetchone()[0])

        state_before_replay = {
            "entry": self.db.execute(
                "SELECT current_revision_id,deleted FROM sleep_diary_entries"
            ).fetchall(),
            "revisions": self.db.execute(
                "SELECT revision_id,parent_revision_id FROM sleep_diary_revisions "
                "ORDER BY revision_id"
            ).fetchall(),
            "intakes": self.db.execute(
                "SELECT revision_id,intake_id,quantity FROM sleep_medication_intakes "
                "ORDER BY revision_id,intake_id"
            ).fetchall(),
        }
        self.assertEqual((0, 1), exchange.apply(self.db, evolved))
        self.assertEqual(state_before_replay, {
            "entry": self.db.execute(
                "SELECT current_revision_id,deleted FROM sleep_diary_entries"
            ).fetchall(),
            "revisions": self.db.execute(
                "SELECT revision_id,parent_revision_id FROM sleep_diary_revisions "
                "ORDER BY revision_id"
            ).fetchall(),
            "intakes": self.db.execute(
                "SELECT revision_id,intake_id,quantity FROM sleep_medication_intakes "
                "ORDER BY revision_id,intake_id"
            ).fetchall(),
        })
        self.assertEqual(2, self.db.execute(
            "SELECT COUNT(*) FROM sleep_diary_revisions"
        ).fetchone()[0])
        self.assertEqual((1, 2), self.db.execute(
            "SELECT COUNT(*),MIN(quantity) FROM sleep_medication_intakes "
            "WHERE revision_id=?",
            (second,),
        ).fetchone())

    def test_stale_ancestor_snapshot_does_not_regress_newer_tip(self):
        first = "slr_20000000-0000-4000-8000-000000000002"
        second = "slr_40000000-0000-4000-8000-000000000004"
        first_document = self.document(entry(first))
        second_document = self.document(entry(second, first))
        second_medication = second_document["medications"][0]
        second_medication["parent_revision_id"] = first_document["medications"][0]["revision_id"]
        second_medication["revision_id"] = "medr_a0000000-0000-4000-8000-00000000000a"
        second_medication["updated_at"] = "2026-10-25T18:05:00+01:00"
        second_medication["name"] = "Synthetic medication revised"

        self.assertEqual((1, 0), exchange.apply(self.db, first_document))
        self.assertEqual((1, 0), exchange.apply(self.db, second_document))
        self.assertEqual((0, 1), exchange.apply(self.db, first_document))
        self.assertEqual(
            second,
            self.db.execute(
                "SELECT current_revision_id FROM sleep_diary_entries WHERE entry_id=?",
                (first_document["entries"][0]["entry_id"],),
            ).fetchone()[0],
        )
        self.assertEqual(
            second_medication["revision_id"],
            self.db.execute(
                "SELECT current_revision_id FROM sleep_medications WHERE medication_id=?",
                (second_medication["medication_id"],),
            ).fetchone()[0],
        )

    def test_ancestry_allows_multi_hop_remote_advance_but_not_sibling(self):
        first = "slr_20000000-0000-4000-8000-000000000002"
        middle = "slr_30000000-0000-4000-8000-000000000003"
        latest = "slr_40000000-0000-4000-8000-000000000004"
        first_document = self.document(entry(first))
        latest_document = self.document(entry(latest, middle))
        latest_document["entries"][0]["ancestry"] = [latest, middle, first]

        medication_first = first_document["medications"][0]["revision_id"]
        medication_middle = "medr_a0000000-0000-4000-8000-00000000000a"
        medication_latest = "medr_b0000000-0000-4000-8000-00000000000b"
        medication = latest_document["medications"][0]
        medication["revision_id"] = medication_latest
        medication["parent_revision_id"] = medication_middle
        medication["ancestry"] = [medication_latest, medication_middle, medication_first]
        medication["updated_at"] = "2026-10-25T18:05:00+01:00"
        medication["name"] = "Synthetic medication revised"

        self.assertEqual((1, 0), exchange.apply(self.db, first_document))
        self.assertEqual((1, 0), exchange.apply(self.db, latest_document))
        self.assertEqual(
            latest,
            self.db.execute(
                "SELECT current_revision_id FROM sleep_diary_entries WHERE entry_id=?",
                (first_document["entries"][0]["entry_id"],),
            ).fetchone()[0],
        )
        self.assertEqual(
            medication_latest,
            self.db.execute(
                "SELECT current_revision_id FROM sleep_medications WHERE medication_id=?",
                (medication["medication_id"],),
            ).fetchone()[0],
        )

        sibling = self.document(
            entry("slr_50000000-0000-4000-8000-000000000005", first)
        )
        sibling["entries"][0]["ancestry"] = [
            sibling["entries"][0]["revision_id"],
            first,
        ]
        with self.assertRaisesRegex(ValueError, "concurrent"):
            exchange.apply(self.db, sibling)

    def test_current_snapshot_seeds_fresh_peer_from_non_root_revisions(self):
        parent = "slr_20000000-0000-4000-8000-000000000002"
        tip = "slr_40000000-0000-4000-8000-000000000004"
        document = self.document(entry(tip, parent))
        document["version"] = 2
        document["entries"][0]["intakes"][0]["quantity"] = 2
        document["entries"][0]["ancestry"] = [tip, parent]

        medication = document["medications"][0]
        medication_parent = medication["revision_id"]
        medication_tip = "medr_a0000000-0000-4000-8000-00000000000a"
        medication["revision_id"] = medication_tip
        medication["parent_revision_id"] = medication_parent
        medication["ancestry"] = [medication_tip, medication_parent]
        medication["updated_at"] = "2026-10-25T18:05:00+01:00"

        rejected = self.document(entry(
            "slr_50000000-0000-4000-8000-000000000005",
            "slr_60000000-0000-4000-8000-000000000006",
        ))
        rejected["version"] = 2
        rejected["entries"][0]["intakes"][0]["quantity"] = 2
        with self.assertRaisesRegex(ValueError, "unknown parent"):
            exchange.apply(self.db, rejected)
        self.db.rollback()

        self.assertEqual((1, 0), exchange.apply(self.db, document))
        self.assertEqual(
            (tip, parent),
            self.db.execute(
                "SELECT revision_id,parent_revision_id FROM sleep_diary_revisions"
            ).fetchone(),
        )
        self.assertEqual(
            (medication_tip, medication_parent),
            self.db.execute(
                "SELECT revision_id,parent_revision_id FROM sleep_medication_revisions"
            ).fetchone(),
        )


    def test_rejects_concurrent_sibling_and_negative_absolute_interval(self):
        first = "slr_20000000-0000-4000-8000-000000000002"
        self.assertEqual((1, 0), exchange.apply(self.db, self.document(entry(first))))
        with self.assertRaisesRegex(ValueError, "concurrent"):
            exchange.apply(self.db, self.document(entry("slr_50000000-0000-4000-8000-000000000005", None)))
        invalid = entry("slr_60000000-0000-4000-8000-000000000006", first)
        invalid["events"][0]["end_at"] = "2026-10-24T22:30:00+02:00"
        with self.assertRaisesRegex(ValueError, "positive"):
            exchange.validate(self.document(invalid))

    def test_draft_is_durable_but_not_exported_until_validated(self):
        revision = "slr_20000000-0000-4000-8000-000000000002"
        item = entry(revision)
        self.db.execute("INSERT INTO sleep_diary_entries VALUES(?,?,?,?,?,?,?)",
            (item["entry_id"], item["night_start_date"], item["night_end_date"],
             item["created_at"], item["updated_at"], revision, 0))
        self.db.execute("INSERT INTO sleep_diary_revisions VALUES(?,?,?,?,?,?,?,?)",
            (revision, item["entry_id"], None, item["updated_at"], "B", "Moy", "TB",
             item["treatment_and_notes"]))
        self.assertEqual([], exchange.build(self.db)["entries"])
        self.db.execute("INSERT INTO sleep_diary_publication_state VALUES(?,?,?,NULL,NULL)",
            (item["entry_id"], revision, item["updated_at"]))
        self.assertEqual(revision, exchange.build(self.db)["entries"][0]["revision_id"])


if __name__ == "__main__":
    unittest.main()
