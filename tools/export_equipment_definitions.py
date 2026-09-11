#!/usr/bin/env python3
"""Export user-created equipment definitions without changing association v2."""
import argparse
import json
import os
import sqlite3
from datetime import datetime
from pathlib import Path

FORMAT = "trainlog-equipment-definitions"
VERSION = 1


def default_database():
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog/trainlog.db"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, default=default_database())
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    connection.row_factory = sqlite3.Row
    try:
        if connection.execute("PRAGMA user_version").fetchone()[0] not in (8, 9, 10, 11, 12):
            raise ValueError("schema desktop v8 à v12 requis")
        equipment = [dict(row) for row in connection.execute(
            "SELECT equipment_id,display_name,label_name,equipment_type,load_semantics "
            "FROM custom_equipment ORDER BY equipment_id")]
        payload = {"format": FORMAT, "version": VERSION,
                   "generated_at": datetime.now().astimezone().isoformat(),
                   "equipment": equipment}
        args.output.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        print("EQUIPMENT_DEFINITIONS_EXPORT=PASS")
        print(f"definitions={len(equipment)}")
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"EQUIPMENT_DEFINITIONS_EXPORT=FAIL {error}")
        raise SystemExit(1)
