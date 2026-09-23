#!/usr/bin/env python3
"""Validate/import completed Android cardio calibration profiles V1."""

import argparse
import json
import re
import sqlite3
from datetime import datetime, timedelta
from pathlib import Path

from trainlog_sqlite import connect_database

FORMAT = "trainlog-cardio-calibrations"
VERSION = 1
MAX_BYTES = 4 * 1024 * 1024
MAX_CALIBRATIONS = 128
UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
CALIBRATION_ID = re.compile(rf"^cal_{UUID4}$")
SESSION_ID = re.compile(rf"^se_{UUID4}$")
ENTRY_ID = re.compile(rf"^sxe_{UUID4}$")
CAPTURE_ID = re.compile(rf"^hrc_{UUID4}$")
CALIBRATION_EXERCISE_ID = "ex_ca1b4a7e-1c2d-4f00-8a11-000000000001"
RECOVERY_OFFSETS = (60, 120, 180)


class CardioCalibrationError(ValueError):
    pass


def fail(message: str) -> None:
    raise CardioCalibrationError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("duplicate JSON field: " + key)
        result[key] = value
    return result


def timestamp(value: object) -> tuple[str, datetime]:
    if not isinstance(value, str) or re.search(r"(?:Z|[+-][0-9]{2}:[0-9]{2})$", value) is None:
        fail("invalid cardio calibration timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise CardioCalibrationError("invalid cardio calibration timestamp") from error
    if parsed.utcoffset() is None:
        fail("cardio calibration timestamp lacks offset")
    return value, parsed


def validate(root: object) -> dict:
    if not isinstance(root, dict) or set(root) != {
        "format", "version", "generated_at", "calibrations"
    }:
        fail("invalid cardio calibration envelope")
    if root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        fail("unsupported cardio calibration format")
    timestamp(root["generated_at"])
    calibrations = root["calibrations"]
    if not isinstance(calibrations, list) or len(calibrations) > MAX_CALIBRATIONS:
        fail("cardio calibration bound exceeded")
    seen = set()
    for item in calibrations:
        keys = {
            "calibration_id", "protocol_version", "session_id", "entry_id",
            "started_at", "effort_end_at", "ended_at", "heart_rate_capture_id",
            "observed_peak_bpm", "recovery",
        }
        if not isinstance(item, dict) or set(item) != keys:
            fail("invalid cardio calibration object")
        calibration_id = item["calibration_id"]
        if (
            not isinstance(calibration_id, str)
            or not CALIBRATION_ID.fullmatch(calibration_id)
            or calibration_id in seen
        ):
            fail("invalid or duplicate cardio calibration identity")
        seen.add(calibration_id)
        if type(item["protocol_version"]) is not int or item["protocol_version"] != 1:
            fail("unsupported cardio calibration protocol")
        if not isinstance(item["session_id"], str) or not SESSION_ID.fullmatch(item["session_id"]):
            fail("invalid cardio calibration session identity")
        if not isinstance(item["entry_id"], str) or not ENTRY_ID.fullmatch(item["entry_id"]):
            fail("invalid cardio calibration entry identity")
        if (
            not isinstance(item["heart_rate_capture_id"], str)
            or not CAPTURE_ID.fullmatch(item["heart_rate_capture_id"])
        ):
            fail("invalid cardio calibration capture identity")
        _, started = timestamp(item["started_at"])
        _, effort_end = timestamp(item["effort_end_at"])
        _, ended = timestamp(item["ended_at"])
        if not started <= effort_end <= ended:
            fail("invalid cardio calibration interval")
        if type(item["observed_peak_bpm"]) is not int or not 0 <= item["observed_peak_bpm"] <= 65535:
            fail("invalid observed peak BPM")
        recovery = item["recovery"]
        if not isinstance(recovery, list) or len(recovery) > 3:
            fail("invalid cardio calibration recovery")
        offsets = []
        for point in recovery:
            if not isinstance(point, dict) or set(point) != {
                "target_offset_seconds", "observed_at", "bpm"
            }:
                fail("invalid recovery point")
            offset = point["target_offset_seconds"]
            if type(offset) is not int or offset not in RECOVERY_OFFSETS or offset in offsets:
                fail("invalid recovery offset")
            offsets.append(offset)
            _, observed = timestamp(point["observed_at"])
            if observed < effort_end + timedelta(seconds=offset) or observed > ended:
                fail("recovery point outside factual interval")
            if type(point["bpm"]) is not int or not 0 <= point["bpm"] <= 65535:
                fail("invalid recovery BPM")
        if offsets != sorted(offsets):
            fail("recovery points must be ordered")
    return root


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("cardio calibration artifact exceeds 4 MiB")
    try:
        return validate(
            json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
        )
    except OSError as error:
        raise CardioCalibrationError("cannot read cardio calibration artifact") from error


def _verify_source_facts(db: sqlite3.Connection, item: dict) -> None:
    session = db.execute(
        "SELECT id,started_at,ended_at,session_kind FROM sessions WHERE session_id=?",
        (item["session_id"],),
    ).fetchone()
    if session is None or session[3] != "cardio":
        fail("calibration session is missing or not cardio")
    if session[1] != item["started_at"] or session[2] != item["ended_at"]:
        fail("calibration session bounds differ from canonical history")

    occurrence = db.execute(
        "SELECT e.exercise_id FROM session_exercises se "
        "JOIN exercises e ON e.id=se.exercise_row_id "
        "WHERE se.session_row_id=? AND se.entry_id=?",
        (session[0], item["entry_id"]),
    ).fetchone()
    if occurrence is None or occurrence[0] != CALIBRATION_EXERCISE_ID:
        fail("calibration occurrence does not match reserved exercise")

    capture = db.execute(
        "SELECT context_kind,context_id,started_at,ended_at FROM heart_rate_captures "
        "WHERE capture_id=?",
        (item["heart_rate_capture_id"],),
    ).fetchone()
    if (
        capture is None
        or capture[0] != "cardio"
        or capture[1] != item["session_id"]
        or capture[2] != item["started_at"]
        or capture[3] != item["ended_at"]
    ):
        fail("calibration heart-rate capture does not match session")

    _, effort_end = timestamp(item["effort_end_at"])
    samples = db.execute(
        "SELECT observed_at,bpm FROM heart_rate_samples WHERE capture_id=? ORDER BY sequence",
        (item["heart_rate_capture_id"],),
    ).fetchall()
    before_effort_end = [
        bpm
        for observed_at, bpm in samples
        if timestamp(observed_at)[1] <= effort_end
    ]
    if not before_effort_end or max(before_effort_end) != item["observed_peak_bpm"]:
        fail("observed peak BPM does not match raw capture")

    sample_pairs = {(observed_at, bpm) for observed_at, bpm in samples}
    for point in item["recovery"]:
        if (point["observed_at"], point["bpm"]) not in sample_pairs:
            fail("recovery point is not a raw heart-rate sample")


def _same(db: sqlite3.Connection, item: dict) -> bool:
    row = db.execute(
        "SELECT protocol_version,session_id,entry_id,started_at,effort_end_at,ended_at,"
        "heart_rate_capture_id,observed_peak_bpm FROM cardio_calibrations "
        "WHERE calibration_id=?",
        (item["calibration_id"],),
    ).fetchone()
    if row is None:
        return False
    expected = (
        item["protocol_version"],
        item["session_id"],
        item["entry_id"],
        item["started_at"],
        item["effort_end_at"],
        item["ended_at"],
        item["heart_rate_capture_id"],
        item["observed_peak_bpm"],
    )
    if tuple(row) != expected:
        fail("calibration identity reused with different metadata")
    stored = [
        tuple(row)
        for row in db.execute(
            "SELECT target_offset_seconds,observed_at,bpm FROM cardio_calibration_recovery "
            "WHERE calibration_id=? ORDER BY target_offset_seconds",
            (item["calibration_id"],),
        )
    ]
    incoming = [
        (p["target_offset_seconds"], p["observed_at"], p["bpm"])
        for p in item["recovery"]
    ]
    if stored != incoming:
        fail("calibration identity reused with different recovery")
    return True


def apply(db: sqlite3.Connection, root: dict) -> tuple[int, int]:
    applied = unchanged = 0
    for item in root["calibrations"]:
        _verify_source_facts(db, item)
        if _same(db, item):
            unchanged += 1
            continue
        db.execute(
            "INSERT INTO cardio_calibrations("
            "calibration_id,protocol_version,session_id,entry_id,started_at,effort_end_at,"
            "ended_at,heart_rate_capture_id,observed_peak_bpm,imported_at"
            ") VALUES(?,?,?,?,?,?,?,?,?,?)",
            (
                item["calibration_id"],
                item["protocol_version"],
                item["session_id"],
                item["entry_id"],
                item["started_at"],
                item["effort_end_at"],
                item["ended_at"],
                item["heart_rate_capture_id"],
                item["observed_peak_bpm"],
                root["generated_at"],
            ),
        )
        db.executemany(
            "INSERT INTO cardio_calibration_recovery("
            "calibration_id,target_offset_seconds,observed_at,bpm) VALUES(?,?,?,?)",
            [
                (
                    item["calibration_id"],
                    point["target_offset_seconds"],
                    point["observed_at"],
                    point["bpm"],
                )
                for point in item["recovery"]
            ],
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
        if db.execute("PRAGMA user_version").fetchone()[0] not in (33, 34, 35):
            fail("desktop schema v33-v35 required")
        db.execute("BEGIN IMMEDIATE")
        apply(db, root)
        db.commit()


if __name__ == "__main__":
    main()
