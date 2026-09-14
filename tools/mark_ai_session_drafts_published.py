#!/usr/bin/env python3
"""Commit the lifecycle cursor for one already-published AI draft batch."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import sqlite3
from pathlib import Path

from export_ai_session_drafts import mark_published
from trainlog_sqlite import connect_database


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    try:
        payload = json.loads(args.artifact.read_text(encoding="utf-8"))
        connection = connect_database(args.database)
        try:
            published_at = dt.datetime.now(dt.timezone.utc).isoformat(
                timespec="seconds").replace("+00:00", "Z")
            count = mark_published(connection, payload, published_at)
        finally:
            connection.close()
        print(f"AI_SESSION_DRAFT_PUBLISH_MARK=PASS drafts={count}")
        return 0
    except (OSError, sqlite3.Error, ValueError, json.JSONDecodeError) as error:
        print(f"AI_SESSION_DRAFT_PUBLISH_MARK=FAIL {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
