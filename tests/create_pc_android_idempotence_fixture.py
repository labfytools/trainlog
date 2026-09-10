#!/usr/bin/env python3
"""Build a deterministic desktop-v8 fixture through the real PC exporters."""

import sqlite3
import subprocess
import sys
from pathlib import Path

from test_exercise_reconciliation import SCHEMA


ROOT = Path(__file__).resolve().parents[1]
DESKTOP_WALK_ID = "ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d"
LEG_PRESS_ID = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde"
PC_ONLY_ID = "ex_7e7cf906-2214-4066-bcb7-c16382d83b3b"
CUSTOM_EQUIPMENT_ID = "eq_44444444-4444-4444-8444-444444444444"
PC_SESSION_ID = "se_11111111-1111-4111-8111-111111111111"
RICH_WALK_ENTRY_ID = "sxe_22222222-2222-4222-8222-222222222221"
LEGACY_WALK_ENTRY_ID = "sxe_22222222-2222-4222-8222-222222222222"
LEG_PRESS_ENTRY_ID = "sxe_22222222-2222-4222-8222-222222222223"
BODY_OBSERVATION_ID = "bo_33333333-3333-4333-8333-333333333333"


def run_export(tool, output, database, *extra):
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / tool), str(output),
         "--database", str(database), *extra],
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stdout + result.stderr)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: create_pc_android_idempotence_fixture.py OUTPUT_DIR")
    output_directory = Path(sys.argv[1])
    output_directory.mkdir(parents=True, exist_ok=True)
    database = output_directory / "pc-v8.db"

    connection = sqlite3.connect(database)
    connection.executescript(SCHEMA)
    connection.execute("PRAGMA user_version=11")
    connection.executemany(
        "INSERT INTO exercises(exercise_id,name,normalized_name,tracking_mode,"
        "recording_mode,data_fields) VALUES(?,?,?,?,?,?);",
        (
            (DESKTOP_WALK_ID, "Marche", "marche", "duration", "continuous", 3),
            (LEG_PRESS_ID, "Leg press", "leg press", "reps", "sets", 0),
            (PC_ONLY_ID, "Hip abduction", "hip abduction", "reps", "sets", 0),
        ),
    )
    connection.execute(
        "INSERT INTO custom_equipment(equipment_id,display_name,label_name,"
        "equipment_type,load_semantics) VALUES(?,?,?,?,?);",
        (CUSTOM_EQUIPMENT_ID, "Machine PC custom", "Machine custom",
         "custom_machine", "external"),
    )
    connection.execute(
        "INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,?);",
        (PC_SESSION_ID, "2026-09-08T16:30:00+02:00", "training"),
    )
    session_row_id = connection.execute(
        "SELECT id FROM sessions WHERE session_id=?;", (PC_SESSION_ID,)
    ).fetchone()[0]

    def exercise_row(exercise_id):
        return connection.execute(
            "SELECT id FROM exercises WHERE exercise_id=?;", (exercise_id,)
        ).fetchone()[0]

    first_walk = connection.execute(
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,"
        "recording_mode,data_fields,position,load_mode,rest_seconds,equipment_id) "
        "VALUES(?,?,?,'continuous',3,0,'none',0,?);",
        (RICH_WALK_ENTRY_ID, session_row_id, exercise_row(DESKTOP_WALK_ID),
         CUSTOM_EQUIPMENT_ID),
    ).lastrowid
    connection.execute(
        "INSERT INTO continuous_activity(session_exercise_row_id,duration_seconds,"
        "speed_kmh,distance_km) VALUES(?,?,?,?);",
        (first_walk, 900, 5.5, 1.2),
    )
    second_walk = connection.execute(
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,"
        "recording_mode,data_fields,position,load_mode,rest_seconds,equipment_id) "
        "VALUES(?,?,?,'continuous',1,1,'none',0,?);",
        (LEGACY_WALK_ENTRY_ID, session_row_id, exercise_row(DESKTOP_WALK_ID),
         CUSTOM_EQUIPMENT_ID),
    ).lastrowid
    connection.execute(
        "INSERT INTO continuous_activity(session_exercise_row_id,duration_seconds,"
        "speed_kmh) VALUES(?,?,?);",
        (second_walk, 300, 4.0),
    )
    leg_press = connection.execute(
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,"
        "recording_mode,data_fields,position,load_mode,rest_seconds,equipment_id) "
        "VALUES(?,?,?,'sets',0,2,'none',0,?);",
        (LEG_PRESS_ENTRY_ID, session_row_id, exercise_row(LEG_PRESS_ID),
         CUSTOM_EQUIPMENT_ID),
    ).lastrowid
    connection.executemany(
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) "
        "VALUES(?,?,?,?);",
        ((leg_press, 0, 9, 80.0), (leg_press, 1, 8, 85.0)),
    )
    connection.execute(
        "INSERT INTO body_observations(observation_id,observed_at,body_weight_kg,"
        "waist_cm) VALUES(?,?,?,?);",
        (BODY_OBSERVATION_ID, "2026-09-08T08:00:00+02:00", 83.7, 84.4),
    )
    connection.commit()
    connection.close()

    for tool, filename in (
        ("export_equipment_definitions.py", "trainlog-pc-equipment-definitions-v1.json"),
        ("export_pc_catalog.py", "trainlog-pc-catalog-v1.json"),
        ("export_pc_mobile.py", "trainlog-pc-mobile-export-v2.json"),
        ("export_equipment_associations.py", "trainlog-equipment-associations-v2.json"),
    ):
        run_export(tool, output_directory / filename, database,
                   *(('--version', '2') if tool == 'export_pc_mobile.py' else ()))

    print("PASS pc_android_idempotence_fixture")


if __name__ == "__main__":
    main()
