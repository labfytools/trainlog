#!/usr/bin/env python3
"""Validate and import the Android-owned Heart Rate V1 companion."""

import argparse
import json
import re
import sqlite3
from datetime import datetime
from pathlib import Path

from trainlog_sqlite import connect_database


FORMAT = "trainlog-heart-rate"
VERSION = 1
MAX_BYTES = 64 * 1024 * 1024
MAX_CAPTURES = 256
MAX_SAMPLES = 200_000
MAX_RR = 64
UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
CAPTURE_ID = re.compile(rf"^hrc_{UUID4}$")
SESSION_ID = re.compile(rf"^se_{UUID4}$")
SLEEP_ID = re.compile(rf"^sl_{UUID4}$")
ENTRY_ID = re.compile(rf"^sxe_{UUID4}$")


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
        fail("invalid heart-rate timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise ValueError("invalid heart-rate timestamp") from error
    if parsed.utcoffset() is None:
        fail("heart-rate timestamp lacks offset")
    return value, parsed

def validate(root: object) -> dict:
    if not isinstance(root, dict) or set(root) != {
        "format", "version", "generated_at", "captures"
    }:
        fail("invalid heart-rate envelope")
    if root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        fail("unsupported heart-rate format")
    timestamp(root["generated_at"])
    captures = root["captures"]
    if not isinstance(captures, list) or len(captures) > MAX_CAPTURES:
        fail("heart-rate capture bound exceeded")
    seen_captures = set()
    for capture in captures:
        keys = {
            "capture_id", "context_kind", "context_id", "started_at", "ended_at",
            "sensor_name", "samples",
        }
        if not isinstance(capture, dict) or set(capture) != keys:
            fail("invalid heart-rate capture")
        capture_id = capture["capture_id"]
        if not isinstance(capture_id, str) or not CAPTURE_ID.fullmatch(capture_id) or capture_id in seen_captures:
            fail("invalid or duplicate heart-rate capture identity")
        seen_captures.add(capture_id)
        kind = capture["context_kind"]
        context_id = capture["context_id"]
        if kind in ("session", "cardio"):
            if not isinstance(context_id, str) or not SESSION_ID.fullmatch(context_id):
                fail("invalid heart-rate session context")
        elif kind == "sleep":
            if not isinstance(context_id, str) or not SLEEP_ID.fullmatch(context_id):
                fail("invalid heart-rate sleep context")
        else:
            fail("invalid heart-rate context kind")
        _, started = timestamp(capture["started_at"])
        _, ended = timestamp(capture["ended_at"])
        if ended < started:
            fail("invalid heart-rate capture interval")
        sensor_name = capture["sensor_name"]
        if sensor_name is not None and (
            not isinstance(sensor_name, str) or not sensor_name.strip() or len(sensor_name.encode()) > 160
        ):
            fail("invalid heart-rate sensor name")

        samples = capture["samples"]
        if not isinstance(samples, list) or len(samples) > MAX_SAMPLES:
            fail("heart-rate sample bound exceeded")
        for expected, sample in enumerate(samples):
            sample_keys = {
                "sequence", "observed_at", "bpm", "exercise_entry_id",
                "sensor_contact_detected", "energy_expended", "rr_intervals_1024",
            }
            if not isinstance(sample, dict) or set(sample) != sample_keys:
                fail("invalid heart-rate sample")
            if type(sample["sequence"]) is not int or sample["sequence"] != expected:
                fail("invalid heart-rate sample sequence")
            _, observed = timestamp(sample["observed_at"])
            if observed < started or observed > ended:
                fail("heart-rate sample outside capture")
            if type(sample["bpm"]) is not int or not 0 <= sample["bpm"] <= 65535:
                fail("invalid heart-rate bpm")
            entry_id = sample["exercise_entry_id"]
            if entry_id is not None and (
                kind not in ("session", "cardio") or
                not isinstance(entry_id, str) or
                not ENTRY_ID.fullmatch(entry_id)
            ):
                fail("invalid heart-rate exercise context")
            contact = sample["sensor_contact_detected"]
            if contact is not None and type(contact) is not bool:
                fail("invalid sensor contact state")
            energy = sample["energy_expended"]
            if energy is not None and (type(energy) is not int or not 0 <= energy <= 65535):
                fail("invalid heart-rate energy value")
            rr = sample["rr_intervals_1024"]
            if not isinstance(rr, list) or len(rr) > MAX_RR or any(
                type(value) is not int or not 0 <= value <= 65535 for value in rr
            ):
                fail("invalid heart-rate RR interval")
    return root


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("heart-rate artifact exceeds 64 MiB")
    return validate(json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object))

