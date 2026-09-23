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
MAX_INTAKES = 32
MAX_MEDICATIONS = 1000
MAX_ANCESTRY = 256
UUID4 = r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
ENTRY_ID = re.compile(rf"^sl_{UUID4}$")
REVISION_ID = re.compile(rf"^slr_{UUID4}$")
EVENT_ID = re.compile(rf"^sle_{UUID4}$")
MEDICATION_ID = re.compile(rf"^med_{UUID4}$")
MEDICATION_REVISION_ID = re.compile(rf"^medr_{UUID4}$")
INTAKE_ID = re.compile(rf"^mdi_{UUID4}$")
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
    if not isinstance(root, dict) or set(root) != {"format", "version", "generated_at", "entries", "medications"}:
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
                "day_form", "treatment_and_notes", "events", "intakes"}
        if not isinstance(item, dict) or set(item) not in (keys, keys | {"ancestry"}):
            fail("invalid sleep diary entry")
        if not ENTRY_ID.fullmatch(item["entry_id"]) or item["entry_id"] in seen_entries:
            fail("invalid or duplicate sleep diary entry identity")
        seen_entries.add(item["entry_id"])
        if not REVISION_ID.fullmatch(item["revision_id"]):
            fail("invalid sleep diary revision identity")
        parent = item["parent_revision_id"]
        if parent is not None and (not isinstance(parent, str) or not REVISION_ID.fullmatch(parent) or parent == item["revision_id"]):
            fail("invalid sleep diary parent revision")
        ancestry = item.get("ancestry")
        if ancestry is not None:
            if (not isinstance(ancestry, list) or not 1 <= len(ancestry) <= MAX_ANCESTRY or
                    ancestry[0] != item["revision_id"] or len(set(ancestry)) != len(ancestry) or
                    any(not isinstance(value, str) or not REVISION_ID.fullmatch(value)
                        for value in ancestry)):
                fail("invalid sleep diary ancestry")
            if parent is None:
                if len(ancestry) != 1:
                    fail("invalid sleep diary ancestry root")
            elif len(ancestry) < 2 or ancestry[1] != parent:
                fail("invalid sleep diary ancestry parent")
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
        intakes = item["intakes"]
        if not isinstance(intakes, list) or len(intakes) > MAX_INTAKES:
            fail("medication intake bound exceeded")
        seen_intakes = set()
        for intake in intakes:
            keys = {"intake_id", "medication_id", "medication_name", "taken_at",
                    "dose_value", "dose_unit", "note", "created_at"}
            if not isinstance(intake, dict) or set(intake) != keys:
                fail("invalid medication intake")
            if (not INTAKE_ID.fullmatch(intake["intake_id"]) or
                    intake["intake_id"] in seen_intakes or
                    not MEDICATION_ID.fullmatch(intake["medication_id"])):
                fail("invalid medication intake identity")
            seen_intakes.add(intake["intake_id"])
            timestamp(intake["taken_at"]); timestamp(intake["created_at"])
            if not isinstance(intake["medication_name"], str) or not intake["medication_name"].strip():
                fail("invalid medication snapshot name")
            has_dose = intake["dose_value"] is not None
            if has_dose != (intake["dose_unit"] is not None) or (has_dose and
                    (type(intake["dose_value"]) not in (int, float) or intake["dose_value"] <= 0 or
                     not isinstance(intake["dose_unit"], str) or not intake["dose_unit"].strip())):
                fail("invalid medication intake dose")
    medications = root["medications"]
    if not isinstance(medications, list) or len(medications) > MAX_MEDICATIONS:
        fail("medication catalog bound exceeded")
    seen_medications = set()
    for medication in medications:
        keys = {"medication_id", "revision_id", "parent_revision_id", "created_at", "updated_at",
                "name", "default_dose_value", "default_dose_unit", "form", "note", "active", "deleted"}
        if not isinstance(medication, dict) or set(medication) not in (keys, keys | {"ancestry"}):
            fail("invalid medication")
        if (not MEDICATION_ID.fullmatch(medication["medication_id"]) or medication["medication_id"] in seen_medications or
                not MEDICATION_REVISION_ID.fullmatch(medication["revision_id"])):
            fail("invalid medication identity")
        seen_medications.add(medication["medication_id"])
        parent = medication["parent_revision_id"]
        if parent is not None and not MEDICATION_REVISION_ID.fullmatch(parent): fail("invalid medication parent")
        ancestry = medication.get("ancestry")
        if ancestry is not None:
            if (not isinstance(ancestry, list) or not 1 <= len(ancestry) <= MAX_ANCESTRY or
                    ancestry[0] != medication["revision_id"] or len(set(ancestry)) != len(ancestry) or
                    any(not isinstance(value, str) or not MEDICATION_REVISION_ID.fullmatch(value)
                        for value in ancestry)):
                fail("invalid medication ancestry")
            if parent is None:
                if len(ancestry) != 1:
                    fail("invalid medication ancestry root")
            elif len(ancestry) < 2 or ancestry[1] != parent:
                fail("invalid medication ancestry parent")
        timestamp(medication["created_at"]); timestamp(medication["updated_at"])
        if not isinstance(medication["name"], str) or not medication["name"].strip(): fail("invalid medication name")
        has_dose = medication["default_dose_value"] is not None
        if has_dose != (medication["default_dose_unit"] is not None) or (has_dose and
                (type(medication["default_dose_value"]) not in (int, float) or medication["default_dose_value"] <= 0)):
            fail("invalid default medication dose")
    return root


