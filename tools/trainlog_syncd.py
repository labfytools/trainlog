#!/usr/bin/env python3
"""Small user-session daemon for Android-triggered Trainlog sync requests."""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path


STOP = False
REQUEST_NAME = "trainlog-sync-request-v1.json"
FULL_GENERATION_REQUEST_NAME = "trainlog-sync-full-generation-request-v1.json"
SEEN_REQUEST_LIMIT = 64
REQUEST_ID = re.compile(r"sr_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")


def request_stop(
    _signum: int,
    _frame: object,
) -> None:
    global STOP
    STOP = True


def append_log(
    text: str,
) -> None:
    state_home = Path.home() / ".local" / "state" / "trainlog"
    state_home.mkdir(
        parents=True,
        exist_ok=True,
    )

    with (
        state_home / "syncd.log"
    ).open(
        "a",
        encoding="utf-8",
    ) as handle:
        handle.write(
            time.strftime(
                "%Y-%m-%d %H:%M:%S "
            )
        )

        handle.write(text.rstrip())
        handle.write("\n")


def default_database() -> Path:
    data_home = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
    return data_home / "trainlog/trainlog.db"


def atomic_json(path: Path, value: dict) -> None:
    raw = (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    descriptor, temporary = tempfile.mkstemp(prefix=".sync-run-", dir=path.parent)
    try:
        os.fchmod(descriptor, 0o600)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass


def load_generation_config(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if (
        not isinstance(value, dict)
        or value.get("format") != "trainlog-sync-orchestrator-config"
        or value.get("enabled") is not True
        or value.get("mode") not in ("bt", "mtp", "auto")
    ):
        raise RuntimeError("trusted full-generation transport configuration is invalid")
    return value


def request(path: Path) -> tuple[str, datetime] | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return None
    if not isinstance(value, dict):
        return None
    candidate = value.get("request_id")
    if (
        set(value) == {"format", "version", "request_id", "requested_at"}
        and value.get("format") == "trainlog-sync-request"
        and type(value.get("version")) is int
        and value.get("version") == 1
        and isinstance(candidate, str)
        and REQUEST_ID.fullmatch(candidate)
    ):
        requested_at = value.get("requested_at")
        if not isinstance(requested_at, str):
            return None
        try:
            timestamp = datetime.fromisoformat(requested_at.replace("Z", "+00:00"))
        except ValueError:
            return None
        if timestamp.tzinfo is None:
            return None
        return candidate, timestamp
    return None


def load_request_state(path: Path) -> tuple[dict, list[str]]:
    try:
        state = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return {}, []
    if not isinstance(state, dict):
        return {}, []
    seen = [item for item in state.get("seen_request_ids", [])
            if isinstance(item, str) and REQUEST_ID.fullmatch(item)]
    historical = state.get("request_id")
    if isinstance(historical, str) and REQUEST_ID.fullmatch(historical) and historical not in seen:
        seen.append(historical)
    return state, seen[-SEEN_REQUEST_LIMIT:]


def select_request(root: Path, seen: list[str]) -> tuple[str, str, list[str]] | None:
    """Select one unseen intent while preventing an older cross-channel replay.

    WHY: modern Android may publish the same intent on full-generation and
    legacy channels, while stale legacy files remain durable on MTP. CONTRACT:
    full-generation wins and an older alternate intent is retired. INVARIANT:
    one request_id starts at most one daemon conversation.
    """
    full = request(root / FULL_GENERATION_REQUEST_NAME)
    legacy = request(root / REQUEST_NAME)
    unseen_full = full if full is not None and full[0] not in seen else None
    unseen_legacy = legacy if legacy is not None and legacy[0] not in seen else None
    if unseen_full is not None:
        selected_id, selected_at = unseen_full
        retired = list(seen)
        if (
            unseen_legacy is not None
            and unseen_legacy[0] != selected_id
            and unseen_legacy[1] <= selected_at
        ):
            retired.append(unseen_legacy[0])
        return selected_id, "full_generation", retired
    if unseen_legacy is not None:
        return unseen_legacy[0], "legacy", list(seen)
    return None


def pull_transport(
    adapter: Path,
    lock_name: str,
    expected_peer: str,
    root: Path,
) -> bool:
    lock_path = root.parent / lock_name
    lock_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    lock_fd = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o600)
    try:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return False
        try:
            pull = subprocess.run(
                [str(adapter), "pull", expected_peer, str(root)],
                capture_output=True,
                text=True,
                check=False,
            )
        except OSError:
            return False
        return pull.returncode == 0
    finally:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_UN)
        finally:
            os.close(lock_fd)


