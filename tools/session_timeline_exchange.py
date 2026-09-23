#!/usr/bin/env python3
"""Strict Android-owned Trainlog session timeline V1 companion."""

import argparse
import json
import re
import sqlite3
from datetime import datetime
from pathlib import Path

from trainlog_sqlite import connect_database

FORMAT = "trainlog-session-timeline"
VERSION = 1
MAX_BYTES = 4 * 1024 * 1024
MAX_SESSIONS = 256
MAX_EXERCISES = 128
UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
SESSION_ID = re.compile(rf"^se_{UUID4}$")
ENTRY_ID = re.compile(rf"^sxe_{UUID4}$")
EXERCISE_ID = re.compile(rf"^ex_{UUID4}$")


def fail(message: str) -> None:
    raise ValueError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("duplicate JSON field: " + key)
        result[key] = value
    return result


def timestamp(value: object) -> tuple[str, datetime]:
    if not isinstance(value, str) or re.search(r"(?:Z|[+-][0-9]{2}:[0-9]{2})$", value) is None:
        fail("invalid session-timeline timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise ValueError("invalid session-timeline timestamp") from error
    if parsed.utcoffset() is None:
        fail("session-timeline timestamp lacks offset")
    return value, parsed


def validate(root: object) -> dict:
    if not isinstance(root, dict) or set(root) != {
        "format", "version", "generated_at", "sessions"
    }:
        fail("invalid session-timeline envelope")
    if root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        fail("unsupported session-timeline format")
    timestamp(root["generated_at"])
    sessions = root["sessions"]
    if not isinstance(sessions, list) or len(sessions) > MAX_SESSIONS:
        fail("session-timeline session bound exceeded")
    seen_sessions = set()
    for session in sessions:
        if not isinstance(session, dict) or set(session) != {
            "session_id", "started_at", "ended_at", "exercises"
        }:
            fail("invalid session-timeline session")
        session_id = session["session_id"]
        if (
            not isinstance(session_id, str)
            or not SESSION_ID.fullmatch(session_id)
            or session_id in seen_sessions
        ):
            fail("invalid or duplicate session-timeline session identity")
        seen_sessions.add(session_id)
        _, session_start = timestamp(session["started_at"])
        _, session_end = timestamp(session["ended_at"])
        if session_end < session_start:
            fail("invalid session-timeline session interval")
        exercises = session["exercises"]
        if not isinstance(exercises, list) or len(exercises) > MAX_EXERCISES:
            fail("session-timeline exercise bound exceeded")
        seen_entries = set()
        previous_start = None
        for exercise in exercises:
            if not isinstance(exercise, dict) or set(exercise) != {
                "entry_id", "exercise_id", "started_at", "ended_at"
            }:
                fail("invalid session-timeline exercise")
            entry_id = exercise["entry_id"]
            exercise_id = exercise["exercise_id"]
            if (
                not isinstance(entry_id, str)
                or not ENTRY_ID.fullmatch(entry_id)
                or entry_id in seen_entries
                or not isinstance(exercise_id, str)
                or not EXERCISE_ID.fullmatch(exercise_id)
            ):
                fail("invalid or duplicate session-timeline exercise identity")
            seen_entries.add(entry_id)
            _, start = timestamp(exercise["started_at"])
            _, end = timestamp(exercise["ended_at"])
            if start < session_start or end < start or end > session_end:
                fail("session-timeline exercise outside session")
            if previous_start is not None and start < previous_start:
                fail("session-timeline exercises are not ordered")
            previous_start = start
    return root


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("session-timeline artifact exceeds 4 MiB")
    return validate(
        json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    )


def _existing_session(db: sqlite3.Connection, session: dict) -> int:
    row = db.execute(
        "SELECT id,started_at,ended_at FROM sessions WHERE session_id=?",
        (session["session_id"],),
    ).fetchone()
    if row is None:
        fail("session-timeline references missing session")
    if row[1] != session["started_at"] or row[2] != session["ended_at"]:
        fail("session-timeline session bounds conflict with history")
    return int(row[0])


def _same_timing(db: sqlite3.Connection, session_id: str, exercise: dict) -> bool:
    row = db.execute(
        "SELECT exercise_id,started_at,ended_at FROM session_exercise_timeline "
        "WHERE session_id=? AND entry_id=?",
        (session_id, exercise["entry_id"]),
    ).fetchone()
    if row is None:
        return False
    if tuple(row) != (
        exercise["exercise_id"],
        exercise["started_at"],
        exercise["ended_at"],
    ):
        fail("session-timeline identity reused with different content")
    return True


def apply(db: sqlite3.Connection, root: dict) -> tuple[int, int]:
    applied = 0
    unchanged = 0
    for session in root["sessions"]:
        session_row_id = _existing_session(db, session)
        for exercise in session["exercises"]:
            owner = db.execute(
                "SELECT e.exercise_id FROM session_exercises se "
                "JOIN exercises e ON e.id=se.exercise_row_id "
                "WHERE se.session_row_id=? AND se.entry_id=?",
                (session_row_id, exercise["entry_id"]),
            ).fetchone()
            if owner is None or owner[0] != exercise["exercise_id"]:
                fail("session-timeline occurrence does not match session history")
            if _same_timing(db, session["session_id"], exercise):
                unchanged += 1
                continue
            db.execute(
                "INSERT INTO session_exercise_timeline("
                "session_id,entry_id,exercise_id,started_at,ended_at,imported_at"
                ") VALUES(?,?,?,?,?,?)",
                (
                    session["session_id"],
                    exercise["entry_id"],
                    exercise["exercise_id"],
                    exercise["started_at"],
                    exercise["ended_at"],
                    root["generated_at"],
                ),
            )
            applied += 1
    return applied, unchanged


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    root = load(args.input)
    with connect_database(args.database) as db:
        if db.execute("PRAGMA user_version").fetchone()[0] not in (31, 32, 33, 34, 35):
            fail("desktop schema v31-v35 required")
        db.execute("BEGIN IMMEDIATE")
        apply(db, root)
        db.commit()


if __name__ == "__main__":
    main()
