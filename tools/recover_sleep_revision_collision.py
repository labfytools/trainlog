#!/usr/bin/env python3
"""Recover one unpublished local Sleep branch from an immutable peer collision."""

import argparse
import hashlib
import json
import re
import sqlite3
import uuid
from datetime import datetime
from pathlib import Path
from typing import Callable

import sleep_diary_exchange as exchange


REVISION_ID = re.compile(r"slr_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")


class RecoveryError(ValueError):
    """A collision cannot be recovered without changing established facts."""


def _new_revision_id() -> str:
    return f"slr_{uuid.uuid4()}"


def _canonical_digest(value: object) -> str:
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def _incoming_entry(document: dict, collision_id: str) -> dict:
    matches = [item for item in document["entries"] if item["revision_id"] == collision_id]
    if len(matches) != 1:
        raise RecoveryError("collision revision is not unique in incoming artifact")
    return matches[0]


def _merge_scalar(local: object, remote: object, label: str) -> object:
    if local == remote:
        return local
    if local is None:
        return remote
    if remote is None:
        return local
    raise RecoveryError(f"human decision required for {label}")


def _merge_identity_list(local: list[dict], remote: list[dict], identity: str,
                         label: str) -> list[dict]:
    merged = {value[identity]: value for value in local}
    for value in remote:
        existing = merged.get(value[identity])
        if existing is not None and existing != value:
            raise RecoveryError(f"{label} identity reused with different content")
        merged[value[identity]] = value
    return [merged[key] for key in sorted(merged)]


def _copy_local_revision(db: sqlite3.Connection, entry_id: str, source_id: str,
                         target_id: str) -> None:
    db.execute(
        "INSERT INTO sleep_diary_revisions "
        "SELECT ?,entry_id,parent_revision_id,created_at,sleep_quality,wake_quality,day_form,"
        "treatment_and_notes FROM sleep_diary_revisions WHERE revision_id=? AND entry_id=?",
        (target_id, source_id, entry_id),
    )
    db.execute(
        "INSERT INTO sleep_diary_events "
        "SELECT ?,event_id,event_type,start_at,end_at FROM sleep_diary_events WHERE revision_id=?",
        (target_id, source_id),
    )
    db.execute(
        "INSERT INTO sleep_medication_intakes(revision_id,intake_id,medication_id,"
        "medication_name,taken_at,dose_value,dose_unit,quantity,note,created_at) "
        "SELECT ?,intake_id,medication_id,medication_name,taken_at,dose_value,dose_unit,"
        "quantity,note,created_at FROM sleep_medication_intakes WHERE revision_id=?",
        (target_id, source_id),
    )


def _insert_revision(db: sqlite3.Connection, item: dict, revision_id: str,
                     parent_revision_id: str | None, created_at: str) -> None:
    db.execute(
        "INSERT INTO sleep_diary_revisions VALUES(?,?,?,?,?,?,?,?)",
        (revision_id, item["entry_id"], parent_revision_id, created_at,
         item["sleep_quality"], item["wake_quality"], item["day_form"],
         item["treatment_and_notes"]),
    )
    db.executemany(
        "INSERT INTO sleep_diary_events VALUES(?,?,?,?,?)",
        [(revision_id, value["event_id"], value["type"], value["start_at"], value["end_at"])
         for value in item["events"]],
    )
    db.executemany(
        "INSERT INTO sleep_medication_intakes(revision_id,intake_id,medication_id,"
        "medication_name,taken_at,dose_value,dose_unit,quantity,note,created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?)",
        [(revision_id, value["intake_id"], value["medication_id"],
          value["medication_name"], value["taken_at"], value["dose_value"],
          value["dose_unit"], value["quantity"], value["note"], value["created_at"])
         for value in item["intakes"]],
    )


