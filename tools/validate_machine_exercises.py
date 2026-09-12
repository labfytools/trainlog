#!/usr/bin/env python3
"""Validate MACHINE_EXERCISE_MODEL_V1 catalogs without inferring science."""

import json
import re
from pathlib import Path

from exercise_names import normalize_catalog_name

ROOT = Path(__file__).resolve().parents[1]
EXERCISE_ID = re.compile(r"^ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")
PROFILE_ID = re.compile(r"^sp_[a-z0-9_]+_v1$")
LOADS = {"none", "external", "assistance", "bodyweight", "cardio"}


def load(name):
    return json.loads((ROOT / "catalog" / name).read_text(encoding="utf-8"))


def main():
    machine = load("machine-exercises-v1.json")
    profiles = load("scientific-profiles-v1.json")
    equipment = load("equipment-v1.json")
    knowledge = load("exercise-knowledge-v1.json")
    equipment_knowledge = load("equipment-knowledge-v1.json")
    if set(machine) != {"format", "version", "exercises"} or machine["format"] != "trainlog-machine-exercises" or machine["version"] != 1:
        raise ValueError("invalid machine exercise envelope")
    if set(profiles) != {"format", "version", "profiles"} or profiles["format"] != "trainlog-scientific-profiles" or profiles["version"] != 1:
        raise ValueError("invalid scientific profile envelope")
    equipment_ids = {row["id"] for row in equipment["equipment"]}
    exercise_knowledge = {row["exercise_id"] for row in knowledge["exercises"]}
    capabilities = {(row["equipment_id"], cap["display_name"])
                    for row in equipment_knowledge["equipment"] for cap in row["capabilities"]}
    profile_ids = set()
    previous = ""
    for row in profiles["profiles"]:
        if set(row) != {"scientific_profile_id", "knowledge_source"}:
            raise ValueError("invalid scientific profile record")
        profile_id = row["scientific_profile_id"]
        if not PROFILE_ID.fullmatch(profile_id) or profile_id <= previous or profile_id in profile_ids:
            raise ValueError("scientific profiles must be unique and sorted")
        previous = profile_id
        profile_ids.add(profile_id)
        source = row["knowledge_source"]
        if source.get("kind") == "exercise":
            if set(source) != {"kind", "exercise_id"} or source["exercise_id"] not in exercise_knowledge:
                raise ValueError("unknown exercise knowledge source")
        elif source.get("kind") == "equipment_capability":
            if set(source) != {"kind", "equipment_id", "capability"} or (source["equipment_id"], source["capability"]) not in capabilities:
                raise ValueError("unknown equipment capability source")
        else:
            raise ValueError("invalid profile source kind")
    seen_ids = set()
    seen_names = set()
    previous = ""
    for row in machine["exercises"]:
        required = {"exercise_id", "display_name", "load_semantics", "machine_variant", "legacy_equipment_id", "scientific_profile_id", "science_state"}
        if set(row) != required or not EXERCISE_ID.fullmatch(row["exercise_id"]):
            raise ValueError("invalid machine exercise record")
        if row["exercise_id"] <= previous or row["exercise_id"] in seen_ids:
            raise ValueError("machine exercises must be unique and sorted")
        previous = row["exercise_id"]
        seen_ids.add(row["exercise_id"])
        name = normalize_catalog_name(row["display_name"])
        if name in seen_names:
            raise ValueError("duplicate machine exercise name")
        seen_names.add(name)
        if row["load_semantics"] is not None and row["load_semantics"] not in LOADS:
            raise ValueError("invalid load semantics")
        if row["legacy_equipment_id"] is not None and row["legacy_equipment_id"] not in equipment_ids:
            raise ValueError("unknown legacy equipment")
        if row["scientific_profile_id"] is not None and row["scientific_profile_id"] not in profile_ids:
            raise ValueError("unknown scientific profile")
        resolved = row["science_state"] == "resolved"
        if row["science_state"] not in {"resolved", "unresolved"} or resolved != (row["scientific_profile_id"] is not None):
            raise ValueError("science state/profile mismatch")
    seated_leg = next(row for row in machine["exercises"] if row["exercise_id"] == "ex_617007f9-7420-4408-91b9-8ffb77900f13")
    if any(seated_leg[key] is not None for key in ("load_semantics", "legacy_equipment_id", "scientific_profile_id")):
        raise ValueError("Seated Leg must remain unresolved")
    print("MACHINE_EXERCISE_MODEL_V1 catalogs: OK")


if __name__ == "__main__":
    main()
