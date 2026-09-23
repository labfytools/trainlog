#!/usr/bin/env python3
"""Validate and import Android-owned dedicated cardio sessions V1."""

import json
from pathlib import Path

import import_mobile_export


FORMAT = "trainlog-cardio-sessions"
VERSION = 1
MAX_BYTES = 64 * 1024 * 1024
ROOT_KEYS = {"format", "version", "generated_at", "exercises", "sessions"}


class CardioSessionError(ValueError):
    pass


def fail(message: str) -> None:
    raise CardioSessionError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("duplicate JSON field: " + key)
        result[key] = value
    return result


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("cardio session artifact exceeds 64 MiB")
    try:
        root = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=unique_object,
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise CardioSessionError("invalid cardio session JSON") from error
    if not isinstance(root, dict) or set(root) != ROOT_KEYS:
        fail("invalid cardio session envelope")
    if root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        fail("unsupported cardio session format")
    if not isinstance(root["exercises"], list) or not isinstance(root["sessions"], list):
        fail("invalid cardio session arrays")
    for index, session in enumerate(root["sessions"]):
        if not isinstance(session, dict) or session.get("session_type") != "cardio":
            fail(f"sessions[{index}].session_type must be cardio")
    return root


def mobile_projection(root: dict) -> dict:
    sessions = []
    for source in root["sessions"]:
        session = dict(source)
        session["session_type"] = "training"
        sessions.append(session)
    return {
        "format": "trainlog-mobile-export",
        "version": 4,
        "generated_at": root["generated_at"],
        "exercises": root["exercises"],
        "sessions": sessions,
        "body_observations": [],
    }


def validate(root: dict, known_equipment_ids: set[str]) -> dict:
    if not isinstance(root, dict) or set(root) != ROOT_KEYS:
        fail("invalid cardio session envelope")
    if root.get("format") != FORMAT or type(root.get("version")) is not int or root["version"] != VERSION:
        fail("unsupported cardio session format")
    if not isinstance(root.get("sessions"), list) or not isinstance(root.get("exercises"), list):
        fail("invalid cardio session arrays")
    for index, session in enumerate(root["sessions"]):
        if not isinstance(session, dict) or session.get("session_type") != "cardio":
            fail(f"sessions[{index}].session_type must be cardio")
    projection = mobile_projection(root)
    try:
        import_mobile_export.validate_payload(projection, known_equipment_ids)
    except Exception as error:
        raise CardioSessionError(str(error)) from error
    return root


def apply(
    connection,
    root: dict,
    *,
    complete_causal_envelope: bool = False,
) -> dict:
    session_ids = [session["session_id"] for session in root["sessions"]]
    for session_id in session_ids:
        row = connection.execute(
            "SELECT session_kind FROM sessions WHERE session_id=?",
            (session_id,),
        ).fetchone()
        if row is not None and row[0] != "cardio":
            fail("cardio session identity collides with non-cardio history")

    projection = mobile_projection(root)
    try:
        report = import_mobile_export.apply_payload(
            connection,
            projection,
            complete_causal_envelope=complete_causal_envelope,
        )
    except Exception as error:
        raise CardioSessionError(str(error)) from error

    for session_id in session_ids:
        row = connection.execute(
            "SELECT session_kind FROM sessions WHERE session_id=?",
            (session_id,),
        ).fetchone()
        if row is not None:
            if row[0] not in ("training", "cardio"):
                fail("cardio session identity has incompatible stored kind")
            connection.execute(
                "UPDATE sessions SET session_kind='cardio' WHERE session_id=?",
                (session_id,),
            )
    return report
