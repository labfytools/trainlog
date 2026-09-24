#!/usr/bin/env python3
"""Validate/import Android-owned guided-cardio control history."""

import argparse
import json
import re
import sqlite3
from datetime import datetime
from pathlib import Path

from trainlog_sqlite import connect_database

FORMAT = "trainlog-cardio-guidance"
VERSION = 1
MAX_BYTES = 8 * 1024 * 1024
MAX_RUNS = 128
MAX_PHASES = 128
MAX_EVENTS = 4096

UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
RUN_ID = re.compile(rf"^cgr_{UUID4}$")
PHASE_ID = re.compile(rf"^cgp_{UUID4}$")
SESSION_ID = re.compile(rf"^se_{UUID4}$")
ENTRY_ID = re.compile(rf"^sxe_{UUID4}$")
CALIBRATION_ID = re.compile(rf"^cal_{UUID4}$")
KINDS = {"warmup", "work", "recovery", "cooldown"}
INSTRUCTIONS = {"accelerate", "maintain", "slow_down", "suspended"}
EXIT_KINDS = {"fixed_duration", "enter_target", "recover_below", "duration_or_recover"}


class CardioGuidanceError(ValueError):
    pass


def fail(message: str) -> None:
    raise CardioGuidanceError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("duplicate cardio guidance JSON field: " + key)
        result[key] = value
    return result


