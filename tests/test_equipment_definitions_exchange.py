"""Strict additive custom-equipment definitions-v1 integration test."""
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPORTER = ROOT / "tools/export_equipment_definitions.py"
IMPORTER = ROOT / "tools/import_equipment_definitions.py"


def database(path):
    connection = sqlite3.connect(path)
    connection.execute("CREATE TABLE custom_equipment(equipment_id TEXT PRIMARY KEY,display_name TEXT NOT NULL,label_name TEXT NOT NULL,equipment_type TEXT NOT NULL,load_semantics TEXT NOT NULL)")
    connection.execute("PRAGMA user_version=8")
    connection.commit()
    return connection


def run(tool, artifact, db):
    return subprocess.run([sys.executable, tool, artifact, "--database", db], text=True, capture_output=True)


def main():
    with tempfile.TemporaryDirectory(prefix="trainlog-definitions-") as temporary:
        root = Path(temporary)
        source, target, artifact = root / "source.db", root / "target.db", root / "definitions.json"
        connection = database(source)
        connection.execute("INSERT INTO custom_equipment VALUES(?,?,?,?,?)",
                           ("eq_custom", "Presse voyage", "Presse", "custom_machine", "external"))
        connection.commit(); connection.close()
        database(target).close()

        result = run(EXPORTER, artifact, source)
        assert result.returncode == 0, result.stdout + result.stderr
        payload = json.loads(artifact.read_text())
        assert payload["format"] == "trainlog-equipment-definitions" and payload["version"] == 1
        assert len(payload["equipment"]) == 1
        first = run(IMPORTER, artifact, target)
        assert first.returncode == 0 and "definitions_imported=1" in first.stdout
        second = run(IMPORTER, artifact, target)
        assert second.returncode == 0 and "definitions_skipped=1" in second.stdout

        # Missing from the next snapshot is not a deletion request.
        payload["equipment"] = []
        artifact.write_text(json.dumps(payload))
        assert run(IMPORTER, artifact, target).returncode == 0
        assert sqlite3.connect(target).execute("SELECT count(*) FROM custom_equipment").fetchone()[0] == 1

        # Divergent same-ID content conflicts without changing the local row.
        payload["equipment"] = [{"equipment_id": "eq_custom", "display_name": "Autre",
            "label_name": "Presse", "equipment_type": "custom_machine", "load_semantics": "external"}]
        artifact.write_text(json.dumps(payload))
        conflict = run(IMPORTER, artifact, target)
        assert conflict.returncode != 0 and "conflit définition" in conflict.stdout
        assert sqlite3.connect(target).execute("SELECT display_name FROM custom_equipment").fetchone()[0] == "Presse voyage"

        # Supplied-manifest identities cannot be overridden by the companion.
        payload["equipment"][0]["equipment_id"] = "leg_press"
        artifact.write_text(json.dumps(payload))
        reserved = run(IMPORTER, artifact, target)
        assert reserved.returncode != 0 and "réservé" in reserved.stdout
    print("PASS equipment definitions exchange")


if __name__ == "__main__": main()
