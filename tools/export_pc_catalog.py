#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
from datetime import datetime
from pathlib import Path


def default_database_path():
    data_home = os.environ.get("XDG_DATA_HOME")

    if data_home:
        return (
            Path(data_home)
            / "trainlog"
            / "trainlog.db"
        )

    return (
        Path.home()
        / ".local"
        / "share"
        / "trainlog"
        / "trainlog.db"
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Export du catalogue canonique PC "
            "vers un artifact Trainlog versionné."
        )
    )

    parser.add_argument(
        "output",
        type=Path,
    )

    parser.add_argument(
        "--database",
        type=Path,
        default=default_database_path(),
    )

    args = parser.parse_args()

    if not args.database.exists():
        raise SystemExit(
            "PC_CATALOG_EXPORT=FAIL database not found"
        )

    connection = sqlite3.connect(
        args.database
    )

    try:
        version = connection.execute(
            "PRAGMA user_version;"
        ).fetchone()[0]

        # CONTRACT: v8 adds only desktop-local custom equipment.  The PC
        # catalogue artifact is unchanged, but it must read the current
        # canonical desktop schema rather than accept a stale pre-v8 database.
        if version not in (8, 9):
            raise SystemExit(
                "PC_CATALOG_EXPORT=FAIL "
                f"schema={version}"
            )

        rows = connection.execute(
            '''
            SELECT
                exercise_id,
                name,
                recording_mode,
                tracking_mode,
                data_fields
            FROM exercises
            ORDER BY
                name COLLATE NOCASE,
                exercise_id;
            '''
        ).fetchall()

        payload = {
            "format": "trainlog-pc-catalog",
            "version": 1,
            "generated_at":
                datetime.now()
                .astimezone()
                .isoformat(),
            "exercises": [
                {
                    "exercise_id": row[0],
                    "name": row[1],
                    "recording_mode": row[2],
                    "tracking_mode": row[3],
                    "data_fields": row[4],
                }
                for row in rows
            ],
        }

        args.output.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        args.output.write_text(
            json.dumps(
                payload,
                ensure_ascii=False,
                separators=(",", ":"),
            ),
            encoding="utf-8",
        )

        print("PC_CATALOG_EXPORT=PASS")
        print(f"exercises={len(rows)}")
        print(f"output={args.output}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
