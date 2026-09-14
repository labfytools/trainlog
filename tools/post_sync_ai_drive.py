#!/usr/bin/env python3
"""Publish the post-sync TRAINLOG_AI_EXPORT_V1 artifact to Google Drive."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


DRIVE_TARGET = "TrainLog Gdrive:Trainlog/AI/trainlog_ai_export_v1.json"


def _detail(result: subprocess.CompletedProcess[str]) -> str:
    """Return one bounded, useful external-command diagnostic."""
    text = (result.stderr or result.stdout or "").strip()
    if not text:
        return f"exit={result.returncode}"
    return text.splitlines()[-1][:1000]


def publish(database: Path, output: Path) -> int:
    """Export first, then upload only the successfully replaced artifact.

    CONTRACT: the exporter remains the sole owner of TRAINLOG_AI_EXPORT_V1
    generation. rclone receives only file and remote arguments; Trainlog never
    reads, stores, or forwards rclone/OAuth configuration. INVARIANT: an export
    failure cannot upload an older file left at the destination path.
    """
    project_root = Path(__file__).resolve().parent.parent
    resolved_output = output if output.is_absolute() else project_root / output
    exporter = project_root / "tools" / "export_ai_history.py"

    try:
        export = subprocess.run(
            [
                sys.executable,
                str(exporter),
                str(resolved_output),
                "--database",
                str(database),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as error:
        print(f"AI_EXPORT=FAIL {error}")
        print("GDRIVE_UPLOAD=SKIPPED")
        return 1

    if export.returncode != 0 or "TRAINLOG_AI_EXPORT_V1=PASS" not in export.stdout:
        print(f"AI_EXPORT=FAIL {_detail(export)}")
        print("GDRIVE_UPLOAD=SKIPPED")
        return 1

    print("AI_EXPORT=PASS")

    try:
        upload = subprocess.run(
            ["rclone", "copyto", str(resolved_output), DRIVE_TARGET],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as error:
        print(f"GDRIVE_UPLOAD=FAIL {error}")
        return 1

    if upload.returncode != 0:
        print(f"GDRIVE_UPLOAD=FAIL {_detail(upload)}")
        return 1

    print("GDRIVE_UPLOAD=PASS")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "output",
        type=Path,
        nargs="?",
        default=Path("trainlog_ai_export_v1.json"),
    )
    parser.add_argument("--database", type=Path, required=True)
    args = parser.parse_args()
    return publish(args.database, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
