#!/usr/bin/env python3
"""Staged causal-deletion service; automatic synchronization remains V3."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import sqlite3
import sys
import uuid
from pathlib import Path

from trainlog_sqlite import connect_database
from validate_json import parse_timestamp
from exercise_names import load_exercise_names

FORMAT = "trainlog-causal-deletions"
VERSION = 1
MAX_BYTES = 4 * 1024 * 1024
MAX_OPERATIONS = 4096
KINDS = {"session", "execution_draft", "exercise", "body_observation",
         "custom_equipment", "feedback", "body_zone_relation"}


class CausalError(RuntimeError):
    pass


def canonical(value) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def digest(value) -> str:
    return hashlib.sha256(canonical(value).encode("utf-8")).hexdigest()


def require_schema(db: sqlite3.Connection) -> None:
    version = db.execute("PRAGMA user_version").fetchone()[0]
    if version not in (20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36):
        raise CausalError(f"desktop schema v20-v36 required, found v{version}")


def target_snapshot(db: sqlite3.Connection, kind: str, target: str):
    if kind == "session":
        row = db.execute("SELECT id,session_id,started_at,ended_at,session_type,notes FROM sessions WHERE session_id=?", (target,)).fetchone()
        if row is None:
            return None
        session_row = row[0]
        occurrences = []
        for occurrence in db.execute(
            "SELECT se.id,se.entry_id,e.exercise_id,se.position,se.recording_mode,se.tracking_mode,se.data_fields,se.equipment_id,se.load_mode,se.rest_seconds,se.target_sets,se.target_reps,se.target_duration_seconds,se.target_weight_kg,se.notes "
            "FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id WHERE se.session_row_id=? ORDER BY se.position,se.entry_id", (session_row,)
        ):
            occurrence_row = occurrence[0]
            sets = [list(value) for value in db.execute(
                "SELECT position,reps,duration_seconds,weight_kg FROM performed_sets WHERE session_exercise_row_id=? ORDER BY position", (occurrence_row,))]
            continuous = db.execute(
                "SELECT duration_seconds,speed_kmh,distance_km FROM continuous_activity WHERE session_exercise_row_id=?", (occurrence_row,)).fetchone()
            maximum = db.execute(
                "SELECT max_weight_kg FROM max_results WHERE session_exercise_row_id=?", (occurrence_row,)).fetchone()
            feedback = []
            for root in db.execute(
                "SELECT feedback_id,observed_at,raw_text FROM exercise_feedback WHERE session_exercise_row_id=? ORDER BY observed_at,feedback_id", (occurrence_row,)):
                revisions = [list(value) for value in db.execute(
                    "SELECT revision_id,created_at,raw_text FROM exercise_feedback_revisions WHERE feedback_id=? ORDER BY created_at,revision_id", (root[0],))]
                feedback.append([*root, revisions])
            occurrence_note = db.execute(
                "SELECT revision_id FROM sync_note_state WHERE owner_kind='occurrence' AND owner_id=?", (occurrence[1],)).fetchone()
            occurrences.append([*occurrence[1:], sets,
                                None if continuous is None else list(continuous),
                                None if maximum is None else maximum[0], feedback,
                                None if occurrence_note is None else occurrence_note[0]])
        session_note = db.execute(
            "SELECT revision_id FROM sync_note_state WHERE owner_kind='session' AND owner_id=?", (target,)).fetchone()
        links = [value[0] for value in db.execute(
            "SELECT observation_id FROM body_observations WHERE session_row_id=? ORDER BY observation_id", (session_row,))]
        return [*row[1:], occurrences, None if session_note is None else session_note[0], links]
    elif kind == "execution_draft":
        row = db.execute("SELECT session_id,revision_id FROM execution_drafts WHERE session_id=?", (target,)).fetchone()
    elif kind == "exercise":
        row = db.execute("SELECT exercise_id,name,recording_mode,tracking_mode,data_fields FROM exercises WHERE exercise_id=?", (target,)).fetchone()
    elif kind == "body_observation":
        row = db.execute("SELECT bo.observation_id,bo.observed_at,bo.body_weight_kg,bo.neck_cm,bo.shoulders_cm,bo.chest_cm,bo.waist_cm,bo.hips_cm,bo.left_arm_cm,bo.right_arm_cm,bo.left_forearm_cm,bo.right_forearm_cm,bo.left_thigh_cm,bo.right_thigh_cm,bo.left_calf_cm,bo.right_calf_cm,bo.notes,s.session_id FROM body_observations bo LEFT JOIN sessions s ON s.id=bo.session_row_id WHERE bo.observation_id=?", (target,)).fetchone()
        if row is not None:
            note = db.execute("SELECT revision_id FROM sync_note_state WHERE owner_kind='observation' AND owner_id=?", (target,)).fetchone()
            return [*row, None if note is None else note[0]]
    elif kind == "custom_equipment":
        row = db.execute("SELECT equipment_id,display_name,label_name,equipment_type,load_semantics FROM custom_equipment WHERE equipment_id=?", (target,)).fetchone()
    elif kind == "feedback":
        row = db.execute("SELECT feedback_id,observed_at,raw_text FROM exercise_feedback WHERE feedback_id=?", (target,)).fetchone()
        if row is not None:
            revisions = [list(value) for value in db.execute("SELECT revision_id,created_at,raw_text FROM exercise_feedback_revisions WHERE feedback_id=? ORDER BY created_at,revision_id", (target,))]
            return [*row, revisions]
    else:
        parts = target.split("|")
        if len(parts) != 3:
            raise CausalError("body-zone target must be exercise_id|zone_id|role")
        row = db.execute("SELECT e.exercise_id,z.zone_id,z.role FROM exercise_body_zones z JOIN exercises e ON e.id=z.exercise_row_id WHERE e.exercise_id=? AND z.zone_id=? AND z.role=?", parts).fetchone()
    return None if row is None else list(row)


def live_revision(db: sqlite3.Connection, kind: str, target: str) -> str | None:
    snapshot = target_snapshot(db, kind, target)
    return None if snapshot is None else "lv_" + digest(snapshot)


def apply_effect(db: sqlite3.Connection, kind: str, target: str, operation_id: str) -> None:
    if kind == "session":
        db.execute("UPDATE body_observations SET session_row_id=NULL WHERE session_row_id=(SELECT id FROM sessions WHERE session_id=?)", (target,))
        db.execute("DELETE FROM sessions WHERE session_id=?", (target,))
        db.execute("INSERT OR IGNORE INTO execution_draft_finalizations VALUES(?,?,?)", (target, "deleted:" + operation_id, dt.datetime.now().astimezone().isoformat()))
    elif kind == "execution_draft":
        if db.execute("SELECT 1 FROM execution_draft_finalizations WHERE session_id=?", (target,)).fetchone():
            raise CausalError("draft finalization conflicts with deletion")
        db.execute("DELETE FROM execution_drafts WHERE session_id=?", (target,))
    elif kind == "body_observation":
        db.execute("DELETE FROM body_observations WHERE observation_id=?", (target,))
    elif kind == "body_zone_relation":
        exercise, zone, role = target.split("|")
        db.execute("DELETE FROM exercise_body_zones WHERE exercise_row_id=(SELECT id FROM exercises WHERE exercise_id=?) AND zone_id=? AND role=?", (exercise, zone, role))
    # Exercise/equipment retirement and feedback withdrawal retain the row and
    # immutable history; availability/current-state readers consult the causal state.


def validate_operation(value: object) -> dict:
    keys = {"operation_id", "target_kind", "target_id", "creator_id",
            "predecessor_revision_id", "created_at", "payload_sha256",
            "publication_context"}
    if not isinstance(value, dict) or set(value) != keys:
        raise CausalError("operation has unknown or missing fields")
    for key, maximum in (("operation_id", 128), ("target_id", 512),
                         ("creator_id", 128), ("predecessor_revision_id", 128)):
        if not isinstance(value[key], str) or not 0 < len(value[key].encode()) <= maximum:
            raise CausalError(f"invalid {key}")
    if value["target_kind"] not in KINDS or value["publication_context"] is not None:
        raise CausalError("invalid target kind or publication context")
    if not isinstance(value["payload_sha256"], str) or len(value["payload_sha256"]) != 64 or any(c not in "0123456789abcdef" for c in value["payload_sha256"]):
        raise CausalError("invalid payload digest")
    parse_timestamp(value["created_at"], "created_at")
    payload = {k: value[k] for k in keys - {"payload_sha256"}}
    if digest(payload) != value["payload_sha256"]:
        raise CausalError("operation payload digest mismatch")
    return value


def apply_operation(db: sqlite3.Connection, operation: dict) -> str:
    op = validate_operation(operation)
    identity = op["operation_id"]
    known = db.execute("SELECT target_kind,target_id,creator_id,predecessor_revision_id,created_at,payload_sha256,publication_context FROM sync_causal_operations WHERE operation_id=?", (identity,)).fetchone()
    expected = (op["target_kind"], op["target_id"], op["creator_id"], op["predecessor_revision_id"], op["created_at"], op["payload_sha256"], None)
    if known:
        if tuple(known) != expected:
            raise CausalError("operation identity reused with different content")
        return "unchanged"
    authorize_target(db, op["target_kind"], op["target_id"])
    state = db.execute("SELECT current_revision_id,deleted FROM sync_causal_state WHERE target_kind=? AND target_id=?", (op["target_kind"], op["target_id"])).fetchone()
    current = state[0] if state else live_revision(db, op["target_kind"], op["target_id"])
    if state and state[1]:
        raise CausalError("concurrent deletion conflict")
    if current is None:
        raise CausalError("unknown target or ancestry")
    if current != op["predecessor_revision_id"]:
        raise CausalError("causal predecessor conflict")
    count = db.execute("SELECT COUNT(*) FROM sync_causal_operations").fetchone()[0]
    if count >= MAX_OPERATIONS:
        raise CausalError("protection set exceeds operation bound")
    db.execute("INSERT INTO sync_causal_operations VALUES(?,?,?,?,?,?,?,NULL)",
               (identity, op["target_kind"], op["target_id"], op["creator_id"],
                op["predecessor_revision_id"], op["created_at"], op["payload_sha256"]))
    if len(canonical(export_document(db)).encode("utf-8")) > MAX_BYTES:
        raise CausalError("protection set exceeds artifact byte bound")
    apply_effect(db, op["target_kind"], op["target_id"], identity)
    db.execute("INSERT OR REPLACE INTO sync_causal_state VALUES(?,?,?,?,?)",
               (op["target_kind"], op["target_id"], identity, 1, identity))
    return "applied"


def authorize_target(db: sqlite3.Connection, kind: str, target: str) -> None:
    if kind == "exercise":
        canonical = db.execute("SELECT canonical_exercise_id FROM exercise_aliases WHERE source_exercise_id=?", (target,)).fetchone() if _table_exists(db, "exercise_aliases") else None
        if target in load_exercise_names() or canonical is not None and canonical[0] in load_exercise_names():
            raise CausalError("built-in exercise identity cannot be retired")
    if kind == "custom_equipment" and _table_exists(db, "equipment") and db.execute(
            "SELECT 1 FROM equipment WHERE equipment_id=? AND equipment_type<>'custom_machine'", (target,)).fetchone():
        raise CausalError("built-in equipment identity cannot be retired")


def _table_exists(db: sqlite3.Connection, table: str) -> bool:
    return db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", (table,)).fetchone() is not None


def local_delete(db: sqlite3.Connection, kind: str, target: str, creator: str) -> tuple[dict, str]:
    authorize_target(db, kind, target)
    state = db.execute("SELECT current_revision_id,deleted FROM sync_causal_state WHERE target_kind=? AND target_id=?", (kind, target)).fetchone()
    if state and state[1]:
        row = db.execute("SELECT operation_id,target_kind,target_id,creator_id,predecessor_revision_id,created_at,payload_sha256,publication_context FROM sync_causal_operations WHERE operation_id=?", (state[0],)).fetchone()
        keys = ("operation_id", "target_kind", "target_id", "creator_id", "predecessor_revision_id", "created_at", "payload_sha256", "publication_context")
        return dict(zip(keys, row)), "unchanged"
    current = state[0] if state and not state[1] else live_revision(db, kind, target)
    if current is None:
        state = db.execute("SELECT operation_id FROM sync_causal_state WHERE target_kind=? AND target_id=? AND deleted=1", (kind, target)).fetchone()
        if state:
            row = db.execute("SELECT operation_id,target_kind,target_id,creator_id,predecessor_revision_id,created_at,payload_sha256,publication_context FROM sync_causal_operations WHERE operation_id=?", state).fetchone()
            keys = ("operation_id", "target_kind", "target_id", "creator_id", "predecessor_revision_id", "created_at", "payload_sha256", "publication_context")
            return dict(zip(keys, row)), "unchanged"
        raise CausalError("target not found")
    operation = {"operation_id": "del_" + str(uuid.uuid4()), "target_kind": kind,
                 "target_id": target, "creator_id": creator,
                 "predecessor_revision_id": current,
                 "created_at": dt.datetime.now().astimezone().isoformat(),
                 "publication_context": None}
    operation["payload_sha256"] = digest(operation)
    return operation, apply_operation(db, operation)


def load(path: Path) -> dict:
    with path.open("rb") as source:
        raw = source.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise CausalError("artifact exceeds 4 MiB")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result: raise CausalError("duplicate JSON key")
            result[key] = value
        return result
    try: root = json.loads(raw.decode("utf-8"), object_pairs_hook=unique)
    except (UnicodeError, json.JSONDecodeError) as error: raise CausalError("invalid UTF-8/JSON") from error
    if not isinstance(root, dict) or set(root) != {"format", "version", "generated_at", "operations"} or root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        raise CausalError("unsupported causal-deletion artifact")
    parse_timestamp(root["generated_at"], "generated_at")
    if not isinstance(root["operations"], list) or len(root["operations"]) > MAX_OPERATIONS:
        raise CausalError("operations bound exceeded")
    ids = set()
    for value in root["operations"]:
        op = validate_operation(value)
        if op["operation_id"] in ids: raise CausalError("duplicate operation_id")
        ids.add(op["operation_id"])
    return root


def export_document(db: sqlite3.Connection) -> dict:
    keys = ("operation_id", "target_kind", "target_id", "creator_id", "predecessor_revision_id", "created_at", "payload_sha256", "publication_context")
    rows = db.execute("SELECT " + ",".join(keys) + " FROM sync_causal_operations ORDER BY operation_id LIMIT ?", (MAX_OPERATIONS + 1,)).fetchall()
    if len(rows) > MAX_OPERATIONS: raise CausalError("protection set exceeds artifact bound")
    document = {"format": FORMAT, "version": VERSION,
            "generated_at": dt.datetime.now().astimezone().isoformat(),
            "operations": [dict(zip(keys, row)) for row in rows]}
    if len(canonical(document).encode("utf-8")) > MAX_BYTES:
        raise CausalError("protection set exceeds artifact byte bound")
    return document


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("delete", "import", "export"))
    parser.add_argument("path", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--kind", choices=sorted(KINDS)); parser.add_argument("--target-id")
    parser.add_argument("--creator-id", default="peer_desktop_local")
    args = parser.parse_args()
    db = connect_database(args.database); require_schema(db)
    try:
        db.execute("BEGIN IMMEDIATE")
        if args.mode == "delete":
            if not args.kind or not args.target_id: raise CausalError("delete requires --kind and --target-id")
            op, outcome = local_delete(db, args.kind, args.target_id, args.creator_id)
            args.path.write_text(canonical(op), encoding="utf-8"); print(outcome)
        elif args.mode == "import":
            for op in load(args.path)["operations"]: print(op["operation_id"] + "=" + apply_operation(db, op))
        else:
            args.path.write_text(canonical(export_document(db)), encoding="utf-8"); print("CAUSAL_DELETE_EXPORT=PASS")
        db.commit()
    except Exception:
        db.rollback(); raise
    finally: db.close()


if __name__ == "__main__":
    try: main()
    except Exception as error:
        print("CAUSAL_DELETE=FAIL " + str(error)); raise SystemExit(1)
