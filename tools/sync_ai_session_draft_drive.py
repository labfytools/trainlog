#!/usr/bin/env python3
"""Fetch, import, and archive-copy the single AI session-draft inbox artifact."""

from __future__ import annotations

import argparse
import datetime as dt
import os
import sqlite3
import stat
import subprocess
import tempfile
from pathlib import Path

from import_ai_session_draft import ImportFailure, import_payload, load
from trainlog_sqlite import connect_database


INBOX_ROOT = "TrainLog Gdrive:Trainlog/AI/inbox"
ARTIFACT_NAME = "trainlog_ai_session_draft_v1.json"
SOURCE = f"{INBOX_ROOT}/{ARTIFACT_NAME}"
ARCHIVE_ROOT = "TrainLog Gdrive:Trainlog/AI/archive"
RCLONE_TIMEOUT_SECONDS = 15
DIAGNOSTIC_LIMIT = 1000


def detail(result):
    value = ((result.stderr or "")[:DIAGNOSTIC_LIMIT] or
             (result.stdout or "")[:DIAGNOSTIC_LIMIT]).strip()
    return (value.splitlines()[-1] if value else f"exit={result.returncode}")[:DIAGNOSTIC_LIMIT]


def run_rclone(runner, argv):
    """Execute a bounded, shell-free rclone command."""
    return runner(argv, capture_output=True, text=True, check=False,
                  timeout=RCLONE_TIMEOUT_SECONDS)


def inbox_contains_artifact(runner):
    """Return whether the exact canonical filename is present in the inbox."""
    try:
        listed = run_rclone(runner, ["rclone", "lsf", INBOX_ROOT,
                                     "--files-only", "--max-depth", "1",
                                     "--include", ARTIFACT_NAME])
    except (OSError, subprocess.TimeoutExpired) as error:
        return None, str(error)[:DIAGNOSTIC_LIMIT]
    if listed.returncode != 0:
        return None, detail(listed)
    names = {(line.strip().rstrip("/")) for line in
             (listed.stdout or "")[:DIAGNOSTIC_LIMIT].splitlines()}
    return ARTIFACT_NAME in names, None


def update_archive(database, draft_id, status, error=None):
    connection = connect_database(database)
    try:
        archived_at = (dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
                       if status == "archived" else None)
        connection.execute(
            "UPDATE ai_session_drafts SET archive_status=?,archived_at=?,archive_error=? WHERE draft_id=?",
            (status, archived_at, error, draft_id),
        )
        connection.commit()
    finally:
        connection.close()


def try_update_archive(database, draft_id, status, error=None):
    try:
        update_archive(database, draft_id, status, error)
        return None
    except (OSError, sqlite3.Error) as update_error:
        return str(update_error)[:1000]


