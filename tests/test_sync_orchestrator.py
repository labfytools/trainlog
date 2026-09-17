import json, os, signal, subprocess, tempfile, time, unittest, uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORCHESTRATOR = ROOT / "tools/sync_orchestrator.py"


def identity(prefix):
    return prefix + str(uuid.uuid4())


class OrchestratorTest(unittest.TestCase):
    def fixture(self, root, **overrides):
        db = root / "trainlog.db"
        db.touch()
        state = Path(str(db) + ".sync-run.json")
        run = identity("sy_")
        request = identity("sy_")
        state.write_text(
            json.dumps(
                {
                    "run_id": run,
                    "request_id": request,
                    "trigger": "web",
                    "phase": "requested",
                    "progress_revision": 1,
                    "result": "running",
                }
            )
        )
        config = {
            "format": "trainlog-sync-orchestrator-config",
            "version": 1,
            "enabled": True,
            "mode": "directory",
            "expected_peer_id": identity("peer_"),
            "transport_root": str(root / "transport"),
            "owned_root": str(root / "owned"),
            "timeout_seconds": 30,
        } | overrides
        (root / "transport").mkdir()
        (root / "owned").mkdir()
        path = root / "config.json"
        path.write_text(json.dumps(config))
        return db, state, path, run, request

    def invoke(self, executable, values):
        db, state, config, run, request = values
        return subprocess.run(
            [
                "python3",
                str(executable),
                "--database",
                str(db),
                "--state",
                str(state),
                "--config",
                str(config),
                "--run-id",
                run,
                "--request-id",
                request,
            ],
            capture_output=True,
            text=True,
        )

    def test_browser_configuration_cannot_select_command_or_boolean_timeout(self):
        for override in (
            {"peer_command": ["evil"]},
            {"timeout_seconds": True},
            {"mode": "unsupported"},
        ):
            with self.subTest(override=override), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                values = self.fixture(root, **override)
                result = self.invoke(ORCHESTRATOR, values)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(json.loads(values[1].read_text())["phase"], "failed")

    def test_stdout_bound_is_enforced_before_complete_accumulation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sandbox = root / "program"
            sandbox.mkdir()
            (sandbox / "sync_orchestrator.py").write_bytes(ORCHESTRATOR.read_bytes())
            (sandbox / "sync_peer_worker.py").write_text(
                "import sys,time\nsys.stdout.write('x'*70000);sys.stdout.flush();time.sleep(30)\n"
            )
            values = self.fixture(root)
            result = self.invoke(sandbox / "sync_orchestrator.py", values)
            self.assertEqual(result.returncode, 2)
            final = json.loads(values[1].read_text())
            self.assertIn("exceeds 64 KiB", final["diagnostic"])

    def test_shutdown_reaps_ignoring_helper_and_descendant(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sandbox = root / "program"
            sandbox.mkdir()
            pidfile = root / "pids"
            (sandbox / "sync_orchestrator.py").write_bytes(ORCHESTRATOR.read_bytes())
            (sandbox / "sync_peer_worker.py").write_text(
                "import os,signal,time\nsignal.signal(signal.SIGTERM,signal.SIG_IGN)\npid=os.fork()\nif pid==0:\n signal.signal(signal.SIGTERM,signal.SIG_IGN);open(%r,'a').write(str(os.getpid())+'\\n');time.sleep(60)\nelse:\n open(%r,'a').write(str(os.getpid())+'\\n');time.sleep(60)\n"
                % (str(pidfile), str(pidfile))
            )
            values = self.fixture(root)
            db, state, config, run, request = values
            process = subprocess.Popen(
                [
                    "python3",
                    str(sandbox / "sync_orchestrator.py"),
                    "--database",
                    str(db),
                    "--state",
                    str(state),
                    "--config",
                    str(config),
                    "--run-id",
                    run,
                    "--request-id",
                    request,
                ]
            )
            deadline = time.monotonic() + 5
            while not pidfile.is_file() and time.monotonic() < deadline:
                time.sleep(0.02)
            process.send_signal(signal.SIGTERM)
            self.assertEqual(process.wait(timeout=10), 2)
            for pid in map(int, pidfile.read_text().split()):
                with self.assertRaises(ProcessLookupError):
                    os.kill(pid, 0)
            self.assertEqual(json.loads(state.read_text())["phase"], "interrupted")


if __name__ == "__main__":
    unittest.main()
