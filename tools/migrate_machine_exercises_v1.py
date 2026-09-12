#!/usr/bin/env python3
"""Apply MACHINE_EXERCISE_MODEL_V1 to an explicit non-live SQLite database.

The executor is intentionally manifest-driven and refuses the canonical live
desktop path. Context splits match entry ID, canonical source exercise ID and
the exact legacy equipment ID before any write begins.
"""

import argparse
import json
import os
import sqlite3
from pathlib import Path

from exercise_names import normalize_catalog_name

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = Path("/tmp/trainlog-machine-exercise-manifest/migration-manifest-v1.json")
LIVE_DB = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog/trainlog.db"
COLUMNS = (
    ("load_semantics", "TEXT CHECK(load_semantics IN ('none','external','assistance','bodyweight','cardio'))"),
    ("machine_variant", "TEXT"),
    ("machine_provenance", "TEXT"),
    ("scientific_profile_id", "TEXT"),
    ("science_state", "TEXT NOT NULL DEFAULT 'unresolved' CHECK(science_state IN ('resolved','unresolved'))"),
    ("legacy_equipment_id", "TEXT"),
)


def fail(message):
    raise RuntimeError(message)


def row_id(connection, exercise_id):
    row = connection.execute("SELECT id FROM exercises WHERE exercise_id=?", (exercise_id,)).fetchone()
    return None if row is None else row[0]


def split_facts(connection, entry_ids):
    placeholders = ",".join("?" for _ in entry_ids)
    return connection.execute(f"""
        SELECT se.entry_id,e.exercise_id,se.equipment_id,
          (SELECT count(*) FROM performed_sets p WHERE p.session_exercise_row_id=se.id),
          (SELECT count(*) FROM max_results m WHERE m.session_exercise_row_id=se.id),
          (SELECT count(*) FROM continuous_activity c WHERE c.session_exercise_row_id=se.id)
        FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id
        WHERE se.entry_id IN ({placeholders}) ORDER BY se.entry_id
    """, entry_ids).fetchall()


def validate_preconditions(connection, manifest):
    version = connection.execute("PRAGMA user_version").fetchone()[0]
    if version not in (12, 13):
        fail(f"schema v12 or v13 required, found v{version}")
    split_entries = []
    for rename in manifest["renames"]:
        row = connection.execute("SELECT name FROM exercises WHERE exercise_id=?", (rename["exercise_id"],)).fetchone()
        if row is None:
            fail(f"rename source missing: {rename['exercise_id']}")
        if version == 12 and row[0] != rename["old_display_name"]:
            fail(f"rename source name mismatch: {rename['exercise_id']}")
        if version == 13 and row[0] != rename["new_display_name"]:
            fail(f"migrated rename mismatch: {rename['exercise_id']}")
    for split in manifest["context_splits"]:
        split_entries.extend(split["entry_ids"])
        facts = split_facts(connection, split["entry_ids"])
        expected_exercise = split["target_exercise_id"] if version == 13 else split["source_exercise_id"]
        if len(facts) != split["expected_occurrence_count"] or {row[0] for row in facts} != set(split["entry_ids"]):
            fail(f"split occurrence mismatch: {split['target_exercise_id']}")
        if any(row[1] != expected_exercise or row[2] != split["source_equipment_id"] for row in facts):
            fail(f"split identity/context mismatch: {split['target_exercise_id']}")
        if sum(row[3] for row in facts) != split["expected_performed_set_count"] or sum(row[4] for row in facts) != split["expected_max_count"] or sum(row[5] for row in facts) != split["expected_continuous_count"]:
            fail(f"split child count mismatch: {split['target_exercise_id']}")
    if len(split_entries) != len(set(split_entries)):
        fail("one entry appears in multiple split targets")
    unresolved = set()
    for item in manifest["unresolved_contexts"]:
        unresolved.update([item["entry_id"]] if "entry_id" in item else item.get("historical_entry_ids", []))
    if unresolved & set(split_entries):
        fail("unresolved entry included in automatic split")


def profile_interpretations():
    profiles = json.loads((ROOT / "catalog/scientific-profiles-v1.json").read_text(encoding="utf-8"))["profiles"]
    exercises = {row["exercise_id"]: row for row in json.loads((ROOT / "catalog/exercise-knowledge-v1.json").read_text(encoding="utf-8"))["exercises"]}
    equipment = {row["equipment_id"]: row for row in json.loads((ROOT / "catalog/equipment-knowledge-v1.json").read_text(encoding="utf-8"))["equipment"]}
    result = {}
    for profile in profiles:
        source = profile["knowledge_source"]
        if source["kind"] == "exercise":
            interpretation = exercises[source["exercise_id"]].get("interpretation")
        else:
            capability = next(cap for cap in equipment[source["equipment_id"]]["capabilities"] if cap["display_name"] == source["capability"])
            interpretation = capability.get("interpretation")
        if interpretation is None:
            fail(f"resolved profile has no interpretation: {profile['scientific_profile_id']}")
        result[profile["scientific_profile_id"]] = interpretation
    return result