def recover(db: sqlite3.Connection, document: dict, collision_id: str, recovered_at: str,
            id_factory: Callable[[], str] = _new_revision_id,
            fail_after_reidentification: bool = False,
            rejected_generation: tuple[dict, str] | None = None) -> dict:
    """Preserve both branches and advance through one non-conflicting successor.

    CONTRACT: only a local current revision that has never crossed an immutable
    publication boundary may be re-identified. The incoming revision retains its
    published identity and exact payload. A fresh successor descends from that
    published revision, while the re-identified local side branch remains durable.
    INVARIANT: every mutation commits in one IMMEDIATE transaction or none does.
    """
    exchange.validate(document)
    if document["version"] != 2 or not REVISION_ID.fullmatch(collision_id):
        raise RecoveryError("Sleep Diary V2 collision identity required")
    incoming = _incoming_entry(document, collision_id)
    incoming_payload = exchange.incoming_revision_payload(incoming, 2)
    owner = db.execute(
        "SELECT entry_id FROM sleep_diary_revisions WHERE revision_id=?", (collision_id,)
    ).fetchone()
    if owner is None or owner[0] != incoming["entry_id"]:
        raise RecoveryError("local collision revision is unavailable")
    entry_id = owner[0]
    current = db.execute(
        "SELECT night_start_date,night_end_date,created_at,updated_at,current_revision_id,deleted "
        "FROM sleep_diary_entries WHERE entry_id=?", (entry_id,)
    ).fetchone()
    if current is None or current[4] != collision_id:
        raise RecoveryError("local collision revision is not the current tip")
    local_payload = exchange.persisted_revision_payload(db, entry_id, collision_id)
    if local_payload == incoming_payload:
        raise RecoveryError("revision is already an identical replay")
    if tuple(current[:3]) != (incoming["night_start_date"], incoming["night_end_date"],
                              incoming["created_at"]) or bool(current[5]) != incoming["deleted"]:
        raise RecoveryError("entry envelope requires a human decision")
    if db.execute(
        "SELECT 1 FROM sleep_diary_revisions WHERE parent_revision_id=? LIMIT 1", (collision_id,)
    ).fetchone() is not None:
        raise RecoveryError("local collision revision already has descendants")
    publication = db.execute(
        "SELECT validated_revision_id,acknowledged_revision_id FROM sleep_diary_publication_state "
        "WHERE entry_id=?", (entry_id,)
    ).fetchone()
    if publication is not None and collision_id in publication:
        raise RecoveryError("local collision revision already crossed publication state")
    try:
        immutable_reference = db.execute(
            "SELECT 1 FROM sleep_diary_generation_entries WHERE entry_id=? AND revision_id=? LIMIT 1",
            (entry_id, collision_id),
        ).fetchone()
    except sqlite3.OperationalError:
        immutable_reference = None
    if immutable_reference is not None:
        raise RecoveryError("local collision revision is referenced by an immutable generation")
    if incoming["parent_revision_id"] is not None and db.execute(
        "SELECT 1 FROM sleep_diary_revisions WHERE revision_id=? AND entry_id=?",
        (incoming["parent_revision_id"], entry_id),
    ).fetchone() is None:
        raise RecoveryError("incoming causal parent is unavailable locally")

    merged = dict(incoming)
    merged["sleep_quality"] = _merge_scalar(
        local_payload["sleep_quality"], incoming["sleep_quality"], "sleep_quality"
    )
    merged["wake_quality"] = _merge_scalar(
        local_payload["wake_quality"], incoming["wake_quality"], "wake_quality"
    )
    merged["day_form"] = _merge_scalar(
        local_payload["day_form"], incoming["day_form"], "day_form"
    )
    if local_payload["treatment_and_notes"] != incoming["treatment_and_notes"]:
        raise RecoveryError("human decision required for treatment_and_notes")
    merged["events"] = _merge_identity_list(
        local_payload["events"], incoming["events"], "event_id", "sleep event"
    )
    merged["intakes"] = _merge_identity_list(
        local_payload["intakes"], incoming["intakes"], "intake_id", "medication intake"
    )

    local_revision_id = id_factory()
    reconciliation_revision_id = id_factory()
    if (not REVISION_ID.fullmatch(local_revision_id) or
            not REVISION_ID.fullmatch(reconciliation_revision_id) or
            len({collision_id, local_revision_id, reconciliation_revision_id}) != 3):
        raise RecoveryError("revision identity generator returned an invalid or duplicate id")

    db.execute("BEGIN IMMEDIATE")
    try:
        _copy_local_revision(db, entry_id, collision_id, local_revision_id)
        changed = db.execute(
            "UPDATE sleep_diary_entries SET current_revision_id=? "
            "WHERE entry_id=? AND current_revision_id=?",
            (local_revision_id, entry_id, collision_id),
        ).rowcount
        if changed != 1:
            raise RecoveryError("guarded local branch re-identification failed")
        if fail_after_reidentification:
            raise RecoveryError("injected recovery failure")
        db.execute("DELETE FROM sleep_diary_events WHERE revision_id=?", (collision_id,))
        db.execute("DELETE FROM sleep_medication_intakes WHERE revision_id=?", (collision_id,))
        db.execute("DELETE FROM sleep_diary_revisions WHERE revision_id=?", (collision_id,))
        _insert_revision(db, incoming, collision_id, incoming["parent_revision_id"],
                         incoming["updated_at"])
        merged["updated_at"] = recovered_at
        _insert_revision(db, merged, reconciliation_revision_id, collision_id, recovered_at)
        changed = db.execute(
            "UPDATE sleep_diary_entries SET updated_at=?,current_revision_id=? "
            "WHERE entry_id=? AND current_revision_id=?",
            (recovered_at, reconciliation_revision_id, entry_id, local_revision_id),
        ).rowcount
        if changed != 1:
            raise RecoveryError("guarded reconciliation advance failed")
        rejection_rearmed = False
        if rejected_generation is not None:
            rejection_rearmed = _rearm_desktop_rejection(
                db, rejected_generation[0], rejected_generation[1]
            )
        if db.execute("PRAGMA foreign_key_check").fetchone() is not None:
            raise RecoveryError("foreign key check failed during recovery")
        db.commit()
    except Exception:
        db.rollback()
        raise
    merged_payload = dict(merged)
    merged_payload["revision_id"] = reconciliation_revision_id
    merged_payload["parent_revision_id"] = collision_id
    return {
        "entry_id": entry_id,
        "collision_revision_id": collision_id,
        "local_reidentified_revision_id": local_revision_id,
        "reconciliation_revision_id": reconciliation_revision_id,
        "reconciliation_parent_revision_id": collision_id,
        "local_payload_sha256": _canonical_digest(local_payload),
        "incoming_payload_sha256": _canonical_digest(incoming_payload),
        "merged_payload_sha256": _canonical_digest(
            exchange.incoming_revision_payload(merged_payload, 2)
        ),
        "event_count": len(merged["events"]),
        "intake_count": len(merged["intakes"]),
        "desktop_rejection_rearmed": rejection_rearmed,
    }


