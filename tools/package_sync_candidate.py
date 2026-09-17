#!/usr/bin/env python3
"""Stage a reproducible, non-secret Trainlog desktop synchronization candidate."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_TOOLS = [
    "sync_orchestrator.py",
    "sync_peer_worker.py",
    "sync_generation_exchange.py",
    "trainlog_sqlite.py",
    "validate_json.py",
    "import_mobile_export.py",
    "import_exercise_aliases.py",
    "import_equipment_definitions.py",
    "import_equipment_associations.py",
    "import_exercise_body_zones.py",
    "import_training_feedback.py",
    "execution_draft_exchange.py",
    "causal_delete_exchange.py",
    "import_exercise_profile_state.py",
    "export_exercise_profile_state.py",
    "export_pc_mobile.py",
    "export_equipment_definitions.py",
    "export_exercise_body_zones.py",
    "export_training_feedback.py",
]


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--build", default=ROOT / "build", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError("candidate output already exists")
    binary = args.build / "tui/trainlog"
    mtp_adapter = args.build / "tui/trainlog-generation-mtp-adapter"
    if not binary.is_file():
        raise RuntimeError("compiled trainlog executable is missing")
    if not mtp_adapter.is_file():
        raise RuntimeError("compiled generation MTP adapter is missing")
    (args.output / "bin").mkdir(parents=True)
    (args.output / "libexec").mkdir(parents=True)
    (args.output / "tools").mkdir(parents=True)
    (args.output / "catalog").mkdir(parents=True)
    shutil.copy2(binary, args.output / "libexec/trainlog")
    shutil.copy2(mtp_adapter, args.output / "libexec/trainlog-generation-mtp-adapter")
    for name in RUNTIME_TOOLS:
        shutil.copy2(ROOT / "tools" / name, args.output / "tools" / name)
    for source in sorted((ROOT / "catalog").glob("*.json")):
        shutil.copy2(source, args.output / "catalog" / source.name)
    launcher = args.output / "bin/trainlog"
    launcher.write_text(
        '#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\nexport TRAINLOG_SYNC_TOOLS_DIR="$root/tools"\nexport TRAINLOG_SYNC_MTP_ADAPTER="$root/libexec/trainlog-generation-mtp-adapter"\nexec "$root/libexec/trainlog" "$@"\n'
    )
    files = sorted(path for path in args.output.rglob("*") if path.is_file())
    inventory = {
        "format": "trainlog-sync-candidate-inventory",
        "version": 1,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "product_version": "0.1.2",
        "desktop_schema": 21,
        "android_schema": 20,
        "protocols": [
            "mobile-export-v3",
            "mobile-history-v4",
            "causal-delete-v1",
            "execution-draft-v1",
            "generation-manifest-v1",
            "generation-ack-v1",
        ],
        "entry_points": [
            "bin/trainlog",
            "libexec/trainlog-generation-mtp-adapter",
            "tools/sync_orchestrator.py",
        ],
        "runtime_dependencies": [
            "python>=3.11",
            "sqlite3",
            "utf8proc",
            "libuuid",
            "libudev",
            "libmtp",
            "notcurses",
        ],
        "files": [
            {
                "path": str(path.relative_to(args.output)),
                "sha256": digest(path),
                "size": path.stat().st_size,
            }
            for path in files
        ],
    }
    (args.output / "candidate-inventory.json").write_text(
        json.dumps(inventory, indent=2, sort_keys=True) + "\n"
    )
    os.chmod(args.output / "bin/trainlog", 0o755)
    os.chmod(args.output / "libexec/trainlog", 0o755)
    os.chmod(args.output / "libexec/trainlog-generation-mtp-adapter", 0o755)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
