#!/usr/bin/env python3
"""Explicit staged execution-draft codec and desktop lifecycle service.

This module is deliberately absent from trainlog_sync_run(): local persistence
success is not peer delivery and the active MTP protocol remains V3.
"""
import argparse
import json
import sqlite3
from datetime import datetime
from pathlib import Path

from trainlog_sqlite import connect_database
from validate_json import TrainlogSemanticError, parse_timestamp

FORMAT = "trainlog-execution-drafts"
VERSION = 1
MAX_DOCUMENT_BYTES = 4 * 1024 * 1024
MAX_DRAFTS = 16
MAX_ENTRIES = 64
ROOT_KEYS = {"format", "version", "generated_at", "drafts"}
DRAFT_KEYS = {"session_id", "session_type", "source_session_id", "started_at",
              "revision_id", "parent_revision_id", "state", "note", "entries"}
ENTRY_KEYS = {"entry_id", "position", "exercise_id", "equipment_id",
              "recording_mode", "tracking_mode", "data_fields", "load_mode",
              "rest_seconds", "target", "sets", "continuous", "max_weight_kg",
              "note", "feedback"}


class LifecycleError(RuntimeError):
    pass


def exact(value, keys, label):
    if not isinstance(value, dict) or set(value) != keys:
        raise LifecycleError(f"{label}: exact keys required")


def text(value, label, nullable=False, maximum=128):
    if nullable and value is None:
        return
    if not isinstance(value, str) or not value or len(value.encode()) > maximum:
        raise LifecycleError(f"{label}: invalid text")


def timestamp(value, label, nullable=False):
    if nullable and value is None:
        return
    text(value, label)
    try:
        parse_timestamp(value, label)
    except TrainlogSemanticError as error:
        raise LifecycleError(f"{label}: invalid timestamp") from error


def note(value, label):
    if value is None:
        return
    exact(value, {"value", "revision_id", "parent_revision_id"}, label)
    text(value["revision_id"], label + ".revision_id")
    text(value["parent_revision_id"], label + ".parent_revision_id", nullable=True)
    if value["parent_revision_id"] == value["revision_id"]:
        raise LifecycleError(f"{label}: self parent")
    if value["value"] is not None and (not isinstance(value["value"], str) or
                                       len(value["value"].encode()) > 4096):
        raise LifecycleError(f"{label}: note exceeds 4096 UTF-8 bytes")


