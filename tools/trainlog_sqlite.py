"""Canonical SQLite connection configuration for Trainlog Python tools."""

import sqlite3
import uuid


PROFILE_NAMESPACE = uuid.UUID("4f4c8ea6-18f6-5e48-9d65-a54a24889149")


def profile_revision(parent, recording, tracking, fields):
    """Return the frozen deterministic profile revision shared with C/Android."""
    if not all(isinstance(value, str) for value in (parent, recording, tracking)):
        raise ValueError("invalid profile revision")
    if type(fields) is not int:
        raise ValueError("invalid profile revision")
    name = f"{parent}\n{recording}\n{tracking}\n{fields}"
    return "pr2_" + str(uuid.uuid5(PROFILE_NAMESPACE, name))


def prototype_profile_valid(token, recording, tracking, fields):
    """Recognize only the unpublished pr1 prototype accepted by migration v17."""
    if not isinstance(token, str) or not isinstance(recording, str) or \
            not isinstance(tracking, str) or type(fields) is not int:
        return 0
    valid = recording in ("sets", "continuous") and tracking in ("reps", "duration") and \
        not (recording == "continuous" and tracking != "duration") and \
        fields >= 0 and fields & ~3 == 0 and not (recording == "sets" and fields != 0)
    return int(valid and token == f"pr1|{recording}|{tracking}|{fields}")


def configure_connection(connection):
    """Install every process-local function referenced by Trainlog schema SQL.

    CONTRACT: callers must use this before preparing statements against a
    Trainlog database. Persisted triggers resolve UDF names at statement
    preparation even when their WHEN clause suppresses the trigger body.
    """
    connection.create_function(
        "trainlog_profile_revision", 4, profile_revision, deterministic=True,
    )
    connection.create_function(
        "trainlog_pr1_valid", 4, prototype_profile_valid, deterministic=True,
    )
    return connection


def connect_database(database, *args, **kwargs):
    """Open and canonically configure one production Trainlog connection."""
    return configure_connection(sqlite3.connect(database, *args, **kwargs))
