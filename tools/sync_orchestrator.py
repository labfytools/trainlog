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
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

MAX_CONFIG = 16 * 1024
MAX_REPORT = 64 * 1024
MAX_DIAGNOSTIC = 1024
PHASES = {"requested", "waiting_android_publication", "running",
          "local_import_committed", "published", "waiting_acknowledgement",
          "peer_consumed", "completed", "failed", "interrupted", "explicitly_degraded"}


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode()


def atomic_write(path: Path, value: dict) -> None:
    raw = canonical(value)
    if len(raw) > MAX_REPORT:
        raise RuntimeError("run report exceeds 64 KiB")
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temporary = tempfile.mkstemp(prefix=".sync-run-", dir=path.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "wb") as stream:
            stream.write(raw); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try: os.fsync(directory)
        finally: os.close(directory)
    finally:
        try: os.unlink(temporary)
        except FileNotFoundError: pass


def load_json(path: Path, limit: int) -> dict:
    raw = path.read_bytes()
    if len(raw) > limit: raise RuntimeError(f"{path.name} exceeds its bound")
    value = json.loads(raw)
    if not isinstance(value, dict): raise RuntimeError(f"{path.name} must contain an object")
    return value


def update(state_path: Path, state: dict, phase: str, **fields: object) -> None:
    if phase not in PHASES: raise RuntimeError("invalid run phase")
    state.update(fields); state["phase"] = phase
    state["progress_revision"] = int(state.get("progress_revision", 0)) + 1
    state["updated_at"] = datetime.now(timezone.utc).isoformat()
    atomic_write(state_path, state)


def validate_result(value: dict, run_id: str) -> dict:
    required = {"format", "version", "run_id", "producer_peer_id", "consumer_peer_id",
                "inbound_generation_id", "outbound_generation_id", "manifest_sha256",
                "result", "sessions_reconciled", "domains", "drafts", "ai_midpoint", "ai_post_sync"}
    if set(value) != required or value.get("format") != "trainlog-sync-worker-report" or value.get("version") != 1:
        raise RuntimeError("malformed peer worker report")
    if value.get("run_id") != run_id or value.get("result") != "completed":
        raise RuntimeError("peer worker did not complete the correlated run")
    for identity, prefix in ((value["producer_peer_id"], "peer_"),
                             (value["consumer_peer_id"], "peer_"),
                             (value["inbound_generation_id"], "gen_"),
                             (value["outbound_generation_id"], "gen_")):
        if not isinstance(identity, str) or not identity.startswith(prefix) or len(identity) > 64:
            raise RuntimeError("invalid worker identity")
    if not isinstance(value["sessions_reconciled"], int) or value["sessions_reconciled"] < 0:
        raise RuntimeError("invalid sessions_reconciled")
    if not isinstance(value["domains"], dict) or not all(isinstance(k, str) and isinstance(v, bool) for k, v in value["domains"].items()):
        raise RuntimeError("invalid domain coverage")
    if not isinstance(value["drafts"], list) or len(value["drafts"]) > 32:
        raise RuntimeError("invalid draft summary")
    return value


def run(args: argparse.Namespace) -> int:
    state_path, config_path = Path(args.state), Path(args.config)
    state = load_json(state_path, MAX_REPORT)
    config = load_json(config_path, MAX_CONFIG)
    if state.get("run_id") != args.run_id or state.get("request_id") != args.request_id:
        raise RuntimeError("admission identity mismatch")
    if set(config) != {"format", "version", "enabled", "mode", "capabilities", "peer_command", "timeout_seconds"}:
        raise RuntimeError("invalid trusted sync configuration")
    if config["format"] != "trainlog-sync-orchestrator-config" or config["version"] != 1 or config["enabled"] is not True:
        raise RuntimeError("full-generation synchronization is disabled")
    required_caps = {"generation-manifest-v1", "generation-ack-v1", "causal-delete-v1", "mobile-history-v4", "execution-draft-v1"}
    caps = config["capabilities"]
    if not isinstance(caps, list) or not required_caps.issubset(set(caps)):
        update(state_path, state, "explicitly_degraded", result="incompatible",
               missing_capabilities=sorted(required_caps - set(caps if isinstance(caps, list) else [])),
               diagnostic="Peer lacks mandatory full-generation capabilities", finished_at=datetime.now(timezone.utc).isoformat())
        return 3
    command = config["peer_command"]
    if not isinstance(command, list) or not command or len(command) > 16 or not all(isinstance(part, str) and 0 < len(part) <= 4096 for part in command):
        raise RuntimeError("invalid trusted peer command")
    timeout = config["timeout_seconds"]
    if not isinstance(timeout, int) or timeout < 1 or timeout > 900:
        raise RuntimeError("invalid worker timeout")
    # CONTRACT: this is the same XDG data-directory lock used by the legacy
    # C engine, so Web, TUI and daemon admissions cannot overlap.
    lock_path = Path(args.database).parent / "sync.lock"
    lock_fd = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o600)
    try:
        try: fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            update(state_path, state, "failed", result="conflict", diagnostic="Another synchronization owns the common lock", finished_at=datetime.now(timezone.utc).isoformat())
            return 4
        update(state_path, state, "waiting_android_publication", result="running", diagnostic="")
        update(state_path, state, "running")
        environment = os.environ.copy()
        environment.update({"TRAINLOG_SYNC_RUN_ID": args.run_id,
                            "TRAINLOG_SYNC_TRIGGER": state["trigger"],
                            "TRAINLOG_SYNC_DATABASE": args.database})
        child = subprocess.Popen(command, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment,
            start_new_session=True)
        try:
            stdout, stderr = child.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGTERM)
            try: stdout, stderr = child.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL); stdout, stderr = child.communicate()
            raise RuntimeError("peer worker timed out")
        if len(stdout) > MAX_REPORT or len(stderr) > MAX_REPORT:
            raise RuntimeError("peer worker output exceeds 64 KiB")
        if child.returncode != 0:
            detail = stderr.decode("utf-8", "replace").strip()[:MAX_DIAGNOSTIC]
            raise RuntimeError(f"peer worker exited {child.returncode}: {detail}")
        result = validate_result(json.loads(stdout), args.run_id)
        update(state_path, state, "local_import_committed",
               inbound_generation_id=result["inbound_generation_id"],
               producer_peer_id=result["producer_peer_id"],
               consumer_peer_id=result["consumer_peer_id"],
               sessions_reconciled=result["sessions_reconciled"], domains=result["domains"], drafts=result["drafts"])
        update(state_path, state, "published", outbound_generation_id=result["outbound_generation_id"])
        update(state_path, state, "waiting_acknowledgement")
        update(state_path, state, "peer_consumed", manifest_sha256=result["manifest_sha256"])
        update(state_path, state, "completed", result="completed", diagnostic="",
               ai_midpoint=result["ai_midpoint"], ai_post_sync=result["ai_post_sync"],
               finished_at=datetime.now(timezone.utc).isoformat())
        return 0
    finally:
        os.close(lock_fd)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", required=True); parser.add_argument("--state", required=True)
    parser.add_argument("--config", required=True); parser.add_argument("--run-id", required=True)
    parser.add_argument("--request-id", required=True)
    args = parser.parse_args()
    try: return run(args)
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError, subprocess.TimeoutExpired) as error:
        try:
            path = Path(args.state); state = load_json(path, MAX_REPORT)
            update(path, state, "failed", result="failed", diagnostic=str(error)[:MAX_DIAGNOSTIC],
                   finished_at=datetime.now(timezone.utc).isoformat())
        except Exception: pass
        return 2


if __name__ == "__main__": raise SystemExit(main())
