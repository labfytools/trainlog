#!/usr/bin/env python3
"""Focused contract tests for the desktop AI session-draft inbox."""

import copy
import contextlib
import importlib.util
import io
import json
import stat
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from import_ai_session_draft import ImportFailure, import_payload, load  # noqa: E402
from export_ai_session_drafts import build_export, mark_published  # noqa: E402

_drive_spec = importlib.util.spec_from_file_location(
    "sync_ai_session_draft_drive", ROOT / "tools/sync_ai_session_draft_drive.py"
)
drive = importlib.util.module_from_spec(_drive_spec)
_drive_spec.loader.exec_module(drive)

EX_REPS = "ex_11111111-1111-4111-8111-111111111111"
EX_DURATION = "ex_22222222-2222-4222-8222-222222222222"
EX_CONTINUOUS = "ex_33333333-3333-4333-8333-333333333333"
EX_ALIAS = "ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
DRAFT_ID = "aid_44444444-4444-4444-8444-444444444444"

SCHEMA = """
PRAGMA foreign_keys=ON;
CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER NOT NULL DEFAULT 0);
CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY,canonical_exercise_id TEXT);
CREATE TABLE performed_sets(id INTEGER PRIMARY KEY);
CREATE TABLE ai_session_drafts(id INTEGER PRIMARY KEY,draft_id TEXT NOT NULL UNIQUE,created_at TEXT NOT NULL,planned_for TEXT,session_type TEXT NOT NULL,title TEXT,notes TEXT,archive_status TEXT NOT NULL DEFAULT 'pending',archived_at TEXT,archive_error TEXT,published_at TEXT,withdrawn_at TEXT);
CREATE TABLE ai_session_draft_entries(id INTEGER PRIMARY KEY,draft_row_id INTEGER NOT NULL REFERENCES ai_session_drafts(id) ON DELETE CASCADE,entry_id TEXT NOT NULL UNIQUE,position INTEGER NOT NULL,exercise_row_id INTEGER NOT NULL REFERENCES exercises(id),source_exercise_id TEXT NOT NULL,recording_mode TEXT NOT NULL,tracking_mode TEXT NOT NULL,data_fields INTEGER NOT NULL,equipment_id TEXT,load_mode TEXT NOT NULL,target_sets INTEGER NOT NULL,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,rest_seconds INTEGER NOT NULL,UNIQUE(draft_row_id,position));
CREATE TABLE ai_session_draft_imports(draft_row_id INTEGER PRIMARY KEY,draft_id TEXT NOT NULL UNIQUE,payload_sha256 TEXT NOT NULL UNIQUE,imported_at TEXT NOT NULL);
CREATE TRIGGER ai_session_draft_import_identity_guard BEFORE DELETE ON ai_session_drafts WHEN EXISTS(SELECT 1 FROM ai_session_draft_imports i WHERE i.draft_id=OLD.draft_id) BEGIN SELECT RAISE(ABORT,'AI draft import identity is permanent');END;
PRAGMA user_version=18;
"""


def artifact():
    return {"format": "TRAINLOG_AI_SESSION_DRAFT", "version": 1, "draft": {
        "draft_id": DRAFT_ID, "created_at": "2026-09-14T08:30:00Z",
        "planned_for": "2026-09-15", "session_type": "training",
        "title": "Séance IA", "notes": None, "entries": [
            {"position": 0, "exercise_id": EX_ALIAS, "target_sets": 4,
             "target_reps": 8, "target_duration_seconds": None,
             "target_weight_kg": 42.5, "rest_seconds": 90},
            {"position": 1, "exercise_id": EX_DURATION, "target_sets": 3,
             "target_reps": None, "target_duration_seconds": 45,
             "target_weight_kg": None, "rest_seconds": 30},
        ]}}


class DraftTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.database = Path(self.temp.name) / "trainlog.db"
        con = sqlite3.connect(self.database)
        con.executescript(SCHEMA)
        con.executemany("INSERT INTO exercises(exercise_id,recording_mode,tracking_mode,data_fields) VALUES(?,?,?,?)", [
            (EX_REPS, "sets", "reps", 3), (EX_DURATION, "sets", "duration", 0),
            (EX_CONTINUOUS, "continuous", "duration", 0)])
        con.execute("INSERT INTO exercise_aliases VALUES(?,?)", (EX_ALIAS, EX_REPS))
        con.commit()
        con.close()

    def tearDown(self):
        self.temp.cleanup()

    def connect(self):
        con = sqlite3.connect(self.database)
        con.execute("PRAGMA foreign_keys=ON")
        return con

    def test_import_alias_targets_only_idempotence_conflict_and_export_shape(self):
        con = self.connect()
        self.assertEqual("imported", import_payload(con, artifact(), "2026-09-14T09:00:00Z"))
        self.assertEqual("skipped", import_payload(con, artifact(), "2026-09-14T09:01:00Z"))
        self.assertEqual(0, con.execute("SELECT count(*) FROM performed_sets").fetchone()[0])
        stored = con.execute(
            "SELECT de.entry_id,de.source_exercise_id,e.exercise_id,de.recording_mode,de.tracking_mode,de.load_mode,de.target_reps,de.target_duration_seconds "
            "FROM ai_session_draft_entries de JOIN exercises e ON e.id=de.exercise_row_id WHERE position=0"
        ).fetchone()
        self.assertRegex(stored[0], r"^sxe_[0-9a-f-]{36}$")
        self.assertEqual((EX_ALIAS, EX_REPS, "sets", "reps", "external", 8, None), stored[1:])
        changed = artifact(); changed["draft"]["title"] = "Autre"
        with self.assertRaisesRegex(ImportFailure, "conflit"):
            import_payload(con, changed)
        exported = build_export(con, "2026-09-14T09:02:00Z")
        self.assertEqual(
            {"format", "version", "generated_at", "drafts", "withdrawals"},
            set(exported),
        )
        self.assertEqual("trainlog-ai-session-drafts", exported["format"])
        self.assertEqual(2, exported["version"])
        self.assertEqual([], exported["withdrawals"])
        outbound = exported["drafts"][0]["entries"][0]
        self.assertEqual(EX_REPS, outbound["exercise_id"])
        self.assertEqual(3, outbound["data_fields"])
        self.assertEqual({"entry_id", "position", "exercise_id", "recording_mode", "tracking_mode", "data_fields", "equipment_id", "load_mode", "rest_seconds", "target"}, set(outbound))
        self.assertEqual({"sets": 4, "reps": 8, "duration_seconds": None, "weight_kg": 42.5}, outbound["target"])
        con.close()

    def test_existing_v18_database_gets_publication_cursor(self):
        con = self.connect()
        con.execute("ALTER TABLE ai_session_drafts DROP COLUMN published_at")
        con.commit()
        self.assertEqual([], build_export(con, "2026-09-14T09:02:00Z")["drafts"])
        self.assertIn("published_at", {row[1] for row in con.execute(
            "PRAGMA table_info(ai_session_drafts)")})
        con.close()

    def test_withdrawn_proposal_is_exported_only_as_a_tombstone(self):
        con = self.connect()
        self.assertEqual("imported", import_payload(con, artifact()))
        con.execute(
            "UPDATE ai_session_drafts SET withdrawn_at=? WHERE draft_id=?",
            ("2026-09-18T12:00:00Z", DRAFT_ID),
        )
        con.commit()

        exported = build_export(con, "2026-09-18T12:01:00Z")

        self.assertEqual([], exported["drafts"])
        self.assertEqual(
            [{"draft_id": DRAFT_ID, "withdrawn_at": "2026-09-18T12:00:00Z"}],
            exported["withdrawals"],
        )
        con.close()

    def test_target_sets_99_and_bounded_publication_lifecycle(self):
        value = artifact()
        value["draft"]["entries"][0]["target_sets"] = 99
        con = self.connect()
        self.assertEqual("imported", import_payload(con, value))
        self.assertEqual(99, con.execute(
            "SELECT target_sets FROM ai_session_draft_entries WHERE position=0"
        ).fetchone()[0])
        base = con.execute(
            "SELECT created_at,planned_for,session_type,title,notes FROM ai_session_drafts WHERE draft_id=?",
            (DRAFT_ID,),
        ).fetchone()
        for index in range(1, 256):
            con.execute(
                "INSERT INTO ai_session_drafts(draft_id,created_at,planned_for,session_type,title,notes) VALUES(?,?,?,?,?,?)",
                (f"aid_00000000-0000-4000-8000-{index:012d}",) + base,
            )
        con.execute(
            "INSERT INTO ai_session_drafts(draft_id,created_at,planned_for,session_type,title,notes) VALUES(?,?,?,?,?,?)",
            ("aid_00000000-0000-4000-8000-000000000256",) + base,
        )
        con.commit()
        first = build_export(con, "2026-09-14T09:02:00Z")
        self.assertEqual(256, len(first["drafts"]))
        # Failed publication performs no mark: deterministic retry is identical.
        self.assertEqual([d["draft_id"] for d in first["drafts"]],
                         [d["draft_id"] for d in build_export(con, "2026-09-14T09:03:00Z")["drafts"]])
        self.assertEqual(256, mark_published(con, first, "2026-09-14T09:04:00Z"))
        second = build_export(con, "2026-09-14T09:05:00Z")
        self.assertEqual(1, len(second["drafts"]))
        self.assertEqual(DRAFT_ID, second["drafts"][0]["draft_id"])
        self.assertEqual(1, mark_published(con, second, "2026-09-14T09:06:00Z"))
        self.assertEqual([], build_export(con, "2026-09-14T09:07:00Z")["drafts"])
        self.assertEqual(1, con.execute("SELECT count(*) FROM ai_session_draft_imports").fetchone()[0])
        con.close()

    def test_strict_failures(self):
        mutations = []
        value = artifact(); value["extra"] = 1; mutations.append(value)
        value = artifact(); value["draft"]["created_at"] = "2026-09-14T08:30:00+00:00"; mutations.append(value)
        value = artifact(); value["draft"]["entries"][1]["position"] = 2; mutations.append(value)
        value = artifact(); value["draft"]["entries"][0]["target_duration_seconds"] = 10; mutations.append(value)
        value = artifact(); value["draft"]["entries"][0]["target_weight_kg"] = float("inf"); mutations.append(value)
        value = artifact(); value["draft"]["entries"][0]["notes"] = None; mutations.append(value)
        value = artifact(); value["draft"]["entries"][0]["exercise_id"] = EX_CONTINUOUS; value["draft"]["entries"][0]["target_reps"] = None; value["draft"]["entries"][0]["target_duration_seconds"] = 20; mutations.append(value)
        con = self.connect()
        for value in mutations:
            with self.assertRaises(ImportFailure):
                import_payload(con, value)
        self.assertEqual(0, con.execute("SELECT count(*) FROM ai_session_drafts").fetchone()[0])
        con.close()
        duplicate = Path(self.temp.name) / "duplicate.json"
        duplicate.write_text('{"format":"TRAINLOG_AI_SESSION_DRAFT","format":"x","version":1,"draft":{}}', encoding="utf-8")
        with self.assertRaisesRegex(ImportFailure, "dupliquée"):
            load(duplicate)

    def test_forced_child_failure_rolls_back_every_row(self):
        con = self.connect()
        con.execute("CREATE TRIGGER reject_second BEFORE INSERT ON ai_session_draft_entries WHEN NEW.position=1 BEGIN SELECT RAISE(ABORT,'forced'); END")
        with self.assertRaises(sqlite3.Error):
            import_payload(con, artifact())
        self.assertEqual((0, 0, 0), (
            con.execute("SELECT count(*) FROM ai_session_drafts").fetchone()[0],
            con.execute("SELECT count(*) FROM ai_session_draft_entries").fetchone()[0],
            con.execute("SELECT count(*) FROM ai_session_draft_imports").fetchone()[0]))
        con.close()

    def test_fake_rclone_listing_failure_and_archive_failure_are_nonfatal(self):
        missing_calls = []
        def missing_runner(argv, **kwargs):
            missing_calls.append(argv)
            return subprocess.CompletedProcess(argv, 4, "", "not found")
        self.assertEqual(0, drive.synchronize(self.database, missing_runner))
        self.assertEqual("rclone", missing_calls[0][0])

        source = artifact(); calls = []
        def archive_fails(argv, **kwargs):
            calls.append(argv)
            if argv[1] == "lsf":
                return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
            if argv[1] == "copyto" and argv[2] == drive.SOURCE:
                Path(argv[3]).write_text(json.dumps(source), encoding="utf-8")
                return subprocess.CompletedProcess(argv, 0, "", "")
            return subprocess.CompletedProcess(argv, 9, "", "offline")
        self.assertEqual(0, drive.synchronize(self.database, archive_fails))
        con = self.connect()
        self.assertEqual(("failed", "offline"), con.execute(
            "SELECT archive_status,archive_error FROM ai_session_drafts WHERE draft_id=?", (DRAFT_ID,)).fetchone())
        self.assertEqual(1, con.execute("SELECT count(*) FROM ai_session_drafts").fetchone()[0])
        con.close()
        self.assertEqual(["rclone", "lsf"], calls[0][:2])
        self.assertEqual(["rclone", "copyto"], calls[1][:2])
        self.assertEqual(["rclone", "copyto"], calls[2][:2])
        self.assertNotEqual(drive.SOURCE, calls[2][2])
        self.assertTrue(calls[2][2].endswith("/draft.json"))
        self.assertEqual(f"{drive.ARCHIVE_ROOT}/{DRAFT_ID}.json", calls[2][3])

    def test_inbound_destination_is_private_during_and_after_copy(self):
        source = artifact()
        observed = []

        def successful_runner(argv, **kwargs):
            if argv[1] == "lsf":
                return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
            if argv[2] == drive.SOURCE:
                destination = Path(argv[3])
                self.assertTrue(destination.exists())
                self.assertEqual(0o600, stat.S_IMODE(destination.stat().st_mode))
                destination.write_text(json.dumps(source), encoding="utf-8")
                observed.append(stat.S_IMODE(destination.stat().st_mode))
            return subprocess.CompletedProcess(argv, 0, "", "")

        self.assertEqual(0, drive.synchronize(self.database, successful_runner))
        self.assertEqual([0o600], observed)

    def test_source_replacement_after_fetch_archives_imported_bytes_without_removing_replacement(self):
        original = artifact()
        replacement = artifact()
        replacement["draft"]["draft_id"] = "aid_55555555-5555-4555-8555-555555555555"
        remote = {drive.SOURCE: json.dumps(original)}
        archived = {}

        def replacing_runner(argv, **kwargs):
            if argv[1] == "lsf":
                return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
            if argv[2] == drive.SOURCE:
                Path(argv[3]).write_text(remote[drive.SOURCE], encoding="utf-8")
                remote[drive.SOURCE] = json.dumps(replacement)
            else:
                archived[argv[3]] = Path(argv[2]).read_text(encoding="utf-8")
            return subprocess.CompletedProcess(argv, 0, "", "")

        self.assertEqual(0, drive.synchronize(self.database, replacing_runner))
        target = f"{drive.ARCHIVE_ROOT}/{DRAFT_ID}.json"
        self.assertEqual(original, json.loads(archived[target]))
        self.assertEqual(replacement, json.loads(remote[drive.SOURCE]))

    def test_archive_status_update_failure_converges_on_replay(self):
        source = artifact()
        archive_copies = []

        def successful_runner(argv, **kwargs):
            if argv[1] == "lsf":
                return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
            if argv[2] == drive.SOURCE:
                Path(argv[3]).write_text(json.dumps(source), encoding="utf-8")
            else:
                archive_copies.append(Path(argv[2]).read_text(encoding="utf-8"))
            return subprocess.CompletedProcess(argv, 0, "", "")

        original_update = drive.try_update_archive
        updates = 0
        def fail_once(*args, **kwargs):
            nonlocal updates
            updates += 1
            if updates == 1:
                return "synthetic update failure"
            return original_update(*args, **kwargs)
        drive.try_update_archive = fail_once
        try:
            first = io.StringIO()
            with contextlib.redirect_stdout(first):
                self.assertEqual(0, drive.synchronize(self.database, successful_runner))
            self.assertIn("AI_SESSION_DRAFT_INBOUND=ARCHIVE_FAIL", first.getvalue())
            second = io.StringIO()
            with contextlib.redirect_stdout(second):
                self.assertEqual(0, drive.synchronize(self.database, successful_runner))
            self.assertIn("AI_SESSION_DRAFT_INBOUND=ALREADY_IMPORTED", second.getvalue())
        finally:
            drive.try_update_archive = original_update
        self.assertEqual(2, len(archive_copies))
        con = self.connect()
        self.assertEqual("archived", con.execute(
            "SELECT archive_status FROM ai_session_drafts WHERE draft_id=?", (DRAFT_ID,)
        ).fetchone()[0])
        con.close()

    def test_helper_reports_imported_then_already_imported(self):
        source = artifact()

        def successful_runner(argv, **kwargs):
            if argv[1] == "lsf":
                return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
            if argv[1] == "copyto" and argv[2] == drive.SOURCE:
                Path(argv[3]).write_text(json.dumps(source), encoding="utf-8")
            return subprocess.CompletedProcess(argv, 0, "", "")

        first = io.StringIO()
        with contextlib.redirect_stdout(first):
            self.assertEqual(0, drive.synchronize(self.database, successful_runner))
        self.assertIn("AI_SESSION_DRAFT_INBOUND=IMPORTED", first.getvalue())
        second = io.StringIO()
        with contextlib.redirect_stdout(second):
            self.assertEqual(0, drive.synchronize(self.database, successful_runner))
        self.assertIn("AI_SESSION_DRAFT_INBOUND=ALREADY_IMPORTED", second.getvalue())

    def test_listing_distinguishes_empty_and_transport_failure(self):
        def empty_runner(argv, **kwargs):
            self.assertEqual("lsf", argv[1])
            self.assertEqual(drive.INBOX_ROOT, argv[2])
            self.assertEqual(["--files-only", "--max-depth", "1", "--include",
                              drive.ARTIFACT_NAME], argv[3:])
            return subprocess.CompletedProcess(argv, 0, "other.json\n", "")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(0, drive.synchronize(self.database, empty_runner))
        self.assertIn("AI_SESSION_DRAFT_INBOUND=NONE", out.getvalue())

        def unavailable_runner(argv, **kwargs):
            raise OSError("authentication unavailable")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(0, drive.synchronize(self.database, unavailable_runner))
        self.assertIn("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL", out.getvalue())

    def test_present_file_copy_failure_or_timeout_is_drive_failure(self):
        def runner(argv, **kwargs):
            if argv[1] == "lsf":
                return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
            raise subprocess.TimeoutExpired(argv, drive.RCLONE_TIMEOUT_SECONDS)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(0, drive.synchronize(self.database, runner))
        self.assertIn("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL", out.getvalue())
        self.assertNotIn("AI_SESSION_DRAFT_INBOUND=NONE", out.getvalue())

    def test_disappearing_source_and_unwritten_temp_are_drive_failures(self):
        stale = Path(self.temp.name) / "draft.json"
        stale.write_text(json.dumps(artifact()), encoding="utf-8")
        for returncode in (7, 0):
            def runner(argv, **kwargs):
                if argv[1] == "lsf":
                    return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
                return subprocess.CompletedProcess(argv, returncode, "", "source disappeared")
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(0, drive.synchronize(self.database, runner))
            self.assertIn("AI_SESSION_DRAFT_INBOUND=DRIVE_FAIL", out.getvalue())
        con = self.connect()
        self.assertEqual(0, con.execute("SELECT count(*) FROM ai_session_drafts").fetchone()[0])
        con.close()

    def test_present_invalid_json_is_rejected(self):
        def runner(argv, **kwargs):
            if argv[1] == "lsf":
                return subprocess.CompletedProcess(argv, 0, drive.ARTIFACT_NAME + "\n", "")
            if argv[1] == "copyto":
                Path(argv[3]).write_text("{invalid", encoding="utf-8")
            return subprocess.CompletedProcess(argv, 0, "", "")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(0, drive.synchronize(self.database, runner))
        self.assertIn("AI_SESSION_DRAFT_INBOUND=REJECTED", out.getvalue())


if __name__ == "__main__":
    unittest.main()
