#!/usr/bin/env python3
"""Real desktop migration and append-only feedback companion regression."""
import json,sqlite3,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/"build/tui/trainlog"
def run(*a):return subprocess.run(a,cwd=ROOT,text=True,capture_output=True,check=True)
def main():
 with tempfile.TemporaryDirectory() as td:
  d=Path(td);db=d/"feedback.db"
  con=sqlite3.connect(db);con.executescript("""PRAGMA foreign_keys=ON;
  CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,normalized_name TEXT,tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER,science_state TEXT);
  CREATE TABLE exercise_aliases(source_exercise_id TEXT PRIMARY KEY,canonical_exercise_id TEXT REFERENCES exercises(exercise_id));
  CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,started_at TEXT,ended_at TEXT,session_type TEXT);
  CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT UNIQUE,session_row_id INTEGER REFERENCES sessions(id),exercise_row_id INTEGER REFERENCES exercises(id),recording_mode TEXT,data_fields INTEGER,position INTEGER,load_mode TEXT,rest_seconds INTEGER);
  CREATE TABLE exercise_feedback(id INTEGER PRIMARY KEY,feedback_id TEXT UNIQUE,session_exercise_row_id INTEGER REFERENCES session_exercises(id),observed_at TEXT,raw_text TEXT);
  CREATE TABLE session_followups(id INTEGER PRIMARY KEY,followup_id TEXT UNIQUE,session_row_id INTEGER REFERENCES sessions(id),observed_at TEXT,raw_text TEXT);
  CREATE TABLE exercise_feedback_revisions(revision_id TEXT PRIMARY KEY,feedback_id TEXT REFERENCES exercise_feedback(feedback_id) ON DELETE CASCADE,created_at TEXT,raw_text TEXT);
  CREATE TABLE session_followup_revisions(revision_id TEXT PRIMARY KEY,followup_id TEXT REFERENCES session_followups(followup_id) ON DELETE CASCADE,created_at TEXT,raw_text TEXT);
  PRAGMA user_version=15;""");assert con.execute("pragma user_version").fetchone()[0]==15
  con.execute("insert into exercises(exercise_id,name,normalized_name,tracking_mode,recording_mode,data_fields,science_state) values('ex_11111111-1111-4111-8111-111111111111','Plate Loaded Leg Press','fixture press','reps','sets',0,'unresolved')")
  con.execute("insert into sessions(session_id,started_at,ended_at,session_type) values('se_11111111-1111-4111-8111-111111111111','2026-09-01T09:00:00+02:00','2026-09-01T10:00:00+02:00','training')")
  con.execute("insert into session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,data_fields,position,load_mode,rest_seconds) values('sxe_fixture',(select id from sessions),(select id from exercises),'sets',0,0,'none',0)");con.commit();con.close()
  p={"format":"trainlog-training-feedback","version":1,"generated_at":"2026-09-01T18:00:00+02:00","exercise_feedback":[{"feedback_id":"fb_22222222-2222-4222-8222-222222222222","session_id":"se_11111111-1111-4111-8111-111111111111","entry_id":"sxe_fixture","exercise_id":"ex_11111111-1111-4111-8111-111111111111","observed_at":"2026-09-01T10:01:00+02:00","raw_text":"ressenti synthétique"}],"session_followups":[{"followup_id":"fu_33333333-3333-4333-8333-333333333333","session_id":"se_11111111-1111-4111-8111-111111111111","observed_at":"2026-09-01T18:00:00+02:00","raw_text":"suivi A"}]};artifact=d/"in.json";artifact.write_text(json.dumps(p),encoding="utf8")
  assert "PASS" in run("python",ROOT/"tools/import_training_feedback.py",artifact,"--database",db).stdout
  assert "roots_skipped=2" in run("python",ROOT/"tools/import_training_feedback.py",artifact,"--database",db).stdout
  p["session_followups"].append({"followup_id":"fu_44444444-4444-4444-8444-444444444444","session_id":p["session_followups"][0]["session_id"],"observed_at":"2026-09-02T09:00:00+02:00","raw_text":"suivi B"});artifact.write_text(json.dumps(p),encoding="utf8");run("python",ROOT/"tools/import_training_feedback.py",artifact,"--database",db)
  out=d/"out.json";run("python",ROOT/"tools/export_training_feedback.py",out,"--database",db);x=json.loads(out.read_text());assert x["version"]==2 and len(x["exercise_feedback"])==1 and len(x["session_followups"])==2
  revision={"revision_id":"fr_55555555-5555-4555-8555-555555555555","created_at":"2026-09-03T10:00:00+02:00","raw_text":"ressenti corrigé"};x["exercise_feedback"][0]["revisions"].append(revision);artifact.write_text(json.dumps(x),encoding="utf8")
  run("python",ROOT/"tools/import_training_feedback.py",artifact,"--database",db);again=run("python",ROOT/"tools/import_training_feedback.py",artifact,"--database",db).stdout;assert "revisions_added=0" in again
  con=sqlite3.connect(db);assert con.execute("select count(*) from exercise_feedback_revisions").fetchone()[0]==2;assert con.execute("select raw_text from exercise_feedback").fetchone()[0]=="ressenti corrigé";con.close()
  x["exercise_feedback"][0]["revisions"][-1]["raw_text"]="conflit";artifact.write_text(json.dumps(x),encoding="utf8");bad=subprocess.run(["python",str(ROOT/"tools/import_training_feedback.py"),str(artifact),"--database",str(db)],capture_output=True);assert bad.returncode!=0
 print("TRAINING_FEEDBACK_SYNC_TEST=PASS")
if __name__=="__main__":main()
