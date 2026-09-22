#!/usr/bin/env python3
"""Durable full-generation synchronization run coordinator.

The HTTP adapter starts this executable with fixed arguments.  Peer transport is
selected only by a trusted local configuration file; request JSON can never
choose commands, paths, databases, capabilities, or causal policy.
"""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import subprocess
import signal
import selectors
import re
import sys
import tempfile
import time
import ctypes
import shutil
from datetime import datetime, timezone
from pathlib import Path

MAX_CONFIG = 16 * 1024
MAX_REPORT = 64 * 1024
MAX_DIAGNOSTIC = 1024
PHASES = {
    "requested",
    "waiting_android_publication",
    "running",
    "local_import_committed",
    "published",
    "waiting_acknowledgement",
    "peer_consumed",
    "completed",
    "failed",
    "interrupted",
    "explicitly_degraded",
}


class OrchestratorInterrupted(RuntimeError):
    pass


_owned_process_group: int | None = None


def _forward_termination(signum: int, _frame: object) -> None:
    """Forward shutdown only to the exactly-owned helper process group."""
    if _owned_process_group is not None:
        try:
            os.killpg(_owned_process_group, signal.SIGTERM)
        except ProcessLookupError:
            pass
    raise OrchestratorInterrupted(f"orchestrator received signal {signum}")


def _child_setup() -> None:
    """Create an owned group and ask Linux to terminate it if this parent dies."""
    os.setsid()
    libc = ctypes.CDLL(None)
    if libc.prctl(1, signal.SIGTERM) != 0:  # PR_SET_PDEATHSIG
        raise OSError("cannot establish parent-death signal")


def canonical(value: object) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode()


def atomic_write(path: Path, value: dict) -> None:
    raw = canonical(value)
    if len(raw) > MAX_REPORT:
        raise RuntimeError("run report exceeds 64 KiB")
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temporary = tempfile.mkstemp(prefix=".sync-run-", dir=path.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "wb") as stream:
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass


def load_json(path: Path, limit: int) -> dict:
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    if len(raw) > limit:
        raise RuntimeError(f"{path.name} exceeds its bound")
    value = json.loads(raw.decode("utf-8", "strict"))
    if not isinstance(value, dict):
        raise RuntimeError(f"{path.name} must contain an object")
    return value


def update(state_path: Path, state: dict, phase: str, **fields: object) -> None:
    if phase not in PHASES:
        raise RuntimeError("invalid run phase")
    state.update(fields)
    state["phase"] = phase
    state["progress_revision"] = int(state.get("progress_revision", 0)) + 1
    state["updated_at"] = datetime.now(timezone.utc).isoformat()
    atomic_write(state_path, state)


def failure_code(error: BaseException) -> str:
    """Map bounded operational evidence to one stable browser-facing code."""
    diagnostic = str(error).lower()
    if "transport_timeout:" in diagnostic or "peer worker timed out" in diagnostic:
        return "transport_timeout"
    if "peer_capacity_exhausted:" in diagnostic:
        return "peer_capacity_exhausted"
    if "expected android peer not found" in diagnostic:
        return "device_unavailable"
    if "capacity exhausted" in diagnostic or "retained generation capacity" in diagnostic:
        return "capacity_exhausted"
    if "conflict" in diagnostic or "wrong consumer" in diagnostic:
        return "data_conflict"
    return "internal_error"


def validate_result(value: dict, run_id: str) -> dict:
    required = {
        "format",
        "version",
        "run_id",
        "producer_peer_id",
        "consumer_peer_id",
        "inbound_generation_id",
        "outbound_generation_id",
        "manifest_sha256",
        "result",
        "sessions_reconciled",
        "domains",
        "drafts",
        "ai_midpoint",
        "ai_post_sync",
    }
    if (
        set(value) != required
        or value.get("format") != "trainlog-sync-worker-report"
        or value.get("version") != 1
    ):
        raise RuntimeError("malformed peer worker report")
    if value.get("run_id") != run_id or value.get("result") != "completed":
        raise RuntimeError("peer worker did not complete the correlated run")
    for identity, prefix in (
        (value["producer_peer_id"], "peer_"),
        (value["consumer_peer_id"], "peer_"),
        (value["inbound_generation_id"], "gen_"),
        (value["outbound_generation_id"], "gen_"),
    ):
        if not isinstance(identity, str) or not re.fullmatch(
            re.escape(prefix)
            + r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
            identity,
        ):
            raise RuntimeError("invalid worker identity")
    if type(value["sessions_reconciled"]) is not int or value["sessions_reconciled"] < 0:
        raise RuntimeError("invalid sessions_reconciled")
    if not isinstance(value["domains"], dict) or not all(
        isinstance(k, str) and isinstance(v, bool) for k, v in value["domains"].items()
    ):
        raise RuntimeError("invalid domain coverage")
    if not isinstance(value["drafts"], list) or len(value["drafts"]) > 32:
        raise RuntimeError("invalid draft summary")
    return value


