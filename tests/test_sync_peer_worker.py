import importlib.util
import subprocess
import tempfile
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

    def test_mtp_adapter_uses_fixed_operation_budget(self):
        completed = subprocess.CompletedProcess([], 0, b"", b"")
        with mock.patch.object(worker.subprocess, "run", return_value=completed) as run:
            worker.run_adapter(
                Path("/fixed/adapter"), "pull", "peer_fixed",
                Path("/private/transport"), 100.0, clock=lambda: 50.0,
            )
        self.assertEqual(worker.MTP_OPERATION_TIMEOUT_SECONDS, run.call_args.kwargs["timeout"])

    def test_mtp_pull_pump_is_throttled(self):
        now = [0.0]
        pulls = []
        pump = worker.MtpPullPump(
            lambda: pulls.append(now[0]), interval=3.0, clock=lambda: now[0]
        )
        for timestamp in (0.05, 0.5, 2.99, 3.0, 3.1, 5.99, 6.0):
            now[0] = timestamp
            pump()
        self.assertEqual([3.0, 6.0], pulls)

    def test_global_deadline_is_not_reported_as_transport_timeout(self):
        with self.assertRaises(worker.ConversationDeadlineExpired):
            worker.run_adapter(
                Path("/fixed/adapter"), "pull", "peer_fixed",
                Path("/private/transport"), 10.0, clock=lambda: 10.0,
            )

    def test_deadline_limited_pull_becomes_protocol_timeout(self):
        with mock.patch.object(
            worker.subprocess, "run", side_effect=subprocess.TimeoutExpired(["adapter"], 0.5)
        ):
            with self.assertRaises(worker.ConversationDeadlineExpired):
                worker.run_adapter(
                    Path("/fixed/adapter"), "pull", "peer_fixed",
                    Path("/private/transport"), 10.5, clock=lambda: 10.0,
                )

    def test_blocked_full_budget_pull_is_transport_timeout(self):
        with mock.patch.object(
            worker.subprocess, "run", side_effect=subprocess.TimeoutExpired(["adapter"], 30)
        ):
            with self.assertRaisesRegex(RuntimeError, "transport_timeout"):
                worker.run_adapter(
                    Path("/fixed/adapter"), "pull", "peer_fixed",
                    Path("/private/transport"), 100.0, clock=lambda: 50.0,
                )

    def test_delayed_correlated_generation_survives_multiple_polls(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "android-generation-v1.json"
            now = [0.0]
            pulls = []

            def pump():
                pulls.append(now[0])
                if len(pulls) == 3:
                    reference.write_text('{"run_id":"sy_fresh"}', encoding="utf-8")

            value = worker.wait_json(
                reference, 10.0,
                lambda candidate: candidate.get("run_id") == "sy_fresh",
                pump, clock=lambda: now[0],
                sleeper=lambda duration: now.__setitem__(0, now[0] + duration),
            )
            self.assertEqual("sy_fresh", value["run_id"])
            self.assertEqual(3, len(pulls))

    def test_stale_reference_is_ignored_until_correlated_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "android-generation-v1.json"
            reference.write_text('{"run_id":"sy_stale"}', encoding="utf-8")
            now = [0.0]
            polls = [0]

            def pump():
                polls[0] += 1
                if polls[0] == 2:
                    reference.write_text('{"run_id":"sy_fresh"}', encoding="utf-8")

            value = worker.wait_json(
                reference, 1.0,
                lambda candidate: candidate.get("run_id") == "sy_fresh",
                pump, clock=lambda: now[0],
                sleeper=lambda duration: now.__setitem__(0, now[0] + duration),
            )
            self.assertEqual("sy_fresh", value["run_id"])

    def test_missing_generation_reports_correlated_protocol_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "android-generation-v1.json"
            reference.write_text('{"run_id":"sy_stale"}', encoding="utf-8")
            now = [0.0]
            with self.assertRaisesRegex(
                RuntimeError, "timeout waiting for correlated android-generation-v1.json"
            ):
                worker.wait_json(
                    reference, 0.2,
                    lambda candidate: candidate.get("run_id") == "sy_fresh",
                    clock=lambda: now[0],
                    sleeper=lambda duration: now.__setitem__(0, now[0] + duration),
                )


if __name__ == "__main__":
    unittest.main()
