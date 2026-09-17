#!/usr/bin/env python3
"""Production full-generation peer conversation over a transport directory.

The directory is the object-level seam implemented by the MTP adapter in a
deployed bundle and directly supplied by isolated tests. Business documents are
always produced and consumed by the existing generation services.
"""

from __future__ import annotations
import argparse, hashlib, json, os, sys, time, uuid
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
}


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
    temporary = path.with_name("." + path.name + ".tmp-" + str(os.getpid()))
    with temporary.open("wb") as stream:
        stream.write(generation.canonical(value))
        stream.flush()
        os.fsync(stream.fileno())
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)


def run_adapter(
    adapter: Path,
    operation: str,
    peer: str,
    root: Path,
    deadline: float,
    allow_missing_peer: bool = False,
) -> bool:
    remaining = max(1, int(deadline - time.monotonic()))
    result = __import__("subprocess").run(
        [str(adapter), operation, peer, str(root)],
        stdin=__import__("subprocess").DEVNULL,
        capture_output=True,
        timeout=min(remaining, 30),
        check=False,
    )
    if result.returncode != 0:
        diagnostic = (
            result.stderr.decode("utf-8", "replace")[:1024] or "MTP adapter failed"
        ).strip()
        missing_peer = diagnostic in {
            "double failed 6 expected Android peer not found",
            "MTP adapter failed status=6 diagnostic=expected Android peer not found",
        }
        if allow_missing_peer and missing_peer:
            return False
        raise RuntimeError(diagnostic)
    return True


def wait_file(path: Path, deadline: float, pump=None) -> None:
    while not path.is_file():
        if time.monotonic() >= deadline:
            raise RuntimeError(f"timeout waiting for {path.name}")
        if pump is not None:
            pump()
        time.sleep(0.05)


def wait_json(path: Path, deadline: float, predicate, pump=None) -> dict:
    while True:
        wait_file(path, deadline, pump)
        try:
            value = json.loads(bounded(path))
        except (OSError, json.JSONDecodeError):
            value = None
        if isinstance(value, dict) and predicate(value):
            return value
        if time.monotonic() >= deadline:
            raise RuntimeError(f"timeout waiting for correlated {path.name}")
        if pump is not None:
            pump()
        time.sleep(0.05)


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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--transport-root", required=True, type=Path)
    parser.add_argument("--owned-root", required=True, type=Path)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--expected-peer", required=True)
    parser.add_argument("--timeout", required=True, type=int)
    parser.add_argument("--mode", choices=("directory", "mtp"), default="directory")
    parser.add_argument("--mtp-adapter", type=Path)
    args = parser.parse_args()
    deadline = time.monotonic() + args.timeout
    pump = None
    if args.mode == "mtp":
        if args.mtp_adapter is None:
            raise RuntimeError("MTP adapter is required")
        pump = lambda: run_adapter(
            args.mtp_adapter,
            "pull",
            args.expected_peer,
            args.transport_root,
            deadline,
            True,
        )
        # Android publishes its durable identity only while participating.  A
        # missing expected advertisement is therefore a bounded wait state;
        # device, transport, ambiguity and malformed-object failures remain
        # immediate and distinct adapter errors.
        wait_file(args.transport_root / "android-peer-v1.json", deadline, pump)
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
        "capabilities": sorted(CAPS),
    }
    request_path = args.transport_root / "request-v1.json"
    publish_json(request_path, request)
    if args.mode == "mtp":
        run_adapter(
            args.mtp_adapter, "push", args.expected_peer, args.transport_root, deadline
        )
    emit(
        args.run_id,
        "waiting_android_publication",
        producer_peer_id=peer["peer_id"],
        consumer_peer_id=desktop_peer,
    )
    inbound_ref = args.transport_root / "android-generation-v1.json"
    inbound = wait_json(
        inbound_ref, deadline, lambda value: value.get("run_id") == args.run_id, pump
    )
    inbound_dir = args.transport_root / inbound["relative_path"]
    ack = generation.consume_desktop(args.database, inbound_dir)
    ack_path = args.transport_root / "desktop-consumption-ack-v1.json"
    publish_json(ack_path, ack)
    if args.mode == "mtp":
        run_adapter(
            args.mtp_adapter, "push", args.expected_peer, args.transport_root, deadline
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
    if args.mode == "mtp":
        run_adapter(
            args.mtp_adapter, "push", args.expected_peer, args.transport_root, deadline
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
        "ai_post_sync": {"result": "not_configured"},
    }
    print(json.dumps(report, sort_keys=True, separators=(",", ":")), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"sync peer failed: {error}", file=sys.stderr)
        raise SystemExit(1) from None
