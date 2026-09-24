#!/usr/bin/env python3
"""Transactionally truncate one acknowledged session heart-rate capture."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from pathlib import Path

import heart_rate_correction_exchange as correction_exchange
from trainlog_sqlite import connect_database


class RecoveryError(ValueError):
    """The requested recovery does not match the persisted factual session."""


def _digest(value: object) -> str:
    raw = json.dumps(value, ensure_ascii=False, sort_keys=True,
                     separators=(",", ":"), default=str).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def _rows(db: sqlite3.Connection, query: str, parameters: tuple = ()) -> list[list]:
    return [list(row) for row in db.execute(query, parameters)]


def protected_snapshot(db: sqlite3.Connection, session_id: str,
                       capture_id: str) -> dict:
    session = db.execute(
        "SELECT id FROM sessions WHERE session_id=?", (session_id,),
    ).fetchone()
    if session is None:
        raise RecoveryError("session not found")
    session_row = session[0]
    occurrence_rows = [row[0] for row in db.execute(
        "SELECT id FROM session_exercises WHERE session_row_id=? ORDER BY id",
        (session_row,),
    )]
    occurrence_marks = ",".join("?" for _ in occurrence_rows) or "NULL"
    protected = {
        "session": _rows(db, "SELECT * FROM sessions WHERE id=?", (session_row,)),
        "occurrences": _rows(
            db, "SELECT * FROM session_exercises WHERE session_row_id=? ORDER BY id",
            (session_row,),
        ),
        "sets": _rows(
            db, f"SELECT * FROM performed_sets WHERE session_exercise_row_id IN "
                f"({occurrence_marks}) ORDER BY id", tuple(occurrence_rows),
        ),
        "continuous": _rows(
            db, f"SELECT * FROM continuous_activity WHERE session_exercise_row_id IN "
                f"({occurrence_marks}) ORDER BY session_exercise_row_id", tuple(occurrence_rows),
        ),
        "maxima": _rows(
            db, f"SELECT * FROM max_results WHERE session_exercise_row_id IN "
                f"({occurrence_marks}) ORDER BY session_exercise_row_id", tuple(occurrence_rows),
        ),
        "other_captures": _rows(
            db, "SELECT capture_id,context_kind,context_id,started_at,ended_at,sensor_name "
                "FROM heart_rate_captures WHERE capture_id<>? ORDER BY capture_id", (capture_id,),
        ),
        "other_sample_counts": _rows(
            db, "SELECT capture_id,COUNT(*),MIN(observed_at),MAX(observed_at) "
                "FROM heart_rate_samples WHERE capture_id<>? GROUP BY capture_id "
                "ORDER BY capture_id", (capture_id,),
        ),
        "other_samples": _rows(
            db, "SELECT * FROM heart_rate_samples WHERE capture_id<>? "
                "ORDER BY capture_id,sequence", (capture_id,),
        ),
        "other_rr": _rows(
            db, "SELECT * FROM heart_rate_rr_intervals WHERE capture_id<>? "
                "ORDER BY capture_id,sample_sequence,rr_index", (capture_id,),
        ),
    }
    if db.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='exercise_feedback'",
    ).fetchone():
        feedback_ids = [row[0] for row in db.execute(
            f"SELECT feedback_id FROM exercise_feedback WHERE session_exercise_row_id IN "
            f"({occurrence_marks}) ORDER BY feedback_id", tuple(occurrence_rows),
        )]
        feedback_marks = ",".join("?" for _ in feedback_ids) or "NULL"
        protected["feedback"] = _rows(
            db, f"SELECT * FROM exercise_feedback WHERE feedback_id IN ({feedback_marks}) "
                "ORDER BY feedback_id", tuple(feedback_ids),
        )
        protected["feedback_revisions"] = _rows(
            db, f"SELECT * FROM exercise_feedback_revisions WHERE feedback_id IN "
                f"({feedback_marks}) ORDER BY feedback_id,revision_id", tuple(feedback_ids),
        )
    if db.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='session_followups'",
    ).fetchone():
        followup_ids = [row[0] for row in db.execute(
            "SELECT followup_id FROM session_followups WHERE session_row_id=? "
            "ORDER BY followup_id", (session_row,),
        )]
        followup_marks = ",".join("?" for _ in followup_ids) or "NULL"
        protected["session_followups"] = _rows(
            db, f"SELECT * FROM session_followups WHERE followup_id IN "
                f"({followup_marks}) ORDER BY followup_id", tuple(followup_ids),
        )
        protected["session_followup_revisions"] = _rows(
            db, f"SELECT * FROM session_followup_revisions WHERE followup_id IN "
                f"({followup_marks}) ORDER BY followup_id,revision_id", tuple(followup_ids),
        )
    if db.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' "
        "AND name='session_timeline_sessions'",
    ).fetchone():
        protected["timeline_session"] = _rows(
            db, "SELECT * FROM session_timeline_sessions WHERE session_id=?", (session_id,),
        )
        protected["timeline_exercises"] = _rows(
            db, "SELECT * FROM session_timeline_exercises WHERE session_id=? "
                "ORDER BY entry_id", (session_id,),
        )
    else:
        protected["timeline_exercises"] = _rows(
            db, "SELECT * FROM session_exercise_timeline WHERE session_id=? "
                "ORDER BY entry_id", (session_id,),
        )
    for table in ("sleep_diary_entries", "sleep_diary_revisions", "sleep_diary_events",
                  "sleep_medications", "sleep_medication_intakes"):
        if db.execute(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", (table,),
        ).fetchone():
            protected[table] = _rows(db, f"SELECT * FROM {table} ORDER BY rowid")
    return protected


def capture_report(db: sqlite3.Connection, capture_id: str, cutoff: str) -> dict:
    cutoff_key = correction_exchange.exact_timestamp(cutoff)
    capture = db.execute(
        "SELECT context_kind,context_id,started_at,ended_at,sensor_name "
        "FROM heart_rate_captures WHERE capture_id=?", (capture_id,),
    ).fetchone()
    if capture is None:
        raise RecoveryError("capture not found")
    samples = db.execute(
        "SELECT sequence,observed_at,bpm,exercise_entry_id FROM heart_rate_samples "
        "WHERE capture_id=? ORDER BY sequence", (capture_id,),
    ).fetchall()
    keep = [row for row in samples
            if correction_exchange.exact_timestamp(row[1]) <= cutoff_key]
    remove = [row for row in samples
              if correction_exchange.exact_timestamp(row[1]) > cutoff_key]
    rr = db.execute(
        "SELECT s.observed_at FROM heart_rate_rr_intervals r "
        "JOIN heart_rate_samples s ON s.capture_id=r.capture_id "
        "AND s.sequence=r.sample_sequence WHERE r.capture_id=?",
        (capture_id,),
    ).fetchall()
    rr_keep = sum(correction_exchange.exact_timestamp(row[0]) <= cutoff_key for row in rr)
    values = [row[2] for row in samples]
    kept_values = [row[2] for row in keep]

    def stats(items: list[int]) -> dict:
        return {
            "minimum": min(items) if items else None,
            "average": sum(items) / len(items) if items else None,
            "maximum": max(items) if items else None,
        }

    return {
        "capture_id": capture_id,
        "context_kind": capture[0],
        "context_id": capture[1],
        "started_at": capture[2],
        "ended_at": capture[3],
        "sensor_name": capture[4],
        "bpm_total": len(samples),
        "bpm_keep": len(keep),
        "bpm_remove": len(remove),
        "rr_total": len(rr),
        "rr_keep": rr_keep,
        "rr_remove": len(rr) - rr_keep,
        "first_removed_at": remove[0][1] if remove else None,
        "last_removed_at": remove[-1][1] if remove else None,
        "removed_exercise_entry_ids": sorted({row[3] for row in remove},
                                             key=lambda value: value or ""),
        "current_stats": stats(values),
        "kept_stats": stats(kept_values),
    }


def load_operation(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(value, dict) and set(value) == {"format", "version", "generated_at",
                                                  "corrections"}:
        correction_exchange.load(path)
        if len(value["corrections"]) != 1:
            raise RecoveryError("correction artifact must contain exactly one operation")
        return value["corrections"][0]
    return correction_exchange.validate_correction(value)


def recover(db: sqlite3.Connection, session_id: str, capture_id: str, cutoff: str,
            creator_id: str, apply: bool, operation: dict | None = None,
            fail_after_effect: bool = False) -> tuple[dict, dict | None]:
    """Apply or simulate one exact correction without touching sibling facts.

    CONTRACT: an emitted operation is the immutable content of the separate
    heart-rate-corrections V1 companion. INVARIANT: every mutation commits
    together or rolls back.
    """
    db.execute("PRAGMA foreign_keys=ON")
    correction_exchange.require_schema(db)
    db.execute("BEGIN IMMEDIATE")
    emitted = operation
    try:
        session = db.execute(
            "SELECT started_at,ended_at FROM sessions WHERE session_id=?", (session_id,),
        ).fetchone()
        if session is None:
            raise RecoveryError("session not found")
        if session[1] != cutoff:
            raise RecoveryError("cutoff does not equal the factual session end")
        capture = db.execute(
            "SELECT context_id FROM heart_rate_captures WHERE capture_id=?", (capture_id,),
        ).fetchone()
        if capture is None or capture[0] != session_id:
            raise RecoveryError("capture does not belong to the requested session")
        before = capture_report(db, capture_id, cutoff)
        protected_before = protected_snapshot(db, session_id, capture_id)
        if operation is not None:
            if operation.get("capture_id") != capture_id or operation.get("cutoff") != cutoff:
                raise RecoveryError("operation does not match requested capture and cutoff")
            outcome = correction_exchange.apply_correction(db, operation)
        elif before["bpm_remove"] == 0 and before["ended_at"] == cutoff:
            outcome = "unchanged"
            emitted = None
        else:
            emitted, outcome = correction_exchange.local_correction(
                db, capture_id, cutoff, creator_id,
            )
        if fail_after_effect:
            raise RecoveryError("injected failure after correction effect")
        after = capture_report(db, capture_id, cutoff)
        protected_after = protected_snapshot(db, session_id, capture_id)
        if protected_before != protected_after:
            raise RecoveryError("recovery modified protected session or unrelated data")
        if after["ended_at"] != cutoff or after["bpm_remove"] != 0 or after["rr_remove"] != 0:
            raise RecoveryError("post-cutoff heart-rate data remain after correction")
        if db.execute("PRAGMA foreign_key_check").fetchone() is not None:
            raise RecoveryError("foreign key check failed after correction")
        if apply:
            db.commit()
        else:
            db.rollback()
        return {
            "mode": "apply" if apply else "dry-run",
            "outcome": outcome,
            "session_id": session_id,
            "cutoff": cutoff,
            "before": before,
            "after": after,
            "protected_sha256": _digest(protected_before),
            "protected_unchanged": True,
        }, emitted
    except Exception:
        db.rollback()
        raise


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--session-id", required=True)
    parser.add_argument("--capture-id", required=True)
    parser.add_argument("--cutoff", required=True)
    parser.add_argument("--creator-id", default="peer_android_recovery")
    parser.add_argument("--operation-input", type=Path)
    parser.add_argument("--operation-output", type=Path)
    parser.add_argument("--report-output", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    operation = load_operation(args.operation_input) if args.operation_input else None
    db = connect_database(args.database)
    try:
        db.execute("PRAGMA foreign_keys=ON")
        report, emitted = recover(
            db, args.session_id, args.capture_id, args.cutoff, args.creator_id,
            args.apply, operation,
        )
    finally:
        db.close()
    if args.operation_output and emitted is not None:
        args.operation_output.write_text(
            json.dumps(emitted, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")), encoding="utf-8",
        )
    rendered = json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2)
    if args.report_output:
        args.report_output.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"HEART_RATE_RECOVERY=FAIL {error}")
        raise SystemExit(1)
