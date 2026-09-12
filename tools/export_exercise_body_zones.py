#!/usr/bin/env python3
"""Export the sole bidirectional exercise/body-zone companion v1."""

import argparse
import json
import os
import re
import sqlite3
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "catalog/body-zones-v1.json"
EXERCISE_ID_PATTERN = re.compile(
    r"ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
)


def assignable_zone_ids():
    root = json.loads(CATALOG.read_text(encoding="utf-8"))
    if root.get("format") != "trainlog-body-zone-catalog" or root.get("version") != 1 or \
            not isinstance(root.get("zones"), list):
        raise ValueError("catalogue zones v1 invalide")
    return {
        item["zone_id"] for item in root["zones"]
        if isinstance(item, dict) and item.get("kind") != "group"
    }


def default_database():
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog/trainlog.db"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, default=default_database())
    args = parser.parse_args()
    assignable = assignable_zone_ids()
    connection = sqlite3.connect(args.database)
    connection.row_factory = sqlite3.Row
    try:
        if connection.execute("PRAGMA user_version").fetchone()[0] not in (11, 12, 13):
            raise ValueError("schema desktop v11/v12/v13 requis")
        exercises = []
        for exercise in connection.execute("SELECT id,exercise_id FROM exercises ORDER BY exercise_id"):
            if EXERCISE_ID_PATTERN.fullmatch(exercise["exercise_id"]) is None:
                raise ValueError(f"exercise_id SQLite invalide: {exercise['exercise_id']}")
            primary = connection.execute(
                "SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=? AND role='primary'",
                (exercise["id"],),
            ).fetchone()
            secondary = [row[0] for row in connection.execute(
                "SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=? AND role='secondary' ORDER BY zone_id",
                (exercise["id"],),
            )]
            direct = ([] if primary is None else [primary[0]]) + secondary
            if any(zone_id not in assignable for zone_id in direct) or \
                    len(direct) != len(set(direct)) or \
                    (primary is None and secondary):
                raise ValueError(
                    f"relations de zones SQLite invalides: {exercise['exercise_id']}"
                )
            exercises.append({
                "exercise_id": exercise["exercise_id"],
                "primary_zone_id": None if primary is None else primary[0],
                "secondary_zone_ids": secondary,
            })
        payload = {
            "format": "trainlog-exercise-body-zones",
            "version": 1,
            "generated_at": datetime.now().astimezone().isoformat(),
            "exercises": exercises,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        print("EXERCISE_BODY_ZONES_EXPORT=PASS")
        print(f"exercises={len(exercises)}")
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"EXERCISE_BODY_ZONES_EXPORT=FAIL {error}")
        raise SystemExit(1)
