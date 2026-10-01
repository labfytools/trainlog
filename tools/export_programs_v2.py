#!/usr/bin/env python3
"""Export the versioned desktop-owned Program planning projection."""

import argparse
import json
from contextlib import closing
from pathlib import Path

from export_programs import export_programs
from trainlog_sqlite import connect_database


def export_programs_v2(database: Path) -> dict:
    # WHY: V1's strict session fields are frozen. V2 carries absolute current
    # dates and ceded-slot state without changing the imported source date.
    value = export_programs(database)
    with closing(connect_database(database)) as db:
        if db.execute("PRAGMA user_version").fetchone()[0] != 37:
            raise ValueError("desktop schema v37 required for Programs V2")
        for program in value["programs"]:
            sequence = db.execute(
                "SELECT revision_sequence FROM program_revision_sequences WHERE program_id=?",
                (program["program_id"],),
            ).fetchone()
            program["revision_sequence"] = sequence[0] if sequence is not None else 0
            planning = {
                row[0]: (row[1], row[2])
                for row in db.execute(
                    "SELECT p.program_session_id,p.current_for,p.state "
                    "FROM program_session_planning p JOIN program_sessions s "
                    "ON s.program_session_id=p.program_session_id WHERE s.program_id=?",
                    (program["program_id"],),
                )
            }
            for position, session in enumerate(program["sessions"]):
                current, state = planning.get(
                    session["program_session_id"], (session["planned_for"], "active")
                )
                session["position"] = position
                session["current_for"] = current
                session["planning_state"] = state
    value["version"] = 2
    return value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(export_programs_v2(args.database), ensure_ascii=False,
                   sort_keys=True, separators=(",", ":")),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
