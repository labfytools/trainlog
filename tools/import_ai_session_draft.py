#!/usr/bin/env python3
"""Strict transactional importer for TRAINLOG_AI_SESSION_DRAFT_V1."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import re
import sqlite3
import sys
from pathlib import Path

from trainlog_sqlite import connect_database


FORMAT = "TRAINLOG_AI_SESSION_DRAFT"
MAX_ENTRIES = 64
MAX_TITLE = 120
MAX_NOTES = 2000
ID_RE = re.compile(
    r"^aid_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
)
EXERCISE_ID_RE = re.compile(
    r"^ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
)
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?Z$")
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


class ImportFailure(ValueError):
    """A stable, user-actionable contract failure."""


def reject_constant(value: str):
    raise ImportFailure(f"nombre non fini interdit: {value}")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ImportFailure(f"clé JSON dupliquée: {key}")
        result[key] = value
    return result


def exact(value, keys, label):
    if not isinstance(value, dict) or set(value) != keys:
        raise ImportFailure(f"clés {label} invalides")


def integer(value, minimum, maximum, label):
    if type(value) is not int or value < minimum or value > maximum:
        raise ImportFailure(f"{label} hors limites")
    return value


def optional_text(value, maximum, label):
    if value is None:
        return None
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise ImportFailure(f"{label} invalide")
    return value


def utc_timestamp(value, label):
    if not isinstance(value, str) or UTC_RE.fullmatch(value) is None:
        raise ImportFailure(f"{label} doit être un instant UTC canonique")
    try:
        parsed = dt.datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as error:
        raise ImportFailure(f"{label} invalide") from error
    if parsed.utcoffset() != dt.timedelta(0):
        raise ImportFailure(f"{label} non UTC")
    return value


def optional_date(value):
    if value is None:
        return None
    if not isinstance(value, str) or DATE_RE.fullmatch(value) is None:
        raise ImportFailure("planned_for invalide")
    try:
        dt.date.fromisoformat(value)
    except ValueError as error:
        raise ImportFailure("planned_for invalide") from error
    return value


def optional_weight(value):
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ImportFailure("target_weight_kg invalide")
    result = float(value)
    if not math.isfinite(result) or result <= 0.0 or result > 10000.0:
        raise ImportFailure("target_weight_kg hors limites")
    return result


def resolve_exercise(connection, source_id):
    direct = connection.execute(
        "SELECT id,recording_mode,tracking_mode,data_fields FROM exercises WHERE exercise_id=?",
        (source_id,),
    ).fetchone()
    if direct is not None:
        return direct
    alias = connection.execute(
        "SELECT e.id,e.recording_mode,e.tracking_mode,e.data_fields FROM exercise_aliases a "
        "JOIN exercises e ON e.exercise_id=a.canonical_exercise_id "
        "WHERE a.source_exercise_id=?",
        (source_id,),
    ).fetchone()
    if alias is None:
        raise ImportFailure(f"exercise_id inconnu ou sans alias explicite: {source_id}")
    return alias


def validate(payload, connection):
    exact(payload, {"format", "version", "draft"}, "racine")
    if payload["format"] != FORMAT or type(payload["version"]) is not int or payload["version"] != 1:
        raise ImportFailure("format/version AI draft non pris en charge")
    draft = payload["draft"]
    exact(draft, {"draft_id", "created_at", "planned_for", "session_type", "title", "notes", "entries"}, "draft")
    if not isinstance(draft["draft_id"], str) or ID_RE.fullmatch(draft["draft_id"]) is None:
        raise ImportFailure("draft_id aid_<uuid-v4> invalide")
    utc_timestamp(draft["created_at"], "created_at")
    optional_date(draft["planned_for"])
    if draft["session_type"] != "training":
        raise ImportFailure("session_type doit valoir training")
    optional_text(draft["title"], MAX_TITLE, "title")
    optional_text(draft["notes"], MAX_NOTES, "notes")
    entries = draft["entries"]
    if not isinstance(entries, list) or not 1 <= len(entries) <= MAX_ENTRIES:
        raise ImportFailure("entries doit contenir 1 à 64 éléments")

    validated = []
    for expected_position, entry in enumerate(entries):
        exact(entry, {"position", "exercise_id", "target_sets", "target_reps", "target_duration_seconds", "target_weight_kg", "rest_seconds"}, f"entries[{expected_position}]")
        position = integer(entry["position"], 0, MAX_ENTRIES - 1, "position")
        if position != expected_position:
            raise ImportFailure("positions entries non contiguës ou hors ordre")
        source_id = entry["exercise_id"]
        if not isinstance(source_id, str) or EXERCISE_ID_RE.fullmatch(source_id) is None:
            raise ImportFailure("exercise_id ex_<uuid-v4> invalide")
        exercise_row_id, recording_mode, tracking_mode, data_fields = resolve_exercise(connection, source_id)
        if recording_mode != "sets":
            raise ImportFailure(f"exercice CONTINUOUS interdit dans un plan SETS: {source_id}")
        target_sets = integer(entry["target_sets"], 1, 99, "target_sets")
        target_reps = entry["target_reps"]
        target_duration = entry["target_duration_seconds"]
        if tracking_mode == "reps":
            target_reps = integer(target_reps, 1, 999, "target_reps")
            if target_duration is not None:
                raise ImportFailure("target_duration_seconds interdit pour tracking reps")
        elif tracking_mode == "duration":
            target_duration = integer(target_duration, 1, 86400, "target_duration_seconds")
            if target_reps is not None:
                raise ImportFailure("target_reps interdit pour tracking duration")
        else:
            raise ImportFailure("profil exercice corrompu")
        weight = optional_weight(entry["target_weight_kg"])
        validated.append((
            position, exercise_row_id, source_id, recording_mode, tracking_mode,
            data_fields, None, "external" if weight is not None else "none", target_sets,
            target_reps, target_duration, weight,
            integer(entry["rest_seconds"], 0, 86400, "rest_seconds"),
        ))
    return draft, validated


def canonical_digest(payload):
    encoded = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def entry_id(draft_id, position):
    """Return a stable UUIDv4-shaped creator ID for one immutable source slot."""
    raw = bytearray(hashlib.sha256(f"{draft_id}\n{position}".encode("ascii")).digest()[:16])
    raw[6] = (raw[6] & 0x0F) | 0x40
    raw[8] = (raw[8] & 0x3F) | 0x80
    value = raw.hex()
    return f"sxe_{value[:8]}-{value[8:12]}-{value[12:16]}-{value[16:20]}-{value[20:]}"


def import_payload(connection, payload, imported_at=None):
    """Validate fully, then atomically insert or return ``skipped``.

    CONTRACT: successful rows represent target-only pending SETS plans.
    INVARIANT: validation and every child insert share one IMMEDIATE transaction;
    any failure leaves no draft, child, or import-identity row behind.
    """
    if connection.execute("PRAGMA user_version").fetchone()[0] not in (18, 19, 20):
        raise ImportFailure("schema desktop v18-v20 requis")
    draft, entries = validate(payload, connection)
    digest = canonical_digest(payload)
    imported_at = imported_at or dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
    connection.execute("BEGIN IMMEDIATE")
    try:
        existing = connection.execute(
            "SELECT payload_sha256 FROM ai_session_draft_imports WHERE draft_id=?",
            (draft["draft_id"],),
        ).fetchone()
        if existing is not None:
            if existing[0] != digest:
                raise ImportFailure("conflit: draft_id déjà importé avec un contenu différent")
            connection.commit()
            return "skipped"
        cursor = connection.execute(
            "INSERT INTO ai_session_drafts(draft_id,created_at,planned_for,session_type,title,notes) VALUES(?,?,?,?,?,?)",
            (draft["draft_id"], draft["created_at"], draft["planned_for"], draft["session_type"], draft["title"], draft["notes"]),
        )
        draft_row_id = cursor.lastrowid
        connection.executemany(
            "INSERT INTO ai_session_draft_entries(draft_row_id,entry_id,position,exercise_row_id,source_exercise_id,recording_mode,tracking_mode,data_fields,equipment_id,load_mode,target_sets,target_reps,target_duration_seconds,target_weight_kg,rest_seconds) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            [(draft_row_id, entry_id(draft["draft_id"], row[0])) + row for row in entries],
        )
        connection.execute(
            "INSERT INTO ai_session_draft_imports(draft_row_id,draft_id,payload_sha256,imported_at) VALUES(?,?,?,?)",
            (draft_row_id, draft["draft_id"], digest, imported_at),
        )
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    return "imported"


def load(path):
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object, parse_constant=reject_constant)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    try:
        payload = load(args.artifact)
        connection = connect_database(args.database)
        try:
            result = import_payload(connection, payload)
        finally:
            connection.close()
        print(f"AI_SESSION_DRAFT_IMPORT=PASS result={result} draft_id={payload['draft']['draft_id']}")
        return 0
    except (OSError, json.JSONDecodeError, sqlite3.Error, ImportFailure) as error:
        print(f"AI_SESSION_DRAFT_IMPORT=FAIL {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
