#!/usr/bin/env python3
"""Add/reconcile a strict custom-equipment definitions-v1 snapshot."""
import argparse
import json
import sqlite3
from pathlib import Path
from trainlog_sqlite import connect_database

FORMAT = "trainlog-equipment-definitions"
VERSION = 1
ROOT_KEYS = {"format", "version", "generated_at", "equipment"}
ITEM_KEYS = {"equipment_id", "display_name", "label_name", "equipment_type", "load_semantics"}
SEMANTICS = {"none", "external", "assistance"}


def fail(message):
    raise ValueError(message)


def supplied_ids(path):
    root = json.loads(path.read_text(encoding="utf-8"))
    if root.get("format") != "trainlog-equipment-catalog" or root.get("version") != 1:
        fail("catalogue équipement fourni non supporté")
    return {item["id"] for item in root["equipment"]}


def validate(payload, reserved):
    if not isinstance(payload, dict) or set(payload) != ROOT_KEYS:
        fail("clés racine définitions équipement invalides")
    if payload["format"] != FORMAT or payload["version"] != VERSION:
        fail("définitions équipement non supportées")
    if not isinstance(payload["generated_at"], str) or not payload["generated_at"]:
        fail("generated_at invalide")
    if not isinstance(payload["equipment"], list):
        fail("equipment doit être un tableau")
    output, seen = [], set()
    for index, item in enumerate(payload["equipment"]):
        if not isinstance(item, dict) or set(item) != ITEM_KEYS:
            fail(f"equipment[{index}]: clés invalides")
        values = tuple(item[key] for key in
                       ("equipment_id", "display_name", "label_name", "equipment_type", "load_semantics"))
        if any(not isinstance(value, str) for value in values):
            fail(f"equipment[{index}]: chaînes attendues")
        equipment_id, display_name, label_name, equipment_type, load_semantics = values
        if (not equipment_id or equipment_id in seen or equipment_id in reserved or
                not display_name.strip() or len(display_name) > 120 or
                not equipment_type or load_semantics not in SEMANTICS):
            fail(f"equipment[{index}]: définition invalide ou ID fourni réservé: {equipment_id}")
        seen.add(equipment_id)
        output.append(values)
    return output


def apply_definitions(connection, definitions, complete_causal_envelope=False):
    """Apply validated definitions without owning the surrounding transaction."""
    supported_versions = range(8, 37)
    if connection.execute("PRAGMA user_version").fetchone()[0] not in supported_versions:
        fail("schema desktop v8 à v36 requis")
    if not complete_causal_envelope and connection.execute("PRAGMA user_version").fetchone()[0] >= 20 and connection.execute("SELECT 1 FROM sync_causal_state WHERE target_kind='custom_equipment' AND deleted=1 LIMIT 1").fetchone():
        fail("causal equipment protection requires the staged artifact")
    imported = skipped = 0
    for definition in definitions:
        row = connection.execute("SELECT equipment_id,display_name,label_name,equipment_type,load_semantics FROM custom_equipment WHERE equipment_id=?", (definition[0],)).fetchone()
        if row is not None and tuple(row) != definition: fail(f"conflit définition équipement: {definition[0]}")
    for definition in definitions:
        cursor = connection.execute("INSERT OR IGNORE INTO custom_equipment(equipment_id,display_name,label_name,equipment_type,load_semantics) VALUES(?,?,?,?,?)", definition)
        if cursor.rowcount == 1: imported += 1
        else: skipped += 1
    return imported, skipped


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--catalog", type=Path,
                        default=Path(__file__).resolve().parents[1] / "catalog/equipment-v1.json")
    args = parser.parse_args()
    definitions = validate(json.loads(args.artifact.read_text(encoding="utf-8")), supplied_ids(args.catalog))
    connection = connect_database(args.database)
    try:
        with connection:
            imported, skipped = apply_definitions(connection, definitions)
        print("EQUIPMENT_DEFINITIONS_IMPORT=PASS")
        print(f"definitions_imported={imported}")
        print(f"definitions_skipped={skipped}")
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"EQUIPMENT_DEFINITIONS_IMPORT=FAIL {error}")
        raise SystemExit(1)
