#!/usr/bin/env python3
"""Bridge one Android-produced V3 artifact through the real desktop tools."""

import argparse
import sqlite3
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests"))

from test_session_exchange_v3 import V11_SCHEMA  # noqa: E402


def run(arguments: list[str]) -> None:
    result = subprocess.run(arguments, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(result.stdout + result.stderr)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("android_export", type=Path)
    parser.add_argument("desktop_database", type=Path)
    parser.add_argument("desktop_export", type=Path)
    args = parser.parse_args()

    # CONTRACT: this is the existing minimal production-compatible desktop V11
    # fixture. Business behavior remains owned by the real importer/exporter.
    with sqlite3.connect(args.desktop_database) as connection:
        connection.executescript(V11_SCHEMA)

    run([
        sys.executable,
        str(ROOT / "tools/import_mobile_export.py"),
        str(args.android_export),
        "--database",
        str(args.desktop_database),
    ])
    run([
        sys.executable,
        str(ROOT / "tools/export_pc_mobile.py"),
        str(args.desktop_export),
        "--database",
        str(args.desktop_database),
    ])


if __name__ == "__main__":
    main()