def _same_capture(db: sqlite3.Connection, capture: dict) -> bool:
    row = db.execute(
        "SELECT context_kind,context_id,started_at,ended_at,sensor_name "
        "FROM heart_rate_captures WHERE capture_id=?",
        (capture["capture_id"],),
    ).fetchone()
    if row is None:
        return False
    if tuple(row) != (
        capture["context_kind"],
        capture["context_id"],
        capture["started_at"],
        capture["ended_at"],
        capture["sensor_name"],
    ):
        fail("heart-rate capture identity reused with different metadata")
    stored_samples = db.execute(
        "SELECT sequence,observed_at,bpm,exercise_entry_id,sensor_contact_detected,"
        "energy_expended FROM heart_rate_samples WHERE capture_id=? ORDER BY sequence",
        (capture["capture_id"],),
    ).fetchall()
    if len(stored_samples) != len(capture["samples"]):
        fail("heart-rate capture identity reused with different samples")
    rr_by_sequence = {}
    for sequence, rr_index, value in db.execute(
        "SELECT sample_sequence,rr_index,value_1024 FROM heart_rate_rr_intervals "
        "WHERE capture_id=? ORDER BY sample_sequence,rr_index",
        (capture["capture_id"],),
    ):
        values = rr_by_sequence.setdefault(sequence, [])
        if rr_index != len(values):
            fail("stored heart-rate RR sequence is not contiguous")
        values.append(value)
    for row, sample in zip(stored_samples, capture["samples"]):
        contact = None if sample["sensor_contact_detected"] is None else int(sample["sensor_contact_detected"])
        if tuple(row) != (
            sample["sequence"], sample["observed_at"], sample["bpm"],
            sample["exercise_entry_id"], contact, sample["energy_expended"],
        ):
            fail("heart-rate sample identity reused with different content")
        if rr_by_sequence.pop(sample["sequence"], []) != sample["rr_intervals_1024"]:
            fail("heart-rate RR identity reused with different content")
    if rr_by_sequence:
        fail("heart-rate capture has orphan RR intervals")
    return True

def apply(db: sqlite3.Connection, root: dict) -> tuple[int, int]:
    applied = 0
    unchanged = 0
    for capture in root["captures"]:
        if _same_capture(db, capture):
            unchanged += 1
            continue
        db.execute(
            "INSERT INTO heart_rate_captures("
            "capture_id,context_kind,context_id,started_at,ended_at,sensor_name,imported_at"
            ") VALUES(?,?,?,?,?,?,?)",
            (
                capture["capture_id"], capture["context_kind"], capture["context_id"],
                capture["started_at"], capture["ended_at"], capture["sensor_name"],
                root["generated_at"],
            ),
        )
        db.executemany(
            "INSERT INTO heart_rate_samples("
            "capture_id,sequence,observed_at,bpm,exercise_entry_id,"
            "sensor_contact_detected,energy_expended) VALUES(?,?,?,?,?,?,?)",
            [
                (
                    capture["capture_id"], sample["sequence"], sample["observed_at"],
                    sample["bpm"], sample["exercise_entry_id"],
                    None if sample["sensor_contact_detected"] is None
                    else int(sample["sensor_contact_detected"]),
                    sample["energy_expended"],
                )
                for sample in capture["samples"]
            ],
        )
        rr_rows = []
        for sample in capture["samples"]:
            rr_rows.extend(
                (
                    capture["capture_id"], sample["sequence"], index, value
                )
                for index, value in enumerate(sample["rr_intervals_1024"])
            )
        db.executemany(
            "INSERT INTO heart_rate_rr_intervals("
            "capture_id,sample_sequence,rr_index,value_1024) VALUES(?,?,?,?)",
            rr_rows,
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
        if db.execute("PRAGMA user_version").fetchone()[0] not in (30, 31):
            fail("desktop schema v30-v31 required")
        db.execute("BEGIN IMMEDIATE")
        apply(db, root)
        db.commit()


if __name__ == "__main__":
    main()
