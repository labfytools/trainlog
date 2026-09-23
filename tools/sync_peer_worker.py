#!/usr/bin/env python3
"""Production full-generation peer conversation over a transport directory.

The directory is the object-level seam implemented by the MTP adapter in a
deployed bundle and directly supplied by isolated tests. Business documents are
always produced and consumed by the existing generation services.
"""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from contextlib import closing, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sync_generation_exchange as generation

CAPS = {
    "generation-manifest-v1",
    "generation-ack-v1",
    "causal-delete-v1",
    "mobile-history-v4",
    "execution-draft-v1",
    "generation-archive-v1",
    "session-preparations-v2",
}

ACK_RECOVERY_CAP = "generation-ack-recovery-v1"

MTP_OPERATION_TIMEOUT_SECONDS = 30.0
MTP_POLL_INTERVAL_SECONDS = 3.0


class ConversationDeadlineExpired(RuntimeError):
    """Internal signal that the protocol deadline, not the transport, expired."""


class MtpPullPump:
    """Throttle complete MTP refreshes while local correlation is polled."""

    def __init__(self, pull, interval=MTP_POLL_INTERVAL_SECONDS, clock=time.monotonic):
        self._pull = pull
        self._interval = interval
        self._clock = clock
        self._last_pull = clock()

    def __call__(self, force: bool = False) -> None:
        now = self._clock()
        if not force and now - self._last_pull < self._interval:
            return
        self._pull()
        self._last_pull = self._clock()


def emit(run_id: str, phase: str, **values: object) -> None:
    value = {
        "format": "trainlog-sync-progress",
        "version": 1,
        "run_id": run_id,
        "phase": phase,
    } | values
    print(json.dumps(value, sort_keys=True, separators=(",", ":")), flush=True)


def bounded(path: Path, limit: int = 65536) -> bytes:
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    if len(raw) > limit:
        raise RuntimeError(f"{path.name} exceeds bound")
    return raw


def publish_json(path: Path, value: dict) -> None:
    """Publish a coordination object only after all bytes are durable."""
    raw = generation.canonical(value)
    if len(raw) > 65536:
        raise RuntimeError(f"{path.name} exceeds bound")
    temporary = path.with_name("." + path.name + ".tmp-" + str(os.getpid()))
    with temporary.open("wb") as stream:
        stream.write(raw)
        stream.flush()
        os.fsync(stream.fileno())
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)


def recovery_acknowledgements(db, producer_peer_id: str, consumer_peer_id: str) -> list[dict]:
    """Load bounded terminal ACK evidence for interrupted peer conversations."""
    rows = db.execute(
        "SELECT ack_json FROM sync_consumed_generations WHERE producer_peer_id=? "
        "AND consumer_peer_id=? AND result IN('consumed','rejected') "
        "ORDER BY consumed_at DESC,generation_id DESC LIMIT 32",
        (producer_peer_id, consumer_peer_id),
    ).fetchall()
    acknowledgements = [json.loads(row[0]) for row in rows]
    # WHY: the bounded envelope must prioritize the newest interrupted
    # conversations. Older acknowledged generations are normally already
    # archived; selecting them first can starve the exact ACKs needed to free
    # active producer capacity.
    # ACKs created before rejection diagnostics were normalized may contain a
    # solidus that Android's platform JSON canonicalizer hashes differently.
    # They remain durable evidence but cannot close a peer row, so do not let
    # one legacy object block recovery of later compatible conversations.
    return [
        value for value in acknowledgements
        if value.get("result") != "rejected" or "/" not in value.get("diagnostic", "")
    ]


def recovery_acknowledgement_envelope(
    raw: bytes,
    run_id: str,
    android_peer_id: str,
    desktop_peer_id: str,
) -> list[dict]:
    """Validate the Android consumer's bounded ACK recovery envelope."""
    value = generation.strict_json(raw, 65536)
    expected = {
        "format",
        "version",
        "run_id",
        "android_peer_id",
        "desktop_peer_id",
        "acknowledgements",
    }
    if (
        not isinstance(value, dict)
        or set(value) != expected
        or value.get("format") != "trainlog-sync-archive-acknowledgements"
        or type(value.get("version")) is not int
        or value.get("version") != 1
        or value.get("run_id") != run_id
        or value.get("android_peer_id") != android_peer_id
        or value.get("desktop_peer_id") != desktop_peer_id
    ):
        raise RuntimeError("Android archive acknowledgement envelope is not correlated")
    acknowledgements = value.get("acknowledgements")
    if not isinstance(acknowledgements, list) or len(acknowledgements) > 32:
        raise RuntimeError("invalid Android archive acknowledgement count")
    if not all(isinstance(ack, dict) for ack in acknowledgements):
        raise RuntimeError("invalid Android archive acknowledgement")
    return acknowledgements