def bluetooth_adapter_path() -> Path:
    configured = os.environ.get("TRAINLOG_SYNC_BT_ADAPTER")
    if configured:
        return Path(configured)
    return Path(__file__).with_name("trainlog_generation_bt_adapter.py")


def drive_adapter_path() -> Path:
    return Path(__file__).with_name("sync_drive_transport.py")


def probe_local_adapter(
    adapter: Path,
    expected_peer: str,
    transport_root: Path,
    timeout: int,
    lock_name: str,
) -> bool:
    if not adapter.is_absolute() or not os.access(adapter, os.X_OK):
        return False
    lock_path = transport_root.parent / lock_name
    lock_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    lock_fd = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o600)
    probe_deadline = time.monotonic() + min(timeout, 30)
    try:
        while time.monotonic() < probe_deadline:
            try:
                fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                time.sleep(0.05)
        else:
            return False
        remaining = max(0.01, probe_deadline - time.monotonic())
        try:
            probe = subprocess.run(
                [str(adapter), "pull", expected_peer, str(transport_root)],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=remaining,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            return False
        return probe.returncode == 0
    finally:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_UN)
        finally:
            os.close(lock_fd)


def mirror_current_exchange(transport_root: Path, remote: str, timeout: int) -> None:
    """Mirror only current protocol objects; never traverse application data."""
    coordination = (
        "android-peer-v1.json",
        "android-generation-v1.json",
        "android-consumption-ack-v1.json",
        "android-generation-error-v1.json",
        "request-v1.json",
        "desktop-archive-acknowledgements-v1.json",
        "desktop-consumption-ack-v1.json",
        "desktop-generation-v1.json",
    )
    with tempfile.TemporaryDirectory(prefix="trainlog-drive-mirror-", dir=transport_root.parent) as raw:
        outbox = Path(raw)
        for name in coordination:
            source = transport_root / name
            if source.is_file() and not source.is_symlink():
                shutil.copy2(source, outbox / name)
        for name in ("android-generation-v1.json", "desktop-generation-v1.json"):
            reference = outbox / name
            if not reference.is_file():
                continue
            value = load_json(reference, MAX_REPORT)
            relative = value.get("relative_path")
            if not isinstance(relative, str):
                raise RuntimeError("invalid generation reference during Drive mirror")
            source = (transport_root / relative).resolve()
            if not source.is_relative_to(transport_root.resolve()) or not source.is_dir():
                raise RuntimeError("generation reference escapes transport root")
            destination = outbox / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(source, destination, copy_function=shutil.copy2)
        result = subprocess.run(
            [sys.executable, str(drive_adapter_path()), "push", remote, str(outbox)],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(
                result.stderr.decode("utf-8", "replace")[:MAX_DIAGNOSTIC].strip()
                or "Drive mirror failed"
            )


def read_worker(
    child: subprocess.Popen[bytes], state_path: Path, state: dict, run_id: str, timeout: int
) -> tuple[dict, bytes]:
    selector = selectors.DefaultSelector()
    assert child.stdout is not None and child.stderr is not None
    selector.register(child.stdout, selectors.EVENT_READ, "stdout")
    selector.register(child.stderr, selectors.EVENT_READ, "stderr")
    buffers = {"stdout": bytearray(), "stderr": bytearray()}
    line_buffer = bytearray()
    final: dict | None = None
    deadline = time.monotonic() + timeout
    allowed = [
        "waiting_android_publication",
        "running",
        "local_import_committed",
        "published",
        "waiting_acknowledgement",
        "peer_consumed",
    ]
    last_index = -1
    while selector.get_map():
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError("peer worker timed out")
        for key, _ in selector.select(min(remaining, 0.25)):
            chunk = os.read(key.fileobj.fileno(), 4096)
            if not chunk:
                selector.unregister(key.fileobj)
                continue
            target = buffers[key.data]
            if len(target) + len(chunk) > MAX_REPORT:
                raise RuntimeError(f"peer worker {key.data} exceeds 64 KiB")
            target.extend(chunk)
            if key.data == "stdout":
                line_buffer.extend(chunk)
                while b"\n" in line_buffer:
                    raw, _, rest = line_buffer.partition(b"\n")
                    line_buffer[:] = rest
                    if len(raw) > 8192:
                        raise RuntimeError("progress frame exceeds 8 KiB")
                    value = json.loads(raw.decode("utf-8", "strict"))
                    if value.get("format") == "trainlog-sync-progress":
                        if (
                            set(value)
                            - {
                                "format",
                                "version",
                                "run_id",
                                "phase",
                                "producer_peer_id",
                                "consumer_peer_id",
                                "inbound_generation_id",
                                "outbound_generation_id",
                                "manifest_sha256",
                            }
                            or value.get("version") != 1
                            or value.get("run_id") != run_id
                            or value.get("phase") not in allowed
                        ):
                            raise RuntimeError("invalid progress frame")
                        index = allowed.index(value["phase"])
                        if index < last_index:
                            raise RuntimeError("progress phase regressed")
                        last_index = index
                        update(
                            state_path,
                            state,
                            value["phase"],
                            **{
                                k: v
                                for k, v in value.items()
                                if k not in {"format", "version", "run_id", "phase"}
                            },
                        )
                    else:
                        final = value
    child.wait()
    if line_buffer.strip():
        raise RuntimeError("unterminated worker frame")
    if child.returncode != 0:
        detail = bytes(buffers["stderr"]).decode("utf-8", "replace").strip()[:MAX_DIAGNOSTIC]
        raise RuntimeError(f"peer worker exited {child.returncode}: {detail}")
    if final is None:
        raise RuntimeError("peer worker omitted final report")
    return validate_result(final, run_id), bytes(buffers["stderr"])


def run(args: argparse.Namespace) -> int:
    global _owned_process_group
    state_path, config_path = Path(args.state), Path(args.config)
    state = load_json(state_path, MAX_REPORT)
    config = load_json(config_path, MAX_CONFIG)
    if state.get("run_id") != args.run_id or state.get("request_id") != args.request_id:
        raise RuntimeError("admission identity mismatch")
    base_keys = {
        "format",
        "version",
        "enabled",
        "mode",
        "expected_peer_id",
        "transport_root",
        "owned_root",
        "timeout_seconds",
    }
    drive_keys = {"drive_enabled", "drive_remote"}
    bluetooth_keys = {"bluetooth_enabled", "bluetooth_device_address"}
    allowed_key_sets = (
        base_keys,
        base_keys | drive_keys,
        base_keys | bluetooth_keys,
        base_keys | drive_keys | bluetooth_keys,
    )
    if set(config) not in allowed_key_sets:
        raise RuntimeError("invalid trusted sync configuration")
    if (
        config["format"] != "trainlog-sync-orchestrator-config"
        or config["version"] not in (1, 2, 3)
        or config["enabled"] is not True
    ):
        raise RuntimeError("full-generation synchronization is disabled")
    if config["mode"] not in ("directory", "bt", "mtp", "drive", "auto"):
        raise RuntimeError("invalid transport mode")
    drive_enabled = config.get("drive_enabled", False)
    drive_remote = config.get("drive_remote", "")
    if type(drive_enabled) is not bool or (
        drive_enabled and (not isinstance(drive_remote, str) or not drive_remote)
    ):
        raise RuntimeError("invalid Drive configuration")
    bluetooth_enabled = config.get("bluetooth_enabled", False)
    bluetooth_address = config.get("bluetooth_device_address", "")
    if type(bluetooth_enabled) is not bool or (
        bluetooth_enabled
        and (
            config["version"] < 3
            or not isinstance(bluetooth_address, str)
            or re.fullmatch(r"[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5}", bluetooth_address)
            is None
        )
    ):
        raise RuntimeError("invalid Bluetooth configuration")
    if config["mode"] == "drive" and not drive_enabled:
        raise RuntimeError("Drive transport is not configured")
    if config["mode"] == "bt" and not bluetooth_enabled:
        raise RuntimeError("Bluetooth transport is not configured")
    for key in ("transport_root", "owned_root"):
        if not isinstance(config[key], str) or not Path(config[key]).is_absolute():
            raise RuntimeError("trusted paths must be absolute")
    if not isinstance(config["expected_peer_id"], str) or not re.fullmatch(
        r"peer_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
        config["expected_peer_id"],
    ):
        raise RuntimeError("invalid expected peer identity")
    timeout = config["timeout_seconds"]
    if type(timeout) is not int or timeout < 1 or timeout > 900:
        raise RuntimeError("invalid worker timeout")
    # CONTRACT: this is the same XDG data-directory lock used by the legacy
    # C engine, so Web, TUI and daemon admissions cannot overlap.
    lock_path = Path(args.database).parent / "sync.lock"
    lock_fd = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o600)
    try:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            update(
                state_path,
                state,
                "failed",
                result="conflict",
                error_code="sync_in_progress",
                diagnostic="Another synchronization owns the common lock",
                finished_at=datetime.now(timezone.utc).isoformat(),
            )
            return 4
        update(state_path, state, "running", result="running", diagnostic="")
        environment = os.environ.copy()
        environment.update(
            {
                "TRAINLOG_SYNC_RUN_ID": args.run_id,
                "TRAINLOG_SYNC_TRIGGER": state["trigger"],
                "TRAINLOG_SYNC_DATABASE": args.database,
            }
        )
        selected_mode = config["mode"]
        adapter = Path(
            os.environ.get(
                "TRAINLOG_SYNC_MTP_ADAPTER",
                str(Path(__file__).resolve().parents[1] / "build/tui/trainlog-generation-mtp-adapter"),
            )
        )
        bt_adapter = bluetooth_adapter_path()
        if selected_mode == "auto":
            transport_root = Path(config["transport_root"])
            selected_mode = ""
            if bluetooth_enabled and probe_local_adapter(
                bt_adapter,
                config["expected_peer_id"],
                transport_root,
                timeout,
                "bt.lock",
            ):
                selected_mode = "bt"
            elif probe_local_adapter(
                adapter,
                config["expected_peer_id"],
                transport_root,
                timeout,
                "mtp.lock",
            ):
                selected_mode = "mtp"
            elif drive_enabled:
                selected_mode = "drive"
            else:
                raise RuntimeError("expected Android peer not found")
        command = [
            sys.executable,
            str(Path(__file__).with_name("sync_peer_worker.py")),
            "--database",
            args.database,
            "--transport-root",
            config["transport_root"],
            "--owned-root",
            config["owned_root"],
            "--run-id",
            args.run_id,
            "--expected-peer",
            config["expected_peer_id"],
            "--timeout",
            str(timeout),
            "--mode",
            selected_mode,
        ]
        if selected_mode == "bt":
            if not bt_adapter.is_absolute() or not os.access(bt_adapter, os.X_OK):
                raise RuntimeError("fixed Bluetooth adapter executable is unavailable")
            command.extend(["--bt-adapter", str(bt_adapter)])
        elif selected_mode == "mtp":
            if not adapter.is_absolute() or not os.access(adapter, os.X_OK):
                raise RuntimeError("fixed MTP adapter executable is unavailable")
            command.extend(["--mtp-adapter", str(adapter)])
        elif selected_mode == "drive":
            command.extend(
                ["--drive-adapter", str(drive_adapter_path()), "--drive-remote", drive_remote]
            )
        child = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=environment,
            preexec_fn=_child_setup,
        )
        _owned_process_group = child.pid
        try:
            result, _ = read_worker(child, state_path, state, args.run_id, timeout)
        except Exception:
            if child.poll() is None:
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(5)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()
            raise
        finally:
            _owned_process_group = None
        drive_state = "disabled"
        drive_diagnostic = ""
        if drive_enabled:
            if selected_mode == "drive":
                drive_state = "success"
            else:
                try:
                    mirror_current_exchange(Path(config["transport_root"]), drive_remote, timeout)
                    drive_state = "mirrored"
                except Exception as error:
                    # CONTRACT: committed USB synchronization is never rolled
                    # back because its additive safety mirror is unavailable.
                    drive_state = "failed"
                    drive_diagnostic = str(error)[:MAX_DIAGNOSTIC]
        update(
            state_path,
            state,
            "completed",
            result="completed",
            diagnostic="",
            inbound_generation_id=result["inbound_generation_id"],
            outbound_generation_id=result["outbound_generation_id"],
            producer_peer_id=result["producer_peer_id"],
            consumer_peer_id=result["consumer_peer_id"],
            sessions_reconciled=result["sessions_reconciled"],
            domains=result["domains"],
            drafts=result["drafts"],
            manifest_sha256=result["manifest_sha256"],
            ai_midpoint=result["ai_midpoint"],
            ai_post_sync=result["ai_post_sync"],
            bluetooth_state="success" if selected_mode == "bt" else "unavailable",
            usb_state="success" if selected_mode == "mtp" else "unavailable",
            drive_state=drive_state,
            drive_diagnostic=drive_diagnostic,
            transport=selected_mode,
            finished_at=datetime.now(timezone.utc).isoformat(),
        )
        return 0
    finally:
        os.close(lock_fd)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", required=True)
    parser.add_argument("--state", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--request-id", required=True)
    args = parser.parse_args()
    signal.signal(signal.SIGTERM, _forward_termination)
    signal.signal(signal.SIGINT, _forward_termination)
    try:
        return run(args)
    except (
        OSError,
        ValueError,
        json.JSONDecodeError,
        RuntimeError,
        subprocess.TimeoutExpired,
    ) as error:
        try:
            path = Path(args.state)
            state = load_json(path, MAX_REPORT)
            phase = "interrupted" if isinstance(error, OrchestratorInterrupted) else "failed"
            update(
                path,
                state,
                phase,
                result=phase,
                error_code=("interrupted" if phase == "interrupted" else failure_code(error)),
                diagnostic=str(error)[:MAX_DIAGNOSTIC],
                finished_at=datetime.now(timezone.utc).isoformat(),
            )
        except Exception:
            pass
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
