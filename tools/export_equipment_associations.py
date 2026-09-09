#!/usr/bin/env python3
"""Export stable desktop session/exercise equipment associations."""
import argparse
import json
import sqlite3
import os
from datetime import datetime
from pathlib import Path


DEFAULT_CATALOG = Path(__file__).resolve().parents[1] / "catalog" / "equipment-v1.json"


def load_supplied_equipment_ids(path):
    catalog = json.loads(path.read_text(encoding="utf-8"))
    if catalog.get("format") != "trainlog-equipment-catalog" or catalog.get("version") != 1:
        raise ValueError("catalogue équipement v1 invalide")
    return {item["id"] for item in catalog["equipment"]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path,
                        default=Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog" / "trainlog.db")
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    try:
        if connection.execute("PRAGMA user_version;").fetchone()[0] not in (8, 9, 10):
            raise ValueError("schema desktop v8, v9 ou v10 requis")
        known_equipment = load_supplied_equipment_ids(args.catalog)
        known_equipment.update(row[0] for row in connection.execute(
            "SELECT equipment_id FROM custom_equipment"))
        rows = connection.execute(
            "SELECT s.session_id,se.entry_id,e.exercise_id,se.equipment_id FROM session_exercises se "
            "JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id "
            "ORDER BY s.started_at,s.id,se.position").fetchall()
        payload = {"format": "trainlog-equipment-associations", "version": 2,
                   "generated_at": datetime.now().astimezone().isoformat(), "associations": []}
        for session_id, entry_id, exercise_id, equipment_id in rows:
            # CONTRACT: definitions-v1 is published before this unchanged v2
            # reference artifact, so every referenced custom ID must exist.
            if equipment_id is not None and equipment_id not in known_equipment:
                raise ValueError(
                    "équipement non transportable "
                    f"session_id={session_id} entry_id={entry_id} "
                    f"equipment_id={equipment_id}"
                )
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
