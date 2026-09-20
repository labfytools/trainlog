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

    def test_android_production_trigger_no_longer_uses_opt_in_or_legacy_export(self):
        source = (ROOT / "android/app/src/main/java/com/labfytools/trainlog/ui/SyncScreen.kt").read_text()
        production = source.split("internal suspend fun publishBundleAndRequest", 1)[1].split(
            "internal suspend fun publishLegacyBundleAndRequest", 1
        )[0]
        self.assertIn("SyncGenerationForegroundCoordinator(repository).run", production)
        self.assertNotIn("generation-opt-in", production)
        self.assertIn("exportMobileBundle", production)
        self.assertIn("generation below", production)


if __name__ == "__main__":
    unittest.main()
