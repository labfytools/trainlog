#!/usr/bin/env python3
"""Export the bounded EXERCISE_MERGE_V1 identity companion."""
import argparse
import json
import sqlite3
from pathlib import Path

MAX_ALIASES = 4096


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    args = parser.parse_args()
    con = sqlite3.connect(args.database)
    try:
        if con.execute("PRAGMA user_version").fetchone()[0] != 12:
            raise ValueError("schema desktop v12 requis")
        rows = con.execute(
            "SELECT source_exercise_id,canonical_exercise_id FROM exercise_aliases "
            "ORDER BY source_exercise_id COLLATE BINARY"
        ).fetchall()
        if len(rows) > MAX_ALIASES:
            raise ValueError("trop d'alias exercice")
        payload = {"format": "trainlog-exercise-aliases", "version": 1,
                   "aliases": [{"source_exercise_id": row[0],
                                "canonical_exercise_id": row[1]} for row in rows]}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(payload, ensure_ascii=False,
                                          separators=(",", ":")), encoding="utf-8")
    finally:
        con.close()
    print(f"EXERCISE_ALIAS_EXPORT=PASS aliases={len(rows)}")


if __name__ == "__main__":
    main()