def timestamp(value: object) -> tuple[str, datetime]:
    if not isinstance(value, str) or re.search(r"(?:Z|[+-][0-9]{2}:[0-9]{2})$", value) is None:
        fail("invalid cardio guidance timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise CardioGuidanceError("invalid cardio guidance timestamp") from error
    if parsed.utcoffset() is None:
        fail("cardio guidance timestamp lacks offset")
    return value, parsed


def _target(value: object) -> dict | None:
    if value is None:
        return None
    keys = {
        "minimum_bpm", "maximum_bpm", "calibration_id",
        "calibration_observed_peak_bpm", "minimum_percent", "maximum_percent",
    }
    if not isinstance(value, dict) or set(value) != keys:
        fail("invalid cardio guidance target")
    minimum = value["minimum_bpm"]
    maximum = value["maximum_bpm"]
    if type(minimum) is not int or type(maximum) is not int or not 1 <= minimum <= maximum <= 65535:
        fail("invalid cardio guidance BPM target")
    provenance = (
        value["calibration_id"],
        value["calibration_observed_peak_bpm"],
        value["minimum_percent"],
        value["maximum_percent"],
    )
    if all(item is None for item in provenance):
        return value
    if any(item is None for item in provenance):
        fail("incomplete cardio calibration target provenance")
    calibration_id, peak, min_percent, max_percent = provenance
    if not isinstance(calibration_id, str) or not CALIBRATION_ID.fullmatch(calibration_id):
        fail("invalid cardio calibration target identity")
    if (
        type(peak) is not int or not 1 <= peak <= 65535
        or type(min_percent) is not int or type(max_percent) is not int
        or not 1 <= min_percent <= max_percent <= 100
    ):
        fail("invalid cardio calibration target provenance")
    return value


def _exit(value: object, target: dict | None) -> dict:
    if not isinstance(value, dict) or set(value) != {"kind", "seconds", "bpm"}:
        fail("invalid cardio phase exit condition")
    kind = value["kind"]
    seconds = value["seconds"]
    bpm = value["bpm"]
    if kind not in EXIT_KINDS:
        fail("invalid cardio phase exit kind")
    if kind == "fixed_duration":
        if type(seconds) is not int or not 1 <= seconds <= 86400 or bpm is not None:
            fail("invalid fixed cardio duration")
    elif kind == "enter_target":
        if target is None or seconds is not None or bpm is not None:
            fail("invalid enter-target condition")
    elif kind == "recover_below":
        if seconds is not None or type(bpm) is not int or not 1 <= bpm <= 65535:
            fail("invalid cardio recovery threshold")
    else:
        if (
            type(seconds) is not int or not 1 <= seconds <= 86400
            or type(bpm) is not int or not 1 <= bpm <= 65535
        ):
            fail("invalid bounded cardio recovery")
    return value


def validate(root: object) -> dict:
    if not isinstance(root, dict) or set(root) != {"format", "version", "generated_at", "runs"}:
        fail("invalid cardio guidance envelope")
    if root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        fail("unsupported cardio guidance format")
    timestamp(root["generated_at"])
    runs = root["runs"]
    if not isinstance(runs, list) or len(runs) > MAX_RUNS:
        fail("cardio guidance run bound exceeded")
    seen_runs = set()
    seen_sessions = set()
    seen_phases = set()
    for run in runs:
        if not isinstance(run, dict) or set(run) != {
            "run_id", "session_id", "started_at", "ended_at", "phases"
        }:
            fail("invalid cardio guidance run")
        if (
            not isinstance(run["run_id"], str) or not RUN_ID.fullmatch(run["run_id"])
            or run["run_id"] in seen_runs
        ):
            fail("invalid or duplicate cardio guidance run identity")
        seen_runs.add(run["run_id"])
        if (
            not isinstance(run["session_id"], str)
            or not SESSION_ID.fullmatch(run["session_id"])
            or run["session_id"] in seen_sessions
        ):
            fail("invalid or duplicate cardio guidance session identity")
        seen_sessions.add(run["session_id"])
        _, run_start = timestamp(run["started_at"])
        _, run_end = timestamp(run["ended_at"])
        if run_end < run_start:
            fail("invalid cardio guidance run interval")
        phases = run["phases"]
        if not isinstance(phases, list) or not phases or len(phases) > MAX_PHASES:
            fail("invalid cardio guidance phases")
        positions = []
        for phase in phases:
            keys = {
                "phase_id", "entry_id", "position", "kind", "target",
                "exit_condition", "started_at", "ended_at", "final_instruction", "events",
            }
            if not isinstance(phase, dict) or set(phase) != keys:
                fail("invalid cardio guidance phase")
            if (
                not isinstance(phase["phase_id"], str) or not PHASE_ID.fullmatch(phase["phase_id"])
                or phase["phase_id"] in seen_phases
            ):
                fail("invalid or duplicate cardio guidance phase identity")
            seen_phases.add(phase["phase_id"])
            if not isinstance(phase["entry_id"], str) or not ENTRY_ID.fullmatch(phase["entry_id"]):
                fail("invalid cardio guidance entry identity")
            if type(phase["position"]) is not int or phase["position"] < 0 or phase["position"] in positions:
                fail("invalid cardio guidance phase position")
            positions.append(phase["position"])
            if phase["kind"] not in KINDS:
                fail("invalid cardio phase kind")
            target = _target(phase["target"])
            _exit(phase["exit_condition"], target)
            _, phase_start = timestamp(phase["started_at"])
            _, phase_end = timestamp(phase["ended_at"])
            if not run_start <= phase_start <= phase_end <= run_end:
                fail("cardio phase outside run")
            if phase["final_instruction"] not in INSTRUCTIONS:
                fail("invalid final cardio instruction")
            events = phase["events"]
            if not isinstance(events, list) or len(events) > MAX_EVENTS:
                fail("cardio guidance event bound exceeded")
            for expected, event in enumerate(events):
                if not isinstance(event, dict) or set(event) != {
                    "sequence", "observed_at", "instruction", "bpm",
                    "target_minimum_bpm", "target_maximum_bpm",
                }:
                    fail("invalid cardio guidance event")
                if type(event["sequence"]) is not int or event["sequence"] != expected:
                    fail("invalid cardio guidance event sequence")
                _, observed = timestamp(event["observed_at"])
                if not phase_start <= observed <= phase_end:
                    fail("cardio guidance event outside phase")
                instruction = event["instruction"]
                if instruction not in INSTRUCTIONS:
                    fail("invalid cardio guidance instruction")
                bpm = event["bpm"]
                if instruction == "suspended":
                    if bpm is not None:
                        fail("suspended guidance cannot contain BPM")
                elif type(bpm) is not int or not 1 <= bpm <= 65535:
                    fail("active guidance requires BPM")
                target_min = event["target_minimum_bpm"]
                target_max = event["target_maximum_bpm"]
                if target is None:
                    if target_min is not None or target_max is not None:
                        fail("guidance event unexpectedly carries target")
                elif target_min != target["minimum_bpm"] or target_max != target["maximum_bpm"]:
                    fail("guidance event target differs from phase snapshot")
        if positions != sorted(positions):
            fail("cardio guidance phases must be ordered")
    return root


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("cardio guidance artifact exceeds 8 MiB")
    return validate(json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object))


