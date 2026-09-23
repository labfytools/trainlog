#!/usr/bin/env python3
"""Export the desktop-owned Programs V1 Android projection."""

import argparse
import datetime as dt
import json
from contextlib import closing
from pathlib import Path

from trainlog_sqlite import connect_database


def nullable(row, index):
    return None if row[index] is None else row[index]


def export_programs(database: Path) -> dict:
    with closing(connect_database(database)) as db:
        if db.execute("PRAGMA user_version").fetchone()[0] not in (26, 27, 28, 29, 30, 31):
            raise ValueError("desktop schema v26, v27, v28 or v29 required")
        programs = []
        for row in db.execute(
            "SELECT program_id,revision_id,title,note,state,start_date,end_date,created_at,"
            "updated_at,source_format,source_version,source_payload_sha256 FROM programs "
            "WHERE deleted_at IS NULL ORDER BY program_id"
        ):
            sessions = []
            for session in db.execute(
                "SELECT program_session_id,title,session_type,planned_for,note FROM "
                "program_sessions WHERE program_id=? ORDER BY position,program_session_id",
                (row[0],),
            ):
                occurrences = []
                for entry in db.execute(
                    "SELECT entry_id,exercise_id,equipment_id,load_mode,rest_seconds,target_sets,"
                    "target_reps,target_duration_seconds,target_weight_kg,notes FROM "
                    "program_session_entries WHERE program_session_id=? ORDER BY position,entry_id",
                    (session[0],),
                ):
                    occurrences.append({
                        "entry_id": entry[0],
                        "exercise_id": entry[1],
                        "equipment_id": nullable(entry, 2),
                        "load_mode": entry[3],
                        "rest_seconds": entry[4],
                        "target_sets": nullable(entry, 5),
                        "target_reps": nullable(entry, 6),
                        "target_duration_seconds": nullable(entry, 7),
                        "target_weight_kg": nullable(entry, 8),
                        "notes": nullable(entry, 9),
                    })
                sessions.append({
                    "program_session_id": session[0],
                    "title": session[1],
                    "session_type": session[2],
                    "planned_for": nullable(session, 3),
                    "note": nullable(session, 4),
                    "occurrences": occurrences,
                })
            programs.append({
                "program_id": row[0],
                "revision_id": row[1],
                "title": row[2],
                "note": nullable(row, 3),
                "state": row[4],
                "start_date": nullable(row, 5),
                "end_date": nullable(row, 6),
                "created_at": row[7],
                "updated_at": row[8],
                "source_format": row[9],
                "source_version": row[10],
                "source_payload_sha256": row[11],
                "sessions": sessions,
            })
        deletions = [{
            "deletion_id": row[0],
            "program_id": row[1],
            "predecessor_revision_id": row[2],
            "revision_id": row[3],
            "requested_at": row[4],
        } for row in db.execute(
            "SELECT request_id,program_id,expected_revision,deleted_revision,deleted_at "
            "FROM program_deletions WHERE acknowledged_at IS NULL ORDER BY request_id"
        )]
    return {
        "format": "trainlog-programs",
        "version": 1,
        "generated_at": dt.datetime.now().astimezone().isoformat(),
        "programs": programs,
        "deletions": deletions,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(export_programs(args.database), ensure_ascii=False, sort_keys=True,
                   separators=(",", ":")),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