def reconcile_recovery_acknowledgements(
    database: Path,
    raw: bytes,
    run_id: str,
    android_peer_id: str,
    desktop_peer_id: str,
) -> int:
    """Repair producer state only from exact ACK evidence retained by Android."""
    acknowledgements = recovery_acknowledgement_envelope(
        raw,
        run_id,
        android_peer_id,
        desktop_peer_id,
    )
    reconciled = 0
    for acknowledgement in acknowledgements:
        result = generation.accept_ack_bytes(
            database,
            generation.canonical(acknowledgement),
        )
        if result not in ("acknowledged", "rejected", "unchanged"):
            raise RuntimeError("unexpected recovered ACK result")
        reconciled += 1
    return reconciled


def run_adapter(
    adapter: Path,
    operation: str,
    peer: str,
    root: Path,
    deadline: float,
    allow_missing_peer: bool = False,
    clock=time.monotonic,
    transport_name: str = "MTP",
    lock_name: str = "mtp.lock",
) -> bool:
    lock_path = root.parent / lock_name
    lock_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    lock_fd = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o600)
    try:
        while True:
            remaining = deadline - clock()
            if remaining <= 0:
                raise ConversationDeadlineExpired()
            try:
                fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                time.sleep(min(0.05, remaining))
        remaining = deadline - clock()
        if remaining <= 0:
            raise ConversationDeadlineExpired()
        timeout = min(remaining, MTP_OPERATION_TIMEOUT_SECONDS)
        deadline_limited = remaining < MTP_OPERATION_TIMEOUT_SECONDS
        try:
            result = subprocess.run(
                [str(adapter), operation, peer, str(root)],
                stdin=subprocess.DEVNULL,
                capture_output=True,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            if deadline_limited:
                raise ConversationDeadlineExpired() from error
            # CONTRACT: an adapter timeout is an ambiguous transport failure. It
            # never implies rollback, replay, or permission to retry a mutation.
            raise RuntimeError(
                f"transport_timeout: {transport_name} {operation} did not finish within "
                f"{MTP_OPERATION_TIMEOUT_SECONDS:g} seconds"
            ) from error
    finally:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_UN)
        finally:
            os.close(lock_fd)
    if result.returncode != 0:
        diagnostic = (
            result.stderr.decode("utf-8", "replace")[:1024]
            or f"{transport_name} adapter failed"
        ).strip()
        missing_peer = diagnostic in {
            "double failed 6 expected Android peer not found",
            "MTP adapter failed status=6 diagnostic=expected Android peer not found",
            "Bluetooth adapter failed status=6 diagnostic=expected Android peer not connected",
        }
        if allow_missing_peer and missing_peer:
            return False
        raise RuntimeError(diagnostic)
    return True


def push_adapter(
    adapter: Path,
    peer: str,
    root: Path,
    relative_paths: list[str],
    deadline: float,
    transport_name: str = "MTP",
    lock_name: str = "mtp.lock",
) -> None:
    """Publish only the phase-owned bounded MTP objects.

    WHY: the durable transport root also retains inbound objects, legacy files,
    and staging evidence; recursively uploading that root makes each exchange
    grow with history and can time out before Android sees the request.
    CONTRACT: callers name fixed relative files/directories already durably
    published below ``root``. The adapter receives a private disposable view.
    INVARIANT: no retained generation, ACK, tombstone, or source object is
    deleted or modified while constructing the view.
    """
    with tempfile.TemporaryDirectory(prefix="trainlog-mtp-outbox-", dir=root.parent) as directory:
        outbox = Path(directory)
        os.chmod(outbox, 0o700)
        for relative in relative_paths:
            source = root / relative
            destination = outbox / relative
            if (
                source.is_symlink()
                or not source.exists()
                or not source.resolve().is_relative_to(root.resolve())
            ):
                raise RuntimeError(f"invalid MTP outbox source: {relative}")
            if source.is_dir() and any(path.is_symlink() for path in source.rglob("*")):
                raise RuntimeError(f"MTP outbox source contains a symbolic link: {relative}")
            destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
            if source.is_dir():
                shutil.copytree(source, destination, copy_function=shutil.copy2)
            elif source.is_file():
                shutil.copy2(source, destination)
            else:
                raise RuntimeError(f"invalid MTP outbox object: {relative}")
        run_adapter(
            adapter,
            "push",
            peer,
            outbox,
            deadline,
            transport_name=transport_name,
            lock_name=lock_name,
        )