def _rearm_desktop_rejection(db: sqlite3.Connection, manifest: dict,
                             manifest_digest: str) -> bool:
    generation_id = manifest["generation_id"]
    row = db.execute(
        "SELECT run_id,producer_peer_id,consumer_peer_id,parent_generation_id,manifest_sha256,"
        "result,durability,diagnostic FROM sync_consumed_generations WHERE generation_id=?",
        (generation_id,),
    ).fetchone()
    if row is None:
        return False
    expected = (
        manifest["run_id"], manifest["producer"]["peer_id"], manifest["consumer_peer_id"],
        manifest["parent_generation_id"], manifest_digest, "rejected", "sqlite-commit-rejection",
    )
    if tuple(row[:7]) != expected:
        raise RecoveryError("generation rejection ledger does not match immutable manifest")
    # The deployed consumer could lose ValueError text while recording this
    # exact collision (StopIteration has an empty message). The collision has
    # already been proven directly against the manifest-owned Sleep artifact.
    if row[7] not in (
            "",
            "sleep diary revision identity reused with different content",
            "schema desktop v8 à v35 requis",
    ):
        raise RecoveryError("generation rejection has an unrelated diagnostic")
    if db.execute(
        "SELECT 1 FROM sync_consumed_generations WHERE parent_generation_id=? LIMIT 1",
        (generation_id,),
    ).fetchone() is not None:
        raise RecoveryError("rejected generation already has a durable descendant")
    ack = db.execute(
        "SELECT ack_id,result,durability,manifest_sha256 FROM sync_acknowledgements "
        "WHERE generation_id=?", (generation_id,),
    ).fetchall()
    expected_ack_id = "ack_" + generation_id.removeprefix("gen_")
    if ack != [(expected_ack_id, "rejected", "sqlite-commit-rejection", manifest_digest)]:
        raise RecoveryError("generation rejection ACK does not match immutable manifest")
    db.execute("DELETE FROM sync_acknowledgements WHERE generation_id=?", (generation_id,))
    db.execute("DELETE FROM sync_consumed_generations WHERE generation_id=?", (generation_id,))
    return True


