#!/usr/bin/env python3
"""Failure-boundary tests for the production generation archive ledger."""

import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import sync_generation_exchange as generation  # noqa: E402


FIXTURE = ROOT / "build" / "tui" / "sync-generation-fixture"
PEER = "peer_11111111-1111-4111-8111-111111111111"


class GenerationArchiveTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="trainlog-archive-test-")
        self.root = Path(self.temporary.name)
        self.database = self.root / "desktop.sqlite"
        subprocess.run([str(FIXTURE), str(self.database)], check=True)
        self.owned = self.root / "owned"
        self.objects = self.root / "objects"

    def tearDown(self):
        self.temporary.cleanup()

    def acknowledge(self, generation_id: str, manifest: dict, digest: str) -> Path:
        generation.publish(self.database, generation_id, self.objects)
        with sqlite3.connect(self.database) as database:
            producer = database.execute(
                "SELECT producer_peer_id FROM sync_generations WHERE generation_id=?",
                (generation_id,),
            ).fetchone()[0]
        ack = generation.ack_document(
            manifest["run_id"], generation_id, producer, PEER, digest,
            "consumed", generation.now(), "",
        )
        path = self.root / f"{generation_id}.ack.json"
        path.write_bytes(generation.canonical(ack))
        self.assertEqual("acknowledged", generation.accept_ack(self.database, path))
        return path

    def create_acknowledged(self):
        values = []
        for _ in range(3):
            stage, manifest, digest = generation.capture_desktop(
                self.database, self.owned, PEER
            )
            del stage
            ack = self.acknowledge(manifest["generation_id"], manifest, digest)
            values.append((manifest, ack))
        return values

    def test_interrupted_archive_resumes_and_replay_stays_idempotent(self):
        values = self.create_acknowledged()
        oldest = values[0][0]["generation_id"]
        temporary = self.owned / "archives" / "generations" / f".{oldest}.tmp"
        temporary.mkdir(parents=True)
        (temporary / "interrupted").write_text("partial")
        self.assertEqual(1, generation.archive_acknowledged(self.database, self.owned, PEER))
        self.assertFalse(temporary.exists())
        self.assertEqual("unchanged", generation.accept_ack(self.database, values[0][1]))
        with sqlite3.connect(self.database) as database:
            self.assertEqual(
                1,
                database.execute(
                    "SELECT COUNT(*) FROM sync_generation_archives WHERE generation_id=?",
                    (oldest,),
                ).fetchone()[0],
            )

    def test_invalid_archive_and_no_space_never_commit_ledger(self):
        values = self.create_acknowledged()
        oldest = values[0][0]["generation_id"]
        destination = self.owned / "archives" / "generations" / oldest
        destination.mkdir(parents=True)
        (destination / "manifest.json").write_text("{}")
        with self.assertRaises(generation.GenerationError):
            generation.archive_acknowledged(self.database, self.owned, PEER)
        with sqlite3.connect(self.database) as database:
            self.assertEqual(0, database.execute("SELECT COUNT(*) FROM sync_generation_archives").fetchone()[0])
        destination.unlink(missing_ok=True) if destination.is_file() else None
        for child in destination.iterdir():
            child.unlink()
        destination.rmdir()
        with mock.patch.object(generation.shutil, "copytree", side_effect=OSError(28, "no space")):
            with self.assertRaises(OSError):
                generation.archive_acknowledged(self.database, self.owned, PEER)
        with sqlite3.connect(self.database) as database:
            self.assertEqual(0, database.execute("SELECT COUNT(*) FROM sync_generation_archives").fetchone()[0])

    def test_historical_acknowledged_rows_without_ack_ledger_do_not_exhaust_admission(self):
        with sqlite3.connect(self.database) as database:
            producer = generation.peer_identity(database, "desktop")
            database.commit()
            parent = None
            for index in range(generation.MAX_RETAINED_PER_PEER):
                suffix = f"{index + 1:012d}"
                generation_id = f"gen_10000000-0000-4000-8000-{suffix}"
                run_id = f"sy_20000000-0000-4000-8000-{suffix}"
                database.execute(
                    "INSERT INTO sync_generations VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
                    (
                        generation_id,
                        run_id,
                        producer,
                        PEER,
                        "desktop",
                        f"2026-09-16T00:00:{index:02d}+00:00",
                        parent,
                        "0" * 64,
                        "acknowledged",
                        "{}",
                        str(self.owned / "staging" / generation_id),
                        f"2026-09-16T00:01:{index:02d}+00:00",
                    ),
                )
                parent = generation_id
            database.commit()

        stage, manifest, _ = generation.capture_desktop(self.database, self.owned, PEER)
        self.assertTrue(stage.is_dir())
        self.assertEqual(parent, manifest["parent_generation_id"])
        with sqlite3.connect(self.database) as database:
            self.assertEqual(
                0,
                database.execute(
                    "SELECT COUNT(*) FROM sync_acknowledgements "
                    "WHERE generation_id LIKE 'gen_10000000-%'"
                ).fetchone()[0],
            )
            self.assertEqual(
                0,
                database.execute(
                    "SELECT COUNT(*) FROM sync_generation_archives "
                    "WHERE generation_id LIKE 'gen_10000000-%'"
                ).fetchone()[0],
            )

    def test_missing_then_late_ack_is_protected_until_correlated(self):
        values = self.create_acknowledged()
        stage, manifest, digest = generation.capture_desktop(self.database, self.owned, PEER)
        del stage
        generation.publish(self.database, manifest["generation_id"], self.objects)
        generation.archive_acknowledged(self.database, self.owned, PEER)
        with sqlite3.connect(self.database) as database:
            self.assertEqual(
                "waiting_acknowledgement",
                database.execute(
                    "SELECT status FROM sync_generations WHERE generation_id=?",
                    (manifest["generation_id"],),
                ).fetchone()[0],
            )
            self.assertEqual(
                0,
                database.execute(
                    "SELECT COUNT(*) FROM sync_generation_archives WHERE generation_id=?",
                    (manifest["generation_id"],),
                ).fetchone()[0],
            )
        self.acknowledge(manifest["generation_id"], manifest, digest)
        self.assertGreaterEqual(generation.archive_acknowledged(self.database, self.owned, PEER), 0)


if __name__ == "__main__":
    unittest.main()
