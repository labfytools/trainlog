import argparse
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
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="trainlog-peer-worker-test-")
        self.transport = Path(self.temporary.name) / "transport"
        self.transport.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    def test_drive_adapter_uses_remaining_conversation_deadline(self):
        completed = subprocess.CompletedProcess([], 0, b"", b"")
        with mock.patch.object(worker.subprocess, "run", return_value=completed) as run:
            worker.run_drive_adapter(
                Path("/fixed/adapter.py"),
                "pull",
                "remote:Trainlog/Sync-Test/run",
                self.transport,
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
                self.transport, 100.0, clock=lambda: 50.0,
            )
        self.assertEqual(worker.MTP_OPERATION_TIMEOUT_SECONDS, run.call_args.kwargs["timeout"])

    def test_mtp_adapter_uses_and_releases_shared_transport_lock(self):
        completed = subprocess.CompletedProcess([], 0, b"", b"")
        real_flock = worker.fcntl.flock
        with mock.patch.object(worker.subprocess, "run", return_value=completed), mock.patch.object(
            worker.fcntl, "flock", wraps=real_flock
        ) as flock:
            worker.run_adapter(
                Path("/fixed/adapter"), "pull", "peer_fixed",
                self.transport, 100.0, clock=lambda: 50.0,
            )
        self.assertTrue((self.transport.parent / "mtp.lock").is_file())
        operations = [call.args[1] for call in flock.call_args_list]
        self.assertIn(worker.fcntl.LOCK_EX | worker.fcntl.LOCK_NB, operations)
        self.assertEqual(worker.fcntl.LOCK_UN, operations[-1])

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
                self.transport, 10.0, clock=lambda: 10.0,
            )

    def test_deadline_limited_pull_becomes_protocol_timeout(self):
        with mock.patch.object(
            worker.subprocess, "run", side_effect=subprocess.TimeoutExpired(["adapter"], 0.5)
        ):
            with self.assertRaises(worker.ConversationDeadlineExpired):
                worker.run_adapter(
                    Path("/fixed/adapter"), "pull", "peer_fixed",
                    self.transport, 10.5, clock=lambda: 10.0,
                )

    def test_blocked_full_budget_pull_is_transport_timeout(self):
        with mock.patch.object(
            worker.subprocess, "run", side_effect=subprocess.TimeoutExpired(["adapter"], 30)
        ):
            with self.assertRaisesRegex(RuntimeError, "transport_timeout"):
                worker.run_adapter(
                    Path("/fixed/adapter"), "pull", "peer_fixed",
                    self.transport, 100.0, clock=lambda: 50.0,
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

    def test_bluetooth_adapter_uses_distinct_transport_lock(self):
        completed = subprocess.CompletedProcess([], 0, b"", b"")
        with mock.patch.object(worker.subprocess, "run", return_value=completed):
            worker.run_adapter(
                Path("/fixed/bt-adapter"),
                "pull",
                "peer_fixed",
                self.transport,
                100.0,
                clock=lambda: 50.0,
                transport_name="Bluetooth",
                lock_name="bt.lock",
            )
        self.assertTrue((self.transport.parent / "bt.lock").is_file())

    def test_ai_export_delivery_is_best_effort_and_uses_bluetooth(self):
        database = Path(self.temporary.name) / "trainlog.db"
        database.touch()
        args = argparse.Namespace(
            database=database,
            transport_root=self.transport,
            mode="bt",
            bt_adapter=Path("/fixed/bt-adapter"),
            mtp_adapter=None,
            expected_peer="peer_fixed",
        )

        def execute(command, **kwargs):
            if command[0] == worker.sys.executable:
                output = Path(command[2])
                output.write_text(
                    '{"format":"TRAINLOG_AI_EXPORT","version":1}\n',
                    encoding="utf-8",
                )
                return subprocess.CompletedProcess(
                    command,
                    0,
                    "TRAINLOG_AI_EXPORT_V1=PASS\n",
                    "",
                )
            self.assertEqual("/fixed/bt-adapter", command[0])
            self.assertEqual("push", command[1])
            return subprocess.CompletedProcess(command, 0, b"", b"")

        with mock.patch.object(worker.subprocess, "run", side_effect=execute):
            result = worker.publish_ai_export_to_android(args, time.monotonic() + 30)
        self.assertEqual({"result": "delivered_to_android"}, result)
        self.assertTrue((self.transport / "trainlog_ai_export_v1.json").is_file())

    def test_ai_export_transport_failure_does_not_raise(self):
        database = Path(self.temporary.name) / "trainlog.db"
        database.touch()
        args = argparse.Namespace(
            database=database,
            transport_root=self.transport,
            mode="bt",
            bt_adapter=Path("/fixed/bt-adapter"),
            mtp_adapter=None,
            expected_peer="peer_fixed",
        )

        def execute(command, **kwargs):
            if command[0] == worker.sys.executable:
                Path(command[2]).write_text(
                    '{"format":"TRAINLOG_AI_EXPORT","version":1}\n',
                    encoding="utf-8",
                )
                return subprocess.CompletedProcess(
                    command,
                    0,
                    "TRAINLOG_AI_EXPORT_V1=PASS\n",
                    "",
                )
            return subprocess.CompletedProcess(
                command,
                2,
                b"",
                b"Bluetooth adapter failed status=6 diagnostic=peer vanished",
            )

        with mock.patch.object(worker.subprocess, "run", side_effect=execute):
            result = worker.publish_ai_export_to_android(args, time.monotonic() + 30)
        self.assertEqual("pending", result["result"])
        self.assertIn("peer vanished", result["diagnostic"])

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
