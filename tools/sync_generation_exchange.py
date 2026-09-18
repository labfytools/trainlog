#!/usr/bin/env python3
"""Staged coherent-generation, manifest, consumption and ACK service.

The active V3 transport does not call this module. Filesystem publication is a
bounded object-store adapter used by explicit staged callers and tests.
"""
import argparse
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import uuid
from contextlib import closing
from pathlib import Path, PurePosixPath

from trainlog_sqlite import connect_database
import causal_delete_exchange
import execution_draft_exchange
import import_equipment_associations
import import_equipment_definitions
import import_exercise_aliases
import import_exercise_body_zones
import import_exercise_profile_state
import import_mobile_export
import import_training_feedback
import export_equipment_definitions
import export_exercise_body_zones
import export_pc_mobile
import export_training_feedback

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "trainlog-sync-manifest"
ACK = "trainlog-sync-ack"
MAX_MANIFEST = 64 * 1024
MAX_ARTIFACTS = 32
MAX_ARTIFACT = 64 * 1024 * 1024
MAX_GENERATION = 256 * 1024 * 1024
MAX_NAME = 64
MAX_PATH = 240
MAX_DIAGNOSTIC = 1024
MAX_RETAINED_PER_PEER = 8
ID = re.compile(r"^(?:gen|peer|sy)_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")
HEX = re.compile(r"^[0-9a-f]{64}$")

ARTIFACTS = (
    ("catalog", "trainlog-pc-catalog", 1, "catalog-v1.json", True,
     "export_pc_catalog.py", ()),
    ("history", "trainlog-mobile-export", 4, "history-v4.json", True,
     "export_pc_mobile.py", ("--version", "4")),
    ("execution-drafts", "trainlog-execution-drafts", 1, "execution-drafts-v1.json", True,
     "execution_draft_exchange.py", ("export",)),
    ("exercise-aliases", "trainlog-exercise-aliases", 1, "exercise-aliases-v1.json", True,
     "export_exercise_aliases.py", ()),
    ("exercise-profile-state", "trainlog-exercise-profile-state", 1, "exercise-profile-state-v1.json", True,
     "export_exercise_profile_state.py", ()),
    ("equipment-definitions", "trainlog-equipment-definitions", 1, "equipment-definitions-v1.json", True,
     "export_equipment_definitions.py", ()),
    ("equipment-associations", "trainlog-equipment-associations", 2, "equipment-associations-v2.json", True,
     "export_equipment_associations.py", ()),
    ("body-zones", "trainlog-exercise-body-zones", 1, "body-zones-v1.json", True,
     "export_exercise_body_zones.py", ()),
    ("feedback", "trainlog-training-feedback", 2, "feedback-v2.json", True,
     "export_training_feedback.py", ()),
    ("causal-deletions", "trainlog-causal-deletions", 1, "causal-deletions-v1.json", True,
     "causal_delete_exchange.py", ("export",)),
    ("ai-proposals", "trainlog-ai-session-drafts", 2, "ai-proposals-v2.json", False,
     "export_ai_session_drafts.py", ()),
    ("session-preparations", "trainlog-session-preparations", 2,
     "session-preparations-v2.json", False, "export_session_preparations.py", ()),
)
SUPPORTED = {(row[1], row[2]) for row in ARTIFACTS}


class GenerationError(RuntimeError):
    pass


def now() -> str:
    return dt.datetime.now().astimezone().isoformat()


def canonical(value) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":")).encode("utf-8")


def digest_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def digest_file(path: Path, declared: int | None = None) -> tuple[int, str]:
    total = 0
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            total += len(block)
            if total > MAX_ARTIFACT or declared is not None and total > declared:
                raise GenerationError("artifact exceeds declared or 64 MiB bound")
            checksum.update(block)
    return total, checksum.hexdigest()


def strict_json(raw: bytes, maximum: int):
    if len(raw) > maximum:
        raise GenerationError("JSON exceeds byte bound")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise GenerationError("duplicate JSON key: " + key)
            result[key] = value
        return result
    try:
        root = json.loads(raw.decode("utf-8"), object_pairs_hook=unique)
        stack = [(root, 1)]; nodes = 0
        while stack:
            value, depth = stack.pop(); nodes += 1
            if depth > 8 or nodes > 8192:
                raise GenerationError("JSON nesting/node bound exceeded")
            if isinstance(value, dict): stack.extend((child, depth + 1) for child in value.values())
            elif isinstance(value, list): stack.extend((child, depth + 1) for child in value)
        return root
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise GenerationError("invalid UTF-8/JSON") from error


def valid_id(value: object, prefix: str) -> bool:
    return isinstance(value, str) and value.startswith(prefix + "_") and ID.fullmatch(value) is not None


