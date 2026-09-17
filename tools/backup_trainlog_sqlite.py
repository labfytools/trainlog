#!/usr/bin/env python3
"""Create or restore a consistent Trainlog SQLite backup.

This tool never discovers a user database. Both source and destination must be
explicit, and restore refuses to overwrite an existing file. SQLite's backup
API includes committed WAL content without relying on a racy file copy.
"""
import argparse
import sqlite3
from pathlib import Path


def backup(source: Path, destination: Path) -> None:
    if not source.is_file() or destination.exists():
        raise RuntimeError("source must exist and destination must not exist")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(f"file:{source}?mode=ro", uri=True) as src, sqlite3.connect(destination) as dst:
        src.backup(dst)
        if dst.execute("PRAGMA integrity_check").fetchone() != ("ok",):
            raise RuntimeError("backup integrity check failed")
        violations = dst.execute("PRAGMA foreign_key_check").fetchall()
        if violations:
            raise RuntimeError(f"backup foreign-key violations: {len(violations)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    backup(args.source, args.destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