def push_desktop_generation(
    adapter: Path,
    peer: str,
    root: Path,
    relative_path: str,
    deadline: float,
    transport_name: str = "MTP",
    lock_name: str = "mtp.lock",
) -> None:
    """Commit immutable bytes before making their correlated reference visible."""
    push_adapter(
        adapter,
        peer,
        root,
        [relative_path],
        deadline,
        transport_name=transport_name,
        lock_name=lock_name,
    )
    push_adapter(
        adapter,
        peer,
        root,
        [
            "request-v1.json",
            "desktop-archive-acknowledgements-v1.json",
            "desktop-consumption-ack-v1.json",
            "desktop-generation-v1.json",
        ],
        deadline,
        transport_name=transport_name,
        lock_name=lock_name,
    )


def run_drive_adapter(
    adapter: Path,
    operation: str,
    remote: str,
    root: Path,
    deadline: float,
) -> None:
    """Run the fixed Drive byte adapter without exposing configuration to HTTP."""
    remaining = max(1, int(deadline - time.monotonic()))
    try:
        result = subprocess.run(
            [sys.executable, str(adapter), operation, remote, str(root)],
            stdin=subprocess.DEVNULL,
            capture_output=True,
            # CONTRACT: the trusted configuration already bounds the complete
            # conversation to at most 900 seconds.  A full verified Drive pull
            # may legitimately need more than 60 seconds because every object
            # is downloaded and hashed before the generation becomes visible.
            timeout=remaining,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("transport_timeout: Drive adapter timed out") from error
    if result.returncode != 0:
        raise RuntimeError(
            result.stderr.decode("utf-8", "replace")[:1024].strip()
            or "Drive adapter failed"
        )


def push_drive(
    adapter: Path,
    remote: str,
    root: Path,
    relative_paths: list[str],
    deadline: float,
) -> None:
    """Publish a phase-owned outbox through the shared Drive byte adapter."""
    with tempfile.TemporaryDirectory(prefix="trainlog-drive-outbox-", dir=root.parent) as directory:
        outbox = Path(directory)
        os.chmod(outbox, 0o700)
        for relative in relative_paths:
            source = root / relative
            destination = outbox / relative
            if source.is_symlink() or not source.exists() or not source.resolve().is_relative_to(root.resolve()):
                raise RuntimeError(f"invalid Drive outbox source: {relative}")
            destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
            if source.is_dir():
                if any(path.is_symlink() for path in source.rglob("*")):
                    raise RuntimeError(f"Drive outbox contains a symbolic link: {relative}")
                shutil.copytree(source, destination, copy_function=shutil.copy2)
            else:
                shutil.copy2(source, destination)
        run_drive_adapter(adapter, "push", remote, outbox, deadline)


def wait_file(path: Path, deadline: float, pump=None, clock=time.monotonic, sleeper=time.sleep) -> None:
    while not path.is_file():
        if clock() >= deadline:
            raise RuntimeError(f"timeout waiting for {path.name}")
        if pump is not None:
            try:
                pump()
            except ConversationDeadlineExpired as error:
                raise RuntimeError(f"timeout waiting for {path.name}") from error
        sleeper(0.05)


def wait_json(
    path: Path,
    deadline: float,
    predicate,
    pump=None,
    clock=time.monotonic,
    sleeper=time.sleep,
) -> dict:
    while True:
        wait_file(path, deadline, pump, clock, sleeper)
        try:
            value = json.loads(bounded(path))
        except (OSError, json.JSONDecodeError):
            value = None
        if isinstance(value, dict) and predicate(value):
            return value
        if clock() >= deadline:
            raise RuntimeError(f"timeout waiting for correlated {path.name}")
        if pump is not None:
            try:
                pump()
            except ConversationDeadlineExpired as error:
                raise RuntimeError(
                    f"timeout waiting for correlated {path.name}"
                ) from error
        sleeper(0.05)


def refresh_mtp_peer(
    adapter: Path,
    peer: str,
    root: Path,
    deadline: float,
) -> None:
    """Require a successful device pull before trusting a retained peer file.

    WHY: the durable transport root legitimately retains coordination objects
    from earlier runs.  A local peer advertisement can therefore predate an
    Android upgrade that adds a required capability.
    CONTRACT: MTP mode validates only an advertisement obtained after at least
    one successful pull of the expected physical peer during this run.
    INVARIANT: an unavailable peer never makes a stale local advertisement
    authoritative and remains a bounded wait state.
    """
    peer_path = root / "android-peer-v1.json"
    while True:
        try:
            pulled = run_adapter(
                adapter,
                "pull",
                peer,
                root,
                deadline,
                True,
            )
        except ConversationDeadlineExpired as error:
            raise RuntimeError("timeout waiting for android-peer-v1.json") from error
        if pulled and peer_path.is_file():
            return
        if time.monotonic() >= deadline:
            raise RuntimeError("timeout waiting for android-peer-v1.json")
        # A complete libmtp discovery is the expensive poll. Keep retries
        # interactive without continuously reopening the USB/MTP session.
        time.sleep(min(MTP_POLL_INTERVAL_SECONDS, max(0.0, deadline - time.monotonic())))


def refresh_bluetooth_peer(
    adapter: Path,
    peer: str,
    root: Path,
    deadline: float,
) -> None:
    """Require one successful Bluetooth pull before trusting retained peer state."""
    peer_path = root / "android-peer-v1.json"
    while True:
        try:
            pulled = run_adapter(
                adapter,
                "pull",
                peer,
                root,
                deadline,
                True,
                transport_name="Bluetooth",
                lock_name="bt.lock",
            )
        except ConversationDeadlineExpired as error:
            raise RuntimeError("timeout waiting for android-peer-v1.json") from error
        if pulled and peer_path.is_file():
            return
        if time.monotonic() >= deadline:
            raise RuntimeError("timeout waiting for android-peer-v1.json")
        time.sleep(min(1.0, max(0.0, deadline - time.monotonic())))


def correlated_peer_error(root: Path, run_id: str) -> None:
    path = root / "android-generation-error-v1.json"
    if not path.is_file():
        return
    try:
        value = json.loads(bounded(path))
    except (OSError, json.JSONDecodeError):
        return
    if (
        isinstance(value, dict)
        and value.get("format") == "trainlog-sync-generation-error"
        and value.get("version") == 1
        and value.get("run_id") == run_id
        and value.get("code") == "peer_capacity_exhausted"
    ):
        raise RuntimeError(
            "peer_capacity_exhausted: Android active generation capacity is exhausted; "
            "archive acknowledged generations, then retry explicitly"
        )


def load_peer(root: Path, expected: str) -> dict:
    value = json.loads(bounded(root / "android-peer-v1.json"))
    if (
        set(value) != {"format", "version", "peer_id", "capabilities"}
        or value["format"] != "trainlog-sync-peer"
        or type(value["version"]) is not int
        or value["version"] != 1
        or value["peer_id"] != expected
        or not isinstance(value["capabilities"], list)
        or not CAPS.issubset(set(value["capabilities"]))
    ):
        raise RuntimeError("incompatible or unpaired Android peer advertisement")
    return value


def publish_ai_export_to_android(
    args: argparse.Namespace,
    deadline: float,
) -> dict:
    """Best-effort post-convergence AI export delivery.

    The generation/ACK conversation is already complete when this runs.
    Export or publication failure therefore remains separately observable and
    never rolls back a durable synchronization.
    """
    remaining = deadline - time.monotonic()
    if remaining <= 1:
        return {"result": "pending", "diagnostic": "sync deadline exhausted before AI export"}
    output = args.transport_root / "trainlog_ai_export_v1.json"
    exporter = Path(__file__).with_name("export_ai_history.py")
    try:
        result = subprocess.run(
            [
                sys.executable,
                str(exporter),
                str(output),
                "--database",
                str(args.database),
            ],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=min(60.0, max(1.0, remaining - 0.5)),
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"result": "pending", "diagnostic": str(error)[:1024]}
    if result.returncode != 0 or "TRAINLOG_AI_EXPORT_V1=PASS" not in result.stdout:
        diagnostic = (result.stderr or result.stdout or "AI export failed").strip()
        return {"result": "pending", "diagnostic": diagnostic[-1024:]}
    if not output.is_file():
        return {"result": "pending", "diagnostic": "AI export file is missing after exporter success"}

    try:
        if args.mode == "bt":
            push_adapter(
                args.bt_adapter,
                args.expected_peer,
                args.transport_root,
                ["trainlog_ai_export_v1.json"],
                deadline,
                transport_name="Bluetooth",
                lock_name="bt.lock",
            )
        elif args.mode == "mtp":
            push_adapter(
                args.mtp_adapter,
                args.expected_peer,
                args.transport_root,
                ["trainlog_ai_export_v1.json"],
                deadline,
            )
        elif args.mode == "directory":
            pass
        else:
            return {
                "result": "local_only",
                "diagnostic": "AI export generated locally; Android delivery transport unavailable",
            }
    except Exception as error:
        return {"result": "pending", "diagnostic": str(error)[:1024]}
    return {"result": "delivered_to_android"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--transport-root", required=True, type=Path)
    parser.add_argument("--owned-root", required=True, type=Path)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--expected-peer", required=True)
    parser.add_argument("--timeout", required=True, type=int)
    parser.add_argument(
        "--mode",
        choices=("directory", "bt", "mtp", "drive"),
        default="directory",
    )
    parser.add_argument("--bt-adapter", type=Path)
    parser.add_argument("--mtp-adapter", type=Path)
    parser.add_argument("--drive-adapter", type=Path)
    parser.add_argument("--drive-remote")
    args = parser.parse_args()
    deadline = time.monotonic() + args.timeout
    pump = None
    if args.mode == "bt":
        if args.bt_adapter is None:
            raise RuntimeError("Bluetooth adapter is required")
        refresh_bluetooth_peer(
            args.bt_adapter,
            args.expected_peer,
            args.transport_root,
            deadline,
        )
        pump = MtpPullPump(
            lambda: run_adapter(
                args.bt_adapter,
                "pull",
                args.expected_peer,
                args.transport_root,
                deadline,
                True,
                transport_name="Bluetooth",
                lock_name="bt.lock",
            ),
            interval=1.0,
        )
    elif args.mode == "mtp":
        if args.mtp_adapter is None:
            raise RuntimeError("MTP adapter is required")
        refresh_mtp_peer(
            args.mtp_adapter,
            args.expected_peer,
            args.transport_root,
            deadline,
        )
        pump = MtpPullPump(
            lambda: run_adapter(
                args.mtp_adapter,
                "pull",
                args.expected_peer,
                args.transport_root,
                deadline,
                True,
            )
        )
    elif args.mode == "drive":
        if args.drive_adapter is None or not args.drive_remote:
            raise RuntimeError("Drive adapter and remote are required")
        last_drive_pull = 0.0

        def pump_drive(force: bool = False) -> None:
            nonlocal last_drive_pull
            now = time.monotonic()
            if not force and now - last_drive_pull < 5.0:
                return
            run_drive_adapter(
                args.drive_adapter,
                "pull",
                args.drive_remote,
                args.transport_root,
                deadline,
            )
            last_drive_pull = time.monotonic()

        pump = pump_drive
        pump_drive(True)
    peer = load_peer(args.transport_root, args.expected_peer)
    with closing(generation.connect_database(args.database)) as db:
        generation.require_schema(db)
        desktop_peer = generation.peer_identity(db, "desktop")
        db.commit()
    request = {
        "format": "trainlog-sync-generation-request",
        "version": 1,
        "run_id": args.run_id,
        "desktop_peer_id": desktop_peer,
        "android_peer_id": peer["peer_id"],
        "capabilities": sorted(CAPS | {ACK_RECOVERY_CAP}),
    }
    request_path = args.transport_root / "request-v1.json"
    publish_json(request_path, request)
    with closing(generation.connect_database(args.database)) as db:
        acknowledgements = recovery_acknowledgements(db, peer["peer_id"], desktop_peer)
    archive_acknowledgements = {
        "format": "trainlog-sync-archive-acknowledgements",
        "version": 1,
        "run_id": args.run_id,
        "android_peer_id": peer["peer_id"],
        "desktop_peer_id": desktop_peer,
        "acknowledgements": acknowledgements,
    }
    archive_ack_path = args.transport_root / "desktop-archive-acknowledgements-v1.json"
    publish_json(archive_ack_path, archive_acknowledgements)
    if args.mode == "bt":
        push_adapter(
            args.bt_adapter,
            args.expected_peer,
            args.transport_root,
            ["request-v1.json", "desktop-archive-acknowledgements-v1.json"],
            deadline,
            transport_name="Bluetooth",
            lock_name="bt.lock",
        )
    elif args.mode == "mtp":
        push_adapter(
            args.mtp_adapter,
            args.expected_peer,
            args.transport_root,
            ["request-v1.json", "desktop-archive-acknowledgements-v1.json"],
            deadline,
        )
    elif args.mode == "drive":
        push_drive(
            args.drive_adapter,
            args.drive_remote,
            args.transport_root,
            ["request-v1.json", "desktop-archive-acknowledgements-v1.json"],
            deadline,
        )
    emit(
        args.run_id,
        "waiting_android_publication",
        producer_peer_id=peer["peer_id"],
        consumer_peer_id=desktop_peer,
    )

    def pump_inbound():
        if pump is not None:
            pump()
        correlated_peer_error(args.transport_root, args.run_id)

    if ACK_RECOVERY_CAP in set(peer.get("capabilities", [])):
        android_archive_ack = args.transport_root / "android-archive-acknowledgements-v1.json"
        wait_json(
            android_archive_ack,
            deadline,
            lambda value: value.get("run_id") == args.run_id
            and value.get("android_peer_id") == peer["peer_id"]
            and value.get("desktop_peer_id") == desktop_peer,
            pump_inbound,
        )
        recovered = reconcile_recovery_acknowledgements(
            args.database,
            bounded(android_archive_ack),
            args.run_id,
            peer["peer_id"],
            desktop_peer,
        )
        emit(
            args.run_id,
            "android_archive_ack_observed",
            recovered_acknowledgements=recovered,
        )

    inbound_ref = args.transport_root / "android-generation-v1.json"
    inbound = wait_json(
        inbound_ref, deadline, lambda value: value.get("run_id") == args.run_id, pump_inbound
    )
    inbound_dir = args.transport_root / inbound["relative_path"]
    ack = generation.consume_desktop(args.database, inbound_dir)
    ack_path = args.transport_root / "desktop-consumption-ack-v1.json"
    publish_json(ack_path, ack)
    if args.mode == "bt":
        push_adapter(
            args.bt_adapter,
            args.expected_peer,
            args.transport_root,
            ["request-v1.json", "desktop-archive-acknowledgements-v1.json", "desktop-consumption-ack-v1.json"],
            deadline,
            transport_name="Bluetooth",
            lock_name="bt.lock",
        )
    elif args.mode == "mtp":
        push_adapter(
            args.mtp_adapter,
            args.expected_peer,
            args.transport_root,
            ["request-v1.json", "desktop-archive-acknowledgements-v1.json", "desktop-consumption-ack-v1.json"],
            deadline,
        )
    elif args.mode == "drive":
        push_drive(
            args.drive_adapter,
            args.drive_remote,
            args.transport_root,
            ["request-v1.json", "desktop-archive-acknowledgements-v1.json", "desktop-consumption-ack-v1.json"],
            deadline,
        )
    if ack["result"] != "consumed":
        raise RuntimeError("desktop rejected Android generation")
    emit(
        args.run_id,
        "local_import_committed",
        inbound_generation_id=ack["generation_id"],
        manifest_sha256=ack["manifest_sha256"],
    )
    outgoing_id = "gen_" + str(uuid.uuid4())
    # Export codecs retain human diagnostics on stdout when invoked as CLIs.
    # The worker stdout contract is typed NDJSON only, so route those bounded
    # diagnostics to the separately bounded stderr channel.
    with redirect_stdout(sys.stderr):
        stage, manifest, digest = generation.capture_desktop(
            args.database, args.owned_root, peer["peer_id"], args.run_id, outgoing_id
        )
    del stage
    published = generation.publish(
        args.database, outgoing_id, args.transport_root / "desktop-objects"
    )
    ref = {
        "format": "trainlog-sync-generation-reference",
        "version": 1,
        "run_id": args.run_id,
        "generation_id": outgoing_id,
        "manifest_sha256": digest,
        "relative_path": str(published.relative_to(args.transport_root)),
    }
    publish_json(args.transport_root / "desktop-generation-v1.json", ref)
    if args.mode == "bt":
        push_desktop_generation(
            args.bt_adapter,
            args.expected_peer,
            args.transport_root,
            ref["relative_path"],
            deadline,
            transport_name="Bluetooth",
            lock_name="bt.lock",
        )
    elif args.mode == "mtp":
        # CONTRACT: the immutable generation must be fully visible before its
        # mutable run-correlated reference. Publishing both in one MTP walk
        # lets Android observe the reference while artifacts are still being
        # transferred, turning a complete generation into a false rejection.
        push_desktop_generation(
            args.mtp_adapter,
            args.expected_peer,
            args.transport_root,
            ref["relative_path"],
            deadline,
        )
    elif args.mode == "drive":
        push_drive(
            args.drive_adapter,
            args.drive_remote,
            args.transport_root,
            [ref["relative_path"]],
            deadline,
        )
        push_drive(
            args.drive_adapter,
            args.drive_remote,
            args.transport_root,
            [
                "request-v1.json",
                "desktop-archive-acknowledgements-v1.json",
                "desktop-consumption-ack-v1.json",
                "desktop-generation-v1.json",
            ],
            deadline,
        )
    emit(
        args.run_id,
        "published",
        outbound_generation_id=outgoing_id,
        manifest_sha256=digest,
    )
    emit(args.run_id, "waiting_acknowledgement", outbound_generation_id=outgoing_id)
    android_ack = args.transport_root / "android-consumption-ack-v1.json"
    android_ack_value = wait_json(
        android_ack,
        deadline,
        lambda value: value.get("run_id") == args.run_id
        and value.get("generation_id") == outgoing_id,
        pump,
    )
    result = generation.accept_ack(args.database, android_ack)
    if result not in ("acknowledged", "unchanged"):
        raise RuntimeError(
            f"Android did not durably consume desktop generation: {result}: {android_ack_value.get('diagnostic','')}"
        )
    with closing(generation.connect_database(args.database)) as db:
        inbound_row = db.execute(
            "SELECT result FROM sync_consumed_generations WHERE generation_id=?",
            (ack["generation_id"],),
        ).fetchone()
        outbound_row = db.execute(
            "SELECT status FROM sync_generations WHERE generation_id=?", (outgoing_id,)
        ).fetchone()
        drafts = [
            {
                "draft_id": row[0],
                "state": row[1],
                "session_type": json.loads(row[2]).get("session_type", "training"),
                "occurrence_count": len(json.loads(row[2]).get("exercises", [])),
            }
            for row in db.execute(
                "SELECT session_id,state,payload_json FROM execution_drafts ORDER BY session_id LIMIT 32"
            )
        ]
    if inbound_row != ("consumed",) or outbound_row != ("acknowledged",):
        raise RuntimeError("generation evidence did not corroborate completion")
    emit(args.run_id, "peer_consumed", outbound_generation_id=outgoing_id)
    ai_post_sync = publish_ai_export_to_android(args, deadline)
    report = {
        "format": "trainlog-sync-worker-report",
        "version": 1,
        "run_id": args.run_id,
        "producer_peer_id": peer["peer_id"],
        "consumer_peer_id": desktop_peer,
        "inbound_generation_id": ack["generation_id"],
        "outbound_generation_id": outgoing_id,
        "manifest_sha256": digest,
        "result": "completed",
        "sessions_reconciled": 0,
        "domains": {
            name: True
            for name in (
                "history-v4",
                "execution-draft-v1",
                "causal-delete-v1",
                "equipment",
                "aliases",
                "profiles",
                "body-zones",
                "feedback",
            )
        },
        "drafts": drafts,
        "ai_midpoint": {"result": "not_configured"},
        "ai_post_sync": ai_post_sync,
    }
    print(json.dumps(report, sort_keys=True, separators=(",", ":")), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"sync peer failed: {error}", file=sys.stderr)
        raise SystemExit(1) from None
