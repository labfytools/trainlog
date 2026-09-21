#!/usr/bin/env python3
"""Export, validate and merge the versioned Sleep Diary V1 companion."""

import argparse
import json
import re
import sqlite3
from datetime import date, datetime
from pathlib import Path

from trainlog_sqlite import connect_database


FORMAT = "trainlog-sleep-diary"
VERSION = 1
MAX_BYTES = 16 * 1024 * 1024
MAX_ENTRIES = 3660
MAX_EVENTS = 64
UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
ENTRY_ID = re.compile(rf"^sl_{UUID4}$")
REVISION_ID = re.compile(rf"^slr_{UUID4}$")
EVENT_ID = re.compile(rf"^sle_{UUID4}$")
POINTS = {"bed_time", "final_get_up", "night_get_up", "daytime_sleepiness"}
INTERVALS = {"sleep", "nap", "long_awake", "half_sleep"}
QUALITIES = {"TB", "B", "Moy", "M", "TM"}


def fail(message: str) -> None:
    raise ValueError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("duplicate JSON field: " + key)
        result[key] = value
    return result


def timestamp(value: object) -> str:
    if not isinstance(value, str) or re.search(r"(?:Z|[+-][0-9]{2}:[0-9]{2})$", value) is None:
        fail("invalid sleep diary timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise ValueError("invalid sleep diary timestamp") from error
    if parsed.utcoffset() is None:
        fail("sleep diary timestamp lacks an offset")
    return value


def validate(root: object) -> dict:
    if not isinstance(root, dict) or set(root) != {"format", "version", "generated_at", "entries"}:
        fail("invalid sleep diary envelope")
    if root["format"] != FORMAT or type(root["version"]) is not int or root["version"] != VERSION:
        fail("unsupported sleep diary format")
    timestamp(root["generated_at"])
    entries = root["entries"]
    if not isinstance(entries, list) or len(entries) > MAX_ENTRIES:
        fail("sleep diary entry bound exceeded")
    seen_entries = set()
    for item in entries:
        keys = {"entry_id", "night_start_date", "night_end_date", "created_at", "updated_at",
                "revision_id", "parent_revision_id", "deleted", "sleep_quality", "wake_quality",
                "day_form", "treatment_and_notes", "events"}
        if not isinstance(item, dict) or set(item) != keys:
            fail("invalid sleep diary entry")
        if not ENTRY_ID.fullmatch(item["entry_id"]) or item["entry_id"] in seen_entries:
            fail("invalid or duplicate sleep diary entry identity")
        seen_entries.add(item["entry_id"])
        if not REVISION_ID.fullmatch(item["revision_id"]):
            fail("invalid sleep diary revision identity")
        parent = item["parent_revision_id"]
        if parent is not None and (not isinstance(parent, str) or not REVISION_ID.fullmatch(parent) or parent == item["revision_id"]):
            fail("invalid sleep diary parent revision")
        try:
            start_date = date.fromisoformat(item["night_start_date"])
            end_date = date.fromisoformat(item["night_end_date"])
        except (TypeError, ValueError) as error:
            raise ValueError("invalid sleep diary date") from error
        if start_date >= end_date:
            fail("sleep diary date range is empty")
        created = datetime.fromisoformat(timestamp(item["created_at"]).replace("Z", "+00:00"))
        updated = datetime.fromisoformat(timestamp(item["updated_at"]).replace("Z", "+00:00"))
        if updated < created or type(item["deleted"]) is not bool:
            fail("invalid sleep diary lifecycle")
        for name in ("sleep_quality", "wake_quality", "day_form"):
            if item[name] is not None and item[name] not in QUALITIES:
                fail("invalid sleep diary appreciation")
        notes = item["treatment_and_notes"]
        if not isinstance(notes, str) or len(notes.encode()) > 16384:
            fail("sleep diary notes bound exceeded")
        events = item["events"]
        if not isinstance(events, list) or len(events) > MAX_EVENTS:
            fail("sleep diary event bound exceeded")
        seen_events = set()
        for event in events:
            if not isinstance(event, dict) or set(event) != {"event_id", "type", "start_at", "end_at"}:
                fail("invalid sleep diary event")
            if not EVENT_ID.fullmatch(event["event_id"]) or event["event_id"] in seen_events:
                fail("invalid or duplicate sleep event identity")
            seen_events.add(event["event_id"])
            start = datetime.fromisoformat(timestamp(event["start_at"]).replace("Z", "+00:00"))
            if event["type"] in POINTS:
                if event["end_at"] is not None:
                    fail("point sleep event has an end")
            elif event["type"] in INTERVALS:
                end = datetime.fromisoformat(timestamp(event["end_at"]).replace("Z", "+00:00"))
                if end <= start:
                    fail("sleep interval is not positive in absolute time")
            else:
                fail("unknown sleep diary event type")
    return root


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("sleep diary artifact exceeds 16 MiB")
    return validate(json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object))


