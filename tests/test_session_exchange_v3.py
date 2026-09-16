#!/usr/bin/env python3
"""Production V3 session exchange regression coverage."""
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
from test_mobile_import_variable_sets import SCHEMA

V11_SCHEMA = SCHEMA.replace("PRAGMA user_version=10;", "") + """
CREATE TABLE exercise_body_zones(
 exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE CASCADE,
 zone_id TEXT NOT NULL, role TEXT NOT NULL,
 PRIMARY KEY(exercise_row_id,zone_id));
CREATE TABLE exercise_body_zone_sync(
 exercise_row_id INTEGER PRIMARY KEY REFERENCES exercises(id) ON DELETE CASCADE,
 synced_primary_zone_id TEXT, synced_secondary_zone_ids TEXT NOT NULL);
PRAGMA user_version=11;
"""

V16_SCHEMA = V11_SCHEMA.replace("PRAGMA user_version=11;", "") + """
ALTER TABLE exercises ADD COLUMN load_semantics TEXT;
ALTER TABLE exercises ADD COLUMN machine_variant TEXT;
ALTER TABLE exercises ADD COLUMN machine_provenance TEXT;
ALTER TABLE exercises ADD COLUMN scientific_profile_id TEXT;
ALTER TABLE exercises ADD COLUMN science_state TEXT NOT NULL DEFAULT 'unresolved';
ALTER TABLE exercises ADD COLUMN legacy_equipment_id TEXT;
ALTER TABLE session_exercises ADD COLUMN tracking_mode TEXT NOT NULL DEFAULT 'reps';
CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY,canonical_exercise_id TEXT NOT NULL);
CREATE TABLE exercise_feedback(id INTEGER PRIMARY KEY,feedback_id TEXT NOT NULL UNIQUE,
 session_exercise_row_id INTEGER NOT NULL REFERENCES session_exercises(id) ON DELETE CASCADE,
 observed_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE exercise_feedback_revisions(revision_id TEXT PRIMARY KEY,feedback_id TEXT NOT NULL,
 created_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE session_followups(id INTEGER PRIMARY KEY,followup_id TEXT NOT NULL UNIQUE,
 session_row_id INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
 observed_at TEXT NOT NULL,raw_text TEXT NOT NULL);
CREATE TABLE session_followup_revisions(revision_id TEXT PRIMARY KEY,followup_id TEXT NOT NULL,
 created_at TEXT NOT NULL,raw_text TEXT NOT NULL);
PRAGMA user_version=16;
"""


def run(command, ok=True):
    result = subprocess.run(command, text=True, capture_output=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stdout + result.stderr)
    return result


def db_value(path, sql):
    with sqlite3.connect(path) as connection:
        return connection.execute(sql).fetchone()[0]


