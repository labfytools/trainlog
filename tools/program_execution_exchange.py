#!/usr/bin/env python3
"""Validate and merge Android program-execution provenance companion V1."""

import argparse
import json
import re
import sqlite3
from datetime import datetime
from pathlib import Path

from trainlog_sqlite import connect_database


FORMAT = "trainlog-program-executions"
VERSION = 1
MAX_BYTES = 4 * 1024 * 1024
MAX_EXECUTIONS = 4096
UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
PROGRAM_ID = re.compile(rf"^pg_{UUID4}$")
PROGRAM_SESSION_ID = re.compile(rf"^pgs_{UUID4}$")
SESSION_ID = re.compile(rf"^se_{UUID4}$")


def fail(message: str) -> None:
    raise ValueError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail(f"duplicate JSON field: {key}")
        result[key] = value
    return result


def timestamp(value: object) -> str:
    if (
        not isinstance(value, str)
        or "T" not in value
        or re.search(r"(?:Z|[+-][0-9]{2}:[0-9]{2})$", value) is None
    ):
        fail("invalid execution timestamp")
    try:
        datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        fail("invalid execution timestamp")
    return value


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("program execution artifact exceeds 4 MiB")
    root = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    if not isinstance(root, dict) or set(root) != {
        "format", "version", "generated_at", "executions"
    }:
        fail("invalid program execution envelope")
    if root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        fail("unsupported program execution format")
    timestamp(root["generated_at"])
    values = root["executions"]
    if not isinstance(values, list) or len(values) > MAX_EXECUTIONS:
        fail("program executions are absent or out of bounds")

    seen_program_sessions = set()
    seen_sessions = set()
    for item in values:
        if not isinstance(item, dict) or set(item) != {
            "program_id", "program_session_id", "session_id", "state", "observed_at"
        }:
            fail("invalid program execution")
        if not PROGRAM_ID.fullmatch(item["program_id"]):
            fail("invalid program_id")
        if not PROGRAM_SESSION_ID.fullmatch(item["program_session_id"]):
            fail("invalid program_session_id")
        if not SESSION_ID.fullmatch(item["session_id"]):
            fail("invalid session_id")
        if item["state"] not in ("in_progress", "completed"):
            fail("invalid program execution state")
        timestamp(item["observed_at"])
        if item["program_session_id"] in seen_program_sessions or item["session_id"] in seen_sessions:
            fail("duplicate program execution identity")
        seen_program_sessions.add(item["program_session_id"])
        seen_sessions.add(item["session_id"])
    return root


def apply_executions(db: sqlite3.Connection, root: dict) -> tuple[int, int, int]:
    if db.execute("PRAGMA user_version").fetchone()[0] not in (27, 28, 29, 30):
        fail("desktop schema v27, v28 or v29 required")
    added = 0
    advanced = 0
    skipped = 0
    for item in root["executions"]:
        program_session = db.execute(
            "SELECT program_id FROM program_sessions WHERE program_session_id=?",
            (item["program_session_id"],),
        ).fetchone()
        if program_session is None or program_session[0] != item["program_id"]:
            fail("program execution references an unknown or mismatched session")
        existing = db.execute(
            "SELECT program_id,session_id,state,observed_at "
            "FROM program_session_executions WHERE program_session_id=?",
            (item["program_session_id"],),
        ).fetchone()
        expected = (
            item["program_id"], item["session_id"], item["state"], item["observed_at"]
        )
        if existing is not None and existing[0] == item["program_id"] and \
                existing[1] == item["session_id"] and existing[2] == "deleted":
            tombstone = db.execute(
                "SELECT 1 FROM sync_causal_state "
                "WHERE target_kind='session' AND target_id=? AND deleted=1",
                (item["session_id"],),
            ).fetchone()
            if tombstone is None:
                fail("deleted program execution lacks causal session tombstone")
            # CONTRACT: a retained Android generation may repeat an execution
            # fact after the desktop has causally deleted its completed
            # history. The tombstone dominates that older fact and the durable
            # terminal provenance row remains unchanged.
            skipped += 1
            continue
        if item["state"] == "completed":
            completed = db.execute(
                "SELECT 1 FROM sessions WHERE session_id=?", (item["session_id"],)
            ).fetchone()
            if completed is None:
                fail("completed program execution references missing history")
        if existing is None:
            db.execute(
                "INSERT INTO program_session_executions("
                "program_session_id,program_id,session_id,state,observed_at) "
                "VALUES(?,?,?,?,?)",
                (item["program_session_id"], *expected),
            )
            added += 1
        elif tuple(existing) == expected:
            skipped += 1
        elif existing[0] == item["program_id"] and existing[1] == item["session_id"] and \
                existing[2] == "in_progress" and item["state"] == "completed":
            db.execute(
                "UPDATE program_session_executions SET state='completed',observed_at=? "
                "WHERE program_session_id=?",
                (item["observed_at"], item["program_session_id"]),
            )
            advanced += 1
        else:
            fail("HARD CONFLICT for program execution provenance")
    return added, advanced, skipped


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    root = load(args.artifact)
    db = connect_database(args.database)
    try:
        db.execute("BEGIN IMMEDIATE")
        result = apply_executions(db, root)
        db.commit()
    except Exception:
        db.rollback()
        raise
    finally:
        db.close()
    print(
        "PROGRAM_EXECUTIONS_IMPORT=PASS "
        f"added={result[0]} advanced={result[1]} skipped={result[2]}"
    )


if __name__ == "__main__":
    main()