def validate_relative(value: object, generation_id: str) -> str:
    if not isinstance(value, str) or not value or len(value.encode()) > MAX_PATH or "\\" in value or "\x00" in value:
        raise GenerationError("invalid artifact path")
    path = PurePosixPath(value)
    parts = path.parts
    if path.is_absolute() or len(parts) > 3 or any(part in ("", ".", "..") for part in parts):
        raise GenerationError("unsafe artifact path")
    if len(parts) != 3 or parts[:2] != ("generations", generation_id):
        raise GenerationError("artifact outside generation namespace")
    return value


def validate_manifest_bytes(raw: bytes, expected_consumer: str | None = None):
    root = strict_json(raw, MAX_MANIFEST)
    keys = {"format", "version", "generation_id", "run_id", "producer",
            "consumer_peer_id", "generated_at", "parent_generation_id", "artifacts"}
    if not isinstance(root, dict) or set(root) != keys or root["format"] != MANIFEST or type(root["version"]) is not int or root["version"] != 1:
        raise GenerationError("unsupported manifest shape/version")
    generation = root["generation_id"]
    if not valid_id(generation, "gen") or not valid_id(root["run_id"], "sy"):
        raise GenerationError("invalid generation/run identity")
    producer = root["producer"]
    if not isinstance(producer, dict) or set(producer) != {"peer_id", "kind"} or not valid_id(producer["peer_id"], "peer") or producer["kind"] not in ("android", "desktop"):
        raise GenerationError("invalid producer")
    if not valid_id(root["consumer_peer_id"], "peer") or expected_consumer and root["consumer_peer_id"] != expected_consumer:
        raise GenerationError("wrong consumer")
    if root["consumer_peer_id"] == producer["peer_id"]:
        raise GenerationError("producer and consumer must differ")
    if root["parent_generation_id"] is not None and (not valid_id(root["parent_generation_id"], "gen") or root["parent_generation_id"] == generation):
        raise GenerationError("invalid parent generation")
    try:
        parsed = dt.datetime.fromisoformat(root["generated_at"].replace("Z", "+00:00"))
        if parsed.utcoffset() is None:
            raise ValueError
    except Exception as error:
        raise GenerationError("invalid generated_at") from error
    values = root["artifacts"]
    if not isinstance(values, list) or not 1 <= len(values) <= MAX_ARTIFACTS:
        raise GenerationError("artifact descriptor bound")
    names, paths, total = set(), set(), 0
    for item in values:
        if not isinstance(item, dict) or set(item) != {"logical_name", "format", "version", "filename", "size", "sha256", "required"}:
            raise GenerationError("invalid artifact descriptor")
        name = item["logical_name"]
        if not isinstance(name, str) or not name.isascii() or not 0 < len(name.encode()) <= MAX_NAME or name in names:
            raise GenerationError("invalid/duplicate logical name")
        names.add(name)
        filename = validate_relative(item["filename"], generation)
        if filename in paths:
            raise GenerationError("duplicate artifact path")
        paths.add(filename)
        if type(item["version"]) is not int or item["version"] < 1 or type(item["size"]) is not int or not 0 <= item["size"] <= MAX_ARTIFACT or type(item["required"]) is not bool or not isinstance(item["format"], str) or not HEX.fullmatch(item["sha256"] or ""):
            raise GenerationError("invalid artifact metadata")
        if item["required"] and (item["format"], item["version"]) not in SUPPORTED:
            raise GenerationError("unsupported required capability")
        total += item["size"]
        if total > MAX_GENERATION:
            raise GenerationError("generation exceeds 256 MiB")
    required = {row[0] for row in ARTIFACTS if row[4]}
    if not required.issubset(names):
        raise GenerationError("missing required domain")
    return root


def require_schema(db: sqlite3.Connection) -> None:
    supported_versions = (24, 25)
    if db.execute("PRAGMA user_version").fetchone()[0] not in supported_versions:
        raise GenerationError("desktop schema v24 or v25 required")


def peer_identity(db: sqlite3.Connection, kind: str) -> str:
    require_schema(db)
    row = db.execute("SELECT peer_id,kind FROM sync_peer_identity WHERE singleton=1").fetchone()
    if row:
        if row[1] != kind:
            raise GenerationError("peer kind conflict")
        return row[0]
    identity = "peer_" + str(uuid.uuid4())
    db.execute("INSERT INTO sync_peer_identity VALUES(1,?,?)", (identity, kind))
    return identity


