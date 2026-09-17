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
    def test_transport_timeout_has_a_stable_code_and_bounded_diagnostic(self):
        timeout = subprocess.TimeoutExpired(["adapter", "push"], 30)
        with mock.patch.object(sync_peer_worker.subprocess, "run", side_effect=timeout):
            with self.assertRaisesRegex(
                RuntimeError,
                r"^transport_timeout: MTP push did not finish within 30 seconds$",
            ) as caught:
                sync_peer_worker.run_adapter(
                    Path("/adapter"),
                    "push",
                    "peer_00000000-0000-4000-8000-000000000001",
                    Path("/transport"),
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


if __name__ == "__main__":
    unittest.main()