def _verify_source_facts(db: sqlite3.Connection, run: dict) -> None:
    session = db.execute(
        "SELECT id,started_at,ended_at,session_kind FROM sessions WHERE session_id=?",
        (run["session_id"],),
    ).fetchone()
    if session is None or session[3] != "cardio":
        fail("guided session is missing or not cardio")
    _, session_start = timestamp(session[1])
    _, session_end = timestamp(session[2])
    _, run_start = timestamp(run["started_at"])
    _, run_end = timestamp(run["ended_at"])
    if not session_start <= run_start <= run_end <= session_end:
        fail("guided run outside canonical cardio session")

    for phase in run["phases"]:
        occurrence = db.execute(
            "SELECT 1 FROM session_exercises se WHERE se.session_row_id=? AND se.entry_id=?",
            (session[0], phase["entry_id"]),
        ).fetchone()
        if occurrence is None:
            fail("guided phase occurrence is missing")
        timing = db.execute(
            "SELECT started_at,ended_at FROM session_exercise_timeline "
            "WHERE session_id=? AND entry_id=?",
            (run["session_id"], phase["entry_id"]),
        ).fetchone()
        if timing is None:
            fail("guided phase exercise timeline is missing")
        _, exercise_start = timestamp(timing[0])
        _, exercise_end = timestamp(timing[1])
        _, phase_start = timestamp(phase["started_at"])
        _, phase_end = timestamp(phase["ended_at"])
        if not exercise_start <= phase_start <= phase_end <= exercise_end:
            fail("guided phase outside exercise timeline")

        target = phase["target"]
        if target is not None and target["calibration_id"] is not None:
            calibration = db.execute(
                "SELECT observed_peak_bpm FROM cardio_calibrations WHERE calibration_id=?",
                (target["calibration_id"],),
            ).fetchone()
            if calibration is None or calibration[0] != target["calibration_observed_peak_bpm"]:
                fail("guided phase calibration provenance is unavailable")

        for event in phase["events"]:
            if event["bpm"] is None:
                continue
            exact = db.execute(
                "SELECT 1 FROM heart_rate_samples s JOIN heart_rate_captures c "
                "ON c.capture_id=s.capture_id "
                "WHERE c.context_kind='cardio' AND c.context_id=? "
                "AND s.observed_at=? AND s.bpm=? LIMIT 1",
                (run["session_id"], event["observed_at"], event["bpm"]),
            ).fetchone()
            if exact is None:
                fail("guided instruction BPM is not a raw heart-rate sample")


