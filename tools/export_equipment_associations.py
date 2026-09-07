#!/usr/bin/env python3
"""Export stable desktop session/exercise equipment associations."""
import argparse
import json
import sqlite3
import os
from datetime import datetime
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path,
                        default=Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog" / "trainlog.db")
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    try:
        if connection.execute("PRAGMA user_version;").fetchone()[0] != 7:
            raise ValueError("schema desktop v7 requis")
        rows = connection.execute(
            "SELECT s.session_id,se.entry_id,e.exercise_id,se.equipment_id FROM session_exercises se "
            "JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id "
            "ORDER BY s.started_at,s.id,se.position").fetchall()
        payload = {"format": "trainlog-equipment-associations", "version": 2,
                   "generated_at": datetime.now().astimezone().isoformat(), "associations": []}
        for session_id, entry_id, exercise_id, equipment_id in rows:
            item = {"session_id": session_id, "entry_id": entry_id, "exercise_id": exercise_id,
                    "state": "set" if equipment_id is not None else "cleared"}
            if equipment_id is not None:
                item["equipment_id"] = equipment_id
            payload["associations"].append(item)
        args.output.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        print("EQUIPMENT_ASSOCIATIONS_EXPORT=PASS")
        print(f"associations={len(rows)}")
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"EQUIPMENT_ASSOCIATIONS_EXPORT=FAIL {error}")
        raise SystemExit(1)
