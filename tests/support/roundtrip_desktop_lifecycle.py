#!/usr/bin/env python3
"""Bridge lifecycle artifacts through the real staged desktop entry points."""

import argparse
import sqlite3
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests"))

from test_session_exchange_v3 import V11_SCHEMA  # noqa: E402


V19_ADDITIONS = """
ALTER TABLE session_exercises ADD COLUMN tracking_mode TEXT;
CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY,canonical_exercise_id TEXT NOT NULL REFERENCES exercises(exercise_id) ON DELETE RESTRICT,CHECK(source_exercise_id<>canonical_exercise_id));
CREATE TABLE exercise_feedback(id INTEGER PRIMARY KEY,feedback_id TEXT NOT NULL UNIQUE,session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id) ON DELETE CASCADE,observed_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE exercise_feedback_revisions(revision_id TEXT PRIMARY KEY,feedback_id TEXT NOT NULL REFERENCES exercise_feedback(feedback_id) ON DELETE CASCADE,created_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE sync_note_revisions(owner_kind TEXT NOT NULL,owner_id TEXT NOT NULL,revision_id TEXT NOT NULL,parent_revision_id TEXT,note TEXT,CHECK(note IS NULL OR length(CAST(note AS BLOB))<=4096),PRIMARY KEY(owner_kind,owner_id,revision_id));
CREATE TABLE sync_note_state(owner_kind TEXT NOT NULL,owner_id TEXT NOT NULL,revision_id TEXT NOT NULL,PRIMARY KEY(owner_kind,owner_id),FOREIGN KEY(owner_kind,owner_id,revision_id) REFERENCES sync_note_revisions(owner_kind,owner_id,revision_id));
CREATE TABLE execution_drafts(session_id TEXT PRIMARY KEY,session_type TEXT NOT NULL,source_session_id TEXT,started_at TEXT,revision_id TEXT NOT NULL,parent_revision_id TEXT,state TEXT NOT NULL,payload_json TEXT NOT NULL);
CREATE TABLE execution_draft_revisions(session_id TEXT NOT NULL,revision_id TEXT NOT NULL,parent_revision_id TEXT,payload_json TEXT NOT NULL,PRIMARY KEY(session_id,revision_id));
CREATE TABLE execution_draft_finalizations(session_id TEXT PRIMARY KEY,final_revision_id TEXT NOT NULL,finalized_at TEXT NOT NULL);
CREATE TABLE sync_causal_operations(operation_id TEXT PRIMARY KEY,target_kind TEXT NOT NULL,target_id TEXT NOT NULL,creator_id TEXT NOT NULL,predecessor_revision_id TEXT NOT NULL,created_at TEXT NOT NULL,payload_sha256 TEXT NOT NULL,publication_context TEXT);
CREATE TABLE sync_causal_state(target_kind TEXT NOT NULL,target_id TEXT NOT NULL,current_revision_id TEXT NOT NULL,deleted INTEGER NOT NULL,operation_id TEXT,PRIMARY KEY(target_kind,target_id));
PRAGMA user_version=20;
"""


def run(arguments: list[str]) -> None:
    result = subprocess.run(arguments, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(result.stdout + result.stderr)


def create_v19(path: Path) -> None:
    # CONTRACT: fixture provisioning supplies only the production schema. All
    # artifact semantics remain owned by the real desktop tools below.
    with sqlite3.connect(path) as connection:
        connection.executescript(V11_SCHEMA)
        connection.executescript(V19_ADDITIONS)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=("history", "draft"))
    parser.add_argument("input", type=Path)
    parser.add_argument("database", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    create_v19(args.database)
    if args.kind == "history":
        run([sys.executable, str(ROOT / "tools/import_mobile_export.py"),
             str(args.input), "--database", str(args.database)])
        run([sys.executable, str(ROOT / "tools/export_pc_mobile.py"),
             str(args.output), "--database", str(args.database), "--version", "4"])
    else:
        tool = str(ROOT / "tools/execution_draft_exchange.py")
        run([sys.executable, tool, "import", str(args.input),
             "--database", str(args.database)])
        run([sys.executable, tool, "export", str(args.output),
             "--database", str(args.database)])


if __name__ == "__main__":
    main()
