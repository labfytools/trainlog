import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import sync_orchestrator  # noqa: E402
import sync_peer_worker  # noqa: E402


class SyncErrorClassificationTest(unittest.TestCase):
    def test_recovery_envelope_includes_consumed_and_rejected_acknowledgements(self):
        with sqlite3.connect(":memory:") as database:
            database.execute(
                "CREATE TABLE sync_consumed_generations("
                "generation_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,"
                "consumed_at TEXT,result TEXT,ack_json TEXT)"
            )
            for generation, result, diagnostic in (
                ("gen_consumed", "consumed", ""),
                ("gen_rejected", "rejected", "bounded rejection"),
                ("gen_legacy", "rejected", "legacy/path"),
                ("gen_other", "consumed", ""),
            ):
                database.execute(
                    "INSERT INTO sync_consumed_generations VALUES(?,?,?,?,?,?)",
                    (generation, "android" if generation != "gen_other" else "other",
                     "desktop", generation, result,
                     json.dumps({"generation_id": generation, "result": result,
                                 "diagnostic": diagnostic})),
                )

            acknowledgements = sync_peer_worker.recovery_acknowledgements(
                database, "android", "desktop"
            )

        self.assertEqual(
            [(value["generation_id"], value["result"]) for value in acknowledgements],
            [("gen_rejected", "rejected"), ("gen_consumed", "consumed")],
        )

    def test_recovery_envelope_prioritizes_recent_terminal_evidence(self):
        with sqlite3.connect(":memory:") as database:
            database.execute(
                "CREATE TABLE sync_consumed_generations("
                "generation_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,"
                "consumed_at TEXT,result TEXT,ack_json TEXT)"
            )
            for index in range(40):
                generation = f"gen_{index:02d}"
                database.execute(
                    "INSERT INTO sync_consumed_generations VALUES(?,?,?,?,?,?)",
                    (
                        generation,
                        "android",
                        "desktop",
                        f"2026-09-18T12:{index:02d}:00Z",
                        "consumed",
                        json.dumps({
                            "generation_id": generation,
                            "result": "consumed",
                            "diagnostic": "",
                        }),
                    ),
                )

            acknowledgements = sync_peer_worker.recovery_acknowledgements(
                database, "android", "desktop"
            )

        self.assertEqual(32, len(acknowledgements))
        self.assertEqual("gen_39", acknowledgements[0]["generation_id"])
        self.assertEqual("gen_08", acknowledgements[-1]["generation_id"])

    def test_mtp_refreshes_a_retained_peer_advertisement_before_validation(self):
        expected_peer = "peer_00000000-0000-4000-8000-000000000001"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            peer_path = root / "android-peer-v1.json"
            peer_path.write_text(
                json.dumps(
                    {
                        "format": "trainlog-sync-peer",
                        "version": 1,
                        "peer_id": expected_peer,
                        "capabilities": sorted(
                            sync_peer_worker.CAPS - {"session-preparations-v2"}
                        ),
                    }
                )
            )
            pulls = []

            def refresh(_adapter, operation, peer, transport, _deadline, allow_missing):
                pulls.append((operation, peer, transport, allow_missing))
                peer_path.write_text(
                    json.dumps(
                        {
                            "format": "trainlog-sync-peer",
                            "version": 1,
                            "peer_id": expected_peer,
                            "capabilities": sorted(sync_peer_worker.CAPS),
                        }
                    )
                )
                return True

            with mock.patch.object(sync_peer_worker, "run_adapter", side_effect=refresh):
                sync_peer_worker.refresh_mtp_peer(
                    Path("/adapter"),
                    expected_peer,
                    root,
                    sync_peer_worker.time.monotonic() + 60,
                )

            value = sync_peer_worker.load_peer(root, expected_peer)
            self.assertEqual(value["peer_id"], expected_peer)
            self.assertEqual(
                pulls,
                [("pull", expected_peer, root, True)],
            )

    def test_transport_timeout_has_a_stable_code_and_bounded_diagnostic(self):
        timeout = subprocess.TimeoutExpired(["adapter", "push"], 30)
        with tempfile.TemporaryDirectory() as directory:
            transport = Path(directory) / "transport"
            transport.mkdir()
            with mock.patch.object(sync_peer_worker.subprocess, "run", side_effect=timeout):
                with self.assertRaisesRegex(
                    RuntimeError,
                    r"^transport_timeout: MTP push did not finish within 30 seconds$",
                ) as caught:
                    sync_peer_worker.run_adapter(
                        Path("/adapter"),
                        "push",
                        "peer_00000000-0000-4000-8000-000000000001",
                        transport,
                        sync_peer_worker.time.monotonic() + 60,
                    )
        self.assertEqual(sync_orchestrator.failure_code(caught.exception), "transport_timeout")

    def test_known_operational_failures_remain_distinct(self):
        self.assertEqual(
            sync_orchestrator.failure_code(RuntimeError("expected Android peer not found")),
            "device_unavailable",
        )
        self.assertEqual(
            sync_orchestrator.failure_code(RuntimeError("pending capacity exhausted")),
            "capacity_exhausted",
        )
        self.assertEqual(
            sync_orchestrator.failure_code(RuntimeError("revision conflict")),
            "data_conflict",
        )

    def test_mtp_outbox_excludes_retained_staging_and_unrelated_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "transport"
            root.mkdir()
            (root / "request-v1.json").write_text("{}")
            (root / "staging").mkdir()
            (root / "staging" / "retained.json").write_text("retained")
            (root / "legacy.json").write_text("legacy")
            observed = []

            def inspect(command, **_kwargs):
                outbox = Path(command[3])
                observed.extend(
                    str(path.relative_to(outbox))
                    for path in outbox.rglob("*")
                    if path.is_file()
                )
                return subprocess.CompletedProcess(command, 0, b"", b"")

            with mock.patch.object(sync_peer_worker.subprocess, "run", side_effect=inspect):
                sync_peer_worker.push_adapter(
                    Path("/adapter"),
                    "peer_00000000-0000-4000-8000-000000000001",
                    root,
                    ["request-v1.json"],
                    sync_peer_worker.time.monotonic() + 60,
                )
            self.assertEqual(observed, ["request-v1.json"])

    def test_desktop_generation_directory_precedes_reference(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            generation = (
                "desktop-objects/generations/"
                "gen_11111111-1111-4111-8111-111111111111"
            )
            (root / generation).mkdir(parents=True)
            (root / generation / "manifest.json").write_text("manifest")
            for name in (
                "request-v1.json",
                "desktop-archive-acknowledgements-v1.json",
                "desktop-consumption-ack-v1.json",
                "desktop-generation-v1.json",
            ):
                (root / name).write_text("{}")
            observed = []

            def inspect(_adapter, _operation, _peer, outbox, _deadline):
                observed.append(
                    sorted(
                        str(path.relative_to(outbox))
                        for path in outbox.rglob("*")
                        if path.is_file()
                    )
                )
                return True

            with mock.patch.object(sync_peer_worker, "run_adapter", side_effect=inspect):
                sync_peer_worker.push_desktop_generation(
                    Path("adapter"),
                    "peer",
                    root,
                    generation,
                    sync_peer_worker.time.monotonic() + 60,
                )
            self.assertEqual(len(observed), 2)
            self.assertEqual(observed[0], [generation + "/manifest.json"])
            self.assertNotIn("desktop-generation-v1.json", observed[0])
            self.assertIn("desktop-generation-v1.json", observed[1])


if __name__ == "__main__":
    unittest.main()