def load(path: Path) -> dict:
    if path.stat().st_size > MAX_BYTES:
        fail("sleep diary artifact exceeds 16 MiB")
    return validate(json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object))


def revision_ancestry(db: sqlite3.Connection, parent_sql: str, owner_id: str,
                      current_revision: str, pattern: re.Pattern[str]) -> list[str]:
    result = []
    revision = current_revision
    seen = set()
    first = True
    while revision is not None:
        if revision in seen or len(result) >= MAX_ANCESTRY or not pattern.fullmatch(revision):
            fail("invalid sleep revision ancestry")
        seen.add(revision)
        result.append(revision)
        row = db.execute(parent_sql, (owner_id, revision)).fetchone()
        if row is None:
            if first:
                fail("sleep current revision is missing")
            break
        revision = row[0]
        first = False
    return result


def build(db: sqlite3.Connection) -> dict:
    entries = []
    # CONTRACT: local durability does not publish a draft. A generation may
    # capture only the current tip explicitly validated by the user.
    rows = db.execute(
        "SELECT e.entry_id,e.night_start_date,e.night_end_date,e.created_at,e.updated_at,"
        "e.current_revision_id,e.deleted FROM sleep_diary_entries e JOIN "
        "sleep_diary_publication_state p ON p.entry_id=e.entry_id AND "
        "p.validated_revision_id=e.current_revision_id ORDER BY e.entry_id"
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
        ancestry = revision_ancestry(
            db,
            "SELECT parent_revision_id FROM sleep_diary_revisions "
            "WHERE entry_id=? AND revision_id=?",
            entry_id,
            revision_id,
            REVISION_ID,
        )
        events = [
            {"event_id": row[0], "type": row[1], "start_at": row[2], "end_at": row[3]}
            for row in db.execute(
                "SELECT event_id,event_type,start_at,end_at FROM sleep_diary_events "
                "WHERE revision_id=? ORDER BY event_id", (revision_id,)
            )
        ]
        intakes = [{"intake_id": row[0], "medication_id": row[1], "medication_name": row[2],
                    "taken_at": row[3], "dose_value": row[4], "dose_unit": row[5],
                    "note": row[6], "created_at": row[7]}
                   for row in db.execute("SELECT intake_id,medication_id,medication_name,taken_at,dose_value,dose_unit,note,created_at FROM sleep_medication_intakes WHERE revision_id=? ORDER BY intake_id", (revision_id,))]
        entries.append({"entry_id": entry_id, "night_start_date": night_start,
            "night_end_date": night_end, "created_at": created_at, "updated_at": updated_at,
            "revision_id": revision_id, "parent_revision_id": revision[0], "ancestry": ancestry,
            "deleted": bool(deleted), "sleep_quality": revision[1], "wake_quality": revision[2], "day_form": revision[3],
            "treatment_and_notes": revision[4] or "", "events": events, "intakes": intakes})
    medications = []
    for row in db.execute("SELECT m.medication_id,m.created_at,m.updated_at,m.current_revision_id,m.deleted,r.parent_revision_id,r.name,r.default_dose_value,r.default_dose_unit,r.form,r.note,r.active FROM sleep_medications m JOIN sleep_medication_revisions r ON r.revision_id=m.current_revision_id ORDER BY m.medication_id"):
        ancestry = revision_ancestry(
            db,
            "SELECT parent_revision_id FROM sleep_medication_revisions "
            "WHERE medication_id=? AND revision_id=?",
            row[0],
            row[3],
            MEDICATION_REVISION_ID,
        )
        medications.append({"medication_id": row[0], "created_at": row[1], "updated_at": row[2],
            "revision_id": row[3], "deleted": bool(row[4]), "parent_revision_id": row[5],
            "ancestry": ancestry, "name": row[6], "default_dose_value": row[7],
            "default_dose_unit": row[8], "form": row[9], "note": row[10], "active": bool(row[11])})
    return validate({"format": FORMAT, "version": VERSION,
                     "generated_at": datetime.now().astimezone().isoformat(), "entries": entries,
                     "medications": medications})


def revision_is_ancestor(db: sqlite3.Connection, parent_sql: str, owner_id: str,
                         ancestor_revision: str, current_revision: str) -> bool:
    """Return whether an incoming current-state revision is already in the local lineage."""
    revision = current_revision
    seen = set()
    while revision is not None:
        if revision == ancestor_revision:
            return True
        if revision in seen:
            fail("sleep revision chain cycle")
        seen.add(revision)
        row = db.execute(parent_sql, (owner_id, revision)).fetchone()
        if row is None:
            return False
        revision = row[0]
    return False


def apply(db: sqlite3.Connection, root: dict) -> tuple[int, int]:
    validate(root)
    applied = 0
    unchanged = 0
    for medication in root["medications"]:
        local = db.execute("SELECT current_revision_id,deleted FROM sleep_medications WHERE medication_id=?", (medication["medication_id"],)).fetchone()
        if local is not None and local[0] == medication["revision_id"]:
            persisted = next(value for value in build(db)["medications"]
                             if value["medication_id"] == medication["medication_id"])
            persisted_payload = dict(persisted)
            incoming_payload = dict(medication)
            persisted_payload.pop("ancestry", None)
            incoming_payload.pop("ancestry", None)
            if persisted_payload != incoming_payload:
                fail("medication revision identity reused with different content")
            continue
        if local is not None and revision_is_ancestor(
            db,
            "SELECT parent_revision_id FROM sleep_medication_revisions "
            "WHERE medication_id=? AND revision_id=?",
            medication["medication_id"],
            medication["revision_id"],
            local[0],
        ):
            continue
        remote_descends_from_local = (
            local is not None and local[0] in medication.get("ancestry", [])[1:]
        )
        if local is not None and not remote_descends_from_local and medication["parent_revision_id"] != local[0]:
            fail("concurrent medication revision")
        if local is None and medication["parent_revision_id"] is not None: fail("unknown medication parent")
        if local is not None and local[1] and not medication["deleted"]: fail("medication resurrection")
        if local is None:
            db.execute("INSERT INTO sleep_medications VALUES(?,?,?,?,?)", (medication["medication_id"], medication["created_at"], medication["updated_at"], medication["revision_id"], int(medication["deleted"])))
        else:
            db.execute("UPDATE sleep_medications SET updated_at=?,current_revision_id=?,deleted=? WHERE medication_id=? AND current_revision_id=?", (medication["updated_at"], medication["revision_id"], int(medication["deleted"]), medication["medication_id"], local[0]))
        db.execute("INSERT INTO sleep_medication_revisions VALUES(?,?,?,?,?,?,?,?,?,?)", (medication["revision_id"], medication["medication_id"], medication["parent_revision_id"], medication["updated_at"], medication["name"], medication["default_dose_value"], medication["default_dose_unit"], medication["form"], medication["note"], int(medication["active"])))
    for item in root["entries"]:
        local = db.execute(
            "SELECT current_revision_id,deleted FROM sleep_diary_entries WHERE entry_id=?",
            (item["entry_id"],),
        ).fetchone()
        if local is not None and local[0] == item["revision_id"]:
            persisted = next(
                value for value in build(db)["entries"] if value["entry_id"] == item["entry_id"]
            )
            persisted_payload = dict(persisted)
            incoming_payload = dict(item)
            persisted_payload.pop("ancestry", None)
            incoming_payload.pop("ancestry", None)
            if persisted_payload != incoming_payload:
                fail("sleep diary revision identity reused with different content")
            db.execute(
                "INSERT INTO sleep_diary_publication_state(entry_id,validated_revision_id,validated_at,"
                "acknowledged_revision_id,acknowledged_at) VALUES(?,?,?,?,?) ON CONFLICT(entry_id) "
                "DO UPDATE SET validated_revision_id=excluded.validated_revision_id,"
                "validated_at=excluded.validated_at,acknowledged_revision_id=excluded.acknowledged_revision_id,"
                "acknowledged_at=excluded.acknowledged_at",
                (item["entry_id"], item["revision_id"], root["generated_at"],
                 item["revision_id"], root["generated_at"]),
            )
            unchanged += 1
            continue
        if local is not None and revision_is_ancestor(
            db,
            "SELECT parent_revision_id FROM sleep_diary_revisions "
            "WHERE entry_id=? AND revision_id=?",
            item["entry_id"],
            item["revision_id"],
            local[0],
        ):
            # Current-state companions may legitimately arrive out of order.
            # A known ancestor is stale evidence, not a concurrent branch.
            unchanged += 1
            continue
        remote_descends_from_local = local is not None and local[0] in item.get("ancestry", [])[1:]
        if local is not None and not remote_descends_from_local and item["parent_revision_id"] != local[0]:
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
        db.executemany("INSERT INTO sleep_medication_intakes VALUES(?,?,?,?,?,?,?,?,?)",
                       [(item["revision_id"], intake["intake_id"], intake["medication_id"],
                         intake["medication_name"], intake["taken_at"], intake["dose_value"],
                         intake["dose_unit"], intake["note"], intake["created_at"])
                        for intake in item["intakes"]])
        # An imported revision has already crossed the synchronization
        # boundary; recording both markers prevents it appearing as a draft.
        db.execute(
            "INSERT INTO sleep_diary_publication_state(entry_id,validated_revision_id,validated_at,"
            "acknowledged_revision_id,acknowledged_at) VALUES(?,?,?,?,?) ON CONFLICT(entry_id) "
            "DO UPDATE SET validated_revision_id=excluded.validated_revision_id,"
            "validated_at=excluded.validated_at,acknowledged_revision_id=excluded.acknowledged_revision_id,"
            "acknowledged_at=excluded.acknowledged_at",
            (item["entry_id"], item["revision_id"], root["generated_at"],
             item["revision_id"], root["generated_at"]),
        )
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
        if db.execute("PRAGMA user_version").fetchone()[0] != 30:
            fail("desktop schema v30 required")
        if args.command == "export":
            args.output.write_text(json.dumps(build(db), ensure_ascii=False, sort_keys=True,
                                                   separators=(",", ":")), encoding="utf-8")
        else:
            apply(db, load(args.input))
            db.commit()


if __name__ == "__main__":
    main()
