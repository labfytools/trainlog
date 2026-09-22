#!/usr/bin/env python3
"""Bounded local adapter between Trainlog sync workers and trainlog-btd."""

from __future__ import annotations

import hashlib
import io
import json
import os
import socket
import stat
import struct
import sys
import tempfile
import zipfile
from pathlib import Path

FRAME_FORMAT = "trainlog-bt-frame"
FRAME_VERSION = 1
MAX_HEADER = 64 * 1024
MAX_PAYLOAD = 256 * 1024 * 1024
MAX_FILES = 128
MAX_FILE = 64 * 1024 * 1024


class AdapterError(RuntimeError):
    pass


def socket_path() -> Path:
    configured = os.environ.get("TRAINLOG_SYNC_BT_SOCKET")
    if configured:
        path = Path(configured)
        if not path.is_absolute():
            raise AdapterError("TRAINLOG_SYNC_BT_SOCKET must be absolute")
        return path
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if not runtime:
        raise AdapterError("XDG_RUNTIME_DIR is unavailable")
    return Path(runtime) / "trainlog-bt.sock"


def canonical_header(value: dict) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def recv_exact(stream: socket.socket, size: int) -> bytes:
    if size < 0 or size > MAX_PAYLOAD:
        raise AdapterError("invalid Bluetooth adapter payload size")
    value = bytearray()
    while len(value) < size:
        chunk = stream.recv(min(64 * 1024, size - len(value)))
        if not chunk:
            raise AdapterError("Bluetooth agent connection closed")
        value.extend(chunk)
    return bytes(value)


def recv_frame(stream: socket.socket) -> tuple[dict, bytes]:
    header_size = struct.unpack(">I", recv_exact(stream, 4))[0]
    if header_size <= 0 or header_size > MAX_HEADER:
        raise AdapterError("invalid Bluetooth adapter header size")
    try:
        header = json.loads(recv_exact(stream, header_size))
    except (UnicodeError, json.JSONDecodeError) as error:
        raise AdapterError("invalid Bluetooth adapter header") from error
    if (
        not isinstance(header, dict)
        or header.get("format") != FRAME_FORMAT
        or header.get("version") != FRAME_VERSION
    ):
        raise AdapterError("unsupported Bluetooth adapter frame")
    payload_size = header.get("payload_size", 0)
    if type(payload_size) is not int or payload_size < 0 or payload_size > MAX_PAYLOAD:
        raise AdapterError("invalid Bluetooth adapter payload size")
    payload = recv_exact(stream, payload_size) if payload_size else b""
    digest = header.get("payload_sha256")
    if payload:
        if not isinstance(digest, str) or hashlib.sha256(payload).hexdigest() != digest:
            raise AdapterError("Bluetooth adapter payload digest mismatch")
    elif digest not in (None, ""):
        raise AdapterError("unexpected Bluetooth adapter payload digest")
    return header, payload


def send_frame(stream: socket.socket, header: dict, payload: bytes = b"") -> None:
    if len(payload) > MAX_PAYLOAD:
        raise AdapterError("Bluetooth adapter payload exceeds bound")
    value = dict(header)
    value.update(
        {
            "format": FRAME_FORMAT,
            "version": FRAME_VERSION,
            "payload_size": len(payload),
        }
    )
    if payload:
        value["payload_sha256"] = hashlib.sha256(payload).hexdigest()
    raw = canonical_header(value)
    if len(raw) > MAX_HEADER:
        raise AdapterError("Bluetooth adapter header exceeds bound")
    stream.sendall(struct.pack(">I", len(raw)) + raw)
    if payload:
        stream.sendall(payload)


def safe_relative(name: str) -> Path:
    if not name or "\\" in name:
        raise AdapterError("invalid archive path")
    path = Path(name)
    if path.is_absolute() or any(piece in ("", ".", "..") for piece in path.parts):
        raise AdapterError("invalid archive path")
    return path


