#!/usr/bin/env python3
"""Apply the versioned equipment companion artifact to a desktop database."""
import argparse
import json
import sqlite3
from pathlib import Path

FORMAT = "trainlog-equipment-associations"
VERSION = 2


def fail(message):
    raise ValueError(message)


def load_catalog(path):
    root = json.loads(path.read_text(encoding="utf-8"))
    if root.get("format") != "trainlog-equipment-catalog" or root.get("version") != 1:
        fail("catalogue équipement non supporté")
    return {item["id"] for item in root["equipment"]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--catalog", type=Path,
                        default=Path(__file__).resolve().parents[1] / "catalog/equipment-v1.json")
    args = parser.parse_args()
    payload = json.loads(args.artifact.read_text(encoding="utf-8"))
    if payload.get("format") != FORMAT or payload.get("version") != VERSION:
        fail("extension équipement non supportée")
    if set(payload) != {"format", "version", "generated_at", "associations"}:
        fail("clés extension équipement invalides")
    known = load_catalog(args.catalog)
    connection = sqlite3.connect(args.database)
    try:
        if connection.execute("PRAGMA user_version;").fetchone()[0] != 7:
            fail("schema desktop v7 requis")
        seen = set()
        with connection:
            for item in payload["associations"]:
                if not isinstance(item, dict):
                    fail("association invalide")
                session_id, entry_id, exercise_id, state = (item.get(k) for k in ("session_id", "entry_id", "exercise_id", "state"))
                key = (session_id, entry_id)
                if not isinstance(session_id, str) or not session_id or not isinstance(entry_id, str) or not entry_id or not isinstance(exercise_id, str) or not exercise_id or key in seen:
                    fail("identité association invalide ou dupliquée")
                seen.add(key)
                if state == "set":
                    if set(item) != {"session_id", "entry_id", "exercise_id", "state", "equipment_id"}:
                        fail("association set invalide")
                    equipment_id = item["equipment_id"]
                    if not isinstance(equipment_id, str) or equipment_id not in known:
                        fail(f"équipement inconnu: {equipment_id}")
                elif state == "cleared":
                    if set(item) != {"session_id", "entry_id", "exercise_id", "state"}:
                        fail("association cleared invalide")
                    equipment_id = None
                else:
                    fail("état association invalide")
                cursor = connection.execute(
                    "UPDATE session_exercises SET equipment_id=? WHERE id=("
                    "SELECT se.id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id "
                    "WHERE s.session_id=? AND se.entry_id=?)",
                    (equipment_id, session_id, entry_id))
                if cursor.rowcount != 1:
                    fail(f"entrée séance inconnue: {session_id}/{entry_id}")
        print("EQUIPMENT_ASSOCIATIONS_IMPORT=PASS")
        print(f"associations={len(seen)}")
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"EQUIPMENT_ASSOCIATIONS_IMPORT=FAIL {error}")
        raise SystemExit(1)
