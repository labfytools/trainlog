#!/usr/bin/env python3
"""Export the bounded, direction-neutral TRAINING_FEEDBACK_V2 companion."""
import argparse, datetime, json, sqlite3
from pathlib import Path

def instant(value): return datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))

def main():
    parser=argparse.ArgumentParser(); parser.add_argument("output",type=Path); parser.add_argument("--database",type=Path,required=True); args=parser.parse_args()
    db=sqlite3.connect(args.database)
    try:
        if db.execute("PRAGMA user_version").fetchone()[0] != 15: raise ValueError("schema desktop v15 requis")
        roots=db.execute("SELECT f.feedback_id,s.session_id,se.entry_id,e.exercise_id,f.observed_at FROM exercise_feedback f JOIN session_exercises se ON se.id=f.session_exercise_row_id JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id").fetchall()
        followups=db.execute("SELECT f.followup_id,s.session_id,f.observed_at FROM session_followups f JOIN sessions s ON s.id=f.session_row_id").fetchall()
        if len(roots)>4096 or len(followups)>4096: raise ValueError("trop d'observations")
        exercise=[]
        for row in roots:
            revisions=db.execute("SELECT revision_id,created_at,raw_text FROM exercise_feedback_revisions WHERE feedback_id=?",(row[0],)).fetchall()
            revisions.sort(key=lambda r:(instant(r[1]),r[0].encode()))
            exercise.append(dict(zip(("feedback_id","session_id","entry_id","exercise_id","observed_at"),row)) | {"revisions":[dict(zip(("revision_id","created_at","raw_text"),r)) for r in revisions]})
        result_followups=[]
        for row in followups:
            revisions=db.execute("SELECT revision_id,created_at,raw_text FROM session_followup_revisions WHERE followup_id=?",(row[0],)).fetchall()
            revisions.sort(key=lambda r:(instant(r[1]),r[0].encode()))
            result_followups.append(dict(zip(("followup_id","session_id","observed_at"),row)) | {"revisions":[dict(zip(("revision_id","created_at","raw_text"),r)) for r in revisions]})
        exercise.sort(key=lambda v:(instant(v["observed_at"]),v["feedback_id"].encode())); result_followups.sort(key=lambda v:(instant(v["observed_at"]),v["followup_id"].encode()))
        payload={"format":"trainlog-training-feedback","version":2,"generated_at":datetime.datetime.now().astimezone().isoformat(),"exercise_feedback":exercise,"session_followups":result_followups}
        args.output.write_text(json.dumps(payload,ensure_ascii=False,separators=(",",":")),encoding="utf-8")
    finally: db.close()
    print(f"TRAINING_FEEDBACK_EXPORT=PASS version=2 exercise={len(exercise)} followups={len(result_followups)}")
if __name__=="__main__": main()
