"""Exercise the real desktop companion exporter/importer on temporary DBs."""
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


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
