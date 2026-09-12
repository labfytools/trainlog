"""Exercise the real desktop companion exporter/importer on temporary DBs."""
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ID = "ex_43c7375f-934c-4650-930c-45807d2f2929"
CANONICAL_ID = "ex_2d488c08-194c-4051-a3c9-34471646c1d3"


def make_db(path, equipment):
    db = sqlite3.connect(path)
    db.executescript("""
        CREATE TABLE exercises(id INTEGER PRIMARY KEY, exercise_id TEXT UNIQUE);
        CREATE TABLE sessions(id INTEGER PRIMARY KEY, session_id TEXT UNIQUE, started_at TEXT);
        CREATE TABLE session_exercises(id INTEGER PRIMARY KEY, entry_id TEXT UNIQUE, session_row_id INTEGER, exercise_row_id INTEGER, position INTEGER, equipment_id TEXT);
        CREATE TABLE custom_equipment(equipment_id TEXT PRIMARY KEY, display_name TEXT NOT NULL, label_name TEXT NOT NULL, equipment_type TEXT NOT NULL, load_semantics TEXT NOT NULL);
    """)
    db.execute("INSERT INTO exercises VALUES(1,'ex_fixture')")
    db.execute("INSERT INTO sessions VALUES(1,'se_fixture','2026-01-01T00:00:00+00:00')")
    db.execute("INSERT INTO session_exercises VALUES(1,'sxe_fixture',1,1,0,?)", (equipment,))
    db.execute("PRAGMA user_version=8")
    db.commit(); db.close()


