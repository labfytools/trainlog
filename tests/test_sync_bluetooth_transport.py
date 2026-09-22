#!/usr/bin/env python3
"""Regression coverage for the bounded Trainlog Bluetooth byte transport."""

from __future__ import annotations

import importlib.util
import io
import json
import os
import socket
import tempfile
import threading
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


adapter = load(
    "trainlog_generation_bt_adapter",
    ROOT / "tools/trainlog_generation_bt_adapter.py",
)
agent = load("trainlog_bt_agent", ROOT / "tools/trainlog_bt_agent.py")


class BluetoothTransportTest(unittest.TestCase):
    def test_archive_roundtrip_preserves_bounded_relative_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            destination = root / "destination"
            (source / "android-objects/generations/gen_test").mkdir(parents=True)
            (source / "android-peer-v1.json").write_text('{"peer":"ok"}')
            artifact = source / "android-objects/generations/gen_test/artifact.json"
            artifact.write_text('{"value":1}')
            raw = adapter.build_archive(source)
            adapter.extract_archive(raw, destination)
            self.assertEqual('{"peer":"ok"}', (destination / "android-peer-v1.json").read_text())
            self.assertEqual('{"value":1}', (destination / artifact.relative_to(source)).read_text())

    def test_archive_rejects_parent_traversal(self):
        payload = io.BytesIO()
        with zipfile.ZipFile(payload, "w") as archive:
            archive.writestr("../escape.json", "{}")
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(adapter.AdapterError):
                adapter.extract_archive(payload.getvalue(), Path(directory))

    def test_agent_hello_and_pull_are_peer_correlated(self):
        peer = "peer_11111111-1111-4111-8111-111111111111"
        state = agent.ConnectionState("AA:BB:CC:DD:EE:FF", peer)
        desktop, android = socket.socketpair()
        payload = b"bounded-archive"
        observed = []

        def phone():
            try:
                agent.send_frame(
                    android,
                    {
                        "type": "hello",
                        "protocol": agent.FILE_PROTOCOL,
                        "peer_id": peer,
                    },
                )
                hello, hello_payload = agent.recv_frame(android)
                observed.append((hello, hello_payload))
                request, request_payload = agent.recv_frame(android)
                observed.append((request, request_payload))
                agent.send_frame(
                    android,
                    {
                        "type": "result",
                        "request_id": request["request_id"],
                        "result": "ok",
                    },
                    payload,
                )
            finally:
                android.close()

        thread = threading.Thread(target=phone)
        thread.start()
        state.install(desktop)
        header, response = state.transact("pull", peer, b"")
        thread.join(timeout=5)
        self.assertFalse(thread.is_alive())
        self.assertEqual("ok", header["result"])
        self.assertEqual(payload, response)
        self.assertEqual("hello_ack", observed[0][0]["type"])
        self.assertEqual(b"", observed[0][1])
        self.assertEqual("pull", observed[1][0]["type"])
        self.assertEqual(b"", observed[1][1])
        state.close()

    def test_config_pins_expected_bluetooth_address(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            path.write_text(
                json.dumps(
                    {
                        "format": "trainlog-sync-orchestrator-config",
                        "version": 3,
                        "enabled": True,
                        "mode": "auto",
                        "expected_peer_id": "peer_11111111-1111-4111-8111-111111111111",
                        "transport_root": "/tmp/transport",
                        "owned_root": "/tmp/owned",
                        "timeout_seconds": 30,
                        "bluetooth_enabled": True,
                        "bluetooth_device_address": "aa:bb:cc:dd:ee:ff",
                    }
                )
            )
            value = agent.load_config(path)
            self.assertEqual("AA:BB:CC:DD:EE:FF", value["bluetooth_device_address"])


if __name__ == "__main__":
    unittest.main()
