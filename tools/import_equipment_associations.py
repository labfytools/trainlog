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


def validate_associations(associations, known):
    """Validate the complete v2 payload before any database mutation."""
    validated = []
    seen = set()
    for item in associations:
        if not isinstance(item, dict):
            fail("association invalide")
        session_id, entry_id, exercise_id, state = (
            item.get(key) for key in ("session_id", "entry_id", "exercise_id", "state"))
        key = (session_id, entry_id)
        if (not isinstance(session_id, str) or not session_id or
                not isinstance(entry_id, str) or not entry_id or
                not isinstance(exercise_id, str) or not exercise_id or key in seen):
            fail("identité association invalide ou dupliquée")
        seen.add(key)
        if state == "set":
            if set(item) != {"session_id", "entry_id", "exercise_id", "state", "equipment_id"}:
                fail("association set invalide")
            equipment_id = item["equipment_id"]
            # CONTRACT: the definitions-v1 companion is imported first, so a
            # custom ID is as valid here as a supplied catalog ID.
            if not isinstance(equipment_id, str) or equipment_id not in known:
                fail("équipement inconnu: "
                     f"session_id={session_id} entry_id={entry_id} equipment_id={equipment_id}")
        elif state == "cleared":
            if set(item) != {"session_id", "entry_id", "exercise_id", "state"}:
                fail("association cleared invalide")
            equipment_id = None
        else:
            fail("état association invalide")
        validated.append((session_id, entry_id, exercise_id, equipment_id))
    return validated


def load_mobile_occurrences(path):
    """Return the source V2 identity claimed for every stable occurrence."""
    payload = json.loads(path.read_text(encoding="utf-8"))
    if (
        payload.get("format") != "trainlog-mobile-export"
        or payload.get("version") != 2
        or not isinstance(payload.get("sessions"), list)
    ):
        fail("snapshot mobile V2 de preuve invalide")

    occurrences = {}
    for session in payload["sessions"]:
        if not isinstance(session, dict) or not isinstance(
            session.get("exercises"), list
        ):
            fail("snapshot mobile V2 de preuve invalide")
        session_id = session.get("session_id")
        for item in session["exercises"]:
            if not isinstance(item, dict):
                fail("snapshot mobile V2 de preuve invalide")
            entry_id = item.get("entry_id")
            exercise_id = item.get("exercise_id")
            key = (session_id, entry_id)
            if (
                not isinstance(session_id, str)
                or not session_id
                or not isinstance(entry_id, str)
                or not entry_id
                or not isinstance(exercise_id, str)
                or not exercise_id
                or key in occurrences
            ):
                fail("identité du snapshot mobile V2 de preuve invalide")
            occurrences[key] = exercise_id
    return occurrences


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--catalog", type=Path,
                        default=Path(__file__).resolve().parents[1] / "catalog/equipment-v1.json")
    parser.add_argument(
        "--mobile-export",
        type=Path,
        help=(
            "snapshot mobile V2 importé juste avant ce companion; requis "
            "pour prouver un exercise_id réconcilié"
        ),
    )
    args = parser.parse_args()
    payload = json.loads(args.artifact.read_text(encoding="utf-8"))
    if payload.get("format") != FORMAT or payload.get("version") != VERSION:
        fail("extension équipement non supportée")
    if set(payload) != {"format", "version", "generated_at", "associations"}:
        fail("clés extension équipement invalides")
    connection = sqlite3.connect(args.database)
    try:
        if connection.execute("PRAGMA user_version;").fetchone()[0] not in (8, 9):
            fail("schema desktop v8 ou v9 requis")
        known = load_catalog(args.catalog)
        known.update(row[0] for row in connection.execute(
            "SELECT equipment_id FROM custom_equipment"))
        associations = validate_associations(payload["associations"], known)
        proof_path = args.mobile_export
        if proof_path is None:
            candidate = args.artifact.with_name("trainlog-mobile-export-v2.json")
            if candidate.exists():
                proof_path = candidate
        mobile_occurrences = (
            load_mobile_occurrences(proof_path)
            if proof_path is not None
            else {}
        )
        for session_id, entry_id, exercise_id, equipment_id in associations:
            exists = connection.execute(
                "SELECT e.exercise_id,se.equipment_id FROM session_exercises se "
                "JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id "
                "WHERE s.session_id=? AND se.entry_id=?",
                (session_id, entry_id)).fetchone()
            if exists is None:
                fail(f"entrée séance inconnue: {session_id}/{entry_id}")
            if exists[0] != exercise_id:
                proof = mobile_occurrences.get((session_id, entry_id))
                incoming_still_exists = connection.execute(
                    "SELECT 1 FROM exercises WHERE exercise_id=?;",
                    (exercise_id,),
                ).fetchone() is not None
                if proof != exercise_id or incoming_still_exists:
                    fail(f"conflit exercice association: {session_id}/{entry_id}")
                # WHY: import_mobile_export may have replaced a safe duplicate
                # creator ID with the canonical desktop ID.  entry_id is the V2
                # occurrence identity; the just-validated source snapshot proves
                # that this stale exercise_id belongs to that same occurrence.
            if exists[1] != equipment_id:
                fail(f"conflit association équipement: {session_id}/{entry_id}")
        print("EQUIPMENT_ASSOCIATIONS_IMPORT=PASS")
        print(f"associations={len(associations)}")
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"EQUIPMENT_ASSOCIATIONS_IMPORT=FAIL {error}")
        raise SystemExit(1)
