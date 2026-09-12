#!/usr/bin/env python3
"""Publish desktop sessions through current planning-aware mobile export V3."""
import argparse
import json
import os
import sqlite3
import math
from datetime import datetime
from pathlib import Path

from validate_json import TrainlogSemanticError, parse_timestamp
from exercise_names import load_exercise_names


CATALOG_PATH = Path(__file__).resolve().parents[1] / "catalog" / "equipment-v1.json"


def default_database():
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog" / "trainlog.db"


def supplied_equipment_ids():
    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    if catalog.get("format") != "trainlog-equipment-catalog" or catalog.get("version") != 1:
        raise ValueError("catalog/equipment-v1.json invalide")
    return {item["id"] for item in catalog["equipment"]}


def validate_plan(entry):
    target_sets = entry["target_sets"]
    if target_sets is None:
        if entry["load_mode"] != "none" or entry["rest_seconds"] != 0 or any(
            entry[name] is not None for name in (
                "target_reps", "target_duration_seconds", "target_weight_kg")):
            raise ValueError("plan cible SQLite incohérent")
        return
    if not 1 <= target_sets <= 64 or not 0 <= entry["rest_seconds"] <= 86400:
        raise ValueError("plan cible SQLite hors bornes")
    reps = entry["target_reps"]
    duration = entry["target_duration_seconds"]
    if entry["tracking_mode"] == "reps":
        if reps is None or not 1 <= reps <= 10000 or duration is not None:
            raise ValueError("cible répétitions SQLite incohérente")
    elif duration is None or not 1 <= duration <= 86400 or reps is not None:
        raise ValueError("cible durée SQLite incohérente")
    weight = entry["target_weight_kg"]
    if weight is None:
        if entry["load_mode"] != "none":
            raise ValueError("cible sans poids exige load_mode=none")
    elif not math.isfinite(weight) or weight <= 0 or entry["load_mode"] not in ("external", "assistance"):
        raise ValueError("cible pondérée SQLite incohérente")


def validate_v3_timestamp(value, label):
    try:
        parse_timestamp(value, label)
    except TrainlogSemanticError as error:
        raise ValueError(f"{label}: date-heure Trainlog persistée invalide") from error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, default=default_database())
    parser.add_argument("--version", type=int, choices=(2, 3), default=3)
    args = parser.parse_args()
    con = sqlite3.connect(args.database)
    con.row_factory = sqlite3.Row
    try:
        schema_version = con.execute("PRAGMA user_version").fetchone()[0]
        if schema_version not in (11, 12, 13, 14, 15) and not (args.version == 2 and schema_version == 10):
            raise ValueError("schema desktop v11/v12/v13 requis (v10 accepté pour export V2 explicite)")
        known_equipment = supplied_equipment_ids()
        canonical_names = load_exercise_names()
        known_equipment.update(row[0] for row in con.execute(
            "SELECT equipment_id FROM custom_equipment"))
        root = {"format": "trainlog-mobile-export", "version": args.version,
                "generated_at": datetime.now().astimezone().isoformat(),
                "exercises": [], "sessions": [], "body_observations": []}
        for row in con.execute("SELECT exercise_id,name,recording_mode,tracking_mode,data_fields FROM exercises ORDER BY exercise_id"):
            exported = dict(row)
            exported["name"] = canonical_names.get(row["exercise_id"], row["name"])
            root["exercises"].append(exported)
        for session in con.execute("SELECT id,session_id,started_at,session_type FROM sessions ORDER BY started_at,id"):
            # CONTRACT: a V3 producer must not publish history which the exact
            # temporal readers reject. V2 export retains its published behavior.
            if args.version == 3:
                validate_v3_timestamp(session["started_at"],
                                      f"session_id={session['session_id']} started_at")
            payload = {key: session[key] for key in ("session_id", "started_at", "session_type")}
            payload["exercises"] = []
            # INVARIANT: tracking mode is catalogue metadata. v7 occurrences
            # retain their stable entry_id but do not duplicate that field.
            sql = "SELECT se.id,se.entry_id,se.position,se.recording_mode,e.tracking_mode,se.data_fields,se.equipment_id,e.exercise_id,e.name,mr.max_weight_kg,se.load_mode,se.rest_seconds,se.target_sets,se.target_reps,se.target_duration_seconds,se.target_weight_kg FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN max_results mr ON mr.session_exercise_row_id=se.id WHERE se.session_row_id=? ORDER BY se.position"
            for entry in con.execute(sql, (session["id"],)):
                validate_plan(entry)
                if entry["recording_mode"] == "continuous" or entry["max_weight_kg"] is not None:
                    if entry["target_sets"] is not None:
                        raise ValueError("continuous/MAX ne peut pas porter de cible")
                # CONTRACT: references remain in mobile-export v2 unchanged;
                # definitions-v1 travels first and makes custom IDs resolvable.
                if entry["equipment_id"] is not None and entry["equipment_id"] not in known_equipment:
                    raise ValueError(
                        "équipement non transportable "
                        f"session_id={session['session_id']} "
                        f"entry_id={entry['entry_id']}: {entry['equipment_id']}"
                    )
                has_target = entry["target_sets"] is not None
                if args.version == 2 and has_target:
                    raise ValueError(
                        "export V2 avec plan interdit "
                        f"session_id={session['session_id']} entry_id={entry['entry_id']}"
                    )
                item = {"entry_id": entry["entry_id"], "position": entry["position"],
                        "exercise_id": entry["exercise_id"],
                        "name": canonical_names.get(entry["exercise_id"], entry["name"]),
                        "recording_mode": entry["recording_mode"], "tracking_mode": entry["tracking_mode"],
                        "data_fields": entry["data_fields"],
                        "load_mode": entry["load_mode"] if args.version == 3 else "none",
                        "rest_seconds": entry["rest_seconds"] if args.version == 3 else 0,
                        "equipment_id": entry["equipment_id"]}
                if args.version == 3:
                    if not has_target:
                        if entry["load_mode"] != "none" or entry["rest_seconds"] != 0 or any(
                            entry[name] is not None for name in (
                                "target_reps", "target_duration_seconds", "target_weight_kg")):
                            raise ValueError("plan cible SQLite incohérent")
                        item["target"] = None
                    else:
                        target = {"sets": entry["target_sets"]}
                        if entry["target_reps"] is not None: target["reps"] = entry["target_reps"]
                        if entry["target_duration_seconds"] is not None: target["duration_seconds"] = entry["target_duration_seconds"]
                        if entry["target_weight_kg"] is not None: target["weight_kg"] = entry["target_weight_kg"]
                        item["target"] = target
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
            if args.version == 3:
                # INVARIANT: validate every stored instant before touching the
                # destination artifact, so corruption cannot clobber a prior file.
                validate_v3_timestamp(row["observed_at"],
                                      f"observation_id={row['observation_id']} observed_at")
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
