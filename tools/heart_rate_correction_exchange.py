#!/usr/bin/env python3
"""Versioned causal corrections for acknowledged Heart Rate V1 captures."""

from __future__ import annotations

import datetime as dt
import hashlib
import json
import re
import sqlite3
import uuid
from pathlib import Path

from validate_json import parse_timestamp


FORMAT = "trainlog-heart-rate-corrections"
VERSION = 1
TARGET_KIND = "heart_rate_tail"
MAX_BYTES = 4 * 1024 * 1024
MAX_CORRECTIONS = 256
UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
CAPTURE_ID = re.compile(rf"^hrc_{UUID4}$")
OPERATION_ID = re.compile(rf"^hrx_{UUID4}$")
HEX = re.compile(r"^[0-9a-f]{64}$")


class CorrectionError(ValueError):
    """A correction is malformed, conflicts, or targets incompatible facts."""


def canonical(value: object) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def digest(value: object) -> str:
    return hashlib.sha256(canonical(value).encode("utf-8")).hexdigest()


def exact_timestamp(value: str):
    try:
        return parse_timestamp(value, "heart_rate_timestamp")
    except (TypeError, ValueError) as error:
        raise CorrectionError("invalid heart-rate timestamp") from error


def target_id(capture_id: str, cutoff: str) -> str:
    if not isinstance(capture_id, str) or not CAPTURE_ID.fullmatch(capture_id):
        raise CorrectionError("invalid heart-rate capture identity")
    exact_timestamp(cutoff)
    return f"{capture_id}|{cutoff}"


def split_target(value: str) -> tuple[str, str]:
    parts = value.split("|", 1)
    if len(parts) != 2:
        raise CorrectionError("invalid heart-rate correction target")
    target_id(parts[0], parts[1])
    return parts[0], parts[1]


def require_schema(db: sqlite3.Connection) -> None:
    version = db.execute("PRAGMA user_version").fetchone()[0]
    if version not in range(27, 37):
        raise CorrectionError(f"heart-rate correction requires schema v27-v36, found v{version}")


def _table_exists(db: sqlite3.Connection, table: str) -> bool:
    return db.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", (table,),
    ).fetchone() is not None


def capture_snapshot(db: sqlite3.Connection, capture_id: str):
    row = db.execute(
        "SELECT capture_id,context_kind,context_id,started_at,ended_at,sensor_name "
        "FROM heart_rate_captures WHERE capture_id=?", (capture_id,),
    ).fetchone()
    if row is None:
        return None
    samples = []
    for sample in db.execute(
        "SELECT sequence,observed_at,bpm,exercise_entry_id,sensor_contact_detected,"
        "energy_expended FROM heart_rate_samples WHERE capture_id=? ORDER BY sequence",
        (capture_id,),
    ):
        rr = [list(value) for value in db.execute(
            "SELECT rr_index,value_1024 FROM heart_rate_rr_intervals "
            "WHERE capture_id=? AND sample_sequence=? ORDER BY rr_index",
            (capture_id, sample[0]),
        )]
        samples.append([*sample, rr])
    return [*row, samples]


def live_revision(db: sqlite3.Connection, capture_id: str) -> str | None:
    snapshot = capture_snapshot(db, capture_id)
    return None if snapshot is None else "lv_" + digest(snapshot)


def authorize(db: sqlite3.Connection, capture_id: str, cutoff: str) -> None:
    row = db.execute(
        "SELECT context_kind,context_id,started_at,ended_at FROM heart_rate_captures "
        "WHERE capture_id=?", (capture_id,),
    ).fetchone()
    if row is None:
        raise CorrectionError("heart-rate capture not found")
    if row[0] not in ("session", "cardio"):
        raise CorrectionError("only session heart-rate captures can be corrected")
    if row[3] is None:
        raise CorrectionError("active heart-rate capture cannot be corrected")
    cutoff_key = exact_timestamp(cutoff)
    if cutoff_key < exact_timestamp(row[2]) or cutoff_key > exact_timestamp(row[3]):
        raise CorrectionError("heart-rate cutoff is outside capture")
    session = db.execute(
        "SELECT ended_at FROM sessions WHERE session_id=?", (row[1],),
    ).fetchone()
    if session is None or session[0] != cutoff:
        raise CorrectionError("heart-rate cutoff must equal the factual session end")
    timeline = ("session_timeline_exercises"
                if _table_exists(db, "session_timeline_exercises")
                else "session_exercise_timeline")
    latest = db.execute(
        f"SELECT MAX(ended_at) FROM {timeline} WHERE session_id=?", (row[1],),
    ).fetchone()[0]
    if latest is not None and exact_timestamp(latest) > cutoff_key:
        raise CorrectionError("heart-rate cutoff precedes the last exercise marker")
    previous = None
    for expected, (sequence, observed_at) in enumerate(db.execute(
        "SELECT sequence,observed_at FROM heart_rate_samples "
        "WHERE capture_id=? ORDER BY sequence", (capture_id,),
    )):
        observed = exact_timestamp(observed_at)
        if sequence != expected or previous is not None and observed < previous:
            raise CorrectionError("heart-rate samples are not one chronological sequence")
        previous = observed


