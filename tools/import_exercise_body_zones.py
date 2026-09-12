#!/usr/bin/env python3
"""Atomically reconcile exercise/body-zone companion v1 with a sync baseline."""

import argparse
import json
import os
import re
import sqlite3
import unicodedata
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "catalog/body-zones-v1.json"


class ImportFailure(RuntimeError):
    pass


EXERCISE_ID_PATTERN = re.compile(
    r"ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
)


def default_database():
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "trainlog/trainlog.db"


def exact(value, keys, label):
    if not isinstance(value, dict) or set(value) != keys:
        raise ImportFailure(f"{label}: forme invalide")


def load_catalog():
    root = json.loads(CATALOG.read_text(encoding="utf-8"))
    if root.get("format") != "trainlog-body-zone-catalog" or root.get("version") != 1:
        raise ImportFailure("catalogue zones v1 invalide")
    return {item["zone_id"]: item for item in root["zones"]}


def state(primary, secondary):
    return (primary or "") + "|" + ",".join(sorted(secondary))


def current(connection, row_id):
    primary = connection.execute(
        "SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=? AND role='primary'", (row_id,),
    ).fetchone()
    secondary = [row[0] for row in connection.execute(
        "SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=? AND role='secondary' ORDER BY zone_id",
        (row_id,),
    )]
    return (None if primary is None else primary[0], secondary)


def normalize_name(value):
    """Match the desktop importer's stable normalized-name comparison."""
    folded = unicodedata.normalize("NFC", value.casefold())
    output = []
    pending_space = False
    wrote_content = False
    for char in folded:
        if char.isspace():
            if wrote_content:
                pending_space = True
            continue
        if pending_space:
            output.append(" ")
            pending_space = False
        output.append(char)
        wrote_content = True
    normalized = "".join(output)
    if not normalized:
        raise ImportFailure("preuve mobile: nom d'exercice vide")
    return normalized


def load_mobile_exercise_proof(path):
    """Load only the already-imported V2 definitions needed to prove aliases."""
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("format") != "trainlog-mobile-export" or payload.get("version") != 2 or \
            not isinstance(payload.get("exercises"), list):
        raise ImportFailure("snapshot mobile V2 de preuve invalide")
    proof = {}
    for index, item in enumerate(payload["exercises"]):
        if not isinstance(item, dict):
            raise ImportFailure(f"preuve mobile exercises[{index}] invalide")
        exercise_id = item.get("exercise_id")
        name = item.get("name")
        recording = item.get("recording_mode")
        tracking = item.get("tracking_mode")
        data_fields = item.get("data_fields")
        if not isinstance(exercise_id, str) or not exercise_id or exercise_id in proof or \
                not isinstance(name, str) or not name or recording not in {"sets", "continuous"} or \
                tracking not in {"reps", "duration"} or isinstance(data_fields, bool) or \
                not isinstance(data_fields, int) or data_fields < 0:
            raise ImportFailure(f"preuve mobile exercises[{index}] invalide")
        proof[exercise_id] = (normalize_name(name), recording, tracking, data_fields)
    return proof


def resolve_exercise_row(connection, exercise_id, mobile_proof):
    row = connection.execute(
        "SELECT id FROM exercises WHERE exercise_id=?", (exercise_id,),
    ).fetchone()
    if row is not None:
        return row[0]
    aliases_available = connection.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='exercise_aliases'",
    ).fetchone() is not None
    if aliases_available:
        row = connection.execute(
            "SELECT e.id FROM exercise_aliases a "
            "JOIN exercises e ON e.exercise_id=a.canonical_exercise_id "
            "WHERE a.source_exercise_id=?",
            (exercise_id,),
        ).fetchone()
        if row is not None:
            return row[0]
    proof = mobile_proof.get(exercise_id)
    if proof is None:
        raise ImportFailure(f"exercice inconnu: {exercise_id}")
    normalized, recording, tracking, data_fields = proof
    row = connection.execute(
        "SELECT id,recording_mode,tracking_mode,data_fields FROM exercises "
        "WHERE normalized_name=?", (normalized,),
    ).fetchone()
    if row is None or row[1] != recording or row[2] != tracking or \
            not ((row[3] & ~data_fields) == 0 or (data_fields & ~row[3]) == 0):
        raise ImportFailure(f"preuve de réconciliation invalide: {exercise_id}")
    # CONTRACT: import_mobile_export.py ran first in the same sync and removed
    # only a profile-compatible duplicate. The source V2 definition proves that
    # this now-absent creator ID resolves to the one normalized desktop row.
    return row[0]


