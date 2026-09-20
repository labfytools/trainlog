import importlib.util
import subprocess
import time
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "sync_peer_worker", ROOT / "tools/sync_peer_worker.py"
)
worker = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(worker)


class SyncPeerWorkerTest(unittest.TestCase):
    def test_drive_adapter_uses_remaining_conversation_deadline(self):
        completed = subprocess.CompletedProcess([], 0, b"", b"")
        with mock.patch.object(worker.subprocess, "run", return_value=completed) as run:
            worker.run_drive_adapter(
                Path("/fixed/adapter.py"),
                "pull",
                "remote:Trainlog/Sync-Test/run",
                Path("/private/transport"),
                time.monotonic() + 180,
            )
        timeout = run.call_args.kwargs["timeout"]
        self.assertGreaterEqual(timeout, 178)
        self.assertLessEqual(timeout, 180)


if __name__ == "__main__":
    unittest.main()
