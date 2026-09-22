#!/usr/bin/env python3
"""Persistent BlueZ RFCOMM bridge for Trainlog generation byte transport."""

from __future__ import annotations

import argparse
import dbus
import dbus.service
import dbus.mainloop.glib
import hashlib
import json
import os
import signal
import socket
import struct
import threading
import uuid
from pathlib import Path

from gi.repository import GLib

FRAME_FORMAT = "trainlog-bt-frame"
FRAME_VERSION = 1
FILE_PROTOCOL = "trainlog-bt-files-v1"
DEFAULT_UUID = "f0d1c0de-7a11-4f62-9b7c-545241494e4c"
DEFAULT_CHANNEL = 23
MAX_HEADER = 64 * 1024
MAX_PAYLOAD = 256 * 1024 * 1024
PROFILE_PATH = "/com/labfytools/trainlog/profile"


class BluetoothTransportError(RuntimeError):
    pass


def canonical_header(value: dict) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")


def recv_exact(stream: socket.socket, size: int) -> bytes:
    if size < 0 or size > MAX_PAYLOAD:
        raise BluetoothTransportError("invalid Bluetooth payload size")
    chunks = bytearray()
    while len(chunks) < size:
        part = stream.recv(min(64 * 1024, size - len(chunks)))
        if not part:
            raise BluetoothTransportError("Bluetooth connection closed")
        chunks.extend(part)
    return bytes(chunks)


def recv_frame(stream: socket.socket) -> tuple[dict, bytes]:
    raw_length = recv_exact(stream, 4)
    header_length = struct.unpack(">I", raw_length)[0]
    if header_length <= 0 or header_length > MAX_HEADER:
        raise BluetoothTransportError("invalid Bluetooth header size")
    try:
        header = json.loads(recv_exact(stream, header_length).decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError) as error:
        raise BluetoothTransportError("invalid Bluetooth header") from error
    if not isinstance(header, dict):
        raise BluetoothTransportError("invalid Bluetooth header")
    if header.get("format") != FRAME_FORMAT or header.get("version") != FRAME_VERSION:
        raise BluetoothTransportError("unsupported Bluetooth frame")
    payload_length = header.get("payload_size", 0)
    if type(payload_length) is not int or payload_length < 0 or payload_length > MAX_PAYLOAD:
        raise BluetoothTransportError("invalid Bluetooth payload size")
    payload = recv_exact(stream, payload_length) if payload_length else b""
    expected = header.get("payload_sha256")
    if payload:
        if not isinstance(expected, str) or len(expected) != 64:
            raise BluetoothTransportError("missing Bluetooth payload digest")
        if hashlib.sha256(payload).hexdigest() != expected:
            raise BluetoothTransportError("Bluetooth payload digest mismatch")
    elif expected not in (None, ""):
        raise BluetoothTransportError("unexpected Bluetooth payload digest")
    return header, payload


def send_frame(stream: socket.socket, header: dict, payload: bytes = b"") -> None:
    if len(payload) > MAX_PAYLOAD:
        raise BluetoothTransportError("Bluetooth payload exceeds bound")
    value = dict(header)
    value["format"] = FRAME_FORMAT
    value["version"] = FRAME_VERSION
    value["payload_size"] = len(payload)
    if payload:
        value["payload_sha256"] = hashlib.sha256(payload).hexdigest()
    raw = canonical_header(value)
    if len(raw) > MAX_HEADER:
        raise BluetoothTransportError("Bluetooth header exceeds bound")
    stream.sendall(struct.pack(">I", len(raw)))
    stream.sendall(raw)
    if payload:
        stream.sendall(payload)


def normalized_address(value: str) -> str:
    pieces = value.strip().upper().split(":")
    if len(pieces) != 6 or any(len(piece) != 2 for piece in pieces):
        raise BluetoothTransportError("invalid configured Bluetooth address")
    try:
        if any(int(piece, 16) > 255 for piece in pieces):
            raise ValueError
    except ValueError as error:
        raise BluetoothTransportError("invalid configured Bluetooth address") from error
    return ":".join(pieces)


def runtime_socket_path(config: dict) -> Path:
    configured = config.get("bluetooth_socket")
    if configured:
        path = Path(configured)
        if not path.is_absolute():
            raise BluetoothTransportError("bluetooth_socket must be absolute")
        return path
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if not runtime:
        raise BluetoothTransportError("XDG_RUNTIME_DIR is unavailable")
    return Path(runtime) / "trainlog-bt.sock"