def _same(db: sqlite3.Connection, run: dict) -> bool:
    row = db.execute(
        "SELECT session_id,started_at,ended_at FROM cardio_guidance_runs WHERE run_id=?",
        (run["run_id"],),
    ).fetchone()
    if row is None:
        return False
    if tuple(row) != (run["session_id"], run["started_at"], run["ended_at"]):
        fail("cardio guidance run identity reused with different metadata")
    stored_phases = db.execute(
        "SELECT phase_id,entry_id,position,kind,target_min_bpm,target_max_bpm,"
        "calibration_id,calibration_observed_peak_bpm,minimum_percent,maximum_percent,"
        "exit_kind,exit_seconds,exit_bpm,started_at,ended_at,final_instruction "
        "FROM cardio_guidance_phases WHERE run_id=? ORDER BY position,phase_id",
        (run["run_id"],),
    ).fetchall()
    if len(stored_phases) != len(run["phases"]):
        fail("cardio guidance run identity reused with different phases")
    for stored, phase in zip(stored_phases, run["phases"]):
        target = phase["target"]
        exit_condition = phase["exit_condition"]
        expected = (
            phase["phase_id"], phase["entry_id"], phase["position"], phase["kind"],
            None if target is None else target["minimum_bpm"],
            None if target is None else target["maximum_bpm"],
            None if target is None else target["calibration_id"],
            None if target is None else target["calibration_observed_peak_bpm"],
            None if target is None else target["minimum_percent"],
            None if target is None else target["maximum_percent"],
            exit_condition["kind"], exit_condition["seconds"], exit_condition["bpm"],
            phase["started_at"], phase["ended_at"], phase["final_instruction"],
        )
        if tuple(stored) != expected:
            fail("cardio guidance phase identity reused with different content")
        stored_events = [
            tuple(row)
            for row in db.execute(
                "SELECT sequence,observed_at,instruction,bpm,target_min_bpm,target_max_bpm "
                "FROM cardio_guidance_events WHERE run_id=? AND phase_id=? ORDER BY sequence",
                (run["run_id"], phase["phase_id"]),
            )
        ]
        incoming = [
            (
                event["sequence"], event["observed_at"], event["instruction"], event["bpm"],
                event["target_minimum_bpm"], event["target_maximum_bpm"],
            )
            for event in phase["events"]
        ]
        if stored_events != incoming:
            fail("cardio guidance phase identity reused with different events")
    return True


def apply(db: sqlite3.Connection, root: dict) -> tuple[int, int]:
    applied = unchanged = 0
    for run in root["runs"]:
        _verify_source_facts(db, run)
        if _same(db, run):
            unchanged += 1
            continue
        db.execute(
            "INSERT INTO cardio_guidance_runs(run_id,session_id,started_at,ended_at,imported_at) "
            "VALUES(?,?,?,?,?)",
            (run["run_id"], run["session_id"], run["started_at"], run["ended_at"], root["generated_at"]),
        )
        for phase in run["phases"]:
            target = phase["target"]
            exit_condition = phase["exit_condition"]
            db.execute(
                "INSERT INTO cardio_guidance_phases("
                "run_id,phase_id,entry_id,position,kind,target_min_bpm,target_max_bpm,"
                "calibration_id,calibration_observed_peak_bpm,minimum_percent,maximum_percent,"
                "exit_kind,exit_seconds,exit_bpm,started_at,ended_at,final_instruction"
                ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (
                    run["run_id"], phase["phase_id"], phase["entry_id"], phase["position"],
                    phase["kind"],
                    None if target is None else target["minimum_bpm"],
                    None if target is None else target["maximum_bpm"],
                    None if target is None else target["calibration_id"],
                    None if target is None else target["calibration_observed_peak_bpm"],
                    None if target is None else target["minimum_percent"],
                    None if target is None else target["maximum_percent"],
                    exit_condition["kind"], exit_condition["seconds"], exit_condition["bpm"],
                    phase["started_at"], phase["ended_at"], phase["final_instruction"],
                ),
            )
            db.executemany(
                "INSERT INTO cardio_guidance_events("
                "run_id,phase_id,sequence,observed_at,instruction,bpm,target_min_bpm,target_max_bpm"
                ") VALUES(?,?,?,?,?,?,?,?)",
                [
                    (
                        run["run_id"], phase["phase_id"], event["sequence"], event["observed_at"],
                        event["instruction"], event["bpm"], event["target_minimum_bpm"],
                        event["target_maximum_bpm"],
                    )
                    for event in phase["events"]
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
        if db.execute("PRAGMA user_version").fetchone()[0] not in (34, 35, 36):
            fail("desktop schema v34-v36 required")
        db.execute("BEGIN IMMEDIATE")
        apply(db, root)
        db.commit()


if __name__ == "__main__":
    main()