def correction_payload(correction: dict) -> dict:
    return {key: correction[key] for key in (
        "operation_id", "capture_id", "cutoff", "creator_id",
        "predecessor_revision_id", "created_at",
    )}


def validate_correction(value: object) -> dict:
    keys = {"operation_id", "capture_id", "cutoff", "creator_id",
            "predecessor_revision_id", "created_at", "payload_sha256"}
    if not isinstance(value, dict) or set(value) != keys:
        raise CorrectionError("invalid heart-rate correction shape")
    if not isinstance(value["operation_id"], str) or not OPERATION_ID.fullmatch(value["operation_id"]):
        raise CorrectionError("invalid heart-rate correction identity")
    target_id(value["capture_id"], value["cutoff"])
    for field in ("creator_id", "predecessor_revision_id"):
        if not isinstance(value[field], str) or not 0 < len(value[field].encode()) <= 128:
            raise CorrectionError(f"invalid {field}")
    exact_timestamp(value["created_at"])
    if not isinstance(value["payload_sha256"], str) or not HEX.fullmatch(value["payload_sha256"]):
        raise CorrectionError("invalid heart-rate correction digest")
    if digest(correction_payload(value)) != value["payload_sha256"]:
        raise CorrectionError("heart-rate correction digest mismatch")
    return value


def apply_effect(db: sqlite3.Connection, capture_id: str, cutoff: str) -> None:
    # WHY: a stopped capture may cross an immutable generation before recovery
    # establishes the factual session end.
    # CONTRACT: the cutoff is inclusive; only its strict BPM/RR tail is removed.
    # INVARIANT: acknowledged generation bytes and retained identities never change.
    cutoff_key = exact_timestamp(cutoff)
    removed = [sequence for sequence, observed_at in db.execute(
        "SELECT sequence,observed_at FROM heart_rate_samples "
        "WHERE capture_id=? ORDER BY sequence", (capture_id,),
    ) if exact_timestamp(observed_at) > cutoff_key]
    db.executemany(
        "DELETE FROM heart_rate_samples WHERE capture_id=? AND sequence=?",
        [(capture_id, sequence) for sequence in removed],
    )
    if db.execute(
        "UPDATE heart_rate_captures SET ended_at=? WHERE capture_id=?",
        (cutoff, capture_id),
    ).rowcount != 1:
        raise CorrectionError("heart-rate capture disappeared during correction")


def apply_correction(db: sqlite3.Connection, value: dict) -> str:
    correction = validate_correction(value)
    operation_id = correction["operation_id"]
    target = target_id(correction["capture_id"], correction["cutoff"])
    known = db.execute(
        "SELECT target_kind,target_id,creator_id,predecessor_revision_id,created_at,"
        "payload_sha256,publication_context FROM sync_causal_operations "
        "WHERE operation_id=?", (operation_id,),
    ).fetchone()
    expected = (TARGET_KIND, target, correction["creator_id"],
                correction["predecessor_revision_id"], correction["created_at"],
                correction["payload_sha256"], None)
    if known is not None:
        if tuple(known) != expected:
            raise CorrectionError("heart-rate correction identity reused")
        return "unchanged"
    authorize(db, correction["capture_id"], correction["cutoff"])
    state = db.execute(
        "SELECT current_revision_id,deleted FROM sync_causal_state "
        "WHERE target_kind=? AND target_id=?", (TARGET_KIND, target),
    ).fetchone()
    current = state[0] if state else live_revision(db, correction["capture_id"])
    if state and state[1]:
        raise CorrectionError("concurrent heart-rate correction conflict")
    if current is None or current != correction["predecessor_revision_id"]:
        raise CorrectionError("heart-rate correction predecessor conflict")
    count = db.execute(
        "SELECT COUNT(*) FROM sync_causal_operations WHERE target_kind=?", (TARGET_KIND,),
    ).fetchone()[0]
    if count >= MAX_CORRECTIONS:
        raise CorrectionError("heart-rate correction bound exceeded")
    db.execute(
        "INSERT INTO sync_causal_operations VALUES(?,?,?,?,?,?,?,NULL)",
        (operation_id, TARGET_KIND, target, correction["creator_id"], current,
         correction["created_at"], correction["payload_sha256"]),
    )
    apply_effect(db, correction["capture_id"], correction["cutoff"])
    db.execute(
        "INSERT OR REPLACE INTO sync_causal_state VALUES(?,?,?,?,?)",
        (TARGET_KIND, target, operation_id, 1, operation_id),
    )
    if len(canonical(export_document(db)).encode("utf-8")) > MAX_BYTES:
        raise CorrectionError("heart-rate correction artifact exceeds byte bound")
    return "applied"


