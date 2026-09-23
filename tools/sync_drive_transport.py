#!/usr/bin/env python3
"""Bounded byte transport for the Trainlog generation namespace on rclone.

Business validation, causality, import and acknowledgement remain owned by the
existing generation services.  This adapter only moves immutable bytes and
mutable run-correlated coordination objects.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path, PurePosixPath

COORDINATION = (
    "android-peer-v1.json",
    "android-generation-v1.json",
    "android-consumption-ack-v1.json",
    "android-archive-acknowledgements-v1.json",
    "android-generation-error-v1.json",
    "request-v1.json",
    "desktop-archive-acknowledgements-v1.json",
    "desktop-consumption-ack-v1.json",
    "desktop-generation-v1.json",
)
MAX_FILE = 64 * 1024 * 1024
MAX_GENERATION = 256 * 1024 * 1024


class DriveTransportError(RuntimeError):
    pass


def safe_relative(value: str) -> Path:
    candidate = PurePosixPath(value)
    if (
        not value
        or value.startswith("/")
        or "\\" in value
        or "\x00" in value
        or any(part in ("", ".", "..") for part in candidate.parts)
        or len(candidate.parts) > 4
    ):
        raise DriveTransportError("unsafe Drive object path")
    return Path(*candidate.parts)


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            value.update(chunk)
    return value.hexdigest()


class RcloneDrive:
    def __init__(self, executable: str, remote: str):
        if not remote or "\n" in remote or "\r" in remote or "Trainlog/AI" in remote:
            raise DriveTransportError("invalid or AI Drive namespace")
        self.executable = executable
        self.remote = remote.rstrip("/")

    def target(self, relative: str) -> str:
        safe_relative(relative)
        return f"{self.remote}/{relative}"

    def run(self, *arguments: str, timeout: int = 60) -> subprocess.CompletedProcess[bytes]:
        try:
            return subprocess.run(
                [self.executable, *arguments],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=timeout,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            raise DriveTransportError(f"Drive transport unavailable: {error}") from error

    def download(self, relative: str, destination: Path, optional: bool = False) -> bool:
        destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        with tempfile.TemporaryDirectory(prefix=".trainlog-drive-pull-", dir=destination.parent) as raw:
            temporary = Path(raw) / "object"
            result = self.run("copyto", self.target(relative), str(temporary))
            if result.returncode != 0:
                message = result.stderr.decode("utf-8", "replace")[:1024]
                if optional and any(token in message.lower() for token in ("not found", "doesn't exist", "directory not found")):
                    return False
                raise DriveTransportError(message.strip() or "Drive download failed")
            if not temporary.is_file() or temporary.stat().st_size > MAX_FILE:
                raise DriveTransportError("Drive object exceeds local bound")
            os.replace(temporary, destination)
        return True

    def upload_verified(self, source: Path, relative: str) -> None:
        if source.is_symlink() or not source.is_file() or source.stat().st_size > MAX_FILE:
            raise DriveTransportError("invalid Drive upload source")
        result = self.run("copyto", str(source), self.target(relative))
        if result.returncode != 0:
            raise DriveTransportError(result.stderr.decode("utf-8", "replace")[:1024].strip())
        with tempfile.TemporaryDirectory(prefix="trainlog-drive-verify-") as raw:
            verification = Path(raw) / "object"
            self.download(relative, verification)
            if verification.stat().st_size != source.stat().st_size or digest(verification) != digest(source):
                raise DriveTransportError("Drive upload verification failed")


def generation_from_reference(root: Path, name: str) -> str | None:
    path = root / name
    if not path.is_file() or path.stat().st_size > 65536:
        return None
    value = json.loads(path.read_text(encoding="utf-8"))
    relative = value.get("relative_path")
    if not isinstance(relative, str):
        raise DriveTransportError("invalid generation reference")
    safe = safe_relative(relative)
    expected_root = (
        "android-objects" if name == "android-generation-v1.json" else "desktop-objects"
    )
    # CONTRACT: references point only at the immutable generation namespace
    # owned by their producer.  The generation identity is the third path
    # component; the middle component is the literal ``generations``.
    if (
        len(safe.parts) != 3
        or safe.parts[0] != expected_root
        or safe.parts[1] != "generations"
        or safe.parts[2] != value.get("generation_id")
    ):
        raise DriveTransportError("uncorrelated generation reference")
    return relative


def pull_generation(drive: RcloneDrive, root: Path, relative: str) -> None:
    generation_path = safe_relative(relative)
    producer_root = generation_path.parts[0]
    generation_relative = Path(*generation_path.parts[1:])
    destination = root / relative
    # WHY: polling must remain bounded even when Drive retains many immutable
    # generations. CONTRACT: os.replace below makes a generation visible
    # locally only after every listed artifact has downloaded. INVARIANT: a
    # committed local generation is immutable and is validated by the shared
    # consumer before import; an incomplete pre-existing path is never hidden.
    if destination.is_dir() and (destination / "manifest.json").is_file():
        return
    if destination.exists():
        raise DriveTransportError("incomplete local Drive generation")
    manifest_relative = f"{relative}/manifest.json"
    with tempfile.TemporaryDirectory(prefix=".trainlog-drive-generation-", dir=root) as raw:
        staging = Path(raw) / relative
        drive.download(manifest_relative, staging / "manifest.json")
        manifest = json.loads((staging / "manifest.json").read_text(encoding="utf-8"))
        artifacts = manifest.get("artifacts")
        if not isinstance(artifacts, list) or len(artifacts) > 32:
            raise DriveTransportError("invalid Drive generation manifest")
        total = (staging / "manifest.json").stat().st_size
        for artifact in artifacts:
            filename = artifact.get("filename") if isinstance(artifact, dict) else None
            if not isinstance(filename, str):
                raise DriveTransportError("artifact outside Drive generation")
            artifact_path = safe_relative(filename)
            if artifact_path.parent != generation_relative:
                raise DriveTransportError("artifact outside Drive generation")
            artifact_destination = staging / artifact_path.name
            drive.download(f"{producer_root}/{filename}", artifact_destination)
            total += artifact_destination.stat().st_size
            if total > MAX_GENERATION:
                raise DriveTransportError("Drive generation exceeds bound")
        destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        os.replace(staging, destination)


def pull(drive: RcloneDrive, root: Path) -> None:
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    for name in COORDINATION:
        drive.download(name, root / name, optional=True)
    for name in ("android-generation-v1.json", "desktop-generation-v1.json"):
        relative = generation_from_reference(root, name)
        if relative is not None:
            pull_generation(drive, root, relative)


def ordered_files(root: Path) -> list[Path]:
    files = [path for path in root.rglob("*") if path.is_file()]
    if any(path.is_symlink() for path in files):
        raise DriveTransportError("Drive outbox contains a symbolic link")
    # CONTRACT: a generation is consumable only after its manifest exists;
    # coordination references are published after the referenced directory.
    return sorted(
        files,
        key=lambda path: (
            2 if path.name.endswith("generation-v1.json") else
            1 if path.name == "manifest.json" else 0,
            path.relative_to(root).as_posix(),
        ),
    )


def push(drive: RcloneDrive, root: Path) -> None:
    if not root.is_dir():
        raise DriveTransportError("Drive outbox is unavailable")
    for source in ordered_files(root):
        drive.upload_verified(source, source.relative_to(root).as_posix())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("operation", choices=("pull", "push"))
    parser.add_argument("remote")
    parser.add_argument("root", type=Path)
    parser.add_argument("--rclone", default=os.environ.get("TRAINLOG_RCLONE", "rclone"))
    args = parser.parse_args()
    drive = RcloneDrive(args.rclone, args.remote)
    if args.operation == "pull":
        pull(drive, args.root)
    else:
        push(drive, args.root)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (DriveTransportError, json.JSONDecodeError, OSError) as error:
        print(f"Drive transport failed: {error}", file=os.sys.stderr)
        raise SystemExit(1) from None
