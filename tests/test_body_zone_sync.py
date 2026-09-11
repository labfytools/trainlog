#!/usr/bin/env python3
"""Desktop body-zone companion replay, one-sided update and conflict regression."""

import json
import sqlite3
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPORT = ROOT / "tools/export_exercise_body_zones.py"
IMPORT = ROOT / "tools/import_exercise_body_zones.py"
EXERCISE_ID = "ex_11111111-1111-4111-8111-111111111111"


def database(path, exercise_id=EXERCISE_ID, with_mapping=True, with_baseline=True):
    connection = sqlite3.connect(path)
    connection.executescript(
        """
        PRAGMA foreign_keys=ON;
        CREATE TABLE exercises(
          id INTEGER PRIMARY KEY,exercise_id TEXT NOT NULL UNIQUE,
          name TEXT NOT NULL,normalized_name TEXT NOT NULL UNIQUE,
          recording_mode TEXT NOT NULL,tracking_mode TEXT NOT NULL,data_fields INTEGER NOT NULL);
        CREATE TABLE exercise_body_zones(
          exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE CASCADE,
          zone_id TEXT NOT NULL,
          role TEXT NOT NULL CHECK(role IN('primary','secondary')),
          PRIMARY KEY(exercise_row_id,zone_id));
        CREATE UNIQUE INDEX exercise_body_zones_one_primary
          ON exercise_body_zones(exercise_row_id) WHERE role='primary';
        CREATE TABLE exercise_body_zone_sync(
          exercise_row_id INTEGER PRIMARY KEY REFERENCES exercises(id) ON DELETE CASCADE,
          synced_state TEXT NOT NULL);
        """
    )
    connection.execute(
        "INSERT INTO exercises VALUES(1,?,'Alias exercise','alias exercise','sets','reps',0)",
        (exercise_id,),
    )
    if with_mapping:
        connection.executescript("""
            INSERT INTO exercise_body_zones VALUES(1,'chest','primary');
            INSERT INTO exercise_body_zones VALUES(1,'arms','secondary');
        """)
        if with_baseline:
            connection.execute(
                "INSERT INTO exercise_body_zone_sync VALUES(1,'chest|arms')"
            )
    connection.execute("PRAGMA user_version=11")
    connection.commit()
    connection.close()


def run(script, artifact, db, *extra):
    result = subprocess.run(
        ["python3", str(script), str(artifact), "--database", str(db), *map(str, extra)],
        cwd=ROOT, text=True, capture_output=True,
    )
    return result


def mapping(db):
    connection = sqlite3.connect(db)
    rows = connection.execute(
        "SELECT zone_id,role FROM exercise_body_zones ORDER BY role,zone_id",
    ).fetchall()
    baseline = connection.execute("SELECT synced_state FROM exercise_body_zone_sync").fetchone()[0]
    connection.close()
    return rows, baseline


def write_companion(path, entries):
    path.write_text(json.dumps({
        "format": "trainlog-exercise-body-zones", "version": 1,
        "generated_at": "2032-01-01T00:00:00+00:00",
        "exercises": entries,
    }), encoding="utf-8")


def entry(exercise_id, primary="back", secondary=None):
    return {
        "exercise_id": exercise_id,
        "primary_zone_id": primary,
        "secondary_zone_ids": ["arms"] if secondary is None else secondary,
    }


def add_alias(db, source_id, canonical_id):
    connection = sqlite3.connect(db)
    connection.execute(
        "CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY, "
        "canonical_exercise_id TEXT NOT NULL)"
    )
    connection.execute(
        "INSERT INTO exercise_aliases VALUES(?,?)", (source_id, canonical_id)
    )
    connection.execute("PRAGMA user_version=12")
    connection.commit()
    connection.close()


def assert_alias_identity(db, source_id, canonical_id):
    connection = sqlite3.connect(db)
    assert connection.execute(
        "SELECT COUNT(*) FROM exercises WHERE exercise_id=?", (source_id,)
    ).fetchone()[0] == 0
    assert connection.execute(
        "SELECT canonical_exercise_id FROM exercise_aliases WHERE source_exercise_id=?",
        (source_id,),
    ).fetchone() == (canonical_id,)
    assert connection.execute(
        "SELECT COUNT(*) FROM exercise_body_zones"
    ).fetchone()[0] == 2
    connection.close()