def payload():
    exercises = [
        {"exercise_id": "ex_reps", "name": "Reps", "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0},
        {"exercise_id": "ex_duration", "name": "Duration", "recording_mode": "sets", "tracking_mode": "duration", "data_fields": 0},
        {"exercise_id": "ex_walk", "name": "Walk", "recording_mode": "continuous", "tracking_mode": "duration", "data_fields": 0},
    ]
    base = {"format": "trainlog-mobile-export", "version": 3,
            "generated_at": "2026-09-09T12:00:00+02:00", "exercises": exercises,
            "sessions": [], "body_observations": []}
    entries = [
        {"entry_id": "sxe_reps", "position": 0, "exercise_id": "ex_reps", "name": "Reps",
         "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0,
         "load_mode": "external", "rest_seconds": 120, "equipment_id": "leg_press",
         "target": {"sets": 3, "reps": 8, "weight_kg": 42.5},
         "sets": [{"reps": 8, "weight_kg": 40.0}, {"reps": 7, "weight_kg": 0.0}]},
        {"entry_id": "sxe_duration", "position": 1, "exercise_id": "ex_duration", "name": "Duration",
         "recording_mode": "sets", "tracking_mode": "duration", "data_fields": 0,
         "load_mode": "assistance", "rest_seconds": 60, "equipment_id": "leg_press",
         "target": {"sets": 2, "duration_seconds": 45, "weight_kg": 15.0},
         "sets": [{"duration_seconds": 40}]},
        {"entry_id": "sxe_unweighted", "position": 2, "exercise_id": "ex_reps", "name": "Reps",
         "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0,
         "load_mode": "none", "rest_seconds": 30, "equipment_id": None,
         "target": {"sets": 1, "reps": 12}, "sets": [{"reps": 11}]},
        {"entry_id": "sxe_walk", "position": 3, "exercise_id": "ex_walk", "name": "Walk",
         "recording_mode": "continuous", "tracking_mode": "duration", "data_fields": 0,
         "load_mode": "none", "rest_seconds": 0, "equipment_id": None,
         "target": None, "continuous": {"duration_seconds": 600}},
    ]
    base["sessions"] = [{"session_id": "se_v3", "started_at": "2026-09-09T12:01:00+02:00",
                         "session_type": "training", "exercises": entries}]
    base["sessions"].append({"session_id": "se_v3_max", "started_at": "2026-09-09T13:00:00+02:00",
        "session_type": "max_test", "exercises": [{
            "entry_id": "sxe_max", "position": 0, "exercise_id": "ex_reps", "name": "Reps",
            "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0,
            "load_mode": "none", "rest_seconds": 0, "equipment_id": "leg_press",
            "target": None, "max_weight_kg": 100.0}]})
    base["body_observations"] = [{"observation_id": "bo_v3", "observed_at": "2026-09-09T14:00:00+02:00",
                                  "body_weight_kg": 72.5}]
    return base


def snapshot(db):
    with sqlite3.connect(db) as con:
        return tuple(con.execute(f"SELECT * FROM {table} ORDER BY rowid").fetchall()
                     for table in ("exercises", "sessions", "session_exercises", "performed_sets",
                                   "continuous_activity", "body_observations"))


def main():
    with tempfile.TemporaryDirectory(prefix="trainlog-v3-") as directory:
        root = Path(directory); db = root / "db.sqlite"; artifact = root / "mobile-v3.json"
        with sqlite3.connect(db) as con:
            con.executescript(V11_SCHEMA)
        artifact.write_text(json.dumps(payload()), encoding="utf-8")
        first = run([sys.executable, str(IMPORTER), str(artifact), "--database", str(db)])
        assert "sessions_imported=2" in first.stdout
        replay = run([sys.executable, str(IMPORTER), str(artifact), "--database", str(db)])
        assert "sessions_skipped=2" in replay.stdout
        with sqlite3.connect(db) as con:
            plans = con.execute("SELECT entry_id,load_mode,rest_seconds,target_sets,target_reps,target_duration_seconds,target_weight_kg,equipment_id FROM session_exercises ORDER BY id").fetchall()
        assert plans == [
            ("sxe_reps", "external", 120, 3, 8, None, 42.5, "leg_press"),
            ("sxe_duration", "assistance", 60, 2, None, 45, 15.0, "leg_press"),
            ("sxe_unweighted", "none", 30, 1, 12, None, None, None),
            ("sxe_walk", "none", 0, None, None, None, None, None),
            ("sxe_max", "none", 0, None, None, None, None, "leg_press"),
        ]
        exported = root / "pc-v3.json"
        run([sys.executable, str(EXPORTER), str(exported), "--database", str(db)])
        roundtrip = json.loads(exported.read_text())
        assert roundtrip["version"] == 3
        assert roundtrip["sessions"][0]["exercises"][0]["target"] == {"sets": 3, "reps": 8, "weight_kg": 42.5}
        assert roundtrip["sessions"][0]["exercises"][2]["target"] == {"sets": 1, "reps": 12}
        assert roundtrip["sessions"][0]["exercises"][3]["target"] is None
        assert roundtrip["sessions"][1]["exercises"][0]["max_weight_kg"] == 100.0

        # CONTRACT: Android completed-session reorder is a V3 correction of
        # positions under the same session_id/entry_id identities, never a new
        # session. Facts and occurrence owners survive the reconciliation.
        reordered = copy.deepcopy(payload())
        entries = reordered["sessions"][0]["exercises"]
        entries.insert(0, entries.pop(3))
        for position, entry in enumerate(entries):
            entry["position"] = position
        reordered_path = root / "reordered.json"
        reordered_path.write_text(json.dumps(reordered), encoding="utf-8")
        reconciled = run([sys.executable, str(IMPORTER), str(reordered_path), "--database", str(db)])
        assert "sessions_reconciled=1" in reconciled.stdout
        with sqlite3.connect(db) as con:
            assert con.execute("SELECT COUNT(*) FROM sessions WHERE session_id='se_v3'").fetchone()[0] == 1
            assert con.execute(
                "SELECT entry_id FROM session_exercises WHERE session_row_id="
                "(SELECT id FROM sessions WHERE session_id='se_v3') ORDER BY position"
            ).fetchall() == [("sxe_walk",), ("sxe_reps",), ("sxe_duration",), ("sxe_unweighted",)]
            assert con.execute(
                "SELECT COUNT(DISTINCT entry_id) FROM session_exercises WHERE session_row_id="
                "(SELECT id FROM sessions WHERE session_id='se_v3')"
            ).fetchone()[0] == 4
        legacy = copy.deepcopy(payload()); legacy["version"] = 2
        for session in legacy["sessions"]:
            for entry in session["exercises"]:
                entry.pop("target"); entry["load_mode"] = "none"; entry["rest_seconds"] = 0
        legacy_path = root / "legacy-v2.json"; legacy_path.write_text(json.dumps(legacy), encoding="utf-8")
        before = snapshot(db)
        legacy_replay = run([sys.executable, str(IMPORTER), str(legacy_path), "--database", str(db)], ok=False)
        assert "conflit" in (legacy_replay.stdout + legacy_replay.stderr) and snapshot(db) == before, \
            legacy_replay.stdout + legacy_replay.stderr
        downgrade = run([sys.executable, str(EXPORTER), str(root / "v2.json"), "--database", str(db), "--version", "2"], ok=False)
        assert "export V2 avec plan interdit" in downgrade.stdout

        divergent = copy.deepcopy(payload()); divergent["sessions"][0]["exercises"][0]["target"]["reps"] = 9
        divergent_path = root / "divergent.json"; divergent_path.write_text(json.dumps(divergent), encoding="utf-8")
        corrected = run([sys.executable, str(IMPORTER), str(divergent_path), "--database", str(db)])
        assert "sessions_reconciled=1" in corrected.stdout
        replay = run([sys.executable, str(IMPORTER), str(divergent_path), "--database", str(db)])
        assert "sessions_reconciled=0" in replay.stdout
        assert db_value(db, "SELECT target_reps FROM session_exercises WHERE entry_id='sxe_reps'") == 9
        before = snapshot(db)

        mutations = [
            lambda e: e["target"].update(sets=0),
            lambda e: e["target"].update(reps=10001),
            lambda e: e.update(rest_seconds=86401),
            lambda e: e.update(load_mode="none"),
            lambda e: e.update(target=None),
            lambda e: e.update(position=100001),
            lambda e: e["target"].update(weight_kg=float("nan")),
        ]
        for index, mutate in enumerate(mutations):
            invalid = copy.deepcopy(payload()); mutate(invalid["sessions"][0]["exercises"][0])
            path = root / f"invalid-{index}.json"; path.write_text(json.dumps(invalid), encoding="utf-8")
            run([sys.executable, str(IMPORTER), str(path), "--database", str(db)], ok=False)
            assert snapshot(db) == before

        # WHY: V3 validation is a pre-transaction boundary. A malformed instant
        # must not leave even definitions or body rows behind.
        for index, mutate in enumerate((
            lambda document: document["sessions"][0].update(started_at="not-a-time"),
            lambda document: document["body_observations"][0].update(observed_at="2026-02-30T12:00Z"),
            lambda document: document.update(generated_at="2026-09-09 12:00:00Z"),
        )):
            invalid_time = copy.deepcopy(payload()); mutate(invalid_time)
            path = root / f"invalid-time-{index}.json"; path.write_text(json.dumps(invalid_time), encoding="utf-8")
            result = run([sys.executable, str(IMPORTER), str(path), "--database", str(db)], ok=False)
            assert "date-heure Trainlog invalide" in result.stderr and snapshot(db) == before

        # The exact shared language includes omitted seconds, lowercase t/z,
        # ±23:59 offsets and arbitrary fraction precision.
        exact = copy.deepcopy(payload())
        exact["generated_at"] = "2026-09-09t12:00z"
        exact["sessions"][0]["started_at"] = "2026-09-09T12:01+23:59"
        exact["sessions"][1]["started_at"] = "2026-09-09T13:00:00.123456789012345678900-23:59"
        exact["body_observations"][0]["observed_at"] = "2026-09-09t14:00:00.1000z"
        exact_db = root / "exact.sqlite"
        with sqlite3.connect(exact_db) as con: con.executescript(V11_SCHEMA)
        exact_path = root / "exact.json"; exact_path.write_text(json.dumps(exact), encoding="utf-8")
        run([sys.executable, str(IMPORTER), str(exact_path), "--database", str(exact_db)])

        # Published V2 admission remains unchanged, including its historical
        # nonempty timestamp rule.
        legacy_time = copy.deepcopy(legacy)
        legacy_time["sessions"][0]["session_id"] = "se_legacy_bad_time"
        legacy_time["sessions"][0]["started_at"] = "not-a-time"
        legacy_time["sessions"] = legacy_time["sessions"][:1]
        legacy_time["body_observations"] = []
        legacy_db = root / "legacy-time.sqlite"
        with sqlite3.connect(legacy_db) as con: con.executescript(V11_SCHEMA)
        legacy_time_path = root / "legacy-time.json"; legacy_time_path.write_text(json.dumps(legacy_time), encoding="utf-8")
        run([sys.executable, str(IMPORTER), str(legacy_time_path), "--database", str(legacy_db)])

        # INVARIANT: corrupt stored V3 instants fail before destination write.
        sentinel = root / "sentinel.json"; sentinel.write_text("prior-valid-artifact", encoding="utf-8")
        with sqlite3.connect(db) as con:
            con.execute("UPDATE sessions SET started_at='bad-stored-time' WHERE session_id='se_v3'")
        failed_export = run([sys.executable, str(EXPORTER), str(sentinel), "--database", str(db)], ok=False)
        assert "started_at" in failed_export.stdout and sentinel.read_text() == "prior-valid-artifact"
        with sqlite3.connect(db) as con:
            con.execute("UPDATE sessions SET started_at='2026-09-09T12:01:00+02:00' WHERE session_id='se_v3'")
            con.execute("UPDATE body_observations SET observed_at='bad-stored-time' WHERE observation_id='bo_v3'")
        failed_export = run([sys.executable, str(EXPORTER), str(sentinel), "--database", str(db)], ok=False)
        assert "observed_at" in failed_export.stdout and sentinel.read_text() == "prior-valid-artifact"
        with sqlite3.connect(db) as con:
            con.execute("UPDATE body_observations SET observed_at='2026-09-09T14:00:00+02:00' WHERE observation_id='bo_v3'")
        duplicate = artifact.read_text().replace('"sets": 3', '"sets": 3, "sets": 4', 1)
        duplicate_path = root / "duplicate.json"; duplicate_path.write_text(duplicate)
        rejected = run([sys.executable, str(IMPORTER), str(duplicate_path), "--database", str(db)], ok=False)
        assert "dupliqué" in rejected.stderr and snapshot(db) == before

        # The same checked-in artifact is consumed by Robolectric, proving the
        # wire shape rather than two independently authored platform fixtures.
        shared_db = root / "shared.sqlite"
        with sqlite3.connect(shared_db) as con: con.executescript(V11_SCHEMA)
        shared = ROOT / "tests/fixtures/session-mobile-export-v3.json"
        run([sys.executable, str(IMPORTER), str(shared), "--database", str(shared_db)])
        shared_out = root / "shared-out.json"
        run([sys.executable, str(EXPORTER), str(shared_out), "--database", str(shared_db)])
        shared_entry = json.loads(shared_out.read_text())["sessions"][0]["exercises"][0]
        assert shared_entry["target"] == {"sets": 3, "reps": 9, "weight_kg": 55.5}
        assert shared_entry["sets"] == [{"reps": 9, "weight_kg": 52.5}, {"reps": 8}, {"reps": 7, "weight_kg": 0.0}]

        # Desktop v16 stores the unit on the occurrence. A later catalogue
        # profile change therefore cannot reinterpret or rewrite old facts.
        v16_db = root / "v16.sqlite"
        with sqlite3.connect(v16_db) as con:
            con.executescript(V16_SCHEMA)
        run([sys.executable, str(IMPORTER), str(artifact), "--database", str(v16_db)])
        with sqlite3.connect(v16_db) as con:
            assert con.execute("SELECT tracking_mode FROM session_exercises WHERE entry_id='sxe_reps'").fetchone()[0] == "reps"
            con.execute("UPDATE exercises SET tracking_mode='duration' WHERE exercise_id='ex_reps'")

        # V2 has the same completed-occurrence ownership contract: catalogue
        # membership proves identity, while the entry's own profile validates
        # and preserves the historical payload.
        historical_v2 = payload()
        historical_v2["version"] = 2
        historical_v2["exercises"][0]["tracking_mode"] = "duration"
        historical_entry = copy.deepcopy(historical_v2["sessions"][0]["exercises"][0])
        historical_entry.pop("target")
        historical_entry["entry_id"] = "sxe_v16_historical_v2"
        historical_entry["load_mode"] = "none"
        historical_entry["rest_seconds"] = 0
        historical_v2["sessions"] = [{
            "session_id": "se_v16_historical_v2",
            "started_at": "2026-09-09T13:00:00+02:00",
            "session_type": "training",
            "exercises": [historical_entry],
        }]
        historical_v2["body_observations"] = []
        historical_v2_path = root / "v16-historical-v2.json"
        historical_v2_path.write_text(json.dumps(historical_v2), encoding="utf-8")
        run([sys.executable, str(IMPORTER), str(historical_v2_path), "--database", str(v16_db)])
        replay_v2 = run([sys.executable, str(IMPORTER), str(historical_v2_path), "--database", str(v16_db)])
        assert "sessions_skipped=1" in replay_v2.stdout
        with sqlite3.connect(v16_db) as con:
            assert con.execute("SELECT tracking_mode FROM session_exercises WHERE entry_id='sxe_v16_historical_v2'").fetchone()[0] == "reps"
            assert con.execute("SELECT reps FROM performed_sets ps JOIN session_exercises se ON se.id=ps.session_exercise_row_id WHERE se.entry_id='sxe_v16_historical_v2' ORDER BY ps.position LIMIT 1").fetchone()[0] == 8
        v16_old_export = root / "v16-old.json"
        run([sys.executable, str(EXPORTER), str(v16_old_export), "--database", str(v16_db)])
        old_entry = json.loads(v16_old_export.read_text())["sessions"][0]["exercises"][0]
        assert old_entry["tracking_mode"] == "reps"
        assert old_entry["sets"][0]["reps"] == 8

        future = payload()
        future["exercises"][0]["tracking_mode"] = "duration"
        old_after_profile_change = copy.deepcopy(payload()["sessions"][0])
        future["sessions"] = [old_after_profile_change, {
            "session_id": "se_v16_future", "started_at": "2026-09-10T12:00:00+02:00",
            "session_type": "training", "exercises": [{
                "entry_id": "sxe_v16_future", "position": 0, "exercise_id": "ex_reps",
                "name": "Reps", "recording_mode": "sets", "tracking_mode": "duration",
                "data_fields": 0, "load_mode": "none", "rest_seconds": 0,
                "equipment_id": None, "target": {"sets": 3, "duration_seconds": 30},
                "sets": [{"duration_seconds": 30}, {"duration_seconds": 30}, {"duration_seconds": 30}],
            }],
        }]
        future["body_observations"] = []
        future_path = root / "v16-future.json"
        future_path.write_text(json.dumps(future), encoding="utf-8")
        run([sys.executable, str(IMPORTER), str(future_path), "--database", str(v16_db)])
        idempotent = run([sys.executable, str(IMPORTER), str(future_path), "--database", str(v16_db)])
        assert "sessions_reconciled=0" in idempotent.stdout
        with sqlite3.connect(v16_db) as con:
            assert con.execute("SELECT tracking_mode FROM session_exercises WHERE entry_id='sxe_reps'").fetchone()[0] == "reps"
            assert con.execute("SELECT tracking_mode FROM session_exercises WHERE entry_id='sxe_v16_future'").fetchone()[0] == "duration"
            assert con.execute("SELECT group_concat(duration_seconds, ',') FROM performed_sets ps JOIN session_exercises se ON se.id=ps.session_exercise_row_id WHERE se.entry_id='sxe_v16_future' ORDER BY ps.position").fetchone()[0] == "30,30,30"
    print("PASS session_exchange_v3")


if __name__ == "__main__": main()
