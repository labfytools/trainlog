#!/usr/bin/env python3
"""Export raw desktop history as the read-only TRAINLOG_AI_EXPORT_V1 artifact."""

import argparse
import datetime
import json
import os
import sqlite3
import tempfile
from pathlib import Path

from trainlog_sqlite import configure_connection


SUPPORTED_SCHEMA_VERSIONS = (15, 16, 17, 18, 19, 20, 21, 22, 23)
MEASUREMENT_FIELDS = (
    "body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm", "waist_cm",
    "hips_cm", "left_arm_cm", "right_arm_cm", "left_forearm_cm",
    "right_forearm_cm", "left_thigh_cm", "right_thigh_cm", "left_calf_cm",
    "right_calf_cm",
)


def default_database():
    return Path(os.environ.get(
        "XDG_DATA_HOME", str(Path.home() / ".local/share")
    )) / "trainlog" / "trainlog.db"


def _readonly_connection(database):
    """Open SQLite without any path capable of changing persistent state.

    CONTRACT: mode=ro rejects writes at SQLite's file-opening boundary and
    query_only is a second guard against accidental write SQL added later.
    INVARIANT: exporting cannot migrate, create, replace, or update DB content.
    """
    uri = database.resolve().as_uri() + "?mode=ro"
    connection = configure_connection(sqlite3.connect(uri, uri=True))
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA query_only=ON")
    return connection


def _revisions(connection, table, owner_column, owner_id):
    rows = connection.execute(
        f"SELECT revision_id,created_at,raw_text FROM {table} "
        f"WHERE {owner_column}=? "
        "ORDER BY created_at COLLATE BINARY,revision_id COLLATE BINARY",
        (owner_id,),
    )
    return [dict(row) for row in rows]


def _feedback(connection, session_row_id):
    result = {}
    rows = connection.execute(
        "SELECT f.feedback_id,se.entry_id,f.observed_at "
        "FROM exercise_feedback f "
        "JOIN session_exercises se ON se.id=f.session_exercise_row_id "
        "WHERE se.session_row_id=? "
        "ORDER BY se.position,f.observed_at COLLATE BINARY,"
        "f.feedback_id COLLATE BINARY",
        (session_row_id,),
    )
    for row in rows:
        item = {
            "feedback_id": row["feedback_id"],
            "observed_at": row["observed_at"],
            "revisions": _revisions(
                connection, "exercise_feedback_revisions", "feedback_id",
                row["feedback_id"],
            ),
        }
        result.setdefault(row["entry_id"], []).append(item)
    return result


def _followups(connection, session_row_id):
    rows = connection.execute(
        "SELECT followup_id,observed_at FROM session_followups "
        "WHERE session_row_id=? "
        "ORDER BY observed_at COLLATE BINARY,followup_id COLLATE BINARY",
        (session_row_id,),
    )
    return [{
        "followup_id": row["followup_id"],
        "observed_at": row["observed_at"],
        "revisions": _revisions(
            connection, "session_followup_revisions", "followup_id",
            row["followup_id"],
        ),
    } for row in rows]


def _entry_result(connection, entry):
    maximum = connection.execute(
        "SELECT max_weight_kg FROM max_results "
        "WHERE session_exercise_row_id=?", (entry["row_id"],),
    ).fetchone()
    if maximum is not None:
        return "max_weight_kg", maximum["max_weight_kg"]
    if entry["recording_mode"] == "continuous":
        activity = connection.execute(
            "SELECT duration_seconds,speed_kmh,distance_km "
            "FROM continuous_activity WHERE session_exercise_row_id=?",
            (entry["row_id"],),
        ).fetchone()
        return "continuous_activity", (
            None if activity is None else
            {key: activity[key] for key in activity.keys()
             if activity[key] is not None}
        )
    sets = connection.execute(
        "SELECT position,reps,duration_seconds,weight_kg FROM performed_sets "
        "WHERE session_exercise_row_id=? "
        "ORDER BY position,id", (entry["row_id"],),
    )
    return "sets", [
        {key: row[key] for key in row.keys() if row[key] is not None}
        for row in sets
    ]