def run_full_generation(args: argparse.Namespace, config_path: Path) -> int:
    config = load_generation_config(config_path)
    root = Path(config["transport_root"])
    expected_peer = config["expected_peer_id"]
    pulled = False
    if config.get("bluetooth_enabled") is True and args.bt_adapter is not None:
        pulled = pull_transport(args.bt_adapter, "bt.lock", expected_peer, root)
    if not pulled and config.get("mode") in ("mtp", "auto"):
        pulled = pull_transport(args.mtp_adapter, "mtp.lock", expected_peer, root)
    if not pulled:
        return 3
    _, seen = load_request_state(args.state)
    selected = select_request(root, seen)
    if selected is None:
        return 3
    correlated_request, request_channel, seen = selected
    seen.append(correlated_request)
    seen = list(dict.fromkeys(seen))[-SEEN_REQUEST_LIMIT:]
    run_id = "sy_" + str(uuid.uuid4())
    now = datetime.now(timezone.utc).isoformat()
    atomic_json(
        args.state,
        {
            "format": "trainlog-sync-run-report",
            "version": 1,
            "run_id": run_id,
            "request_id": correlated_request,
            "request_channel": request_channel,
            "seen_request_ids": seen,
            "trigger": args.trigger,
            "requested_mode": "full_generation_v1",
            "effective_mode": "full_generation_v1",
            "phase": "requested",
            "progress_revision": 1,
            "result": "running",
            "started_at": now,
            "updated_at": now,
        },
    )
    result = subprocess.run(
        [
            sys.executable,
            str(args.orchestrator),
            "--database",
            str(args.database),
            "--state",
            str(args.state),
            "--config",
            str(config_path),
            "--run-id",
            run_id,
            "--request-id",
            correlated_request,
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    append_log(result.stdout if result.returncode == 0 else result.stderr or result.stdout)
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--sync-once",
        required=True,
        type=Path,
    )

    parser.add_argument("--trigger", choices=("android", "daemon"), default="android")
    parser.add_argument(
        "--generation-config",
        type=Path,
        default=os.environ.get("TRAINLOG_SYNC_GENERATION_CONFIG"),
    )
    parser.add_argument(
        "--orchestrator",
        type=Path,
        default=Path(__file__).with_name("sync_orchestrator.py"),
    )
    parser.add_argument("--database", type=Path, default=default_database())
    parser.add_argument("--state", type=Path)
    parser.add_argument(
        "--bt-adapter",
        type=Path,
        default=Path(os.environ.get("TRAINLOG_SYNC_BT_ADAPTER", "")),
    )
    parser.add_argument(
        "--mtp-adapter",
        type=Path,
        default=Path(os.environ.get("TRAINLOG_SYNC_MTP_ADAPTER", "")),
    )

    parser.add_argument(
        "--interval",
        type=float,
        default=3.0,
    )

    args = parser.parse_args()

    if args.state is None:
        # The Web adapter owns .sync-run.json. The daemon keeps independent
        # durable request replay evidence while sharing the database lock.
        args.state = Path(str(args.database) + ".syncd-run.json")

    if not args.sync_once.exists():
        raise SystemExit(
            f"sync executable missing: {args.sync_once}"
        )

    if args.generation_config is not None:
        config = load_generation_config(args.generation_config)
        if not args.orchestrator.is_file():
            raise SystemExit("full-generation runtime is incomplete")
        if config.get("mode") in ("mtp", "auto") and not args.mtp_adapter.is_file():
            raise SystemExit("full-generation MTP runtime is incomplete")
        if config.get("bluetooth_enabled") is True and not args.bt_adapter.is_file():
            raise SystemExit("full-generation Bluetooth runtime is incomplete")

    signal.signal(
        signal.SIGTERM,
        request_stop,
    )

    signal.signal(
        signal.SIGINT,
        request_stop,
    )

    append_log(
        "trainlog-syncd started"
    )

    while not STOP:
        if args.generation_config is not None:
            code = run_full_generation(args, args.generation_config)
            result = subprocess.CompletedProcess([], code, "", "")
        else:
            result = subprocess.run(
                [
                    str(args.sync_once),
                    "--request-only",
                    "--trigger",
                    args.trigger,
                ],
                capture_output=True,
                text=True,
                check=False,
            )

        if result.returncode == 0:
            append_log(
                result.stdout
            )
        elif result.returncode not in (
            3,
        ):
            detail = (
                result.stderr.strip()
                or result.stdout.strip()
                or f"exit={result.returncode}"
            )

            append_log(
                detail
            )

        deadline = (
            time.monotonic()
            + max(
                args.interval,
                1.0,
            )
        )

        while (
            not STOP
            and time.monotonic()
            < deadline
        ):
            time.sleep(0.2)

    append_log(
        "trainlog-syncd stopped"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
