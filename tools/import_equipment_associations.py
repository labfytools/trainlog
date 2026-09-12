#!/usr/bin/env python3
"""Apply the versioned equipment companion artifact to a desktop database."""
import argparse
import json
import sqlite3
from pathlib import Path

FORMAT = "trainlog-equipment-associations"
VERSION = 2

# CONTRACT: MACHINE_EXERCISE_MODEL_V1 split only these completed occurrences.
# The legacy exercise IDs are deliberately not aliases: other occurrences of
# the same source exercise remain valid unsplit history.
MACHINE_EXERCISE_SPLITS = {
    ("se_c0d07454-b58b-403e-8b79-744619815fc5", "sxe_draft_legacy_7"):
        ("ex_b432623f-bfe9-4daf-a653-60ec7fdffbde",
         "ex_0e26c06f-a458-40a4-be20-4ed219ede30d"),
    ("se_c0d07454-b58b-403e-8b79-744619815fc5",
     "sxe_093c1331-beaa-4b69-91b3-240292709be6"):
        ("ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d",
         "ex_f01d2a46-6984-4dec-8934-4d82fca6dfc2"),
    ("se_ac3908d6-8e3e-4ac6-81a6-62dc2c39075a",
     "sxe_f25142c8-455e-4346-9bfc-31d0989e275d"):
        ("ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d",
         "ex_f01d2a46-6984-4dec-8934-4d82fca6dfc2"),
    ("se_8a7ac0cb-9e67-42b6-bac9-179bbedf0024",
     "sxe_f2242691-ba6c-48a0-b938-a1f744375d75"):
        ("ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d",
         "ex_f01d2a46-6984-4dec-8934-4d82fca6dfc2"),
    ("se_8e6baa18-53ff-486b-b011-02a15291789e",
     "sxe_9f8882f4-069e-49d5-8216-19d16b467e4a"):
        ("ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d",
         "ex_f01d2a46-6984-4dec-8934-4d82fca6dfc2"),
}


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
    """Return the current V2/V3 identity claimed for every stable occurrence."""
    payload = json.loads(path.read_text(encoding="utf-8"))
    if (
        payload.get("format") != "trainlog-mobile-export"
        or payload.get("version") not in (2, 3)
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


def approved_machine_split(session_id, entry_id, incoming_id, local_id,
                           mobile_occurrences):
    """Corroborate one frozen completed-history split with the current snapshot."""
    expected = MACHINE_EXERCISE_SPLITS.get((session_id, entry_id))
    if expected != (incoming_id, local_id):
        return False
    # INVARIANT: the stale companion alone is never authority. The mobile
    # snapshot selected and imported in this same sync must already carry the
    # exact v13 target identity for this stable completed occurrence.
    return mobile_occurrences.get((session_id, entry_id)) == local_id


def canonical_exercise_id(connection, exercise_id):
    """Resolve one flattened durable alias without inventing identity."""
    aliases_available = connection.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='exercise_aliases';"
    ).fetchone() is not None
    if not aliases_available:
        return exercise_id
    alias = connection.execute(
        "SELECT canonical_exercise_id FROM exercise_aliases "
        "WHERE source_exercise_id=?;",
        (exercise_id,),
    ).fetchone()
    if alias is None:
        return exercise_id
    target_exists = connection.execute(
        "SELECT 1 FROM exercises WHERE exercise_id=?;", (alias[0],)
    ).fetchone() is not None
    if not target_exists:
        fail(f"alias exercice sans cible canonique: {exercise_id}")
    return alias[0]


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
            "snapshot mobile V2/V3 importé juste avant ce companion; requis "
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
        if connection.execute("PRAGMA user_version;").fetchone()[0] not in (8, 9, 10, 11, 12, 13):
            fail("schema desktop v8 à v13 requis")
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
            # WHY: the association companion can outlive the creator ID used
            # by its source occurrence. The durable flattened alias is the
            # synchronization identity evidence and is stronger than raw text.
            stored_canonical = canonical_exercise_id(connection, exists[0])
            incoming_canonical = canonical_exercise_id(connection, exercise_id)
            if stored_canonical != incoming_canonical:
                proof = mobile_occurrences.get((session_id, entry_id))
                incoming_still_exists = connection.execute(
                    "SELECT 1 FROM exercises WHERE exercise_id=?;",
                    (exercise_id,),
                ).fetchone() is not None
                alias_reconciliation = proof == exercise_id and not incoming_still_exists
                split_reconciliation = approved_machine_split(
                    session_id, entry_id, incoming_canonical,
                    stored_canonical, mobile_occurrences)
                if not alias_reconciliation and not split_reconciliation:
                    fail(f"conflit exercice association: {session_id}/{entry_id}")
                # CONTRACT: the first fallback is only for schemas/runs without
                # a persistent alias. The second is the closed v13 split table
                # above; no source ID becomes a global one-to-many alias.
            # INVARIANT: identity reconciliation never changes session_id,
            # entry_id, equipment_id, or any persisted occurrence. A distinct
            # live canonical exercise therefore remains a hard conflict.
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