def replace(connection, row_id, primary, secondary):
    connection.execute("DELETE FROM exercise_body_zones WHERE exercise_row_id=?", (row_id,))
    if primary is not None:
        connection.execute(
            "INSERT INTO exercise_body_zones VALUES(?,?,'primary')", (row_id, primary),
        )
    connection.executemany(
        "INSERT INTO exercise_body_zones VALUES(?,?,'secondary')",
        [(row_id, zone_id) for zone_id in secondary],
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--database", type=Path, default=default_database())
    parser.add_argument(
        "--mobile-export",
        type=Path,
        help="snapshot V2 importé juste avant, utilisé seulement pour prouver un ID réconcilié",
    )
    args = parser.parse_args()
    zones = load_catalog()
    payload = json.loads(args.input.read_text(encoding="utf-8"))
    exact(payload, {"format", "version", "generated_at", "exercises"}, "racine")
    if payload["format"] != "trainlog-exercise-body-zones" or payload["version"] != 1 or \
            isinstance(payload["version"], bool) or \
            not isinstance(payload["generated_at"], str) or not payload["generated_at"] or \
            not isinstance(payload["exercises"], list):
        raise ImportFailure("companion zones v1 invalide")
    try:
        generated_at = datetime.fromisoformat(payload["generated_at"].replace("Z", "+00:00"))
    except ValueError as error:
        raise ImportFailure("generated_at invalide") from error
    if generated_at.utcoffset() is None:
        raise ImportFailure("generated_at sans offset")
    parsed = []
    seen = set()
    for index, item in enumerate(payload["exercises"]):
        exact(item, {"exercise_id", "primary_zone_id", "secondary_zone_ids"}, f"exercises[{index}]")
        exercise_id = item["exercise_id"]
        primary = item["primary_zone_id"]
        secondary = item["secondary_zone_ids"]
        if not isinstance(exercise_id, str) or EXERCISE_ID_PATTERN.fullmatch(exercise_id) is None or \
                exercise_id in seen:
            raise ImportFailure(f"exercises[{index}].exercise_id invalide")
        if primary is not None and (not isinstance(primary, str) or primary not in zones):
            raise ImportFailure(f"exercises[{index}].primary_zone_id invalide")
        if not isinstance(secondary, list) or any(not isinstance(zone, str) or zone not in zones for zone in secondary):
            raise ImportFailure(f"exercises[{index}].secondary_zone_ids invalide")
        if len(secondary) != len(set(secondary)) or primary in secondary:
            raise ImportFailure(f"exercises[{index}]: zones dupliquées")
        if primary is None and secondary:
            raise ImportFailure(f"exercises[{index}]: secondaires sans zone principale")
        if any(zones[zone]["kind"] == "group" for zone in ([primary] if primary else []) + secondary):
            raise ImportFailure(f"exercises[{index}]: relation parent dérivable interdite")
        seen.add(exercise_id)
        parsed.append((exercise_id, primary, sorted(secondary)))

    # A V2 snapshot is reconciliation evidence only when the caller selected
    # and supplied that exact snapshot.  Never discover a sibling implicitly:
    # a stale V2 file must not prove IDs for a separately selected V3/V1 run.
    mobile_proof = (load_mobile_exercise_proof(args.mobile_export)
                    if args.mobile_export is not None else {})

    connection = sqlite3.connect(args.database)
    updated = skipped = kept_local = 0
    try:
        if connection.execute("PRAGMA user_version").fetchone()[0] not in (11, 12, 13, 14, 15):
            raise ImportFailure("schema desktop v11/v12/v13 requis")
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("BEGIN IMMEDIATE")
        grouped = {}
        for exercise_id, primary, secondary in parsed:
            row_id = resolve_exercise_row(connection, exercise_id, mobile_proof)
            incoming = (primary, secondary)
            group = grouped.get(row_id)
            if group is None:
                grouped[row_id] = (exercise_id, incoming)
            elif group[1] != incoming:
                # CONTRACT: secondary zones are direct ordered-set metadata, so
                # alias coalescing may accept equality but must never union two
                # companion claims or choose between primary/secondary roles.
                raise ImportFailure(
                    f"données de zones incompatibles après résolution d'alias: "
                    f"{group[0]} / {exercise_id}"
                )

        # INVARIANT: resolve and compare the complete alias groups before the
        # first write. This makes one reconciliation decision per canonical
        # exercise and prevents input order from changing the chosen payload.
        planned = []
        for row_id, (exercise_id, incoming) in grouped.items():
            primary, secondary = incoming
            local_primary, local_secondary = current(connection, row_id)
            local_state = state(local_primary, local_secondary)
            incoming_state = state(primary, secondary)
            baseline_row = connection.execute(
                "SELECT synced_state FROM exercise_body_zone_sync WHERE exercise_row_id=?", (row_id,),
            ).fetchone()
            baseline = None if baseline_row is None else baseline_row[0]
            if local_state == incoming_state:
                planned.append(("skip", row_id, primary, secondary, incoming_state))
            elif baseline is not None and local_state == baseline:
                planned.append(("update", row_id, primary, secondary, incoming_state))
            elif baseline is not None and incoming_state == baseline:
                planned.append(("keep", row_id, primary, secondary, incoming_state))
            elif baseline is None and local_state == "|":
                planned.append(("update", row_id, primary, secondary, incoming_state))
            else:
                raise ImportFailure(f"conflit zones simultané: {exercise_id}")

        # WHY: aliases are compatibility identities, not additional exercises.
        # Applying the plan once per resolved row avoids duplicate mutations and
        # cannot recreate a retired source ID.
        for action, row_id, primary, secondary, incoming_state in planned:
            if action == "keep":
                kept_local += 1
                continue
            if action == "update":
                replace(connection, row_id, primary, secondary)
                updated += 1
            else:
                skipped += 1
            connection.execute(
                "INSERT OR REPLACE INTO exercise_body_zone_sync VALUES(?,?)",
                (row_id, incoming_state),
            )
        connection.commit()
        print("EXERCISE_BODY_ZONES_IMPORT=PASS")
        print(f"zones_updated={updated}")
        print(f"zones_skipped={skipped}")
        print(f"zones_kept_local={kept_local}")
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"EXERCISE_BODY_ZONES_IMPORT=FAIL {error}")
        raise SystemExit(1)
