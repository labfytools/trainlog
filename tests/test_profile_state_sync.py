#!/usr/bin/env python3
import json,sqlite3,subprocess,sys,tempfile,unittest,uuid
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]; IMPORT=ROOT/"tools/import_exercise_profile_state.py";EXPORT=ROOT/"tools/export_exercise_profile_state.py"
sys.path.insert(0,str(ROOT/"tools"))
from trainlog_sqlite import connect_database
PLANche="ex_999fd503-ce89-49f9-979d-c19e6adbe80d"
NS=uuid.UUID("4f4c8ea6-18f6-5e48-9d65-a54a24889149")
def revision(parent,tracking): return "pr2_"+str(uuid.uuid5(NS,f"{parent}\nsets\n{tracking}\n0"))

def make_db(path,tracking="reps",machine=None):
 c=sqlite3.connect(path);c.executescript("""
 CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,normalized_name TEXT,
 tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER,load_semantics TEXT,machine_variant TEXT,
 machine_provenance TEXT,scientific_profile_id TEXT,science_state TEXT,legacy_equipment_id TEXT);
 CREATE TABLE exercise_profile_state(exercise_row_id INTEGER PRIMARY KEY,revision_id TEXT,parent_revision_id TEXT,legacy_seed INTEGER);
 CREATE TABLE exercise_profile_revisions(exercise_row_id INTEGER,revision_id TEXT,parent_revision_id TEXT,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER,legacy_seed INTEGER,PRIMARY KEY(exercise_row_id,revision_id));
 CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,exercise_row_id INTEGER,tracking_mode TEXT,recording_mode TEXT,data_fields INTEGER);
 /* Reproduce the production schema dependency: SQLite resolves this UDF while
    preparing UPDATE exercises even when the authoritative-import guard is false. */
 CREATE TRIGGER exercise_profile_state_update AFTER UPDATE OF recording_mode,tracking_mode,data_fields ON exercises
 WHEN NOT EXISTS(SELECT 1 FROM exercise_profile_state s JOIN exercise_profile_revisions r
   ON r.exercise_row_id=s.exercise_row_id AND r.revision_id=s.revision_id
   WHERE s.exercise_row_id=NEW.id AND r.recording_mode=NEW.recording_mode
   AND r.tracking_mode=NEW.tracking_mode AND r.data_fields=NEW.data_fields)
 BEGIN
   SELECT trainlog_profile_revision((SELECT revision_id FROM exercise_profile_state WHERE exercise_row_id=NEW.id),
     NEW.recording_mode,NEW.tracking_mode,NEW.data_fields);
 END;
 PRAGMA user_version=17;""")
 c.execute("INSERT INTO exercises VALUES(1,?,'Planche droite sol','planche droite sol',?,'sets',0,NULL,?,NULL,NULL,'unresolved',NULL)",(PLANche,tracking,machine))
 c.execute("INSERT INTO exercise_profile_state VALUES(1,'pr_legacy_v1',NULL,1)")
 c.execute("INSERT INTO exercise_profile_revisions VALUES(1,'pr_legacy_v1',NULL,'sets',?,0,1)",(tracking,))
 c.execute("INSERT INTO session_exercises VALUES(1,1,'reps','sets',0)");c.commit();c.close()

def history_record(tracking,revision,parent,legacy=False):
 return {"recording_mode":"sets","tracking_mode":tracking,"data_fields":0,"revision_id":revision,"parent_revision_id":parent,"legacy_seed":legacy}

def artifact(tracking="duration",revision="pr_legacy_v1",parent=None,legacy=True,machine=None,history=None):
 if history is None:
  history=([history_record("duration","pr_legacy_v1",None,True)] if not legacy and parent=="pr_legacy_v1" else [])+[history_record(tracking,revision,parent,legacy)]
 return {"format":"trainlog-exercise-profile-state","version":1,"generated_at":"2026-09-13T10:00:00+02:00","exercises":[{
  "exercise_id":PLANche,"recording_mode":"sets","tracking_mode":tracking,"data_fields":0,
  "load_semantics":None,"machine_variant":machine,"machine_provenance":None,"scientific_profile_id":None,
  "science_state":"unresolved","legacy_equipment_id":None,"revision_id":revision,
  "parent_revision_id":parent,"legacy_seed":legacy,"history":history}]}

