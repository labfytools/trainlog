#!/usr/bin/env python3
"""Export user-created equipment definitions without changing association v2."""
import argparse
import json
import os
import sqlite3
from datetime import datetime
from pathlib import Path
from trainlog_sqlite import connect_database

FORMAT = "trainlog-equipment-definitions"
VERSION = 1


def default_database():
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog/trainlog.db"


def main(complete_causal_envelope=False):
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, default=default_database())
    args = parser.parse_args()
    connection = connect_database(args.database)
    connection.row_factory = sqlite3.Row
    try:
        if connection.execute("PRAGMA user_version").fetchone()[0] not in (8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22):
            raise ValueError("schema desktop v8 à v16 requis")
        if not complete_causal_envelope and connection.execute("PRAGMA user_version").fetchone()[0] >= 20 and connection.execute("SELECT 1 FROM sync_causal_state WHERE target_kind='custom_equipment' AND deleted=1 LIMIT 1").fetchone():
            raise ValueError("causal equipment protection requires the staged artifact")
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
