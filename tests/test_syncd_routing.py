#!/usr/bin/env python3
"""Regression coverage for Android/daemon full-generation admission."""

import argparse
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("trainlog_syncd", ROOT / "tools/trainlog_syncd.py")
syncd = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(syncd)


class SyncdRoutingTest(unittest.TestCase):
    def test_android_and_daemon_triggers_route_to_existing_orchestrator(self):
        for trigger in ("android", "daemon"):
            with self.subTest(trigger=trigger), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                transport = root / "transport"
                transport.mkdir()
                config = root / "config.json"
                config.write_text(
                    json.dumps(
                        {
                            "format": "trainlog-sync-orchestrator-config",
                            "version": 1,
                            "enabled": True,
                            "mode": "mtp",
                            "expected_peer_id": "peer_11111111-1111-4111-8111-111111111111",
                            "transport_root": str(transport),
                            "owned_root": str(root / "owned"),
                            "timeout_seconds": 30,
                        }
                    )
                )
                args = argparse.Namespace(
                    mtp_adapter=root / "adapter",
                    state=root / "trainlog.db.sync-run.json",
                    trigger=trigger,
                    orchestrator=root / "sync_orchestrator.py",
                    database=root / "trainlog.db",
                )
                commands = []

                def execute(command, **_kwargs):
                    commands.append(command)
                    if command[0] == str(args.mtp_adapter):
                        (transport / syncd.REQUEST_NAME).write_text(
                            json.dumps(
                                {
                                    "format": "trainlog-sync-request",
                                    "version": 1,
                                    "request_id": "sr_22222222-2222-4222-8222-222222222222",
                                    "requested_at": "2026-09-20T12:00:00Z",
                                }
                            )
                        )
                    return subprocess.CompletedProcess(command, 0, "PASS\n", "")

                with mock.patch.object(syncd.subprocess, "run", side_effect=execute), mock.patch.object(
                    syncd, "append_log"
                ):
                    self.assertEqual(0, syncd.run_full_generation(args, config))
                self.assertEqual(str(args.mtp_adapter), commands[0][0])
                self.assertEqual(str(args.orchestrator), commands[1][1])
                self.assertNotIn("trainlog-sync-once", " ".join(commands[1]))
                state = json.loads(args.state.read_text())
                self.assertEqual(trigger, state["trigger"])
                self.assertEqual("full_generation_v1", state["effective_mode"])

    def test_consumed_request_id_does_not_create_a_second_desktop_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            transport = root / "transport"
            transport.mkdir()
            request = "sr_22222222-2222-4222-8222-222222222222"
            config = root / "config.json"
            config.write_text(
                json.dumps(
                    {
                        "format": "trainlog-sync-orchestrator-config",
                        "version": 1,
                        "enabled": True,
                        "mode": "mtp",
                        "expected_peer_id": "peer_11111111-1111-4111-8111-111111111111",
                        "transport_root": str(transport),
                        "owned_root": str(root / "owned"),
                        "timeout_seconds": 30,
                    }
                )
            )
            state = root / "trainlog.db.sync-run.json"
            state.write_text(json.dumps({"request_id": request, "run_id": "sy_old"}))
            args = argparse.Namespace(
                mtp_adapter=root / "adapter",
                state=state,
                trigger="android",
                orchestrator=root / "sync_orchestrator.py",
                database=root / "trainlog.db",
            )
            commands = []

            def execute(command, **_kwargs):
                commands.append(command)
                (transport / syncd.REQUEST_NAME).write_text(
                    json.dumps(
                        {
                            "format": "trainlog-sync-request",
                            "version": 1,
                            "request_id": request,
                            "requested_at": "2026-09-20T12:00:00Z",
                        }
                    )
                )
                return subprocess.CompletedProcess(command, 0, "PASS\n", "")

            with mock.patch.object(syncd.subprocess, "run", side_effect=execute):
                self.assertEqual(3, syncd.run_full_generation(args, config))
            self.assertEqual(str(args.mtp_adapter), commands[0][0])
            self.assertEqual(1, len(commands))
            self.assertEqual("sy_old", json.loads(state.read_text())["run_id"])

    def test_transient_terminal_request_remains_eligible_after_reconnect(self):
        terminal_states = (
            ("failed", "device_unavailable"),
            ("failed", "sync_in_progress"),
            ("failed", "transport_timeout"),
            ("interrupted", "interrupted"),
        )
        for phase, error_code in terminal_states:
            with (
                self.subTest(phase=phase, error_code=error_code),
                tempfile.TemporaryDirectory() as directory,
            ):
                root = Path(directory)
                request = "sr_22222222-2222-4222-8222-222222222222"
                state = root / "trainlog.db.sync-run.json"
                state.write_text(
                    json.dumps(
                        {
                            "phase": phase,
                            "result": phase,
                            "error_code": error_code,
                            "request_id": request,
                            "seen_request_ids": [request],
                        }
                    )
                )
                self._write_request(
                    root / syncd.FULL_GENERATION_REQUEST_NAME,
                    request,
                    "2026-09-25T07:49:01Z",
                )

                _, seen = syncd.load_request_state(state)

                self.assertNotIn(request, seen)
                self.assertEqual(
                    (request, "full_generation", []),
                    syncd.select_request(root, seen),
                )

    def test_non_transient_terminal_request_remains_consumed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            request = "sr_22222222-2222-4222-8222-222222222222"
            state = root / "trainlog.db.sync-run.json"
            state.write_text(
                json.dumps(
                    {
                        "phase": "failed",
                        "result": "failed",
                        "error_code": "data_conflict",
                        "request_id": request,
                        "seen_request_ids": [request],
                    }
                )
            )
            self._write_request(
                root / syncd.FULL_GENERATION_REQUEST_NAME,
                request,
                "2026-09-25T07:49:01Z",
            )

            _, seen = syncd.load_request_state(state)

            self.assertIn(request, seen)
            self.assertIsNone(syncd.select_request(root, seen))

    def test_daemon_skips_mtp_probe_while_transport_lock_is_busy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            transport = root / "transport"
            transport.mkdir()
            config = root / "config.json"
            config.write_text(
                json.dumps(
                    {
                        "format": "trainlog-sync-orchestrator-config",
                        "version": 1,
                        "enabled": True,
                        "mode": "mtp",
                        "expected_peer_id": "peer_11111111-1111-4111-8111-111111111111",
                        "transport_root": str(transport),
                        "owned_root": str(root / "owned"),
                        "timeout_seconds": 30,
                    }
                )
            )
            args = argparse.Namespace(
                mtp_adapter=root / "adapter",
                state=root / "trainlog.db.sync-run.json",
                trigger="android",
                orchestrator=root / "sync_orchestrator.py",
                database=root / "trainlog.db",
            )

            def flock(_fd, operation):
                if operation == syncd.fcntl.LOCK_EX | syncd.fcntl.LOCK_NB:
                    raise BlockingIOError
                self.assertEqual(syncd.fcntl.LOCK_UN, operation)

            with mock.patch.object(syncd.fcntl, "flock", side_effect=flock), mock.patch.object(
                syncd.subprocess, "run"
            ) as run:
                self.assertEqual(3, syncd.run_full_generation(args, config))
            run.assert_not_called()

    def test_android_production_publishes_full_generation_before_optional_legacy(self):
        source = (ROOT / "android/app/src/main/java/com/labfytools/trainlog/ui/SyncScreen.kt").read_text()
        production = source.split("internal suspend fun publishBundleAndRequest", 1)[1].split(
            "internal suspend fun publishLegacyBundleAndRequest", 1
        )[0]
        self.assertIn("coordinator.run", production)
        self.assertNotIn("generation-opt-in", production)
        self.assertIn("exportMobileBundle", production)
        self.assertLess(
            production.index("publishFullGeneration"), production.index("exportMobileBundle")
        )
        self.assertGreater(
            production.index("publishLegacy"), production.index("exportMobileBundle")
        )

    def test_full_generation_wins_and_retires_older_legacy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            old = "sr_11111111-1111-4111-8111-111111111111"
            new = "sr_22222222-2222-4222-8222-222222222222"
            self._write_request(root / syncd.REQUEST_NAME, old, "2026-09-20T11:00:00Z")
            self._write_request(
                root / syncd.FULL_GENERATION_REQUEST_NAME, new, "2026-09-20T12:00:00Z"
            )
            selected = syncd.select_request(root, [])
            self.assertEqual((new, "full_generation", [old]), selected)
            self.assertIsNone(syncd.select_request(root, [old, new]))

    def test_dual_channel_same_intent_is_one_request(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            request = "sr_22222222-2222-4222-8222-222222222222"
            for name in (syncd.REQUEST_NAME, syncd.FULL_GENERATION_REQUEST_NAME):
                self._write_request(root / name, request, "2026-09-20T12:00:00Z")
            self.assertEqual(
                (request, "full_generation", []), syncd.select_request(root, [])
            )
            self.assertIsNone(syncd.select_request(root, [request]))

    def test_newer_legacy_remains_eligible_after_full_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            full = "sr_22222222-2222-4222-8222-222222222222"
            legacy = "sr_33333333-3333-4333-8333-333333333333"
            self._write_request(
                root / syncd.FULL_GENERATION_REQUEST_NAME, full, "2026-09-20T12:00:00Z"
            )
            self._write_request(root / syncd.REQUEST_NAME, legacy, "2026-09-20T13:00:00Z")
            self.assertEqual((full, "full_generation", []), syncd.select_request(root, []))
            self.assertEqual((legacy, "legacy", [full]), syncd.select_request(root, [full]))

    @staticmethod
    def _write_request(path, request_id, requested_at):
        path.write_text(json.dumps({
            "format": "trainlog-sync-request",
            "version": 1,
            "request_id": request_id,
            "requested_at": requested_at,
        }))


if __name__ == "__main__":
    unittest.main()
