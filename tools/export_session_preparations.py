#!/usr/bin/env python3
"""Export immutable ready preparation deliveries for Android."""

import argparse
import datetime as dt
import json
import os
import sqlite3
import tempfile
from pathlib import Path

from trainlog_sqlite import connect_database

MAX_DELIVERIES = 128
MAX_ENTRIES = 64
MAX_WITHDRAWALS = 128


def build_export(connection: sqlite3.Connection) -> dict:
    supported_versions = (24, 25, 26, 27, 28, 29, 30, 31, 32, 33)
    if connection.execute("PRAGMA user_version").fetchone()[0] not in supported_versions:
        raise ValueError("desktop schema v24, v25, v26 through v32 required")
    deliveries = []
    rows = connection.execute(
        "SELECT d.delivery_id,d.preparation_id,d.revision_id,d.execution_session_id,"
        "CASE WHEN d.state='acknowledged' THEN 'remote_unknown' ELSE d.state END,"
        "r.title,r.session_type,r.planned_for,r.notes,p.source_proposal_id,p.source_payload_sha256,"
        "p.source_program_id,p.source_program_session_id "
        "FROM session_preparation_deliveries d "
        "JOIN session_preparations p ON p.preparation_id=d.preparation_id "
        "JOIN session_preparation_revisions r ON r.revision_id=d.revision_id "
        "WHERE p.withdrawn_at IS NULL AND ("
        "d.state IN('pending','remote_unknown') OR ("
        "d.state='acknowledged' AND p.source_program_id IS NOT NULL AND "
        "p.source_program_session_id IS NOT NULL AND NOT EXISTS("
        "SELECT 1 FROM program_session_executions x "
        "WHERE x.program_session_id=p.source_program_session_id "
        "AND x.program_id=p.source_program_id AND x.session_id=d.execution_session_id"
        "))) "
        "ORDER BY d.created_at,d.delivery_id LIMIT ?",
        (MAX_DELIVERIES + 1,),
    ).fetchall()
    if len(rows) > MAX_DELIVERIES:
        raise ValueError("preparation delivery capacity exceeded")
    for row in rows:
        entries = connection.execute(
            "SELECT entry_id,position,exercise_id,equipment_id,recording_mode,tracking_mode,"
            "data_fields,load_mode,rest_seconds,target_sets,target_reps,target_duration_seconds,"
            "target_weight_kg,notes FROM session_preparation_entries "
            "WHERE revision_id=? ORDER BY position,entry_id LIMIT ?",
            (row[2], MAX_ENTRIES + 1),
        ).fetchall()
        if not entries or len(entries) > MAX_ENTRIES:
            raise ValueError("invalid preparation occurrence count")
        deliveries.append({
            "delivery_id": row[0], "preparation_id": row[1], "revision_id": row[2],
            "execution_session_id": row[3], "state": row[4], "title": row[5],
            "session_type": row[6], "planned_for": row[7], "notes": row[8],
            "source_proposal_id": row[9], "source_payload_sha256": row[10],
            "source_program_id": row[11], "source_program_session_id": row[12],
            "occurrences": [{
                "entry_id": entry[0], "position": entry[1], "exercise_id": entry[2],
                "equipment_id": entry[3], "recording_mode": entry[4],
                "tracking_mode": entry[5], "data_fields": entry[6],
                "load_mode": entry[7], "rest_seconds": entry[8],
                "target_sets": entry[9], "target_reps": entry[10],
                "target_duration_seconds": entry[11], "target_weight_kg": entry[12],
                "notes": entry[13],
            } for entry in entries],
        })
    withdrawals = []
    withdrawal_rows = connection.execute(
        "SELECT withdrawal_id,preparation_id,revision_id,requested_at "
        "FROM session_preparation_withdrawals WHERE acknowledged_at IS NULL "
        "ORDER BY requested_at,withdrawal_id LIMIT ?",
        (MAX_WITHDRAWALS + 1,),
    ).fetchall()
    if len(withdrawal_rows) > MAX_WITHDRAWALS:
        raise ValueError("preparation withdrawal capacity exceeded")
    for row in withdrawal_rows:
        related = connection.execute(
            "SELECT delivery_id,revision_id,execution_session_id "
            "FROM session_preparation_deliveries WHERE preparation_id=? "
            "ORDER BY created_at,delivery_id LIMIT ?",
            (row[1], MAX_DELIVERIES + 1),
        ).fetchall()
        if len(related) > MAX_DELIVERIES:
            raise ValueError("preparation withdrawal delivery capacity exceeded")
        withdrawals.append({
            "withdrawal_id": row[0],
            "preparation_id": row[1],
            "revision_id": row[2],
            "requested_at": row[3],
            "deliveries": [{
                "delivery_id": delivery[0],
                "revision_id": delivery[1],
                "execution_session_id": delivery[2],
            } for delivery in related],
        })
    return {
        "format": "trainlog-session-preparations", "version": 2,
        "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "deliveries": deliveries,
        "withdrawals": withdrawals,
    }


def write_atomic(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, ensure_ascii=False, separators=(",", ":"), allow_nan=False)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        with connect_database(arguments.database) as connection:
            payload = build_export(connection)
        write_atomic(arguments.output, payload)
        print(f"SESSION_PREPARATION_EXPORT=PASS deliveries={len(payload['deliveries'])}")
        return 0
    except (OSError, sqlite3.Error, ValueError) as error:
        print(f"SESSION_PREPARATION_EXPORT=FAIL {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
