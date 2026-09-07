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
    """)
    db.execute("INSERT INTO exercises VALUES(1,'ex_fixture')")
    db.execute("INSERT INTO sessions VALUES(1,'se_fixture','2026-01-01T00:00:00+00:00')")
    db.execute("INSERT INTO session_exercises VALUES(1,'sxe_fixture',1,1,0,?)", (equipment,))
    db.execute("PRAGMA user_version=7")
    db.commit(); db.close()


def main():
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        source, target, artifact = directory / 'source.db', directory / 'target.db', directory / 'equipment.json'
        make_db(source, 'leg_press'); make_db(target, None)
        subprocess.run([sys.executable, ROOT / 'tools/export_equipment_associations.py', artifact, '--database', source], check=True)
        subprocess.run([sys.executable, ROOT / 'tools/import_equipment_associations.py', artifact, '--database', target], check=True)
        assert sqlite3.connect(target).execute('SELECT equipment_id FROM session_exercises').fetchone()[0] == 'leg_press'
        payload = json.loads(artifact.read_text()); payload['associations'][0] = {'session_id':'se_fixture','entry_id':'sxe_fixture','exercise_id':'ex_fixture','state':'cleared'}
        artifact.write_text(json.dumps(payload))
        subprocess.run([sys.executable, ROOT / 'tools/import_equipment_associations.py', artifact, '--database', target], check=True)
        assert sqlite3.connect(target).execute('SELECT equipment_id FROM session_exercises').fetchone()[0] is None
    print('PASS equipment association exchange')


if __name__ == '__main__': main()