def validate(document):
    exact(document, ROOT_KEYS, "root")
    if document["format"] != FORMAT or document["version"] != VERSION:
        raise LifecycleError("unsupported execution-draft format/version")
    timestamp(document["generated_at"], "generated_at")
    drafts = document["drafts"]
    if not isinstance(drafts, list) or len(drafts) > MAX_DRAFTS:
        raise LifecycleError("drafts: invalid bound")
    identities = set()
    for di, draft in enumerate(drafts):
        label = f"drafts[{di}]"
        exact(draft, DRAFT_KEYS, label)
        text(draft["session_id"], label + ".session_id")
        if draft["session_id"] in identities:
            raise LifecycleError(label + ": duplicate session_id")
        identities.add(draft["session_id"])
        if draft["session_type"] not in ("training", "max_test") or draft["state"] not in ("active", "pending"):
            raise LifecycleError(label + ": invalid lifecycle enum")
        text(draft["source_session_id"], label + ".source_session_id", nullable=True)
        timestamp(draft["started_at"], label + ".started_at", nullable=True)
        text(draft["revision_id"], label + ".revision_id")
        text(draft["parent_revision_id"], label + ".parent_revision_id", nullable=True)
        if draft["revision_id"] == draft["parent_revision_id"]:
            raise LifecycleError(label + ": self parent")
        note(draft["note"], label + ".note")
        entries = draft["entries"]
        if not isinstance(entries, list) or len(entries) > MAX_ENTRIES:
            raise LifecycleError(label + ": entries bound")
        entry_ids, positions = set(), set()
        for ei, entry in enumerate(entries):
            elabel = f"{label}.entries[{ei}]"
            exact(entry, ENTRY_KEYS, elabel)
            text(entry["entry_id"], elabel + ".entry_id")
            text(entry["exercise_id"], elabel + ".exercise_id")
            text(entry["equipment_id"], elabel + ".equipment_id", nullable=True)
            if entry["entry_id"] in entry_ids or not isinstance(entry["position"], int) or entry["position"] < 0 or entry["position"] in positions:
                raise LifecycleError(elabel + ": duplicate identity/position")
            entry_ids.add(entry["entry_id"]); positions.add(entry["position"])
            if entry["recording_mode"] not in ("sets", "continuous") or entry["tracking_mode"] not in ("reps", "duration"):
                raise LifecycleError(elabel + ": invalid profile")
            if entry["recording_mode"] == "continuous" and entry["tracking_mode"] != "duration":
                raise LifecycleError(elabel + ": continuous requires duration")
            if not isinstance(entry["data_fields"], int) or entry["data_fields"] < 0 or entry["data_fields"] & ~3:
                raise LifecycleError(elabel + ": invalid data_fields")
            if entry["load_mode"] not in ("none", "external", "assistance") or not isinstance(entry["rest_seconds"], int) or not 0 <= entry["rest_seconds"] <= 86400:
                raise LifecycleError(elabel + ": invalid plan")
            note(entry["note"], elabel + ".note")
            if not isinstance(entry["feedback"], list) or len(entry["feedback"]) > 256:
                raise LifecycleError(elabel + ": feedback bound")
            feedback_ids = set()
            for fi, feedback in enumerate(entry["feedback"]):
                flabel = f"{elabel}.feedback[{fi}]"
                exact(feedback, {"feedback_id", "observed_at", "raw_text", "revisions"}, flabel)
                text(feedback["feedback_id"], flabel + ".feedback_id")
                if feedback["feedback_id"] in feedback_ids:
                    raise LifecycleError(flabel + ": duplicate feedback_id")
                feedback_ids.add(feedback["feedback_id"])
                timestamp(feedback["observed_at"], flabel + ".observed_at")
                text(feedback["raw_text"], flabel + ".raw_text", maximum=8192)
                if not isinstance(feedback["revisions"], list) or len(feedback["revisions"]) > 32:
                    raise LifecycleError(flabel + ": revisions bound")
                revision_ids = set()
                for ri, revision in enumerate(feedback["revisions"]):
                    rlabel = f"{flabel}.revisions[{ri}]"
                    exact(revision, {"revision_id", "created_at", "raw_text"}, rlabel)
                    text(revision["revision_id"], rlabel + ".revision_id")
                    timestamp(revision["created_at"], rlabel + ".created_at")
                    text(revision["raw_text"], rlabel + ".raw_text", maximum=8192)
                    if revision["revision_id"] in revision_ids:
                        raise LifecycleError(rlabel + ": duplicate revision_id")
                    revision_ids.add(revision["revision_id"])
            # The V4 history validator owns exact performed-data shapes. Draft
            # import refuses ambiguous simultaneous representations here.
            shapes = sum(value is not None for value in
                         (entry["sets"], entry["continuous"], entry["max_weight_kg"]))
            if shapes > 1:
                raise LifecycleError(elabel + ": contradictory actual shapes")
    return document


def load(path):
    raw = path.read_bytes()
    if len(raw) > MAX_DOCUMENT_BYTES:
        raise LifecycleError("document exceeds 4 MiB")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise LifecycleError("duplicate JSON key: " + key)
            result[key] = value
        return result
    try:
        return validate(json.loads(raw.decode("utf-8"), object_pairs_hook=unique))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise LifecycleError("invalid UTF-8/JSON") from error