def secure_local_draft(directory):
    """Create the inbound destination and verify its private permissions."""
    directory_path = Path(directory)
    os.chmod(directory_path, 0o700)
    if stat.S_IMODE(directory_path.stat().st_mode) != 0o700:
        raise OSError("le répertoire temporaire n'est pas privé")
    local = directory_path / "draft.json"
    descriptor = os.open(local, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.close(descriptor)
    if stat.S_IMODE(local.stat().st_mode) != 0o600:
        raise OSError("le brouillon temporaire n'est pas privé")
    return local


def enforce_private_local_draft(local):
    """Restore and verify the inbound destination's private permissions."""
    os.chmod(local, 0o600)
    if stat.S_IMODE(local.stat().st_mode) != 0o600:
        raise OSError("le brouillon temporaire n'est pas privé")


def synchronize(database, runner=subprocess.run):
    """Run shell-free rclone operations; transport failures are nonfatal.

    WHY: a fixed /tmp filename could re-import stale content after a failed
    download. CONTRACT: copyto writes only a fresh mode-0600 tempfile and argv
    is never interpreted by a shell. INVARIANT: the exact validated tempfile is
    archive-copied only after the importer commits; the mutable inbox object is
    never moved or deleted, so every failure remains retryable.
    """
    with tempfile.TemporaryDirectory(prefix="trainlog-ai-draft-") as directory:
        present, listing_error = inbox_contains_artifact(runner)
        if present is None:
            print(f"AI_SESSION_DRAFT_FETCH=FAIL {listing_error}")
            print("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL")
            return 0
        if not present:
            print("AI_SESSION_DRAFT_FETCH=NONE")
            print("AI_SESSION_DRAFT_INBOUND=NONE")
            return 0
        try:
            local = secure_local_draft(directory)
        except OSError as error:
            print(f"AI_SESSION_DRAFT_FETCH=FAIL {error}")
            print("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL")
            return 0
        try:
            fetched = run_rclone(runner, ["rclone", "copyto", SOURCE, str(local)])
        except (OSError, subprocess.TimeoutExpired) as error:
            print(f"AI_SESSION_DRAFT_FETCH=FAIL {error}")
            print("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL")
            return 0
        if fetched.returncode != 0:
            print(f"AI_SESSION_DRAFT_FETCH=FAIL {detail(fetched)}")
            print("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL")
            return 0
        try:
            if not local.is_file() or local.stat().st_size == 0:
                raise OSError("rclone n'a produit aucun fichier")
            enforce_private_local_draft(local)
        except OSError as error:
            print(f"AI_SESSION_DRAFT_FETCH=FAIL {error}")
            print("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL")
            return 0
        try:
            payload = load(local)
            connection = connect_database(database)
            try:
                result = import_payload(connection, payload)
            finally:
                connection.close()
        except (OSError, sqlite3.Error, ImportFailure, ValueError) as error:
            print(f"AI_SESSION_DRAFT_IMPORT=FAIL {error}")
            print("AI_SESSION_DRAFT_INBOUND=REJECTED")
            return 0
        draft_id = payload["draft"]["draft_id"]
        print(f"AI_SESSION_DRAFT_IMPORT=PASS result={result} draft_id={draft_id}")
        target = f"{ARCHIVE_ROOT}/{draft_id}.json"
        try:
            archived = run_rclone(runner, ["rclone", "copyto", str(local), target])
        except (OSError, subprocess.TimeoutExpired) as error:
            update_error = try_update_archive(database, draft_id, "failed", str(error)[:1000])
            print(f"AI_SESSION_DRAFT_ARCHIVE=FAIL {error}")
            if update_error is not None:
                print(f"AI_SESSION_DRAFT_ARCHIVE_STATUS=FAIL {update_error}")
            print("AI_SESSION_DRAFT_INBOUND=ARCHIVE_FAIL")
            return 0
        if archived.returncode != 0:
            error = detail(archived)
            update_error = try_update_archive(database, draft_id, "failed", error)
            print(f"AI_SESSION_DRAFT_ARCHIVE=FAIL {error}")
            if update_error is not None:
                print(f"AI_SESSION_DRAFT_ARCHIVE_STATUS=FAIL {update_error}")
            print("AI_SESSION_DRAFT_INBOUND=ARCHIVE_FAIL")
            return 0
        update_error = try_update_archive(database, draft_id, "archived")
        if update_error is not None:
            print(f"AI_SESSION_DRAFT_ARCHIVE_STATUS=FAIL {update_error}")
            print("AI_SESSION_DRAFT_INBOUND=ARCHIVE_FAIL")
            return 0
        print("AI_SESSION_DRAFT_ARCHIVE=PASS")
        print("AI_SESSION_DRAFT_INBOUND=" +
              ("IMPORTED" if result == "imported" else "ALREADY_IMPORTED"))
        return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", help="reserved sync-tool positional argument")
    parser.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    if args.artifact != ARTIFACT_NAME:
        parser.error(f"artifact must be {ARTIFACT_NAME}")
    return synchronize(args.database)


if __name__ == "__main__":
    raise SystemExit(main())
