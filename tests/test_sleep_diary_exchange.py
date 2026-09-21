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
                 "start_at": "2026-10-24T23:30:00+02:00", "end_at": "2026-10-25T06:30:00+01:00"}]}


class SleepExchangeTest(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(":memory:")
        self.db.executescript("""
          CREATE TABLE sleep_diary_entries(entry_id TEXT PRIMARY KEY,night_start_date TEXT,night_end_date TEXT,created_at TEXT,updated_at TEXT,current_revision_id TEXT,deleted INTEGER);
          CREATE TABLE sleep_diary_revisions(revision_id TEXT PRIMARY KEY,entry_id TEXT,parent_revision_id TEXT,created_at TEXT,sleep_quality TEXT,wake_quality TEXT,day_form TEXT,treatment_and_notes TEXT);
          CREATE TABLE sleep_diary_events(revision_id TEXT,event_id TEXT,event_type TEXT,start_at TEXT,end_at TEXT,PRIMARY KEY(revision_id,event_id));
        """)

    def document(self, item):
        return {"format": exchange.FORMAT, "version": 1, "generated_at": "2026-10-25T18:00:00+01:00", "entries": [item]}

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

    def test_rejects_concurrent_sibling_and_negative_absolute_interval(self):
        first = "slr_20000000-0000-4000-8000-000000000002"
        self.assertEqual((1, 0), exchange.apply(self.db, self.document(entry(first))))
        with self.assertRaisesRegex(ValueError, "concurrent"):
            exchange.apply(self.db, self.document(entry("slr_50000000-0000-4000-8000-000000000005", None)))
        invalid = entry("slr_60000000-0000-4000-8000-000000000006", first)
        invalid["events"][0]["end_at"] = "2026-10-24T22:30:00+02:00"
        with self.assertRaisesRegex(ValueError, "positive"):
            exchange.validate(self.document(invalid))


if __name__ == "__main__":
    unittest.main()