def build_export(connection, generated_at):
    """Build the raw export using only explicitly ordered SELECT statements."""
    schema_version = connection.execute("PRAGMA user_version").fetchone()[0]
    if schema_version not in SUPPORTED_SCHEMA_VERSIONS:
        raise ValueError("schema desktop v15-v18 requis")

    root = {
        "format": "TRAINLOG_AI_EXPORT",
        "version": 1,
        "generated_at": generated_at,
        "sessions": [],
        "max_history": [],
        "measurements": [],
    }
    tracking = "se.tracking_mode" if schema_version >= 16 else "e.tracking_mode"
    sessions = connection.execute(
        "SELECT id,session_id,started_at,ended_at,session_type,notes "
        "FROM sessions "
        "ORDER BY started_at COLLATE BINARY,session_id COLLATE BINARY"
    )
    for session in sessions:
        immediate = _feedback(connection, session["id"])
        exported_session = {
            key: session[key] for key in (
                "session_id", "started_at", "ended_at", "session_type", "notes"
            )
        }
        exported_session["entries"] = []
        entries = connection.execute(
            "SELECT se.id AS row_id,se.entry_id,se.position,e.exercise_id,"
            "e.name AS exercise_name,se.recording_mode," + tracking +
            " AS tracking_mode,se.data_fields,se.equipment_id,se.notes "
            "FROM session_exercises se "
            "JOIN exercises e ON e.id=se.exercise_row_id "
            "WHERE se.session_row_id=? "
            "ORDER BY se.position,se.entry_id COLLATE BINARY",
            (session["id"],),
        )
        for entry in entries:
            item = {key: entry[key] for key in (
                "entry_id", "exercise_id", "exercise_name", "position",
                "recording_mode", "tracking_mode", "data_fields",
                "equipment_id", "notes",
            )}
            result_name, result = _entry_result(connection, entry)
            item[result_name] = result
            item["immediate_feedback"] = immediate.get(entry["entry_id"], [])
            exported_session["entries"].append(item)
        # CONTRACT: follow-ups belong to the session only. The exporter never
        # infers an exercise cause from time, text, or occurrence proximity.
        exported_session["session_followups"] = _followups(
            connection, session["id"]
        )
        root["sessions"].append(exported_session)

    maxima = connection.execute(
        "SELECT s.session_id,s.started_at,se.entry_id,e.exercise_id,"
        "e.name AS exercise_name,m.max_weight_kg "
        "FROM max_results m "
        "JOIN session_exercises se ON se.id=m.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id "
        "JOIN exercises e ON e.id=se.exercise_row_id "
        "ORDER BY s.started_at COLLATE BINARY,s.session_id COLLATE BINARY,"
        "se.position,se.entry_id COLLATE BINARY"
    )
    root["max_history"] = [dict(row) for row in maxima]

    columns = ",".join(("b.observation_id", "b.observed_at", "s.session_id") +
                       tuple("b." + name for name in MEASUREMENT_FIELDS) +
                       ("b.notes",))
    measurements = connection.execute(
        f"SELECT {columns} FROM body_observations b "
        "LEFT JOIN sessions s ON s.id=b.session_row_id "
        "ORDER BY b.observed_at COLLATE BINARY,b.observation_id COLLATE BINARY"
    )
    root["measurements"] = [
        {key: row[key] for key in row.keys() if row[key] is not None}
        for row in measurements
    ]
    return root


def _write_atomic(output, payload):
    output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=output.name + ".", suffix=".tmp", dir=output.parent
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(payload, stream, ensure_ascii=False, separators=(",", ":"))
            stream.write("\n")
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
    parser = argparse.ArgumentParser(
        description="Export local Trainlog history for external analysis."
    )
    parser.add_argument(
        "output", type=Path, nargs="?", default=Path("trainlog_ai_export_v1.json")
    )
    parser.add_argument("--database", type=Path, default=default_database())
    args = parser.parse_args()
    generated_at = datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="seconds"
    ).replace("+00:00", "Z")
    connection = _readonly_connection(args.database)
    try:
        payload = build_export(connection, generated_at)
    finally:
        connection.close()
    _write_atomic(args.output, payload)
    print("TRAINLOG_AI_EXPORT_V1=PASS")
    print("output=" + str(args.output.resolve()))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("TRAINLOG_AI_EXPORT_V1=FAIL " + str(error))
        raise SystemExit(1)