def load_config(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("format") != "trainlog-sync-orchestrator-config":
        raise BluetoothTransportError("invalid Trainlog sync configuration")
    peer = value.get("expected_peer_id")
    if not isinstance(peer, str) or not peer.startswith("peer_"):
        raise BluetoothTransportError("invalid expected peer")
    address = value.get("bluetooth_device_address")
    if not isinstance(address, str):
        raise BluetoothTransportError("Bluetooth device address is not configured")
    value["bluetooth_device_address"] = normalized_address(address)
    return value


class ConnectionState:
    def __init__(self, expected_address: str, expected_peer: str):
        self.expected_address = expected_address
        self.expected_peer = expected_peer
        self._condition = threading.Condition()
        self._stream: socket.socket | None = None
        self._peer_id: str | None = None
        self._operation_lock = threading.Lock()

    def close(self) -> None:
        with self._condition:
            stream = self._stream
            self._stream = None
            self._peer_id = None
            self._condition.notify_all()
        if stream is not None:
            try:
                stream.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            stream.close()

    def install(self, stream: socket.socket) -> None:
        stream.settimeout(15)
        try:
            header, payload = recv_frame(stream)
            if payload or header.get("type") != "hello":
                raise BluetoothTransportError("Android Bluetooth hello is missing")
            if header.get("protocol") != FILE_PROTOCOL:
                raise BluetoothTransportError("Android Bluetooth protocol mismatch")
            peer = header.get("peer_id")
            if peer != self.expected_peer:
                raise BluetoothTransportError("Android Bluetooth peer identity mismatch")
            send_frame(
                stream,
                {
                    "type": "hello_ack",
                    "protocol": FILE_PROTOCOL,
                    "peer_id": peer,
                },
            )
            stream.settimeout(60)
        except Exception:
            stream.close()
            raise
        with self._condition:
            old = self._stream
            self._stream = stream
            self._peer_id = peer
            self._condition.notify_all()
        if old is not None and old is not stream:
            try:
                old.close()
            except OSError:
                pass

    def transact(self, operation: str, peer: str, payload: bytes) -> tuple[dict, bytes]:
        if peer != self.expected_peer:
            raise BluetoothTransportError("expected Android peer mismatch")
        if operation not in {"pull", "push"}:
            raise BluetoothTransportError("unsupported Bluetooth operation")
        with self._operation_lock:
            with self._condition:
                stream = self._stream
                connected_peer = self._peer_id
            if stream is None or connected_peer != peer:
                raise BluetoothTransportError("expected Android peer not connected")
            request_id = "bt_" + str(uuid.uuid4())
            try:
                send_frame(
                    stream,
                    {
                        "type": operation,
                        "request_id": request_id,
                        "protocol": FILE_PROTOCOL,
                        "peer_id": peer,
                    },
                    payload,
                )
                header, response = recv_frame(stream)
                if header.get("type") != "result" or header.get("request_id") != request_id:
                    raise BluetoothTransportError("uncorrelated Bluetooth response")
                if header.get("result") != "ok":
                    diagnostic = header.get("diagnostic")
                    raise BluetoothTransportError(
                        diagnostic if isinstance(diagnostic, str) and diagnostic
                        else "Android Bluetooth operation failed"
                    )
                if operation == "pull" and not response:
                    raise BluetoothTransportError("Android Bluetooth pull returned no archive")
                if operation == "push" and response:
                    raise BluetoothTransportError("Android Bluetooth push returned unexpected payload")
                return header, response
            except Exception:
                self.close()
                raise


class TrainlogProfile(dbus.service.Object):
    def __init__(self, bus: dbus.SystemBus, state: ConnectionState):
        super().__init__(bus, PROFILE_PATH)
        self.bus = bus
        self.state = state

    def _device_address(self, device: str) -> str:
        proxy = self.bus.get_object("org.bluez", device)
        properties = dbus.Interface(proxy, "org.freedesktop.DBus.Properties")
        return normalized_address(str(properties.Get("org.bluez.Device1", "Address")))

    @dbus.service.method("org.bluez.Profile1", in_signature="", out_signature="")
    def Release(self):
        self.state.close()

    @dbus.service.method("org.bluez.Profile1", in_signature="oha{sv}", out_signature="")
    def NewConnection(self, device, fd, properties):
        del properties
        address = self._device_address(str(device))
        raw_fd = fd.take()
        stream = socket.socket(fileno=raw_fd)
        if address != self.state.expected_address:
            stream.close()
            raise dbus.exceptions.DBusException(
                "org.bluez.Error.Rejected", "unauthorized Bluetooth device"
            )

        def establish() -> None:
            try:
                self.state.install(stream)
            except Exception:
                try:
                    stream.close()
                except OSError:
                    pass

        threading.Thread(target=establish, name="trainlog-bt-hello", daemon=True).start()

    @dbus.service.method("org.bluez.Profile1", in_signature="o", out_signature="")
    def RequestDisconnection(self, device):
        del device
        self.state.close()

    @dbus.service.method("org.bluez.Profile1", in_signature="", out_signature="")
    def Cancel(self):
        self.state.close()


class LocalAdapterServer:
    def __init__(self, path: Path, state: ConnectionState):
        self.path = path
        self.state = state
        self.stop = threading.Event()
        self.listener: socket.socket | None = None

    def serve(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        try:
            self.path.unlink()
        except FileNotFoundError:
            pass
        listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener = listener
        listener.bind(str(self.path))
        os.chmod(self.path, 0o600)
        listener.listen(4)
        listener.settimeout(1)
        while not self.stop.is_set():
            try:
                client, _ = listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            threading.Thread(
                target=self._client,
                args=(client,),
                name="trainlog-bt-local",
                daemon=True,
            ).start()
        try:
            listener.close()
        finally:
            try:
                self.path.unlink()
            except FileNotFoundError:
                pass

    def _client(self, client: socket.socket) -> None:
        with client:
            client.settimeout(60)
            try:
                header, payload = recv_frame(client)
                if header.get("type") != "adapter_request":
                    raise BluetoothTransportError("invalid local Bluetooth adapter request")
                operation = header.get("operation")
                peer = header.get("peer_id")
                if not isinstance(operation, str) or not isinstance(peer, str):
                    raise BluetoothTransportError("invalid local Bluetooth adapter request")
                _, response = self.state.transact(operation, peer, payload)
                send_frame(
                    client,
                    {
                        "type": "adapter_result",
                        "operation": operation,
                        "result": "ok",
                    },
                    response,
                )
            except Exception as error:
                try:
                    send_frame(
                        client,
                        {
                            "type": "adapter_result",
                            "result": "error",
                            "diagnostic": str(error)[:1024],
                        },
                    )
                except Exception:
                    pass

    def shutdown(self) -> None:
        self.stop.set()
        if self.listener is not None:
            try:
                self.listener.close()
            except OSError:
                pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--uuid", default=DEFAULT_UUID)
    parser.add_argument("--channel", type=int, default=DEFAULT_CHANNEL)
    args = parser.parse_args()

    config = load_config(args.config)
    expected_address = config["bluetooth_device_address"]
    expected_peer = config["expected_peer_id"]
    socket_path = runtime_socket_path(config)

    if args.channel < 1 or args.channel > 30:
        raise SystemExit("RFCOMM channel must be in 1..30")

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    state = ConnectionState(expected_address, expected_peer)
    profile = TrainlogProfile(bus, state)
    manager = dbus.Interface(
        bus.get_object("org.bluez", "/org/bluez"),
        "org.bluez.ProfileManager1",
    )
    manager.RegisterProfile(
        PROFILE_PATH,
        args.uuid,
        {
            "Name": "Trainlog Sync",
            "Role": "server",
            "Channel": dbus.UInt16(args.channel),
            "RequireAuthentication": dbus.Boolean(True),
            "RequireAuthorization": dbus.Boolean(False),
        },
    )

    local = LocalAdapterServer(socket_path, state)
    thread = threading.Thread(target=local.serve, name="trainlog-bt-unix", daemon=True)
    thread.start()
    loop = GLib.MainLoop()

    def stop(_signum, _frame):
        local.shutdown()
        state.close()
        loop.quit()

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        loop.run()
    finally:
        local.shutdown()
        state.close()
        try:
            manager.UnregisterProfile(PROFILE_PATH)
        except dbus.DBusException:
            pass
        thread.join(timeout=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
