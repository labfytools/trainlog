#!/usr/bin/env python3
"""Export mutable current exercise profiles with deterministic ancestry."""
import argparse
import json
import sqlite3
import re
from datetime import datetime
from pathlib import Path
from trainlog_sqlite import connect_database, profile_revision

REVISION_RE=re.compile(r"^pr2_[0-9a-f]{8}-[0-9a-f]{4}-5[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")

def valid_profile(recording, tracking, fields):
    return recording in ("sets","continuous") and tracking in ("reps","duration") and \
        not (recording=="continuous" and tracking!="duration") and type(fields) is int and \
        0<=fields<=3 and not (recording=="sets" and fields!=0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--database", required=True, type=Path)
    args = parser.parse_args()
    con = connect_database(args.database)
    con.row_factory = sqlite3.Row
    try:
        version = con.execute("PRAGMA user_version").fetchone()[0]
        if version not in (17, 18, 19, 20, 21, 22):
            raise SystemExit(f"EXERCISE_PROFILE_STATE_EXPORT=FAIL schema={version}")
        rows = con.execute("""
            SELECT e.exercise_id,e.recording_mode,e.tracking_mode,e.data_fields,
              e.load_semantics,e.machine_variant,e.machine_provenance,
              e.scientific_profile_id,e.science_state,e.legacy_equipment_id,
              s.revision_id,s.parent_revision_id,s.legacy_seed
            FROM exercises e JOIN exercise_profile_state s ON s.exercise_row_id=e.id
            ORDER BY e.exercise_id
        """).fetchall()
        exercises=[]
        for row in rows:
            # exercise_row_id is numeric; resolve it without coupling JSON identity to SQLite row IDs.
            exercise_row_id=con.execute("SELECT id FROM exercises WHERE exercise_id=?",(row["exercise_id"],)).fetchone()[0]
            history=con.execute("""WITH RECURSIVE lineage(revision_id,parent_revision_id,recording_mode,tracking_mode,data_fields,legacy_seed,level) AS (
              SELECT revision_id,parent_revision_id,recording_mode,tracking_mode,data_fields,legacy_seed,0 FROM exercise_profile_revisions WHERE exercise_row_id=? AND revision_id=?
              UNION ALL SELECT r.revision_id,r.parent_revision_id,r.recording_mode,r.tracking_mode,r.data_fields,r.legacy_seed,lineage.level+1 FROM exercise_profile_revisions r JOIN lineage ON r.revision_id=lineage.parent_revision_id WHERE r.exercise_row_id=? AND lineage.level<31)
              SELECT revision_id,parent_revision_id,recording_mode,tracking_mode,data_fields,legacy_seed FROM lineage ORDER BY level DESC""",
              (exercise_row_id,row["revision_id"],exercise_row_id)).fetchall()
            if not history or history[-1]["revision_id"] != row["revision_id"] or len(history)>32:
                raise SystemExit("EXERCISE_PROFILE_STATE_EXPORT=FAIL invalid/bounded history")
            previous=None
            for index, revision in enumerate(history):
                rid=revision["revision_id"];parent=revision["parent_revision_id"]
                recording=revision["recording_mode"];tracking=revision["tracking_mode"]
                fields=revision["data_fields"];legacy=revision["legacy_seed"]
                valid = valid_profile(recording,tracking,fields) and (
                    index==0 and legacy==1 and rid=="pr_legacy_v1" and parent is None or
                    index>0 and legacy==0 and isinstance(parent,str) and parent==previous and
                    isinstance(rid,str) and REVISION_RE.fullmatch(rid) and
                    rid==profile_revision(parent,recording,tracking,fields))
                if not valid:
                    raise SystemExit("EXERCISE_PROFILE_STATE_EXPORT=FAIL invalid rooted history")
                previous=rid
            if (history[-1]["parent_revision_id"]!=row["parent_revision_id"] or
                history[-1]["legacy_seed"]!=row["legacy_seed"] or
                history[-1]["recording_mode"]!=row["recording_mode"] or
                history[-1]["tracking_mode"]!=row["tracking_mode"] or
                history[-1]["data_fields"]!=row["data_fields"]):
                raise SystemExit("EXERCISE_PROFILE_STATE_EXPORT=FAIL history tip mismatch")
            exercises.append(dict(row) | {"legacy_seed":bool(row["legacy_seed"]),"history":[dict(h)|{"legacy_seed":bool(h["legacy_seed"])} for h in history]})
        payload = {"format":"trainlog-exercise-profile-state","version":1,
            "generated_at":datetime.now().astimezone().isoformat(),"exercises":exercises}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(payload,ensure_ascii=False,separators=(",",":")),encoding="utf-8")
        print(f"EXERCISE_PROFILE_STATE_EXPORT=PASS exercises={len(rows)}")
    finally:
        con.close()

if __name__ == "__main__": main()