def archive_digest(directory: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(value for value in directory.rglob("*") if value.is_file()):
        if path.is_symlink():
            raise GenerationError("archive contains symbolic link")
        relative = path.relative_to(directory).as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        digest.update(bytes.fromhex(digest_file(path)[1]))
    return digest.hexdigest()


def archive_acknowledged(database: Path, owned_root: Path, consumer: str) -> int:
    """Durably archive eligible acknowledged payloads without deleting evidence.

    WHY: acknowledged generations otherwise consume the bounded active window
    forever. CONTRACT: exact correlated consumed ACK evidence is required and
    the newest two acknowledged lineage members remain active for recovery.
    INVARIANT: generation, artifact, ACK and causal rows are never removed or
    rewritten; a ledger row is committed only after a verified atomic copy.
    """
    archive_root = owned_root / "archives" / "generations"
    archive_root.mkdir(parents=True, exist_ok=True, mode=0o700)
    with closing(connect_database(database)) as db:
        require_schema(db)
        rows = db.execute(
            "SELECT g.generation_id,g.run_id,g.producer_peer_id,g.consumer_peer_id,"
            "g.manifest_sha256,g.staging_path,g.manifest_json FROM sync_generations g "
            "WHERE g.consumer_peer_id=? AND g.status='acknowledged' "
            "AND NOT EXISTS(SELECT 1 FROM sync_generation_archives a WHERE a.generation_id=g.generation_id) "
            "ORDER BY g.generated_at DESC,g.generation_id DESC",
            (consumer,),
        ).fetchall()
    archived = 0
    for generation, run, producer, target, manifest_sha, staging, manifest_json in rows[2:]:
        with closing(connect_database(database)) as db:
            ack = db.execute(
                "SELECT ack_id FROM sync_acknowledgements WHERE generation_id=? AND run_id=? "
                "AND producer_peer_id=? AND consumer_peer_id=? AND manifest_sha256=? "
                "AND result='consumed' AND durability='sqlite-commit-full' LIMIT 2",
                (generation, run, producer, target, manifest_sha),
            ).fetchall()
        if len(ack) != 1:
            continue
        source = Path(staging)
        if source.is_symlink():
            raise GenerationError("generation staging is a symbolic link")
        manifest = validate_manifest_bytes(manifest_json.encode(), target)
        validate_published(source, target)
        destination = archive_root / generation
        temporary = archive_root / ("." + generation + ".tmp")
        if destination.exists():
            if destination.is_symlink():
                raise GenerationError("generation archive is a symbolic link")
            validate_published(destination, target)
            checksum = archive_digest(destination)
        else:
            if temporary.exists():
                if temporary.is_symlink():
                    raise GenerationError("temporary generation archive is a symbolic link")
                shutil.rmtree(temporary)
            shutil.copytree(source, temporary, copy_function=shutil.copy2)
            validate_published(temporary, target)
            checksum = archive_digest(temporary)
            for copied in temporary.rglob("*"):
                if copied.is_file():
                    with copied.open("rb") as stream:
                        os.fsync(stream.fileno())
            os.replace(temporary, destination)
            directory_fd = os.open(archive_root, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(directory_fd)
            finally:
                os.close(directory_fd)
        audit = canonical({
            "format": "trainlog-sync-generation-archive-audit",
            "version": 1,
            "generation_id": generation,
            "run_id": run,
            "manifest_sha256": manifest_sha,
            "ack_id": ack[0][0],
            "archive_sha256": checksum,
        }).decode()
        with closing(connect_database(database)) as db:
            db.execute("BEGIN IMMEDIATE")
            current = db.execute(
                "SELECT status,manifest_sha256 FROM sync_generations WHERE generation_id=?",
                (generation,),
            ).fetchone()
            if current != ("acknowledged", manifest_sha):
                raise GenerationError("generation changed during archive")
            db.execute(
                "INSERT OR IGNORE INTO sync_generation_archives VALUES(?,?,?,?,?,?)",
                (generation, str(destination), manifest_sha, checksum, now(), audit),
            )
            db.commit()
        archived += 1
    return archived


def run_export(tool: str, extra: tuple[str, ...], output: Path, snapshot: Path) -> None:
    command = [sys.executable, str(ROOT / "tools" / tool)]
    if tool in ("execution_draft_exchange.py", "causal_delete_exchange.py"):
        command.extend(extra); command.append(str(output))
    else:
        command.append(str(output)); command.extend(extra)
    command.extend(("--database", str(snapshot)))
    generation_exporters = {
        "export_pc_mobile.py": export_pc_mobile,
        "export_equipment_definitions.py": export_equipment_definitions,
        "export_exercise_body_zones.py": export_exercise_body_zones,
        "export_training_feedback.py": export_training_feedback,
    }
    if tool in generation_exporters:
        previous = sys.argv
        try:
            sys.argv = command[1:]
            generation_exporters[tool].main(complete_causal_envelope=True)
            return
        finally:
            sys.argv = previous
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise GenerationError(f"export {tool} failed: {result.stdout}{result.stderr}")


def capture_desktop(database: Path, owned_root: Path, consumer: str,
                    run_id: str | None = None, generation_id: str | None = None):
    if not valid_id(consumer, "peer"):
        raise GenerationError("invalid expected consumer")
    generation = generation_id or "gen_" + str(uuid.uuid4())
    run = run_id or "sy_" + str(uuid.uuid4())
    if not valid_id(generation, "gen") or not valid_id(run, "sy"):
        raise GenerationError("invalid generation/run")
    archive_acknowledged(database, owned_root, consumer)
    with closing(connect_database(database)) as admission:
        require_schema(admission)
        retained = admission.execute("SELECT COUNT(*) FROM sync_generations g WHERE consumer_peer_id=? AND status IN('captured','published','waiting_acknowledgement','acknowledged') AND NOT EXISTS(SELECT 1 FROM sync_generation_archives a WHERE a.generation_id=g.generation_id)", (consumer,)).fetchone()[0]
        if retained >= MAX_RETAINED_PER_PEER:
            raise GenerationError("generation retention capacity exhausted")
    stage = owned_root / "staging" / generation
    if stage.exists():
        raise GenerationError("generation staging already exists")
    stage.mkdir(parents=True, mode=0o700)
    snapshot = stage / ".snapshot.sqlite"
    try:
        with closing(connect_database(database)) as source, closing(sqlite3.connect(snapshot)) as target:
            require_schema(source)
            producer = peer_identity(source, "desktop")
            source.commit()
            source.backup(target)
        descriptors = []
        total = 0
        for logical, fmt, version, filename, required, tool, extra in ARTIFACTS:
            output = stage / filename
            run_export(tool, extra, output, snapshot)
            size, checksum = digest_file(output)
            total += size
            if total > MAX_GENERATION:
                raise GenerationError("generation exceeds 256 MiB")
            descriptors.append({"logical_name": logical, "format": fmt,
                "version": version, "filename": f"generations/{generation}/{filename}",
                "size": size, "sha256": checksum, "required": required})
        parent = None
        with closing(connect_database(database)) as db:
            parent_rows = db.execute("SELECT g.generation_id FROM sync_generations g WHERE g.producer_peer_id=? AND g.consumer_peer_id=? AND g.status='acknowledged' AND NOT EXISTS(SELECT 1 FROM sync_generations c WHERE c.parent_generation_id=g.generation_id AND c.status='acknowledged') LIMIT 2", (producer, consumer)).fetchall()
            if len(parent_rows) > 1: raise GenerationError("outgoing generation lineage has multiple tips")
            parent = parent_rows[0][0] if parent_rows else None
        manifest = {"format": MANIFEST, "version": 1, "generation_id": generation,
            "run_id": run, "producer": {"peer_id": producer, "kind": "desktop"},
            "consumer_peer_id": consumer, "generated_at": now(),
            "parent_generation_id": parent, "artifacts": descriptors}
        raw = canonical(manifest)
        validate_manifest_bytes(raw, consumer)
        checksum = digest_bytes(raw)
        (stage / "manifest.json").write_bytes(raw)
        os.chmod(stage / "manifest.json", 0o600)
        snapshot.unlink()
        with closing(connect_database(database)) as db:
            db.execute("BEGIN IMMEDIATE")
            peer_identity(db, "desktop")
            db.execute("INSERT INTO sync_generations VALUES(?,?,?,?,?,?,?,?,?,?,?,NULL)",
                (generation, run, producer, consumer, "desktop", manifest["generated_at"],
                 parent, checksum, "captured", raw.decode(), str(stage)))
            db.executemany("INSERT INTO sync_generation_artifacts VALUES(?,?,?,?,?,?,?,?)",
                [(generation, value["logical_name"], value["format"], value["version"],
                  value["filename"], value["size"], value["sha256"], int(value["required"]))
                 for value in descriptors])
            # CONTRACT: the exact generation relation is recorded only after
            # its complete immutable artifact has been captured successfully.
            preparation_path = stage / "session-preparations-v2.json"
            if preparation_path.is_file():
                captured_preparations = strict_json(
                    preparation_path.read_bytes(), MAX_ARTIFACT
                )
                for delivery in captured_preparations["deliveries"]:
                    cursor = db.execute(
                        "UPDATE session_preparation_deliveries SET generation_id=? "
                        "WHERE delivery_id=? AND revision_id=? AND "
                        "state IN('pending','remote_unknown') AND generation_id IS NULL",
                        (generation, delivery["delivery_id"], delivery["revision_id"]),
                    )
                    if cursor.rowcount != 1:
                        raise GenerationError("preparation delivery changed during capture")
                for withdrawal in captured_preparations["withdrawals"]:
                    cursor = db.execute(
                        "UPDATE session_preparation_withdrawals SET generation_id=? "
                        "WHERE withdrawal_id=? AND revision_id=? AND generation_id IS NULL",
                        (generation, withdrawal["withdrawal_id"], withdrawal["revision_id"]),
                    )
                    if cursor.rowcount != 1:
                        raise GenerationError("preparation withdrawal changed during capture")
            causal_path = stage / "causal-deletions-v1.json"
            if causal_path.is_file():
                for operation in json.loads(causal_path.read_text())["operations"]:
                    existed = db.execute("SELECT 1 FROM sync_causal_publications WHERE operation_id=?", (operation["operation_id"],)).fetchone()
                    db.execute("INSERT INTO sync_causal_publications VALUES(?,?,?)",
                               (operation["operation_id"], generation, 0 if existed else 1))
            db.commit()
        return stage, manifest, checksum
    except Exception:
        if stage.exists():
            shutil.rmtree(stage)
        raise


def publish(database: Path, generation: str, object_root: Path) -> Path:
    with closing(connect_database(database)) as db:
        require_schema(db)
        row = db.execute("SELECT manifest_json,manifest_sha256,staging_path,status FROM sync_generations WHERE generation_id=?", (generation,)).fetchone()
        if not row:
            raise GenerationError("unknown generation")
        if row[3] not in ("captured", "published", "waiting_acknowledgement"):
            raise GenerationError("generation is not publishable")
        manifest = validate_manifest_bytes(row[0].encode())
        destination = object_root / "generations" / generation
        source = Path(row[2])
        marker = destination / "manifest.json"
        if marker.is_file():
            existing = marker.read_bytes()
            if digest_bytes(existing) != row[1]: raise GenerationError("same generation has different published bytes")
            validate_published(destination, manifest["consumer_peer_id"])
        else:
            if destination.exists() and (not destination.is_dir() or destination.is_symlink()):
                raise GenerationError("unsafe generation namespace")
            destination.mkdir(parents=True, mode=0o700)
            for item in manifest["artifacts"]:
                source_file = source / PurePosixPath(item["filename"]).name
                target = destination / source_file.name
                if not source_file.is_file() or source_file.is_symlink() or source_file.resolve().parent != source.resolve():
                    raise GenerationError("unsafe staging substitution")
                size, checksum = digest_file(source_file, item["size"])
                if size != item["size"] or checksum != item["sha256"]:
                    raise GenerationError("staging artifact changed after capture")
                if target.exists():
                    if target.is_symlink() or digest_file(target, item["size"]) != (item["size"], item["sha256"]):
                        raise GenerationError("conflicting partial publication")
                else:
                    with source_file.open("rb") as incoming, target.open("xb") as outgoing:
                        shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
                    os.chmod(target, 0o600)
            # Commit marker is deliberately last.
            with marker.open("xb") as stream:
                stream.write(row[0].encode())
            os.chmod(marker, 0o600)
        db.execute("UPDATE sync_generations SET status='waiting_acknowledgement' WHERE generation_id=?", (generation,))
        db.commit()
        return destination


def validate_published(directory: Path, consumer: str):
    marker = directory / "manifest.json"
    if not marker.is_file() or marker.is_symlink():
        raise GenerationError("complete manifest marker missing")
    raw = marker.read_bytes() if marker.stat().st_size <= MAX_MANIFEST else b"x" * (MAX_MANIFEST + 1)
    manifest = validate_manifest_bytes(raw, consumer)
    total = 0
    for item in manifest["artifacts"]:
        path = directory / PurePosixPath(item["filename"]).name
        if not path.is_file() or path.is_symlink() or path.resolve().parent != directory.resolve():
            raise GenerationError("missing or unsafe listed artifact")
        size, checksum = digest_file(path, item["size"])
        if size != item["size"] or checksum != item["sha256"]:
            raise GenerationError("artifact size/digest mismatch")
        total += size
        if total > MAX_GENERATION:
            raise GenerationError("generation exceeds bound")
    return manifest, digest_bytes(raw)


def ack_document(run: str, generation: str, producer: str, consumer: str,
                 manifest_digest: str, result: str, consumed_at: str,
                 diagnostic: str = ""):
    if len(diagnostic.encode()) > MAX_DIAGNOSTIC:
        raise GenerationError("diagnostic exceeds bound")
    value = {"format": ACK, "version": 1, "ack_id": "ack_" + generation[4:],
        "run_id": run, "generation_id": generation, "producer_peer_id": producer,
        "consumer_peer_id": consumer, "manifest_sha256": manifest_digest,
        "result": result, "durability": "sqlite-commit-full" if result == "consumed" else "sqlite-commit-rejection", "consumed_at": consumed_at,
        "diagnostic": diagnostic}
    value["payload_sha256"] = digest_bytes(canonical(value))
    return value


def record_consumed(db: sqlite3.Connection, manifest: dict, manifest_digest: str,
                    diagnostic: str = "") -> dict:
    """Record consumption inside the caller-owned business transaction."""
    require_schema(db)
    generation = manifest["generation_id"]
    known = db.execute("SELECT manifest_sha256,ack_json FROM sync_consumed_generations WHERE generation_id=?", (generation,)).fetchone()
    if known:
        if known[0] != manifest_digest:
            raise GenerationError("generation identity reused with different manifest")
        return json.loads(known[1])
    consumer = peer_identity(db, "desktop")
    if consumer != manifest["consumer_peer_id"]:
        raise GenerationError("wrong local consumer")
    consumed_at = now()
    ack = ack_document(manifest["run_id"], generation, manifest["producer"]["peer_id"],
                       consumer, manifest_digest, "consumed", consumed_at, diagnostic)
    db.execute("INSERT INTO sync_consumed_generations VALUES(?,?,?,?,?,?,?,?,?,?,?)",
        (generation, manifest["run_id"], manifest["producer"]["peer_id"], consumer,
         manifest["parent_generation_id"], manifest_digest, consumed_at, "consumed", "sqlite-commit-full", diagnostic,
         canonical(ack).decode()))
    db.execute("INSERT INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)",
        (ack["ack_id"], generation, ack["run_id"], ack["producer_peer_id"], consumer,
         manifest_digest, "consumed", ack["durability"], consumed_at, diagnostic,
         ack["payload_sha256"]))
    return ack


def record_rejected(database: Path, manifest: dict, manifest_digest: str, diagnostic: str) -> dict:
    diagnostic = diagnostic.encode("utf-8")[:MAX_DIAGNOSTIC].decode("utf-8", "ignore")
    with closing(connect_database(database)) as db:
        require_schema(db); db.execute("BEGIN IMMEDIATE")
        consumer = peer_identity(db, "desktop"); rejected_at = now()
        ack = ack_document(manifest["run_id"], manifest["generation_id"], manifest["producer"]["peer_id"], consumer, manifest_digest, "rejected", rejected_at, diagnostic)
        db.execute("INSERT OR IGNORE INTO sync_consumed_generations VALUES(?,?,?,?,?,?,?,?,?,?,?)", (manifest["generation_id"],manifest["run_id"],manifest["producer"]["peer_id"],consumer,manifest["parent_generation_id"],manifest_digest,rejected_at,"rejected",ack["durability"],diagnostic,canonical(ack).decode()))
        db.execute("INSERT OR IGNORE INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)", (ack["ack_id"],manifest["generation_id"],manifest["run_id"],manifest["producer"]["peer_id"],consumer,manifest_digest,"rejected",ack["durability"],rejected_at,diagnostic,ack["payload_sha256"]))
        db.commit(); return ack


def consume_desktop(database: Path, directory: Path) -> dict:
    """Consume one complete Android generation in one desktop transaction."""
    with closing(connect_database(database)) as identity_db:
        consumer = peer_identity(identity_db, "desktop")
        identity_db.commit()
    manifest, manifest_digest = validate_published(directory, consumer)
    listed = {item["logical_name"]: directory / PurePosixPath(item["filename"]).name
              for item in manifest["artifacts"]}
    def required(name):
        try:
            return listed[name]
        except KeyError as error:
            raise GenerationError("missing required domain: " + name) from error

    # Parse and domain-validate bounded bytes before opening the write scope.
    causal = causal_delete_exchange.load(required("causal-deletions"))
    drafts = execution_draft_exchange.load(required("execution-drafts"))
    aliases = import_exercise_aliases.load(required("exercise-aliases"))
    profile = strict_json(required("exercise-profile-state").read_bytes(), MAX_ARTIFACT)
    equipment_payload = strict_json(required("equipment-definitions").read_bytes(), MAX_ARTIFACT)
    reserved = import_equipment_definitions.supplied_ids(ROOT / "catalog/equipment-v1.json")
    definitions = import_equipment_definitions.validate(equipment_payload, reserved)
    history = import_mobile_export.load_payload(required("history"))
    known_equipment = set(reserved) | {item[0] for item in definitions}
    import_mobile_export.validate_payload(history, known_equipment)
    association_payload = strict_json(required("equipment-associations").read_bytes(), MAX_ARTIFACT)
    zones_payload = strict_json(required("body-zones").read_bytes(), MAX_ARTIFACT)
    parsed_zones = import_exercise_body_zones.parse_payload(zones_payload)
    feedback = import_training_feedback.load(required("feedback"))
    mobile_occurrences = import_equipment_associations.load_mobile_occurrences(required("history"))

    with closing(connect_database(database)) as db:
        require_schema(db)
        db.execute("PRAGMA foreign_keys=ON")
        db.execute("BEGIN IMMEDIATE")
        try:
            known = db.execute("SELECT manifest_sha256,ack_json FROM sync_consumed_generations WHERE generation_id=?", (manifest["generation_id"],)).fetchone()
            if known:
                if known[0] != manifest_digest:
                    raise GenerationError("generation identity reused with different manifest")
                db.rollback()
                return json.loads(known[1])
            predecessors = db.execute("SELECT g.generation_id FROM sync_consumed_generations g WHERE g.producer_peer_id=? AND g.result='consumed' AND NOT EXISTS(SELECT 1 FROM sync_consumed_generations c WHERE c.parent_generation_id=g.generation_id AND c.result='consumed') LIMIT 2", (manifest["producer"]["peer_id"],)).fetchall()
            if len(predecessors) > 1: raise GenerationError("consumed generation lineage has multiple tips")
            expected_parent = predecessors[0][0] if predecessors else None
            if manifest["parent_generation_id"] != expected_parent:
                raise GenerationError("generation lineage is stale or unrelated")
            # Causal state is applied first. Complete-envelope helpers retain
            # item-level conflict rules while avoiding the legacy global guard.
            for operation in causal["operations"]:
                causal_delete_exchange.apply_operation(db, operation)
            import_exercise_profile_state.apply_profile_state(db, profile, allow_pending=True)
            import_equipment_definitions.apply_definitions(db, definitions, complete_causal_envelope=True)
            import_mobile_export.apply_payload(db, history)
            import_exercise_profile_state.apply_profile_state(db, profile)
            import_exercise_aliases.apply_aliases(db, aliases)
            import_equipment_associations.apply_associations(db, association_payload, reserved, mobile_occurrences)
            import_exercise_body_zones.apply_body_zones(db, parsed_zones, {}, complete_causal_envelope=True)
            import_training_feedback.apply_feedback(db, feedback, complete_causal_envelope=True)
            execution_draft_exchange.import_document(db, drafts, own_transaction=False)
            ack = record_consumed(db, manifest, manifest_digest)
            db.commit()
            return ack
        except Exception as error:
            db.rollback()
            if isinstance(error, sqlite3.Error):
                raise
            return record_rejected(database, manifest, manifest_digest, str(error))


def validate_ack(raw: bytes, pending: sqlite3.Row | tuple) -> dict:
    value = strict_json(raw, MAX_MANIFEST)
    keys = {"format", "version", "ack_id", "run_id", "generation_id", "producer_peer_id",
            "consumer_peer_id", "manifest_sha256", "result", "durability", "consumed_at",
            "diagnostic", "payload_sha256"}
    if not isinstance(value, dict) or set(value) != keys or value["format"] != ACK or type(value["version"]) is not int or value["version"] != 1:
        raise GenerationError("invalid ACK")
    if not valid_id(value["generation_id"], "gen") or value["ack_id"] != "ack_" + value["generation_id"][4:] or not valid_id(value["run_id"], "sy") or not valid_id(value["producer_peer_id"], "peer") or not valid_id(value["consumer_peer_id"], "peer") or not isinstance(value["manifest_sha256"], str) or not HEX.fullmatch(value["manifest_sha256"]) or not isinstance(value["diagnostic"], str):
        raise GenerationError("invalid ACK identity/field")
    try:
        parsed = dt.datetime.fromisoformat(value["consumed_at"].replace("Z", "+00:00"))
        if parsed.utcoffset() is None: raise ValueError
    except Exception as error:
        raise GenerationError("invalid ACK timestamp") from error
    checksum = value["payload_sha256"]
    payload = dict(value); del payload["payload_sha256"]
    if not HEX.fullmatch(checksum or "") or digest_bytes(canonical(payload)) != checksum:
        raise GenerationError("ACK digest mismatch")
    expected = (pending[1], pending[0], pending[2], pending[3], pending[4])
    actual = (value["run_id"], value["generation_id"], value["producer_peer_id"],
              value["consumer_peer_id"], value["manifest_sha256"])
    expected_durability = "sqlite-commit-full" if value["result"] == "consumed" else "sqlite-commit-rejection"
    if actual != expected or value["result"] not in ("consumed", "rejected") or value["durability"] != expected_durability or len(value["diagnostic"].encode()) > MAX_DIAGNOSTIC:
        raise GenerationError("ACK does not match pending generation")
    return value


def accept_ack(database: Path, path: Path) -> str:
    raw = path.read_bytes()
    with closing(connect_database(database)) as db:
        require_schema(db); db.execute("BEGIN IMMEDIATE")
        value = strict_json(raw, MAX_MANIFEST)
        row = db.execute("SELECT generation_id,run_id,producer_peer_id,consumer_peer_id,manifest_sha256,status FROM sync_generations WHERE generation_id=?", (value.get("generation_id"),)).fetchone()
        if not row:
            raise GenerationError("ACK has no pending generation")
        ack = validate_ack(raw, row)
        status = "acknowledged" if ack["result"] == "consumed" else "rejected"
        db.execute(
            "INSERT OR IGNORE INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)",
            (ack["ack_id"], ack["generation_id"], ack["run_id"],
             ack["producer_peer_id"], ack["consumer_peer_id"],
             ack["manifest_sha256"], ack["result"], ack["durability"],
             ack["consumed_at"], ack["diagnostic"], ack["payload_sha256"]),
        )
        stored = db.execute(
            "SELECT payload_sha256 FROM sync_acknowledgements WHERE ack_id=?",
            (ack["ack_id"],),
        ).fetchone()
        if stored != (ack["payload_sha256"],):
            raise GenerationError("conflicting ACK replay")
        if row[5] in ("acknowledged", "rejected"):
            if row[5] != status:
                raise GenerationError("conflicting ACK replay")
            db.commit(); return "unchanged"
        if row[5] != "waiting_acknowledgement":
            raise GenerationError("generation is not awaiting ACK")
        db.execute("UPDATE sync_generations SET status=?,acknowledged_at=? WHERE generation_id=?",
                   (status, ack["consumed_at"], ack["generation_id"]))
        if status == "acknowledged":
            db.execute(
                "UPDATE session_preparation_deliveries SET state='acknowledged',acknowledged_at=? "
                "WHERE generation_id=? AND state IN('pending','remote_unknown')",
                (ack["consumed_at"], ack["generation_id"]),
            )
            db.execute(
                "UPDATE session_preparation_withdrawals SET acknowledged_at=? "
                "WHERE generation_id=? AND acknowledged_at IS NULL",
                (ack["consumed_at"], ack["generation_id"]),
            )
            db.execute(
                "UPDATE session_preparations SET delivery_state='acknowledged' WHERE EXISTS("
                "SELECT 1 FROM session_preparation_deliveries d WHERE "
                "d.preparation_id=session_preparations.preparation_id AND "
                "d.revision_id=session_preparations.current_revision_id AND "
                "d.generation_id=? AND d.state='acknowledged')",
                (ack["generation_id"],),
            )
        db.commit(); return status


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    capture = sub.add_parser("capture-desktop"); capture.add_argument("--database", type=Path, required=True); capture.add_argument("--owned-root", type=Path, required=True); capture.add_argument("--consumer-peer", required=True); capture.add_argument("--run-id"); capture.add_argument("--generation-id")
    publication = sub.add_parser("publish"); publication.add_argument("--database", type=Path, required=True); publication.add_argument("--generation-id", required=True); publication.add_argument("--object-root", type=Path, required=True)
    validate = sub.add_parser("validate"); validate.add_argument("directory", type=Path); validate.add_argument("--consumer-peer", required=True)
    consume = sub.add_parser("consume-desktop"); consume.add_argument("directory", type=Path); consume.add_argument("--database", type=Path, required=True); consume.add_argument("--ack-output", type=Path, required=True)
    peer = sub.add_parser("peer-id"); peer.add_argument("--database", type=Path, required=True); peer.add_argument("--kind", choices=("desktop",), default="desktop")
    accept = sub.add_parser("accept-ack"); accept.add_argument("ack", type=Path); accept.add_argument("--database", type=Path, required=True)
    args = parser.parse_args()
    if args.mode == "capture-desktop":
        stage, manifest, checksum = capture_desktop(args.database, args.owned_root,
            args.consumer_peer, args.run_id, args.generation_id)
        print(f"GENERATION_CAPTURE=PASS generation_id={manifest['generation_id']} manifest_sha256={checksum} path={stage}")
    elif args.mode == "publish":
        print("GENERATION_PUBLISH=PASS path=" + str(publish(args.database, args.generation_id, args.object_root)))
    elif args.mode == "validate":
        manifest, checksum = validate_published(args.directory, args.consumer_peer)
        print(f"GENERATION_VALIDATE=PASS generation_id={manifest['generation_id']} manifest_sha256={checksum}")
    elif args.mode == "consume-desktop":
        ack = consume_desktop(args.database, args.directory)
        args.ack_output.write_bytes(canonical(ack))
        print("GENERATION_CONSUME=PASS generation_id=" + ack["generation_id"])
    elif args.mode == "peer-id":
        with closing(connect_database(args.database)) as db:
            value = peer_identity(db, args.kind); db.commit()
        print("PEER_ID=" + value)
    else:
        print("ACK_ACCEPT=" + accept_ack(args.database, args.ack))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("SYNC_GENERATION=FAIL " + str(error), file=sys.stderr)
        raise SystemExit(1)
