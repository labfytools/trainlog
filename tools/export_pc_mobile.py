#!/usr/bin/env python3
"""Publish desktop sessions through the occurrence-aware mobile export V2."""
import argparse
import json
import os
import sqlite3
from datetime import datetime
from pathlib import Path


def default_database():
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog" / "trainlog.db"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, default=default_database())
    args = parser.parse_args()
    con = sqlite3.connect(args.database)
    con.row_factory = sqlite3.Row
    try:
        if con.execute("PRAGMA user_version").fetchone()[0] != 7:
            raise ValueError("schema desktop v7 requis")
        root = {"format": "trainlog-mobile-export", "version": 2,
                "generated_at": datetime.now().astimezone().isoformat(),
                "exercises": [], "sessions": [], "body_observations": []}
        for row in con.execute("SELECT exercise_id,name,recording_mode,tracking_mode,data_fields FROM exercises ORDER BY exercise_id"):
            root["exercises"].append(dict(row))
        for session in con.execute("SELECT id,session_id,started_at,session_type FROM sessions ORDER BY started_at,id"):
            payload = {key: session[key] for key in ("session_id", "started_at", "session_type")}
            payload["exercises"] = []
            # INVARIANT: tracking mode is catalogue metadata. v7 occurrences
            # retain their stable entry_id but do not duplicate that field.
            sql = "SELECT se.id,se.entry_id,se.position,se.recording_mode,e.tracking_mode,se.data_fields,se.equipment_id,e.exercise_id,e.name FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id WHERE se.session_row_id=? ORDER BY se.position"
            for entry in con.execute(sql, (session["id"],)):
                item = {"entry_id": entry["entry_id"], "position": entry["position"],
                        "exercise_id": entry["exercise_id"], "name": entry["name"],
                        "recording_mode": entry["recording_mode"], "tracking_mode": entry["tracking_mode"],
                        "data_fields": entry["data_fields"], "load_mode": "none", "rest_seconds": 0,
                        "equipment_id": entry["equipment_id"]}
                if entry["recording_mode"] == "continuous":
                    activity = con.execute("SELECT duration_seconds,speed_kmh,distance_km FROM continuous_activity WHERE session_exercise_row_id=?", (entry["id"],)).fetchone()
                    if activity is None: raise ValueError("activité continue absente")
                    item["continuous"] = {key: activity[key] for key in activity.keys() if activity[key] is not None}
                else:
                    item["sets"] = []
                    for value in con.execute("SELECT reps,duration_seconds,weight_kg FROM performed_sets WHERE session_exercise_row_id=? ORDER BY position", (entry["id"],)):
                        metric = "reps" if entry["tracking_mode"] == "reps" else "duration_seconds"
                        set_value = {metric: value[metric]}
                        if value["weight_kg"] is not None: set_value["weight_kg"] = value["weight_kg"]
                        item["sets"].append(set_value)
                payload["exercises"].append(item)
            root["sessions"].append(payload)
        args.output.write_text(json.dumps(root, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        print("PC_MOBILE_EXPORT=PASS")
        print("sessions=" + str(len(root["sessions"])))
    finally:
        con.close()


if __name__ == "__main__":
    try: main()
    except Exception as exc:
        print("PC_MOBILE_EXPORT=FAIL " + str(exc))
        raise SystemExit(1)