def build_archive(root: Path) -> bytes:
    if not root.is_dir():
        raise AdapterError("Bluetooth adapter root is unavailable")
    buffer = io.BytesIO()
    count = 0
    total = 0
    with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_STORED) as archive:
        for path in sorted(root.rglob("*")):
            if path.is_symlink():
                raise AdapterError("Bluetooth adapter refuses symbolic links")
            if not path.is_file():
                continue
            relative = path.relative_to(root)
            safe_relative(relative.as_posix())
            mode = path.stat().st_mode
            if not stat.S_ISREG(mode):
                raise AdapterError("Bluetooth adapter refuses non-regular files")
            size = path.stat().st_size
            if size > MAX_FILE:
                raise AdapterError("Bluetooth adapter file exceeds bound")
            count += 1
            total += size
            if count > MAX_FILES or total > MAX_PAYLOAD:
                raise AdapterError("Bluetooth adapter archive exceeds bound")
            archive.write(path, relative.as_posix())
    raw = buffer.getvalue()
    if len(raw) > MAX_PAYLOAD:
        raise AdapterError("Bluetooth adapter archive exceeds bound")
    return raw


def extract_archive(raw: bytes, root: Path) -> None:
    if not raw or len(raw) > MAX_PAYLOAD:
        raise AdapterError("invalid Bluetooth adapter archive")
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    seen: set[str] = set()
    total = 0
    count = 0
    with zipfile.ZipFile(io.BytesIO(raw), "r") as archive:
        for info in archive.infolist():
            if info.is_dir():
                continue
            relative = safe_relative(info.filename)
            if info.filename in seen:
                raise AdapterError("duplicate Bluetooth archive path")
            seen.add(info.filename)
            count += 1
            total += info.file_size
            if (
                count > MAX_FILES
                or info.file_size < 0
                or info.file_size > MAX_FILE
                or total > MAX_PAYLOAD
            ):
                raise AdapterError("Bluetooth adapter archive exceeds bound")
            destination = root / relative
            resolved_parent = destination.parent.resolve()
            if root.resolve() != resolved_parent and root.resolve() not in resolved_parent.parents:
                raise AdapterError("Bluetooth archive escapes transport root")
            destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
            descriptor, temporary_name = tempfile.mkstemp(
                prefix="." + destination.name + ".bt-",
                dir=destination.parent,
            )
            try:
                os.fchmod(descriptor, 0o600)
                with os.fdopen(descriptor, "wb") as output, archive.open(info, "r") as source:
                    copied = 0
                    while True:
                        chunk = source.read(64 * 1024)
                        if not chunk:
                            break
                        copied += len(chunk)
                        if copied > info.file_size:
                            raise AdapterError("Bluetooth archive size mismatch")
                        output.write(chunk)
                    if copied != info.file_size:
                        raise AdapterError("Bluetooth archive size mismatch")
                    output.flush()
                    os.fsync(output.fileno())
                os.replace(temporary_name, destination)
            finally:
                try:
                    os.unlink(temporary_name)
                except FileNotFoundError:
                    pass


def run(operation: str, peer: str, root: Path) -> int:
    if operation not in {"pull", "push"}:
        raise AdapterError("usage: trainlog-generation-bt-adapter <pull|push> PEER ROOT")
    if not peer.startswith("peer_"):
        raise AdapterError("invalid expected Android peer")
    payload = build_archive(root) if operation == "push" else b""
    stream = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    stream.settimeout(45)
    try:
        stream.connect(str(socket_path()))
        send_frame(
            stream,
            {
                "type": "adapter_request",
                "operation": operation,
                "peer_id": peer,
            },
            payload,
        )
        header, response = recv_frame(stream)
    except (FileNotFoundError, ConnectionRefusedError):
        print(
            "Bluetooth adapter failed status=6 diagnostic=expected Android peer not connected",
            file=sys.stderr,
        )
        return 2
    finally:
        stream.close()
    if header.get("type") != "adapter_result":
        raise AdapterError("invalid Bluetooth agent response")
    if header.get("result") != "ok":
        diagnostic = header.get("diagnostic")
        text = diagnostic if isinstance(diagnostic, str) and diagnostic else "Bluetooth transport failed"
        print(f"Bluetooth adapter failed status=6 diagnostic={text}", file=sys.stderr)
        return 2
    if operation == "pull":
        extract_archive(response, root)
    elif response:
        raise AdapterError("unexpected Bluetooth push response payload")
    return 0


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit("usage: trainlog-generation-bt-adapter <pull|push> PEER ROOT")
    try:
        return run(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
    except (AdapterError, OSError, zipfile.BadZipFile) as error:
        print(f"Bluetooth adapter failed status=6 diagnostic={str(error)[:1024]}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