class ProfileStateSyncTest(unittest.TestCase):
 def apply(self,db,payload,ok=True):
  p=db.with_suffix('.json');p.write_text(json.dumps(payload),encoding='utf-8')
  r=subprocess.run(["python3",str(IMPORT),str(p),"--database",str(db)],text=True,capture_output=True)
  self.assertEqual(ok,r.returncode==0,r.stdout+r.stderr);return r
 def test_canonical_connection_registers_complete_schema_function_set(self):
  with tempfile.TemporaryDirectory() as d:
   db=connect_database(Path(d)/"configured.db")
   expected=revision("pr_legacy_v1","duration")
   self.assertEqual(expected,db.execute(
    "SELECT trainlog_profile_revision('pr_legacy_v1','sets','duration',0)").fetchone()[0])
   self.assertEqual((1,0),db.execute(
    "SELECT trainlog_pr1_valid('pr1|sets|duration|0','sets','duration',0),"
    "trainlog_pr1_valid('pr1|sets|duration|00','sets','duration',0)").fetchone())
   db.close()
 def test_exact_planche_legacy_repair_preserves_old_snapshot_and_is_idempotent(self):
  with tempfile.TemporaryDirectory() as d:
   db=Path(d)/"pc.db";make_db(db);payload=artifact();first=self.apply(db,payload)
   self.assertIn("EXERCISE_PROFILE_STATE_IMPORT=PASS",first.stdout)
   c=sqlite3.connect(db);before=(
    c.execute("SELECT revision_id,parent_revision_id,legacy_seed FROM exercise_profile_state").fetchone(),
    c.execute("SELECT COUNT(*) FROM exercise_profile_revisions").fetchone()[0])
   second=self.apply(db,payload);self.assertIn("EXERCISE_PROFILE_STATE_IMPORT=PASS",second.stdout)
   after=(c.execute("SELECT revision_id,parent_revision_id,legacy_seed FROM exercise_profile_state").fetchone(),
    c.execute("SELECT COUNT(*) FROM exercise_profile_revisions").fetchone()[0])
   self.assertEqual(before,after)
   self.assertEqual(("duration","reps"),(
    c.execute("SELECT tracking_mode FROM exercises").fetchone()[0],
    c.execute("SELECT tracking_mode FROM session_exercises").fetchone()[0]));c.close()
 def test_direct_descendant_converges_and_sibling_conflicts(self):
  with tempfile.TemporaryDirectory() as d:
   db=Path(d)/"pc.db";make_db(db,"duration");self.apply(db,artifact())
   child=artifact("reps",revision("pr_legacy_v1","reps"),"pr_legacy_v1",False);self.apply(db,child)
   sibling=artifact("duration",revision("pr_legacy_v1","duration"),"pr_legacy_v1",False);self.apply(db,sibling,False)
 def test_same_profile_from_different_parents_is_not_equal(self):
  with tempfile.TemporaryDirectory() as d:
   db=Path(d)/"pc.db";make_db(db,"duration");self.apply(db,artifact())
   first=revision("pr_legacy_v1","reps");first_history=[history_record("duration","pr_legacy_v1",None,True),history_record("reps",first,"pr_legacy_v1")]
   self.apply(db,artifact("reps",first,"pr_legacy_v1",False,history=first_history))
   other_parent=revision("pr_legacy_v1","duration")
   other=revision(other_parent,"reps")
   self.apply(db,artifact("reps",other,other_parent,False,history=[history_record("duration","pr_legacy_v1",None,True),history_record("duration",other_parent,"pr_legacy_v1"),history_record("reps",other,other_parent)]),False)
 def test_transitive_stale_ancestor_is_retained(self):
  with tempfile.TemporaryDirectory() as d:
   db=Path(d)/"pc.db";make_db(db,"duration");self.apply(db,artifact())
   first=revision("pr_legacy_v1","reps");second=revision(first,"duration")
   chain=[history_record("duration","pr_legacy_v1",None,True),history_record("reps",first,"pr_legacy_v1"),history_record("duration",second,first)]
   self.apply(db,artifact("duration",second,first,False,history=chain))
   self.apply(db,artifact("reps",first,"pr_legacy_v1",False,history=chain[:2]))
   self.assertEqual(second,sqlite3.connect(db).execute("SELECT revision_id FROM exercise_profile_state").fetchone()[0])
 def test_coerced_scalar_types_and_oversized_history_are_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   for key,value in (("version","1"),("data_fields","0"),("legacy_seed","true"),("science_state",True)):
    db=Path(d)/(key+".db");make_db(db);bad=artifact()
    if key=="version": bad[key]=value
    else: bad["exercises"][0][key]=value
    self.apply(db,bad,False)
   db=Path(d)/"bound.db";make_db(db);bad=artifact();bad["exercises"][0]["history"]*=33;self.apply(db,bad,False)
 def test_generated_at_uses_canonical_timestamp_grammar(self):
  with tempfile.TemporaryDirectory() as d:
   db=Path(d)/"pc.db";make_db(db);bad=artifact();bad["generated_at"]="2026-09-13 10:00:00Z"
   self.apply(db,bad,False)
 def test_history_must_be_canonical_root_to_exact_tip(self):
  with tempfile.TemporaryDirectory() as d:
   root="pr_legacy_v1";child=revision(root,"reps")
   valid=[history_record("duration",root,None,True),history_record("reps",child,root)]
   def changed_history(mutator):
    history=[dict(entry) for entry in valid];mutator(history)
    return artifact("reps",child,root,False,history=history)
   other=revision(root,"duration")
   cases={
    "A-root-omitted":artifact("reps",child,root,False,history=valid[1:]),
    "B-root-parent":changed_history(lambda h:h[0].update(parent_revision_id="detached")),
    "C-root-not-legacy":changed_history(lambda h:h[0].update(legacy_seed=False)),
    "D-second-legacy":changed_history(lambda h:h[1].update(legacy_seed=True)),
    "E-middle-missing":artifact("duration",revision(child,"duration"),child,False,
     history=[valid[0],history_record("duration",revision(child,"duration"),child)]),
    "F-nonprevious-parent":changed_history(lambda h:h[1].update(parent_revision_id="detached")),
    "G-wrong-uuid":changed_history(lambda h:h[1].update(revision_id=other)),
    "H-current-absent":artifact("duration",other,root,False,history=valid),
    "I-current-not-terminal":artifact("duration",root,None,True,history=valid),
    "J-duplicate":artifact("reps",child,root,False,history=valid+[dict(valid[1])]),
    "K-33-revisions":artifact("reps",child,root,False,history=[dict(valid[0])]*33),
    "L-malformed-pr1":artifact("reps","pr1|sets|reps|00",None,True,
     history=[history_record("reps","pr1|sets|reps|00",None,True)]),
   }
   for name,bad in cases.items():
    db=Path(d)/(name+".db");make_db(db);self.apply(db,bad,False)
 def test_export_carries_oldest_to_current_bounded_chain(self):
  with tempfile.TemporaryDirectory() as d:
   db=Path(d)/"pc.db";make_db(db,"duration");self.apply(db,artifact())
   child=revision("pr_legacy_v1","reps");self.apply(db,artifact("reps",child,"pr_legacy_v1",False))
   output=Path(d)/"state.json";result=subprocess.run(["python3",str(EXPORT),str(output),"--database",str(db)],text=True,capture_output=True)
   self.assertEqual(0,result.returncode,result.stdout+result.stderr)
   history=json.loads(output.read_text())["exercises"][0]["history"]
   self.assertEqual(["pr_legacy_v1",child],[entry["revision_id"] for entry in history])
 def test_immutable_machine_difference_is_hard_conflict(self):
  with tempfile.TemporaryDirectory() as d:
   db=Path(d)/"pc.db";make_db(db);self.apply(db,artifact(machine="selectorized"),False)

if __name__=="__main__": unittest.main()