def rearm_desktop_consumer(db: sqlite3.Connection, document: dict, manifest: dict,
                           manifest_digest: str, collision_id: str) -> dict:
    """Rearm an exact retry after recovery or a recovery-adjacent schema rejection.

    CONTRACT: the published collision identity must already contain the exact
    manifest-owned payload. Only the matching rejected consumption and ACK are
    removed; immutable generation bytes and recovered Sleep history are unchanged.
    """
    exchange.validate(document)
    incoming = _incoming_entry(document, collision_id)
    persisted = exchange.persisted_revision_payload(db, incoming["entry_id"], collision_id)
    if persisted != exchange.incoming_revision_payload(incoming, 2):
        raise RecoveryError("recovered collision payload no longer matches immutable artifact")
    db.execute("BEGIN IMMEDIATE")
    try:
        rearmed = _rearm_desktop_rejection(db, manifest, manifest_digest)
        if not rearmed:
            raise RecoveryError("desktop has no matching rejected generation to rearm")
        db.commit()
    except Exception:
        db.rollback()
        raise
    return {
        "generation_id": manifest["generation_id"],
        "manifest_sha256": manifest_digest,
        "status": "ready_for_retry",
        "immutable_artifacts_rewritten": False,
    }


def rearm_android_producer(db: sqlite3.Connection, manifest: dict, manifest_digest: str,
                           collision_id: str) -> dict:
    """Rearm one rejected immutable Android generation after causal recovery.

    CONTRACT: manifest bytes and generation-to-Sleep mapping remain unchanged.
    Only the exact rejection ACK is removed and its producer state returns to
    waiting_acknowledgement so the normal retry can record a consumed ACK.
    """
    generation_id = manifest["generation_id"]
    db.execute("BEGIN IMMEDIATE")
    try:
        row = db.execute(
            "SELECT run_id,producer_peer_id,consumer_peer_id,generated_at,parent_generation_id,"
            "manifest_sha256,status FROM sync_generations WHERE generation_id=?",
            (generation_id,),
        ).fetchone()
        expected = (
            manifest["run_id"], manifest["producer"]["peer_id"], manifest["consumer_peer_id"],
            manifest["generated_at"], manifest["parent_generation_id"], manifest_digest, "rejected",
        )
        if row is None or tuple(row) != expected:
            raise RecoveryError("Android rejected generation does not match immutable manifest")
        mapping = db.execute(
            "SELECT revision_id FROM sleep_diary_generation_entries WHERE generation_id=? "
            "AND entry_id=(SELECT entry_id FROM sleep_diary_revisions WHERE revision_id=?)",
            (generation_id, collision_id),
        ).fetchone()
        if mapping != (collision_id,):
            raise RecoveryError("Android generation does not own the collided Sleep revision")
        ack = db.execute(
            "SELECT ack_id,result,durability,manifest_sha256 FROM sync_acknowledgements "
            "WHERE generation_id=?", (generation_id,),
        ).fetchall()
        expected_ack_id = "ack_" + generation_id.removeprefix("gen_")
        if ack != [(expected_ack_id, "rejected", "sqlite-commit-rejection", manifest_digest)]:
            raise RecoveryError("Android rejection ACK does not match immutable manifest")
        db.execute("DELETE FROM sync_acknowledgements WHERE generation_id=?", (generation_id,))
        changed = db.execute(
            "UPDATE sync_generations SET status='waiting_acknowledgement',acknowledged_at=NULL "
            "WHERE generation_id=? AND status='rejected'", (generation_id,),
        ).rowcount
        if changed != 1:
            raise RecoveryError("Android generation rearm guard failed")
        if db.execute("PRAGMA foreign_key_check").fetchone() is not None:
            raise RecoveryError("foreign key check failed during Android rearm")
        db.commit()
    except Exception:
        db.rollback()
        raise
    return {
        "generation_id": generation_id,
        "manifest_sha256": manifest_digest,
        "status": "waiting_acknowledgement",
        "immutable_artifacts_rewritten": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    desktop = subparsers.add_parser("recover-desktop")
    desktop.add_argument("--database", required=True, type=Path)
    desktop.add_argument("--generation-directory", required=True, type=Path)
    desktop.add_argument("--collision-id", required=True)
    desktop.add_argument("--recovered-at", default=None)
    desktop.add_argument("--report", type=Path)
    desktop_retry = subparsers.add_parser("rearm-desktop")
    desktop_retry.add_argument("--database", required=True, type=Path)
    desktop_retry.add_argument("--generation-directory", required=True, type=Path)
    desktop_retry.add_argument("--collision-id", required=True)
    desktop_retry.add_argument("--report", type=Path)
    android = subparsers.add_parser("rearm-android")
    android.add_argument("--database", required=True, type=Path)
    android.add_argument("--generation-directory", required=True, type=Path)
    android.add_argument("--collision-id", required=True)
    android.add_argument("--report", type=Path)
    args = parser.parse_args()
    import sync_generation_exchange as generation

    manifest_path = args.generation_directory / "manifest.json"
    preliminary = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest, manifest_digest = generation.validate_published(
        args.generation_directory, preliminary["consumer_peer_id"]
    )
    sleep_descriptor = next(
        (value for value in manifest["artifacts"] if value["logical_name"] == "sleep-diary"),
        None,
    )
    if sleep_descriptor is None:
        raise RecoveryError("generation has no Sleep Diary artifact")
    artifact = args.generation_directory / Path(sleep_descriptor["filename"]).name
    document = exchange.load(artifact)
    with exchange.connect_database(args.database) as db:
        version = db.execute("PRAGMA user_version").fetchone()[0]
        if args.command == "recover-desktop":
            if version != 35:
                raise RecoveryError("desktop schema v35 pre-immutability recovery required")
            recovered_at = args.recovered_at or datetime.now().astimezone().isoformat()
            report = recover(
                db, document, args.collision_id, recovered_at,
                rejected_generation=(manifest, manifest_digest),
            )
        elif args.command == "rearm-desktop":
            if version != 36:
                raise RecoveryError("desktop schema v36 post-recovery retry required")
            report = rearm_desktop_consumer(
                db, document, manifest, manifest_digest, args.collision_id
            )
        else:
            if version not in (32, 33):
                raise RecoveryError("Android schema v32 or v33 required")
            report = rearm_android_producer(
                db, manifest, manifest_digest, args.collision_id
            )
    encoded = json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    if args.report is not None:
        args.report.write_text(encoded, encoding="utf-8")
    print(encoded, end="")


if __name__ == "__main__":
    main()
