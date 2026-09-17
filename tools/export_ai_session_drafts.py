#!/usr/bin/env python3
"""Export the PC→Android trainlog-ai-session-drafts v1 companion."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import sqlite3
import tempfile
from pathlib import Path

from trainlog_sqlite import connect_database

MAX_DRAFTS = 256
ID_PREFIX = "aid_"


def ensure_publication_state(connection):
    """Install the additive v18 cursor for direct syncd/tool invocations."""
    owned_transaction = not connection.in_transaction
    if connection.execute("PRAGMA user_version").fetchone()[0] not in (18, 19):
        raise ValueError("schema desktop v18 requis")
    columns = {row[1] for row in connection.execute(
        "PRAGMA table_info(ai_session_drafts)")}
    if "published_at" not in columns:
        connection.execute(
            "ALTER TABLE ai_session_drafts ADD COLUMN published_at TEXT")
    # INVARIANT: publication never weakens the permanent Drive replay ledger,
    # including on an older v18 database whose FK was declared cascading.
    connection.execute(
        "CREATE TRIGGER IF NOT EXISTS ai_session_draft_import_identity_guard "
        "BEFORE DELETE ON ai_session_drafts WHEN EXISTS(SELECT 1 FROM "
        "ai_session_draft_imports i WHERE i.draft_id=OLD.draft_id) BEGIN "
        "SELECT RAISE(ABORT,'AI draft import identity is permanent');END;"
    )
    if owned_transaction:
        connection.commit()


def build_export(connection, generated_at):
    ensure_publication_state(connection)
    drafts = []
    rows = connection.execute(
        "SELECT id,draft_id,created_at,planned_for,session_type,title,notes "
        "FROM ai_session_drafts WHERE published_at IS NULL "
        "ORDER BY created_at COLLATE BINARY,draft_id COLLATE BINARY LIMIT ?",
        (MAX_DRAFTS,),
    ).fetchall()
    for row in rows:
        entries = connection.execute(
            "SELECT de.entry_id,de.position,e.exercise_id,de.recording_mode,de.tracking_mode,"
            "de.data_fields,de.equipment_id,de.load_mode,de.rest_seconds,de.target_sets,"
            "de.target_reps,de.target_duration_seconds,de.target_weight_kg "
            "FROM ai_session_draft_entries de JOIN exercises e ON e.id=de.exercise_row_id "
            "WHERE de.draft_row_id=? ORDER BY de.position",
            (row[0],),
        ).fetchall()
        drafts.append({
            "draft_id": row[1], "created_at": row[2], "planned_for": row[3],
            "session_type": row[4], "title": row[5], "notes": row[6],
            "entries": [{
                "entry_id": entry[0], "position": entry[1], "exercise_id": entry[2],
                "recording_mode": entry[3], "tracking_mode": entry[4],
                "data_fields": entry[5], "equipment_id": entry[6],
                "load_mode": entry[7], "rest_seconds": entry[8],
                "target": {"sets": entry[9], "reps": entry[10],
                           "duration_seconds": entry[11], "weight_kg": entry[12]},
            } for entry in entries],
        })
    # CONTRACT: this companion is separate from every mobile-export version.
    # INVARIANT: archive bookkeeping is desktop-private and is never published.
    return {"format": "trainlog-ai-session-drafts", "version": 1,
            "generated_at": generated_at, "drafts": drafts}


def mark_published(connection, payload, published_at):
    """Mark exactly one successfully transported outbound batch.

    CONTRACT: callers invoke this only after the existing MTP publication
    operation succeeds. INVARIANT: a failed or stale batch marks no row, so a
    retry exports the same deterministic IDs and the import ledger is retained.
    """
    ensure_publication_state(connection)
    if (not isinstance(payload, dict) or set(payload) != {"format", "version", "generated_at", "drafts"}
            or payload["format"] != "trainlog-ai-session-drafts" or payload["version"] != 1
            or not isinstance(payload["drafts"], list) or len(payload["drafts"]) > MAX_DRAFTS):
        raise ValueError("lot de brouillons IA publié invalide")
    draft_ids = []
    for draft in payload["drafts"]:
        draft_id = draft.get("draft_id") if isinstance(draft, dict) else None
        if not isinstance(draft_id, str) or not draft_id.startswith(ID_PREFIX) or draft_id in draft_ids:
            raise ValueError("identité de brouillon IA publiée invalide")
        draft_ids.append(draft_id)
    connection.execute("BEGIN IMMEDIATE")
    try:
        for draft_id in draft_ids:
            cursor = connection.execute(
                "UPDATE ai_session_drafts SET published_at=? WHERE draft_id=? AND published_at IS NULL",
                (published_at, draft_id),
            )
            if cursor.rowcount != 1:
                raise ValueError(f"lot publié obsolète ou inconnu: {draft_id}")
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    return len(draft_ids)


def write_atomic(output, payload):
    output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=output.name + ".", dir=output.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, ensure_ascii=False, separators=(",", ":"), allow_nan=False)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    try:
        connection = connect_database(args.database)
        try:
            generated_at = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
            payload = build_export(connection, generated_at)
        finally:
            connection.close()
        write_atomic(args.output, payload)
        print(f"AI_SESSION_DRAFT_EXPORT=PASS drafts={len(payload['drafts'])}")
        return 0
    except (OSError, sqlite3.Error, ValueError) as error:
        print(f"AI_SESSION_DRAFT_EXPORT=FAIL {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