def canonical(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def causal_payload(draft):
    """Return immutable revision content without receiver scheduling state.

    CONTRACT: ``revision_id`` identifies the draft content and causal parent;
    ``active`` versus ``pending`` is selected independently by each receiver.
    A round trip may therefore rewrite only ``state`` without creating a
    contradictory revision.
    """
    value = dict(draft)
    value.pop("state", None)
    return canonical(value)


def require_schema(connection):
    version = connection.execute("PRAGMA user_version").fetchone()[0]
    if version not in (19, 20, 21, 22, 23, 24, 25, 26, 27):
        raise LifecycleError(f"desktop schema v19-v25 required, found v{version}")


def import_document(connection, document, own_transaction=True):
    require_schema(connection)
    outcomes = []
    if own_transaction:
        connection.execute("BEGIN IMMEDIATE")
    try:
        for draft in document["drafts"]:
            session_id, revision = draft["session_id"], draft["revision_id"]
            payload = canonical(draft)
            causal = causal_payload(draft)
            if connection.execute("PRAGMA user_version").fetchone()[0] >= 20 and connection.execute("SELECT 1 FROM sync_causal_state WHERE target_kind='execution_draft' AND target_id=? AND deleted=1", (session_id,)).fetchone():
                outcomes.append((session_id, "stale")); continue
            if connection.execute("SELECT 1 FROM execution_draft_finalizations WHERE session_id=?", (session_id,)).fetchone():
                outcomes.append((session_id, "stale")); continue
            current = connection.execute(
                "SELECT revision_id,payload_json,state FROM execution_drafts WHERE session_id=?", (session_id,)).fetchone()
            known = connection.execute(
                "SELECT parent_revision_id,payload_json FROM execution_draft_revisions WHERE session_id=? AND revision_id=?",
                (session_id, revision)).fetchone()
            if known:
                if (known[0] != draft["parent_revision_id"] or
                        causal_payload(json.loads(known[1])) != causal):
                    raise LifecycleError("contradictory revision: " + session_id)
                outcomes.append((session_id, "unchanged" if current and current[0] == revision else "stale")); continue
            if current and draft["parent_revision_id"] != current[0]:
                outcomes.append((session_id, "conflict")); continue
            if not current:
                active = connection.execute("SELECT 1 FROM execution_drafts WHERE state='active'").fetchone()
                pending = connection.execute("SELECT COUNT(*) FROM execution_drafts WHERE state='pending'").fetchone()[0]
                state = "pending" if active else "active"
                if state == "pending" and pending >= MAX_DRAFTS:
                    raise LifecycleError("pending execution-draft capacity exhausted")
                connection.execute(
                    "INSERT INTO execution_drafts VALUES(?,?,?,?,?,?,?,?)",
                    (session_id, draft["session_type"], draft["source_session_id"], draft["started_at"],
                     revision, draft["parent_revision_id"], state, payload))
                outcome = state
            else:
                connection.execute(
                    "UPDATE execution_drafts SET revision_id=?,parent_revision_id=?,payload_json=? WHERE session_id=?",
                    (revision, draft["parent_revision_id"], payload, session_id))
                outcome = "applied"
            connection.execute("INSERT INTO execution_draft_revisions VALUES(?,?,?,?)",
                               (session_id, revision, draft["parent_revision_id"], payload))
            outcomes.append((session_id, outcome))
        if own_transaction:
            connection.commit()
    except Exception:
        if own_transaction:
            connection.rollback()
        raise
    return outcomes


def export_document(connection):
    require_schema(connection)
    drafts = [json.loads(row[0]) for row in connection.execute(
        "SELECT payload_json FROM execution_drafts ORDER BY CASE state WHEN 'active' THEN 0 ELSE 1 END,session_id")]
    for draft, row in zip(drafts, connection.execute(
            "SELECT state FROM execution_drafts ORDER BY CASE state WHEN 'active' THEN 0 ELSE 1 END,session_id")):
        draft["state"] = row[0]
    return {"format": FORMAT, "version": VERSION,
            "generated_at": datetime.now().astimezone().isoformat(), "drafts": drafts}


def activate(connection, session_id):
    require_schema(connection)
    connection.execute("BEGIN IMMEDIATE")
    try:
        if connection.execute("SELECT 1 FROM execution_drafts WHERE state='active'").fetchone():
            result = "occupied"
        elif connection.execute("SELECT 1 FROM execution_drafts WHERE session_id=? AND state='pending'", (session_id,)).fetchone():
            connection.execute("UPDATE execution_drafts SET state='active' WHERE session_id=?", (session_id,)); result = "activated"
        else:
            result = "not_found"
        connection.commit(); return result
    except Exception:
        connection.rollback(); raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("import", "export", "activate"))
    parser.add_argument("path", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--session-id")
    args = parser.parse_args()
    connection = connect_database(args.database)
    try:
        if args.mode == "import":
            for identity, outcome in import_document(connection, load(args.path)):
                print(f"{identity}={outcome}")
        elif args.mode == "export":
            args.path.write_text(canonical(export_document(connection)), encoding="utf-8")
            print("EXECUTION_DRAFT_EXPORT=PASS")
        else:
            if not args.session_id: raise LifecycleError("--session-id required")
            print("activation=" + activate(connection, args.session_id))
    finally:
        connection.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("EXECUTION_DRAFT=FAIL " + str(error))
        raise SystemExit(1)