def seed_zones(connection, exercise_id, interpretation):
    target = row_id(connection, exercise_id)
    if target is None:
        fail(f"cannot seed zones for missing exercise: {exercise_id}")
    existing = connection.execute("SELECT zone_id,role FROM exercise_body_zones WHERE exercise_row_id=? ORDER BY role,zone_id", (target,)).fetchall()
    expected = [(interpretation["primary_zone_id"], "primary")] + [(zone, "secondary") for zone in sorted(interpretation["secondary_zone_ids"])]
    if existing:
        if sorted(existing) != sorted(expected):
            fail(f"BODY ZONE conflict for {exercise_id}")
    else:
        connection.executemany("INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) VALUES(?,?,?)", [(target, zone, role) for zone, role in expected])
    state = interpretation["primary_zone_id"] + "|" + ",".join(sorted(interpretation["secondary_zone_ids"]))
    connection.execute("INSERT OR IGNORE INTO exercise_body_zone_sync(exercise_row_id,synced_state) VALUES(?,?)", (target, state))


def apply(connection, manifest):
    validate_preconditions(connection, manifest)
    if connection.execute("PRAGMA user_version").fetchone()[0] == 13:
        return False
    interpretations = profile_interpretations()
    connection.execute("BEGIN IMMEDIATE")
    try:
        for name, definition in COLUMNS:
            connection.execute(f"ALTER TABLE exercises ADD COLUMN {name} {definition}")
        for rename in manifest["renames"]:
            changed = connection.execute("UPDATE exercises SET name=?,normalized_name=?,load_semantics=?,machine_variant=NULL,scientific_profile_id=?,science_state='resolved',legacy_equipment_id=? WHERE exercise_id=? AND name=?",
                (rename["new_display_name"], normalize_catalog_name(rename["new_display_name"]), rename["load_semantics"], rename["scientific_profile_id"], rename["legacy_equipment_id"], rename["exercise_id"], rename["old_display_name"])).rowcount
            if changed != 1:
                fail(f"rename update failed: {rename['exercise_id']}")
        machine_rows = {row["exercise_id"]: row for row in json.loads((ROOT / "catalog/machine-exercises-v1.json").read_text(encoding="utf-8"))["exercises"]}
        for rename in manifest["renames"]:
            machine = machine_rows[rename["exercise_id"]]
            connection.execute("UPDATE exercises SET machine_variant=? WHERE exercise_id=?", (machine["machine_variant"], rename["exercise_id"]))
        for item in manifest["new_exercises"]:
            machine = machine_rows[item["exercise_id"]]
            connection.execute("""INSERT INTO exercises(exercise_id,name,normalized_name,tracking_mode,recording_mode,data_fields,load_semantics,machine_variant,machine_provenance,scientific_profile_id,science_state,legacy_equipment_id)
                VALUES(?,?,?,?,?,?,?,?,NULL,?,?,?)""", (item["exercise_id"], item["display_name"], normalize_catalog_name(item["display_name"]), item["tracking_mode"], item["recording_mode"], item["data_fields"], item["load_semantics"], machine["machine_variant"], item["scientific_profile_id"], machine["science_state"], item["legacy_equipment_id"]))
            if item["scientific_profile_id"] is not None:
                seed_zones(connection, item["exercise_id"], interpretations[item["scientific_profile_id"]])
        for split in manifest["context_splits"]:
            target = row_id(connection, split["target_exercise_id"])
            source = row_id(connection, split["source_exercise_id"])
            for entry in split["entry_ids"]:
                changed = connection.execute("UPDATE session_exercises SET exercise_row_id=? WHERE entry_id=? AND exercise_row_id=? AND equipment_id=?", (target, entry, source, split["source_equipment_id"])).rowcount
                if changed != 1:
                    fail(f"exact split update failed: {entry}")
        seated = machine_rows["ex_617007f9-7420-4408-91b9-8ffb77900f13"]
        connection.execute("UPDATE exercises SET load_semantics=NULL,machine_variant=NULL,machine_provenance=NULL,scientific_profile_id=NULL,science_state='unresolved',legacy_equipment_id=NULL WHERE exercise_id=?", (seated["exercise_id"],))
        connection.execute("PRAGMA user_version=13")
        validate_preconditions(connection, manifest)
        if connection.execute("PRAGMA foreign_key_check").fetchone() is not None:
            fail("foreign key check failed")
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--audit", type=Path)
    args = parser.parse_args()
    database = args.database.resolve()
    if database == LIVE_DB.resolve():
        fail("refusing to migrate the canonical live desktop database")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    connection = sqlite3.connect(database)
    connection.execute("PRAGMA foreign_keys=ON")
    changed = apply(connection, manifest)
    integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
    version = connection.execute("PRAGMA user_version").fetchone()[0]
    connection.close()
    audit = {"format":"trainlog-machine-exercise-migration-audit","version":1,"database":str(database),"changed":changed,"schema_version":version,"integrity_check":integrity}
    if args.audit:
        args.audit.write_text(json.dumps(audit, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(audit, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