def local_correction(db: sqlite3.Connection, capture_id: str, cutoff: str,
                     creator_id: str) -> tuple[dict, str]:
    target = target_id(capture_id, cutoff)
    state = db.execute(
        "SELECT current_revision_id,deleted FROM sync_causal_state "
        "WHERE target_kind=? AND target_id=?", (TARGET_KIND, target),
    ).fetchone()
    if state and state[1]:
        row = db.execute(
            "SELECT operation_id,creator_id,predecessor_revision_id,created_at,payload_sha256 "
            "FROM sync_causal_operations WHERE operation_id=?", (state[0],),
        ).fetchone()
        correction = {
            "operation_id": row[0], "capture_id": capture_id, "cutoff": cutoff,
            "creator_id": row[1], "predecessor_revision_id": row[2],
            "created_at": row[3], "payload_sha256": row[4],
        }
        return correction, "unchanged"
    authorize(db, capture_id, cutoff)
    predecessor = state[0] if state else live_revision(db, capture_id)
    if predecessor is None:
        raise CorrectionError("heart-rate capture not found")
    correction = {
        "operation_id": "hrx_" + str(uuid.uuid4()),
        "capture_id": capture_id,
        "cutoff": cutoff,
        "creator_id": creator_id,
        "predecessor_revision_id": predecessor,
        "created_at": dt.datetime.now().astimezone().isoformat(),
    }
    correction["payload_sha256"] = digest(correction_payload(correction))
    return correction, apply_correction(db, correction)


def export_document(db: sqlite3.Connection) -> dict:
    corrections = []
    for row in db.execute(
        "SELECT operation_id,target_id,creator_id,predecessor_revision_id,created_at,"
        "payload_sha256 FROM sync_causal_operations WHERE target_kind=? "
        "ORDER BY operation_id LIMIT ?", (TARGET_KIND, MAX_CORRECTIONS + 1),
    ):
        capture_id, cutoff = split_target(row[1])
        corrections.append({
            "operation_id": row[0], "capture_id": capture_id, "cutoff": cutoff,
            "creator_id": row[2], "predecessor_revision_id": row[3],
            "created_at": row[4], "payload_sha256": row[5],
        })
    if len(corrections) > MAX_CORRECTIONS:
        raise CorrectionError("heart-rate correction bound exceeded")
    result = {
        "format": FORMAT,
        "version": VERSION,
        "generated_at": dt.datetime.now().astimezone().isoformat(),
        "corrections": corrections,
    }
    if len(canonical(result).encode("utf-8")) > MAX_BYTES:
        raise CorrectionError("heart-rate correction artifact exceeds byte bound")
    return result


def validate_document(value: object) -> dict:
    if not isinstance(value, dict) or set(value) != {
            "format", "version", "generated_at", "corrections"}:
        raise CorrectionError("invalid heart-rate correction envelope")
    if value["format"] != FORMAT or type(value["version"]) is not int or value["version"] != VERSION:
        raise CorrectionError("unsupported heart-rate correction artifact")
    exact_timestamp(value["generated_at"])
    if not isinstance(value["corrections"], list) or len(value["corrections"]) > MAX_CORRECTIONS:
        raise CorrectionError("heart-rate correction bound exceeded")
    identities = set()
    for correction in value["corrections"]:
        validate_correction(correction)
        if correction["operation_id"] in identities:
            raise CorrectionError("duplicate heart-rate correction identity")
        identities.add(correction["operation_id"])
    return value


def load(path: Path) -> dict:
    raw = path.read_bytes()
    if len(raw) > MAX_BYTES:
        raise CorrectionError("heart-rate correction artifact exceeds byte bound")
    try:
        value = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise CorrectionError("invalid heart-rate correction JSON") from error
    return validate_document(value)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise CorrectionError("duplicate heart-rate correction JSON field")
        result[key] = value
    return result


def apply_document(db: sqlite3.Connection, value: dict) -> tuple[int, int]:
    validate_document(value)
    applied = 0
    unchanged = 0
    for correction in value["corrections"]:
        outcome = apply_correction(db, correction)
        applied += outcome == "applied"
        unchanged += outcome == "unchanged"
    return applied, unchanged