def main():
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        source, target, artifact = directory / 'source.db', directory / 'target.db', directory / 'equipment.json'
        make_db(source, 'leg_press'); make_db(target, 'leg_press')
        subprocess.run([sys.executable, ROOT / 'tools/export_equipment_associations.py', artifact, '--database', source], check=True)
        subprocess.run([sys.executable, ROOT / 'tools/import_equipment_associations.py', artifact, '--database', target], check=True)
        assert sqlite3.connect(target).execute('SELECT equipment_id FROM session_exercises').fetchone()[0] == 'leg_press'
        payload = json.loads(artifact.read_text()); payload['associations'][0] = {'session_id':'se_fixture','entry_id':'sxe_fixture','exercise_id':'ex_fixture','state':'cleared'}
        artifact.write_text(json.dumps(payload))
        conflict = subprocess.run([sys.executable, ROOT / 'tools/import_equipment_associations.py', artifact, '--database', target], text=True, capture_output=True)
        assert conflict.returncode != 0 and 'conflit association' in conflict.stdout
        assert sqlite3.connect(target).execute('SELECT equipment_id FROM session_exercises').fetchone()[0] == 'leg_press'

        # A V2 entry ID is not sufficient identity: its exercise ID must also
        # agree before an otherwise equal association is treated as a replay.
        payload['associations'][0] = {'session_id':'se_fixture','entry_id':'sxe_fixture','exercise_id':'ex_other','state':'set','equipment_id':'leg_press'}
        artifact.write_text(json.dumps(payload))
        exercise_conflict = subprocess.run([sys.executable, ROOT / 'tools/import_equipment_associations.py', artifact, '--database', target], text=True, capture_output=True)
        assert exercise_conflict.returncode != 0 and 'conflit exercice association' in exercise_conflict.stdout
        assert sqlite3.connect(target).execute('SELECT equipment_id FROM session_exercises').fetchone()[0] == 'leg_press'

        # A durable flattened alias is identity evidence before raw-ID
        # comparison. The retired source must corroborate the canonical
        # occurrence without requiring an optional V2 proof snapshot.
        alias_target = directory / 'alias-target.db'
        make_db(alias_target, 'leg_press')
        alias_db = sqlite3.connect(alias_target)
        alias_db.execute("UPDATE exercises SET exercise_id=?", (CANONICAL_ID,))
        alias_db.execute(
            "CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY, "
            "canonical_exercise_id TEXT NOT NULL REFERENCES exercises(exercise_id))"
        )
        alias_db.execute("INSERT INTO exercise_aliases VALUES(?,?)", (SOURCE_ID, CANONICAL_ID))
        alias_db.execute("PRAGMA user_version=12")
        alias_db.commit(); alias_db.close()
        payload['associations'][0] = {
            'session_id': 'se_fixture', 'entry_id': 'sxe_fixture',
            'exercise_id': SOURCE_ID, 'state': 'set', 'equipment_id': 'leg_press',
        }
        artifact.write_text(json.dumps(payload))
        alias_replay = subprocess.run(
            [sys.executable, ROOT / 'tools/import_equipment_associations.py', artifact,
             '--database', alias_target],
            text=True, capture_output=True,
        )
        assert alias_replay.returncode == 0, alias_replay.stdout + alias_replay.stderr
        with sqlite3.connect(alias_target) as alias_db:
            assert alias_db.execute(
                'SELECT exercise_id FROM exercises'
            ).fetchall() == [(CANONICAL_ID,)]
            assert alias_db.execute(
                'SELECT source_exercise_id,canonical_exercise_id FROM exercise_aliases'
            ).fetchall() == [(SOURCE_ID, CANONICAL_ID)]

        # A different live canonical exercise remains a genuine conflict even
        # when some unrelated alias is present.
        with sqlite3.connect(alias_target) as alias_db:
            alias_db.execute("INSERT INTO exercises VALUES(2,'ex_other')")
        payload['associations'][0]['exercise_id'] = 'ex_other'
        artifact.write_text(json.dumps(payload))
        canonical_conflict = subprocess.run(
            [sys.executable, ROOT / 'tools/import_equipment_associations.py', artifact,
             '--database', alias_target],
            text=True, capture_output=True,
        )
        assert canonical_conflict.returncode != 0
        assert 'conflit exercice association' in canonical_conflict.stdout

        # MACHINE_EXERCISE_MODEL_V1: a stale pre-split companion is accepted
        # only for the closed set of completed stable entries and only when the
        # current V3 snapshot proves the exact machine-specific target.
        split_db = directory / 'machine-splits.db'
        db = sqlite3.connect(split_db)
        db.executescript("""
            CREATE TABLE exercises(id INTEGER PRIMARY KEY, exercise_id TEXT UNIQUE);
            CREATE TABLE sessions(id INTEGER PRIMARY KEY, session_id TEXT UNIQUE, started_at TEXT);
            CREATE TABLE session_exercises(id INTEGER PRIMARY KEY, entry_id TEXT UNIQUE,
                session_row_id INTEGER, exercise_row_id INTEGER, position INTEGER, equipment_id TEXT);
            CREATE TABLE custom_equipment(equipment_id TEXT PRIMARY KEY, display_name TEXT NOT NULL,
                label_name TEXT NOT NULL, equipment_type TEXT NOT NULL, load_semantics TEXT NOT NULL);
            CREATE TABLE draft_session_exercises(entry_id TEXT PRIMARY KEY, exercise_id TEXT, equipment_id TEXT);
            PRAGMA user_version=13;
        """)
        plate_old = 'ex_b432623f-bfe9-4daf-a653-60ec7fdffbde'
        plate_new = 'ex_0e26c06f-a458-40a4-be20-4ed219ede30d'
        walk_old = 'ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d'
        treadmill_new = 'ex_f01d2a46-6984-4dec-8934-4d82fca6dfc2'
        seated_leg = 'ex_617007f9-7420-4408-91b9-8ffb77900f13'
        rear_delt = 'ex_4cd2433e-80b1-478a-b8df-487bf6014ce6'
        ids = [plate_new, walk_old, treadmill_new, seated_leg, rear_delt, 'ex_unrelated']
        db.executemany('INSERT INTO exercises VALUES(?,?)', enumerate(ids, 1))
        sessions = [
            ('se_c0d07454-b58b-403e-8b79-744619815fc5', '2026-09-07T19:27:35+02:00'),
            ('se_ac3908d6-8e3e-4ac6-81a6-62dc2c39075a', '2026-09-08T11:25:30+02:00'),
        ]
        db.executemany('INSERT INTO sessions(id,session_id,started_at) VALUES(?,?,?)',
                       [(i, *row) for i, row in enumerate(sessions, 1)])
        row_by_id = {value: index for index, value in enumerate(ids, 1)}
        occurrences = [
            ('sxe_draft_legacy_7', 1, plate_new, 0, 'plate_loaded_leg_press'),
            ('sxe_093c1331-beaa-4b69-91b3-240292709be6', 1, treadmill_new, 1, 'treadmill'),
            ('sxe_draft_legacy_1', 1, walk_old, 2, None),
            ('sxe_draft_legacy_3', 1, seated_leg, 3, 'seated_leg_curl'),
            ('sxe_8b0b1722-7c32-4364-aefa-b4825fe681d7', 2, rear_delt, 0, None),
        ]
        db.executemany(
            'INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,position,equipment_id) VALUES(?,?,?,?,?)',
            [(entry, session, row_by_id[exercise], position, equipment)
             for entry, session, exercise, position, equipment in occurrences])
        db.execute("INSERT INTO draft_session_exercises VALUES('sxe_draft_current',?,?)",
                   (walk_old, 'treadmill'))
        db.commit(); db.close()

        split_artifact = directory / 'stale-machine-associations.json'
        split_payload = {
            'format': 'trainlog-equipment-associations', 'version': 2,
            'generated_at': '2026-09-12T08:05:57+02:00',
            'associations': [
                {'session_id': sessions[0][0], 'entry_id': 'sxe_draft_legacy_7',
                 'exercise_id': plate_old, 'state': 'set', 'equipment_id': 'plate_loaded_leg_press'},
                {'session_id': sessions[0][0], 'entry_id': 'sxe_093c1331-beaa-4b69-91b3-240292709be6',
                 'exercise_id': walk_old, 'state': 'set', 'equipment_id': 'treadmill'},
                {'session_id': sessions[0][0], 'entry_id': 'sxe_draft_legacy_1',
                 'exercise_id': walk_old, 'state': 'cleared'},
                {'session_id': sessions[0][0], 'entry_id': 'sxe_draft_legacy_3',
                 'exercise_id': seated_leg, 'state': 'set', 'equipment_id': 'seated_leg_curl'},
                {'session_id': sessions[1][0], 'entry_id': 'sxe_8b0b1722-7c32-4364-aefa-b4825fe681d7',
                 'exercise_id': rear_delt, 'state': 'cleared'},
            ],
        }
        split_artifact.write_text(json.dumps(split_payload))
        proof = directory / 'current-machine-v3.json'
        proof.write_text(json.dumps({
            'format': 'trainlog-mobile-export', 'version': 3,
            'generated_at': '2026-09-12T09:26:00+02:00', 'exercises': [],
            'body_observations': [], 'sessions': [
                {'session_id': session_id, 'exercises': [
                    {'entry_id': entry, 'exercise_id': exercise}
                    for entry, session, exercise, _position, _equipment in occurrences
                    if session == session_index
                ]}
                for session_index, (session_id, _started) in enumerate(sessions, 1)
            ],
        }))
        for _ in range(2):
            accepted = subprocess.run(
                [sys.executable, ROOT / 'tools/import_equipment_associations.py', split_artifact,
                 '--database', split_db, '--mobile-export', proof],
                text=True, capture_output=True,
            )
            assert accepted.returncode == 0, accepted.stdout + accepted.stderr
        with sqlite3.connect(split_db) as db:
            assert db.execute('SELECT exercise_id,equipment_id FROM draft_session_exercises').fetchone() == (walk_old, 'treadmill')
            assert db.execute('SELECT exercise_id FROM exercises WHERE exercise_id=?', (seated_leg,)).fetchone() == (seated_leg,)
            assert db.execute('SELECT equipment_id FROM session_exercises WHERE entry_id=?',
                              ('sxe_8b0b1722-7c32-4364-aefa-b4825fe681d7',)).fetchone() == (None,)

        conflict_payload = json.loads(split_artifact.read_text())
        conflict_payload['associations'][0]['exercise_id'] = 'ex_unrelated'
        split_artifact.write_text(json.dumps(conflict_payload))
        true_conflict = subprocess.run(
            [sys.executable, ROOT / 'tools/import_equipment_associations.py', split_artifact,
             '--database', split_db, '--mobile-export', proof],
            text=True, capture_output=True,
        )
        assert true_conflict.returncode != 0
        assert 'conflit exercice association' in true_conflict.stdout

        # The importer must validate the entire artifact before applying its
        # first otherwise-valid association. v2 cannot transport custom IDs.
        prevalidation_artifact = directory / 'prevalidation-rejected.json'
        prevalidation_payload = {
            'format': 'trainlog-equipment-associations',
            'version': 2,
            'generated_at': '2026-01-01T00:00:00+00:00',
            'associations': [
                {'session_id': 'se_fixture', 'entry_id': 'sxe_fixture',
                 'exercise_id': 'ex_fixture', 'state': 'set', 'equipment_id': 'leg_press'},
                {'session_id': 'se_later', 'entry_id': 'sxe_later',
                 'exercise_id': 'ex_later', 'state': 'set', 'equipment_id': 'custom_rack'},
            ],
        }
        prevalidation_artifact.write_text(json.dumps(prevalidation_payload))
        rejected = subprocess.run(
            [sys.executable, ROOT / 'tools/import_equipment_associations.py', prevalidation_artifact, '--database', target],
            text=True,
            capture_output=True,
        )
        assert rejected.returncode != 0
        assert 'session_id=se_later' in rejected.stdout
        assert 'entry_id=sxe_later' in rejected.stdout
        assert 'equipment_id=custom_rack' in rejected.stdout
        assert sqlite3.connect(target).execute('SELECT equipment_id FROM session_exercises').fetchone()[0] == 'leg_press'

        # Definition-v1 is ordered before unchanged association-v2, so the
        # exporter admits a locally-defined custom stable ID.
        custom_source, custom_target = directory / 'custom-source.db', directory / 'custom-target.db'
        make_db(custom_source, 'custom_rack'); make_db(custom_target, None)
        custom_db = sqlite3.connect(custom_source)
        custom_db.execute("INSERT INTO custom_equipment VALUES('custom_rack','Rack','Rack','rack','none')")
        custom_db.commit(); custom_db.close()
        custom_artifact = directory / 'custom-equipment.json'
        exported = subprocess.run(
            [sys.executable, ROOT / 'tools/export_equipment_associations.py', custom_artifact, '--database', custom_source],
            text=True,
            capture_output=True,
        )
        assert exported.returncode == 0, exported.stdout + exported.stderr
        assert json.loads(custom_artifact.read_text())['associations'][0]['equipment_id'] == 'custom_rack'
    print('PASS equipment association exchange')


if __name__ == '__main__': main()