def build(db: sqlite3.Connection) -> dict:
    entries = []
    rows = db.execute(
        "SELECT entry_id,night_start_date,night_end_date,created_at,updated_at,current_revision_id,deleted "
        "FROM sleep_diary_entries ORDER BY entry_id"
    ).fetchall()
    if len(rows) > MAX_ENTRIES:
        fail("sleep diary entry bound exceeded")
    for entry_id, night_start, night_end, created_at, updated_at, revision_id, deleted in rows:
        revision = db.execute(
            "SELECT parent_revision_id,sleep_quality,wake_quality,day_form,treatment_and_notes "
            "FROM sleep_diary_revisions WHERE revision_id=? AND entry_id=?", (revision_id, entry_id)
        ).fetchone()
        if revision is None:
            fail("sleep diary current revision is missing")
        events = [
            {"event_id": row[0], "type": row[1], "start_at": row[2], "end_at": row[3]}
            for row in db.execute(
                "SELECT event_id,event_type,start_at,end_at FROM sleep_diary_events "
                "WHERE revision_id=? ORDER BY event_id", (revision_id,)
            )
        ]
        entries.append({"entry_id": entry_id, "night_start_date": night_start,
            "night_end_date": night_end, "created_at": created_at, "updated_at": updated_at,
            "revision_id": revision_id, "parent_revision_id": revision[0], "deleted": bool(deleted),
            "sleep_quality": revision[1], "wake_quality": revision[2], "day_form": revision[3],
            "treatment_and_notes": revision[4] or "", "events": events})
    return validate({"format": FORMAT, "version": VERSION,
                     "generated_at": datetime.now().astimezone().isoformat(), "entries": entries})


def apply(db: sqlite3.Connection, root: dict) -> tuple[int, int]:
    validate(root)
    applied = 0
    unchanged = 0
    for item in root["entries"]:
        local = db.execute(
            "SELECT current_revision_id,deleted FROM sleep_diary_entries WHERE entry_id=?",
            (item["entry_id"],),
        ).fetchone()
        if local is not None and local[0] == item["revision_id"]:
            persisted = next(
                value for value in build(db)["entries"] if value["entry_id"] == item["entry_id"]
            )
            if persisted != item:
                fail("sleep diary revision identity reused with different content")
            unchanged += 1
            continue
        if local is not None and item["parent_revision_id"] != local[0]:
            fail("concurrent sleep diary revision")
        if local is None and item["parent_revision_id"] is not None:
            fail("sleep diary revision has an unknown parent")
        if local is not None and local[1] and not item["deleted"]:
            fail("sleep diary deletion cannot be resurrected")
        if local is None:
            db.execute("INSERT INTO sleep_diary_entries VALUES(?,?,?,?,?,?,?)",
                       (item["entry_id"], item["night_start_date"], item["night_end_date"],
                        item["created_at"], item["updated_at"], item["revision_id"], int(item["deleted"])))
        else:
            db.execute("UPDATE sleep_diary_entries SET night_start_date=?,night_end_date=?,updated_at=?,"
                       "current_revision_id=?,deleted=? WHERE entry_id=? AND current_revision_id=?",
                       (item["night_start_date"], item["night_end_date"], item["updated_at"],
                        item["revision_id"], int(item["deleted"]), item["entry_id"], local[0]))
        db.execute("INSERT INTO sleep_diary_revisions VALUES(?,?,?,?,?,?,?,?)",
                   (item["revision_id"], item["entry_id"], item["parent_revision_id"], item["updated_at"],
                    item["sleep_quality"], item["wake_quality"], item["day_form"], item["treatment_and_notes"]))
        db.executemany("INSERT INTO sleep_diary_events VALUES(?,?,?,?,?)",
                       [(item["revision_id"], event["event_id"], event["type"], event["start_at"], event["end_at"])
                        for event in item["events"]])
        applied += 1
    return applied, unchanged


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    export = sub.add_parser("export")
    export.add_argument("output", type=Path)
    export.add_argument("--database", required=True, type=Path)
    import_command = sub.add_parser("import")
    import_command.add_argument("input", type=Path)
    import_command.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    with connect_database(args.database) as db:
        if db.execute("PRAGMA user_version").fetchone()[0] != 29:
            fail("desktop schema v29 required")
        if args.command == "export":
            args.output.write_text(json.dumps(build(db), ensure_ascii=False, sort_keys=True,
                                                   separators=(",", ":")), encoding="utf-8")
        else:
            apply(db, load(args.input))
            db.commit()


if __name__ == "__main__":
    main()