def main():
    with tempfile.TemporaryDirectory(prefix="trainlog-body-zone-sync-") as temporary:
        root = Path(temporary)
        source = root / "source.db"
        target = root / "target.db"
        artifact = root / "trainlog-exercise-body-zones-v1.json"
        database(source)
        database(target)

        exported = run(EXPORT, artifact, source)
        assert exported.returncode == 0, exported.stdout + exported.stderr
        replay = run(IMPORT, artifact, target)
        assert replay.returncode == 0 and "zones_skipped=1" in replay.stdout, replay.stdout

        payload = json.loads(artifact.read_text(encoding="utf-8"))
        payload["exercises"][0]["primary_zone_id"] = "shoulders"
        artifact.write_text(json.dumps(payload), encoding="utf-8")
        update = run(IMPORT, artifact, target)
        assert update.returncode == 0 and "zones_updated=1" in update.stdout, update.stdout
        assert mapping(target) == ([('shoulders', 'primary'), ('arms', 'secondary')], "shoulders|arms")

        connection = sqlite3.connect(target)
        connection.execute("DELETE FROM exercise_body_zones WHERE exercise_row_id=1")
        connection.execute("INSERT INTO exercise_body_zones VALUES(1,'back','primary')")
        connection.execute("INSERT INTO exercise_body_zones VALUES(1,'arms','secondary')")
        connection.commit()
        connection.close()
        payload["exercises"][0]["primary_zone_id"] = "chest"
        artifact.write_text(json.dumps(payload), encoding="utf-8")
        conflict = run(IMPORT, artifact, target)
        assert conflict.returncode == 1
        assert f"conflit zones simultané: {EXERCISE_ID}" in conflict.stdout
        assert mapping(target) == ([('back', 'primary'), ('arms', 'secondary')], "shoulders|arms")

        orphan = json.loads(artifact.read_text(encoding="utf-8"))
        orphan["exercises"][0]["primary_zone_id"] = None
        orphan["exercises"][0]["secondary_zone_ids"] = ["arms"]
        artifact.write_text(json.dumps(orphan), encoding="utf-8")
        rejected_orphan = run(IMPORT, artifact, target)
        assert rejected_orphan.returncode == 1
        assert "secondaires sans zone principale" in rejected_orphan.stdout
        assert mapping(target) == ([('back', 'primary'), ('arms', 'secondary')], "shoulders|arms")

        invalid_identity = json.loads(artifact.read_text(encoding="utf-8"))
        invalid_identity["exercises"][0]["primary_zone_id"] = "back"
        invalid_identity["exercises"][0]["secondary_zone_ids"] = []
        invalid_identity["exercises"][0]["exercise_id"] = "ex_not-a-uuid"
        artifact.write_text(json.dumps(invalid_identity), encoding="utf-8")
        assert run(IMPORT, artifact, target).returncode == 1

        invalid_time = json.loads(json.dumps(invalid_identity))
        invalid_time["exercises"][0]["exercise_id"] = EXERCISE_ID
        invalid_time["generated_at"] = "2032-01-01T00:00:00"
        artifact.write_text(json.dumps(invalid_time), encoding="utf-8")
        invalid_time_result = run(IMPORT, artifact, target)
        assert invalid_time_result.returncode == 1
        assert "sans offset" in invalid_time_result.stdout

        corrupt_db = sqlite3.connect(target)
        corrupt_db.execute(
            "DELETE FROM exercise_body_zones WHERE role='primary'"
        )
        corrupt_db.commit()
        corrupt_db.close()
        rejected_export = run(EXPORT, artifact, target)
        assert rejected_export.returncode == 1
        assert "relations de zones SQLite invalides" in rejected_export.stdout
        corrupt_db = sqlite3.connect(target)
        corrupt_db.execute(
            "INSERT INTO exercise_body_zones VALUES(1,'back','primary')"
        )
        corrupt_db.commit()
        corrupt_db.close()

        payload["exercises"][0]["primary_zone_id"] = None
        payload["exercises"][0]["secondary_zone_ids"] = []
        artifact.write_text(json.dumps(payload), encoding="utf-8")
        connection = sqlite3.connect(source)
        connection.execute("DELETE FROM exercise_body_zones")
        connection.execute("UPDATE exercise_body_zone_sync SET synced_state='|'")
        connection.commit(); connection.close()
        no_mapping = run(IMPORT, artifact, source)
        assert no_mapping.returncode == 0 and "zones_skipped=1" in no_mapping.stdout

        # import_mobile_export may have safely coalesced a remote creator ID
        # into the normalized local identity before this companion arrives.
        # The retained source V2 definition is the required proof; name alone
        # is never accepted by the zone importer.
        alias_db = root / "alias.db"
        local_id = "ex_22222222-2222-4222-8222-222222222222"
        remote_id = "ex_33333333-3333-4333-8333-333333333333"
        database(alias_db, local_id, with_mapping=False)
        alias_artifact = root / "alias-zones.json"
        write_companion(alias_artifact, [entry(remote_id)])
        proof = root / "proof.json"
        proof.write_text(json.dumps({
            "format": "trainlog-mobile-export", "version": 2,
            "generated_at": "2032-01-01T00:00:00+00:00", "exercises": [{
                "exercise_id": remote_id, "name": "Alias exercise",
                "recording_mode": "sets", "tracking_mode": "reps", "data_fields": 0,
            }], "sessions": [], "body_observations": [],
        }), encoding="utf-8")
        # Explicitly retained V2 evidence coalesces source+canonical claims by
        # the same exact-payload rule as a durable alias.
        write_companion(alias_artifact, [entry(remote_id), entry(local_id)])
        alias_import = run(
            IMPORT, alias_artifact, alias_db, "--mobile-export", proof,
        )
        assert alias_import.returncode == 0 and "zones_updated=1" in alias_import.stdout, (
            alias_import.stdout + alias_import.stderr
        )
        assert mapping(alias_db) == ([('back', 'primary'), ('arms', 'secondary')], "back|arms")

        # Once EXERCISE_MERGE_V1 has persisted the retired creator identity,
        # the companion resolves it without requiring a mobile snapshot/name
        # proof. This is the normal post-merge transit order.
        persistent_alias_db = root / "persistent-alias.db"
        database(persistent_alias_db, local_id, with_mapping=False)
        add_alias(persistent_alias_db, remote_id, local_id)
        write_companion(alias_artifact, [entry(remote_id), entry(local_id)])
        persistent_alias_import = run(IMPORT, alias_artifact, persistent_alias_db)
        assert persistent_alias_import.returncode == 0 and \
                "zones_updated=1" in persistent_alias_import.stdout, (
            persistent_alias_import.stdout + persistent_alias_import.stderr
        )
        assert mapping(persistent_alias_db) == (
            [('back', 'primary'), ('arms', 'secondary')], "back|arms"
        )
        assert_alias_identity(persistent_alias_db, remote_id, local_id)
        persistent_replay = run(IMPORT, alias_artifact, persistent_alias_db)
        assert persistent_replay.returncode == 0 and \
            "zones_skipped=1" in persistent_replay.stdout, persistent_replay.stdout
        assert_alias_identity(persistent_alias_db, remote_id, local_id)

        # Full canonical groups must agree exactly. In particular, secondary
        # claims are never unioned because that would invent direct relations.
        conflict_cases = [
            ("complementary-secondary", entry(remote_id, "back", ["arms"]),
             entry(local_id, "back", ["shoulders"])),
            ("primary-conflict", entry(remote_id, "back", []),
             entry(local_id, "chest", [])),
            ("role-conflict", entry(remote_id, "back", ["arms"]),
             entry(local_id, "arms", ["back"])),
            ("classified-unclassified", entry(remote_id, "back", []),
             entry(local_id, None, [])),
        ]
        for label, source_entry, canonical_entry in conflict_cases:
            conflict_db = root / f"{label}.db"
            database(conflict_db, local_id)
            add_alias(conflict_db, remote_id, local_id)
            before = mapping(conflict_db)
            write_companion(alias_artifact, [source_entry, canonical_entry])
            result = run(IMPORT, alias_artifact, conflict_db)
            assert result.returncode == 1 and \
                "incompatibles après résolution d'alias" in result.stdout, result.stdout
            assert mapping(conflict_db) == before
            assert_alias_identity(conflict_db, remote_id, local_id)

        # An unresolved source cannot borrow the known canonical entry as
        # identity proof, and the earlier canonical claim remains unmodified.
        unknown_db = root / "unknown-group.db"
        database(unknown_db, local_id)
        before = mapping(unknown_db)
        write_companion(alias_artifact, [
            entry(local_id, "back", ["shoulders"]), entry(remote_id),
        ])
        unknown = run(IMPORT, alias_artifact, unknown_db)
        assert unknown.returncode == 1 and "exercice inconnu" in unknown.stdout
        assert mapping(unknown_db) == before

        # A custom exercise starts with no baseline on its creator. Publishing
        # the exact snapshot acknowledges it locally; a later peer-only edit
        # must then flow back instead of becoming a false simultaneous conflict.
        creator = root / "creator.db"
        peer = root / "peer.db"
        shared = root / "shared.json"
        database(creator, with_mapping=True, with_baseline=False)
        database(peer, with_mapping=False)
        assert run(EXPORT, shared, creator).returncode == 0
        assert run(IMPORT, shared, creator).returncode == 0  # publication ack
        assert run(IMPORT, shared, peer).returncode == 0
        peer_db = sqlite3.connect(peer)
        peer_db.execute("DELETE FROM exercise_body_zones")
        peer_db.execute("INSERT INTO exercise_body_zones VALUES(1,'shoulders','primary')")
        peer_db.execute("INSERT INTO exercise_body_zones VALUES(1,'arms','secondary')")
        peer_db.commit()
        peer_db.close()
        assert run(EXPORT, shared, peer).returncode == 0
        assert run(IMPORT, shared, peer).returncode == 0  # publication ack
        returned = run(IMPORT, shared, creator)
        assert returned.returncode == 0 and "zones_updated=1" in returned.stdout
        assert mapping(creator) == (
            [('shoulders', 'primary'), ('arms', 'secondary')], "shoulders|arms"
        )


if __name__ == "__main__":
    main()
