#!/usr/bin/env python3
"""Contract tests for ordered, non-secret post-sync Drive publication."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import subprocess
import tempfile
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "post_sync_ai_drive.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("post_sync_ai_drive", TOOL)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_case(results):
    module = load_tool()
    output = io.StringIO()
    with tempfile.TemporaryDirectory() as directory:
        database = Path(directory) / "trainlog.db"
        destination = Path(directory) / "export.json"
        with mock.patch.object(module.subprocess, "run", side_effect=results) as run:
            with contextlib.redirect_stdout(output):
                status = module.publish(database, destination)
        return module, status, output.getvalue(), run.call_args_list


def completed(argv, returncode, stdout="", stderr=""):
    return subprocess.CompletedProcess(argv, returncode, stdout, stderr)


def main() -> None:
    module, status, text, calls = run_case([
        completed(["export"], 0, "TRAINLOG_AI_EXPORT_V1=PASS\n"),
        completed(["rclone"], 0),
    ])
    assert status == 0
    assert text == "AI_EXPORT=PASS\nGDRIVE_UPLOAD=PASS\n"
    assert len(calls) == 2
    assert calls[0].args[0][1].endswith("tools/export_ai_history.py")
    assert calls[1].args[0][0:2] == ["rclone", "copyto"]
    assert calls[1].args[0][-1] == module.DRIVE_TARGET

    _, status, text, calls = run_case([
        completed(["export"], 1, "TRAINLOG_AI_EXPORT_V1=FAIL database locked\n"),
    ])
    assert status == 1
    assert "AI_EXPORT=FAIL" in text
    assert "GDRIVE_UPLOAD=SKIPPED" in text
    assert len(calls) == 1

    _, status, text, calls = run_case([
        completed(["export"], 0, "TRAINLOG_AI_EXPORT_V1=PASS\n"),
        FileNotFoundError("rclone unavailable"),
    ])
    assert status == 1
    assert "AI_EXPORT=PASS" in text
    assert "GDRIVE_UPLOAD=FAIL rclone unavailable" in text
    assert len(calls) == 2

    _, status, text, _ = run_case([
        completed(["export"], 0, "TRAINLOG_AI_EXPORT_V1=PASS\n"),
        completed(["rclone"], 9, stderr="remote upload failed\n"),
    ])
    assert status == 1
    assert "GDRIVE_UPLOAD=FAIL remote upload failed" in text
    print("PASS post-sync AI export and Drive ordering")


if __name__ == "__main__":
    main()
