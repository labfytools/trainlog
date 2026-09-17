#!/usr/bin/env python3
"""Bridge lifecycle artifacts through the real staged desktop entry points."""

import argparse
import json
import sqlite3
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests"))

from test_session_exchange_v3 import V11_SCHEMA  # noqa: E402


V19_ADDITIONS = """
ALTER TABLE session_exercises ADD COLUMN tracking_mode TEXT;
DROP TABLE exercise_body_zone_sync;
CREATE TABLE exercise_body_zone_sync(exercise_row_id INTEGER PRIMARY KEY REFERENCES exercises(id) ON DELETE CASCADE,synced_state TEXT NOT NULL);
CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY,canonical_exercise_id TEXT NOT NULL REFERENCES exercises(exercise_id) ON DELETE RESTRICT,CHECK(source_exercise_id<>canonical_exercise_id));
CREATE TABLE exercise_feedback(id INTEGER PRIMARY KEY,feedback_id TEXT NOT NULL UNIQUE,session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id) ON DELETE CASCADE,observed_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE exercise_feedback_revisions(revision_id TEXT PRIMARY KEY,feedback_id TEXT NOT NULL REFERENCES exercise_feedback(feedback_id) ON DELETE CASCADE,created_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE session_followups(id INTEGER PRIMARY KEY,followup_id TEXT NOT NULL UNIQUE,session_row_id INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,observed_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE session_followup_revisions(revision_id TEXT PRIMARY KEY,followup_id TEXT NOT NULL REFERENCES session_followups(followup_id) ON DELETE CASCADE,created_at TEXT NOT NULL,raw_text TEXT NOT NULL);
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


def domain_exchange(manifest_path: Path, database: Path, output: Path) -> None:
    """Provision a replica only through the supported production exchanges."""
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    create_v19(database)
    run([sys.executable, str(ROOT / "tools/import_equipment_definitions.py"),
         manifest["equipment"], "--database", str(database)])
    run([sys.executable, str(ROOT / "tools/import_mobile_export.py"),
         manifest["history"], "--database", str(database)])
    run([sys.executable, str(ROOT / "tools/import_exercise_body_zones.py"),
         manifest["zones"], "--database", str(database)])
    run([sys.executable, str(ROOT / "tools/import_training_feedback.py"),
         manifest["feedback"], "--database", str(database)])
    run([sys.executable, str(ROOT / "tools/import_exercise_aliases.py"),
         manifest["aliases"], "--database", str(database)])
    output.mkdir(parents=True)
    commands = {
        "history": ("export_pc_mobile.py", ["--version", "4"]),
        "catalog": ("export_pc_catalog.py", []),
        "equipment": ("export_equipment_definitions.py", []),
        "zones": ("export_exercise_body_zones.py", []),
        "feedback": ("export_training_feedback.py", []),
        "aliases": ("export_exercise_aliases.py", []),
    }
    for name, (tool, extra) in commands.items():
        target = output / f"{name}.json"
        run([sys.executable, str(ROOT / "tools" / tool), str(target),
             "--database", str(database), *extra])


def inspect_domains(manifest_path: Path, database: Path, output: Path) -> None:
    """Report persisted effects; production code owns every mutation above."""
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    with sqlite3.connect(database) as connection:
        scalar = lambda sql, args=(): connection.execute(sql, args).fetchone()[0]
        states = {kind: scalar(
            "SELECT deleted FROM sync_causal_state WHERE target_kind=? AND target_id=?",
            (kind, target),
        ) for kind, target in manifest["targets"].items()}
        result = {
            "states": states,
            "operation_count": scalar("SELECT COUNT(*) FROM sync_causal_operations"),
            "target_observation_rows": scalar(
                "SELECT COUNT(*) FROM body_observations WHERE observation_id=?",
                (manifest["targets"]["body_observation"],)),
            "other_observation_rows": scalar(
                "SELECT COUNT(*) FROM body_observations WHERE observation_id=?",
                (manifest["other_observation_id"],)),
            "target_exercise_history_rows": scalar(
                "SELECT COUNT(*) FROM exercises WHERE exercise_id=?",
                (manifest["targets"]["exercise"],)),
            "target_exercise_available": scalar(
                "SELECT COUNT(*) FROM exercises e WHERE e.exercise_id=? AND NOT EXISTS("
                "SELECT 1 FROM sync_causal_state s WHERE s.target_kind='exercise' "
                "AND s.target_id=e.exercise_id AND s.deleted=1)",
                (manifest["targets"]["exercise"],)),
            "other_exercise_available": scalar(
                "SELECT COUNT(*) FROM exercises e WHERE e.exercise_id=? AND NOT EXISTS("
                "SELECT 1 FROM sync_causal_state s WHERE s.target_kind='exercise' "
                "AND s.target_id=e.exercise_id AND s.deleted=1)",
                (manifest["other_exercise_id"],)),
            "target_equipment_history_rows": scalar(
                "SELECT COUNT(*) FROM custom_equipment WHERE equipment_id=?",
                (manifest["targets"]["custom_equipment"],)),
            "target_equipment_available": scalar(
                "SELECT COUNT(*) FROM custom_equipment e WHERE e.equipment_id=? AND NOT EXISTS("
                "SELECT 1 FROM sync_causal_state s WHERE s.target_kind='custom_equipment' "
                "AND s.target_id=e.equipment_id AND s.deleted=1)",
                (manifest["targets"]["custom_equipment"],)),
            "other_equipment_available": scalar(
                "SELECT COUNT(*) FROM custom_equipment e WHERE e.equipment_id=? AND NOT EXISTS("
                "SELECT 1 FROM sync_causal_state s WHERE s.target_kind='custom_equipment' "
                "AND s.target_id=e.equipment_id AND s.deleted=1)",
                (manifest["other_equipment_id"],)),
            "builtin_equipment_rows": scalar(
                "SELECT COUNT(*) FROM session_exercises WHERE equipment_id='leg_press'"),
            "feedback_root_rows": scalar(
                "SELECT COUNT(*) FROM exercise_feedback WHERE feedback_id=?",
                (manifest["targets"]["feedback"],)),
            "feedback_revision_rows": scalar(
                "SELECT COUNT(*) FROM exercise_feedback_revisions WHERE feedback_id=?",
                (manifest["targets"]["feedback"],)),
            "feedback_current_rows": scalar(
                "SELECT COUNT(*) FROM exercise_feedback f WHERE f.feedback_id=? AND NOT EXISTS("
                "SELECT 1 FROM sync_causal_state s WHERE s.target_kind='feedback' "
                "AND s.target_id=f.feedback_id AND s.deleted=1)",
                (manifest["targets"]["feedback"],)),
            "other_feedback_current_rows": scalar(
                "SELECT COUNT(*) FROM exercise_feedback f WHERE f.feedback_id=? AND NOT EXISTS("
                "SELECT 1 FROM sync_causal_state s WHERE s.target_kind='feedback' "
                "AND s.target_id=f.feedback_id AND s.deleted=1)",
                (manifest["other_feedback_id"],)),
            "target_zone_rows": scalar(
                "SELECT COUNT(*) FROM exercise_body_zones z JOIN exercises e ON e.id=z.exercise_row_id "
                "WHERE e.exercise_id=? AND z.zone_id=? AND z.role=?",
                tuple(manifest["targets"]["body_zone_relation"].split("|"))),
            "other_zone_rows": scalar(
                "SELECT COUNT(*) FROM exercise_body_zones z JOIN exercises e ON e.id=z.exercise_row_id "
                "WHERE e.exercise_id=? AND z.zone_id=?",
                (manifest["zone_exercise_id"], manifest["other_zone_id"])),
            "session_rows": scalar("SELECT COUNT(*) FROM sessions WHERE session_id=?",
                (manifest["session_id"],)),
        }
    output.write_text(json.dumps(result, sort_keys=True), encoding="utf-8")


def refuse_stale_domains(manifest_path: Path, database: Path, output: Path) -> None:
    """Require every protected legacy producer to refuse stale restoration."""
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    commands = {
        "history": ("import_mobile_export.py", manifest["history"]),
        "equipment": ("import_equipment_definitions.py", manifest["equipment"]),
        "zones": ("import_exercise_body_zones.py", manifest["zones"]),
        "feedback": ("import_training_feedback.py", manifest["feedback"]),
    }
    refused = {}
    for name, (tool, artifact) in commands.items():
        result = subprocess.run([sys.executable, str(ROOT / "tools" / tool), artifact,
                                 "--database", str(database)],
                                text=True, capture_output=True, check=False)
        refused[name] = result.returncode != 0
    if not all(refused.values()):
        raise RuntimeError(f"stale companion unexpectedly accepted: {refused}")
    output.write_text(json.dumps(refused, sort_keys=True), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=("history", "draft", "domains", "inspect-domains", "refuse-domains"))
    parser.add_argument("input", type=Path)
    parser.add_argument("database", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.kind == "domains":
        domain_exchange(args.input, args.database, args.output)
        return
    if args.kind == "inspect-domains":
        inspect_domains(args.input, args.database, args.output)
        return
    if args.kind == "refuse-domains":
        refuse_stale_domains(args.input, args.database, args.output)
        return
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
