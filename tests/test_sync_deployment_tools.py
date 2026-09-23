import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import sync_generation_exchange as generation


class DeploymentToolsTest(unittest.TestCase):
    def test_sqlite_backup_captures_committed_wal_and_restores_invariants(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.db"
            backup = root / "backup.db"
            database = sqlite3.connect(source)
            database.execute("PRAGMA journal_mode=WAL")
            database.execute("PRAGMA foreign_keys=ON")
            database.executescript(
                "CREATE TABLE parent(id INTEGER PRIMARY KEY);"
                "CREATE TABLE child(parent_id INTEGER REFERENCES parent);"
                "INSERT INTO parent VALUES(7);"
                "INSERT INTO child VALUES(7);"
            )
            database.commit()
            result = subprocess.run(
                [
                    "python3",
                    str(ROOT / "tools/backup_trainlog_sqlite.py"),
                    str(source),
                    str(backup),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            with sqlite3.connect(backup) as restored:
                self.assertEqual(
                    restored.execute("SELECT * FROM child").fetchall(),
                    [(7,)],
                )
                self.assertEqual(
                    restored.execute("PRAGMA integrity_check").fetchone(),
                    ("ok",),
                )
            database.close()

    def test_candidate_is_self_contained_and_hashed(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "candidate"
            result = subprocess.run(
                [
                    "python3",
                    str(ROOT / "tools/package_sync_candidate.py"),
                    "--output",
                    str(output),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            inventory = json.loads((output / "candidate-inventory.json").read_text())
            self.assertEqual(inventory["product_version"], "0.1.5")
            self.assertEqual(inventory["desktop_schema"], 33)
            self.assertEqual(inventory["android_schema"], 30)
            self.assertIn("session-preparations-v2", inventory["protocols"])
            self.assertIn("programs-v1", inventory["protocols"])
            self.assertIn("program-executions-v1", inventory["protocols"])
            self.assertIn("sleep-diary-v1", inventory["protocols"])
            self.assertIn("cardio-sessions-v1", inventory["protocols"])
            self.assertIn("cardio-calibrations-v1", inventory["protocols"])
            self.assertIn("heart-rate-v1", inventory["protocols"])
            self.assertIn("session-timeline-v1", inventory["protocols"])
            self.assertIn("bluetooth-files-v1", inventory["protocols"])
            self.assertTrue((output / "bin/trainlog").is_file())
            self.assertTrue((output / "bin/trainlog-sync-once").is_file())
            self.assertTrue((output / "bin/trainlog-syncd").is_file())
            self.assertTrue((output / "bin/trainlog-btd").is_file())
            self.assertTrue((output / "libexec/trainlog-sync-once").is_file())
            self.assertTrue((output / "libexec/trainlog-generation-bt-adapter").is_file())
            self.assertIn(
                'TRAINLOG_SYNC_BT_ADAPTER="$root/libexec/trainlog-generation-bt-adapter"',
                (output / "bin/trainlog-syncd").read_text(),
            )
            self.assertIn(
                'TRAINLOG_SYNC_MTP_ADAPTER="$root/libexec/trainlog-generation-mtp-adapter"',
                (output / "bin/trainlog-syncd").read_text(),
            )
            stable_bin = Path(directory) / "stable-bin"
            stable_bin.mkdir()
            (stable_bin / "trainlog").symlink_to(output / "bin/trainlog")
            linked = subprocess.run(
                [str(stable_bin / "trainlog"), "--version"],
                capture_output=True,
                text=True,
            )
            self.assertEqual(linked.returncode, 0, linked.stderr)
            self.assertIn("trainlog 0.1.5", linked.stdout)
            imported = subprocess.run(
                [
                    "python3",
                    "-c",
                    "import pathlib; import sync_peer_worker; import sync_orchestrator; "
                    "import sync_generation_exchange as g; "
                    "assert all((pathlib.Path.cwd()/row[5]).is_file() "
                    "for row in g.ARTIFACTS)",
                ],
                cwd=output / "tools",
                capture_output=True,
                text=True,
            )
            self.assertEqual(imported.returncode, 0, imported.stderr)
            packaged_tools = {
                row["path"].removeprefix("tools/")
                for row in inventory["files"]
                if row["path"].startswith("tools/")
            }
            exporters = {
                row[5]
                for row in generation.ARTIFACTS
            }
            self.assertTrue(exporters <= packaged_tools)
            self.assertTrue(
                any(
                    row["path"] == "tools/trainlog_syncd.py"
                    for row in inventory["files"]
                )
            )
            self.assertTrue(
                any(
                    row["path"] == "tools/sync_peer_worker.py"
                    for row in inventory["files"]
                )
            )
            for required in (
                "tools/sync_drive_transport.py",
                "tools/sync_ai_session_draft_drive.py",
                "tools/post_sync_ai_drive.py",
            ):
                self.assertTrue(
                    any(row["path"] == required for row in inventory["files"]),
                    required,
                )


if __name__ == "__main__":
    unittest.main()
