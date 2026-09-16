#!/usr/bin/env python3
"""Characterize current V3 gaps without defining future wire behavior."""

import copy
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPORTER = ROOT / "tools/import_mobile_export.py"
EXPORTER = ROOT / "tools/export_pc_mobile.py"

sys.path.insert(0, str(ROOT / "tests"))
from test_session_exchange_v3 import V11_SCHEMA, payload  # noqa: E402


def run(command, *, succeeds=True):
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != succeeds:
        raise AssertionError(result.stdout + result.stderr)
    return result


def create_database(path):
    with sqlite3.connect(path) as connection:
        connection.executescript(V11_SCHEMA)


def import_document(path, database, document, name="mobile-v3.json", *, succeeds=True):
    artifact = path / name
    artifact.write_text(json.dumps(document), encoding="utf-8")
    return run(
        [sys.executable, str(IMPORTER), str(artifact), "--database", str(database)],
        succeeds=succeeds,
    )


def main():
    with tempfile.TemporaryDirectory(prefix="trainlog-sync-gap-") as directory:
        root = Path(directory)
        source = root / "source.sqlite"
        create_database(source)
        current = payload()
        import_document(root, source, current)

        # WHY: these persisted values are real business data, but V3 has no
        # fields for them. This test records loss at the production boundary;
        # it does not approve that loss or extend the frozen wire format.
        with sqlite3.connect(source) as connection:
            session_row = connection.execute(
                "SELECT id FROM sessions WHERE session_id='se_v3'"
            ).fetchone()[0]
            connection.execute(
                "UPDATE sessions SET ended_at=?, notes=? WHERE id=?",
                ("2026-09-09T12:59:00+02:00", "session note", session_row),
            )
            connection.execute(
                "UPDATE session_exercises SET notes='occurrence note' "
                "WHERE entry_id='sxe_reps'"
            )
            connection.execute(
                "UPDATE body_observations SET session_row_id=?, notes='body note' "
                "WHERE observation_id='bo_v3'",
                (session_row,),
            )

        exported = root / "pc-v3.json"
        run([sys.executable, str(EXPORTER), str(exported), "--database", str(source)])
        wire = json.loads(exported.read_text(encoding="utf-8"))
        session = next(item for item in wire["sessions"] if item["session_id"] == "se_v3")
        occurrence = next(item for item in session["exercises"] if item["entry_id"] == "sxe_reps")
        observation = next(
            item for item in wire["body_observations"] if item["observation_id"] == "bo_v3"
        )
        assert "ended_at" not in session
        assert "notes" not in session
        assert "notes" not in occurrence
        assert "notes" not in observation
        assert "session_id" not in observation and "session_row_id" not in observation

        destination = root / "destination.sqlite"
        create_database(destination)
        run([sys.executable, str(IMPORTER), str(exported), "--database", str(destination)])
        with sqlite3.connect(destination) as connection:
            connection.execute("PRAGMA foreign_keys=ON")
            assert connection.execute(
                "SELECT ended_at, notes FROM sessions WHERE session_id='se_v3'"
            ).fetchone() == (None, None)
            assert connection.execute(
                "SELECT notes FROM session_exercises WHERE entry_id='sxe_reps'"
            ).fetchone()[0] is None
            assert connection.execute(
                "SELECT session_row_id, notes FROM body_observations "
                "WHERE observation_id='bo_v3'"
            ).fetchone() == (None, None)

            # CONTRACT: current snapshots have no session tombstone. Local
            # deletion followed by replay therefore resurrects the identity.
            connection.execute("DELETE FROM sessions WHERE session_id='se_v3'")
        replay = run(
            [sys.executable, str(IMPORTER), str(exported), "--database", str(destination)]
        )
        assert "sessions_imported=1" in replay.stdout
        with sqlite3.connect(destination) as connection:
            assert connection.execute(
                "SELECT count(*) FROM sessions WHERE session_id='se_v3'"
            ).fetchone()[0] == 1

        # INVARIANT: validation/transactionality is per artifact. A malformed
        # V3 artifact leaves none of its otherwise-valid definitions or rows,
        # while the earlier valid artifact remains committed independently.
        before = None
        with sqlite3.connect(destination) as connection:
            before = tuple(
                connection.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
                for table in ("exercises", "sessions", "session_exercises", "body_observations")
            )
        malformed = copy.deepcopy(current)
        malformed["exercises"].append(
            {
                "exercise_id": "ex_should_rollback",
                "name": "Rollback sentinel",
                "recording_mode": "sets",
                "tracking_mode": "reps",
                "data_fields": 0,
            }
        )
        malformed["sessions"][0]["exercises"][0]["target"]["sets"] = 0
        import_document(root, destination, malformed, "malformed-v3.json", succeeds=False)
        with sqlite3.connect(destination) as connection:
            after = tuple(
                connection.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
                for table in ("exercises", "sessions", "session_exercises", "body_observations")
            )
            assert after == before
            assert connection.execute(
                "SELECT count(*) FROM exercises WHERE exercise_id='ex_should_rollback'"
            ).fetchone()[0] == 0

    print("PASS sync_gap_characterization")


if __name__ == "__main__":
    main()
