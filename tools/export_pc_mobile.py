#!/usr/bin/env python3
"""Publish desktop sessions through the occurrence-aware mobile export V2."""
import argparse
import json
import os
import sqlite3
from datetime import datetime
from pathlib import Path


CATALOG_PATH = Path(__file__).resolve().parents[1] / "catalog" / "equipment-v1.json"


def default_database():
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog" / "trainlog.db"


def supplied_equipment_ids():
    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    if catalog.get("format") != "trainlog-equipment-catalog" or catalog.get("version") != 1:
        raise ValueError("catalog/equipment-v1.json invalide")
    return {item["id"] for item in catalog["equipment"]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, default=default_database())
    args = parser.parse_args()
    con = sqlite3.connect(args.database)
    con.row_factory = sqlite3.Row
    try:
        if con.execute("PRAGMA user_version").fetchone()[0] != 9:
            raise ValueError("schema desktop v9 requis")
        known_equipment = supplied_equipment_ids()
        known_equipment.update(row[0] for row in con.execute(
            "SELECT equipment_id FROM custom_equipment"))
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
            sql = "SELECT se.id,se.entry_id,se.position,se.recording_mode,e.tracking_mode,se.data_fields,se.equipment_id,e.exercise_id,e.name,mr.max_weight_kg FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN max_results mr ON mr.session_exercise_row_id=se.id WHERE se.session_row_id=? ORDER BY se.position"
            for entry in con.execute(sql, (session["id"],)):
                # CONTRACT: references remain in mobile-export v2 unchanged;
                # definitions-v1 travels first and makes custom IDs resolvable.
                if entry["equipment_id"] is not None and entry["equipment_id"] not in known_equipment:
                    raise ValueError(
                        "équipement non transportable "
                        f"session_id={session['session_id']} "
                        f"entry_id={entry['entry_id']}: {entry['equipment_id']}"
                    )
                item = {"entry_id": entry["entry_id"], "position": entry["position"],
                        "exercise_id": entry["exercise_id"], "name": entry["name"],
                        "recording_mode": entry["recording_mode"], "tracking_mode": entry["tracking_mode"],
                        "data_fields": entry["data_fields"], "load_mode": "none", "rest_seconds": 0,
                        "equipment_id": entry["equipment_id"]}
                if entry["max_weight_kg"] is not None:
                    # CONTRACT: explicit max is an occurrence result, never a
                    # synthetic one-repetition performed set.
                    item["max_weight_kg"] = entry["max_weight_kg"]
                elif entry["recording_mode"] == "continuous":
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
        metric_names = ("body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm",
                        "waist_cm", "hips_cm", "left_arm_cm", "right_arm_cm",
                        "left_forearm_cm", "right_forearm_cm", "left_thigh_cm",
                        "right_thigh_cm", "left_calf_cm", "right_calf_cm")
        columns = ",".join(("observation_id", "observed_at") + metric_names)
        for row in con.execute(f"SELECT {columns} FROM body_observations ORDER BY observed_at,id"):
            item = {"observation_id": row["observation_id"], "observed_at": row["observed_at"]}
            item.update({name: row[name] for name in metric_names if row[name] is not None})
            root["body_observations"].append(item)
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
