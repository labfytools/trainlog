#!/usr/bin/env python3
"""Strict V1-compatible, append-only TRAINING_FEEDBACK_V2 desktop merger."""
import argparse,json,re,sqlite3
from datetime import datetime
from pathlib import Path
ID=lambda p:re.compile(r"^"+p+r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")
FB,FU,FR=ID("fb_"),ID("fu_"),ID("fr_")
def fail(message): raise ValueError(message)
def instant(value):
    if not isinstance(value,str) or "T" not in value or not re.search(r"(?:Z|[+-]\d\d:\d\d)$",value): fail("timestamp offset-aware invalide")
    try:return datetime.fromisoformat(value.replace("Z","+00:00"))
    except ValueError:fail("timestamp invalide")
def valid_text(value):
    if not isinstance(value,str) or not value.strip() or len(value.encode())>8192: fail("texte vide ou supérieur à 8192 octets UTF-8")
def load(path):
    if path.stat().st_size>40*1024*1024:fail("artifact trop volumineux")
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:fail("champ JSON dupliqué: "+key)
            result[key]=value
        return result
    root=json.loads(path.read_text(encoding="utf-8"),object_pairs_hook=unique)
    if set(root)!={"format","version","generated_at","exercise_feedback","session_followups"} or root["format"]!="trainlog-training-feedback" or type(root["version"]) is not int or root["version"] not in (1,2):fail("artifact feedback v1/v2 invalide")
    instant(root["generated_at"])
    if not isinstance(root["exercise_feedback"],list) or not isinstance(root["session_followups"],list) or len(root["exercise_feedback"])>4096 or len(root["session_followups"])>4096:fail("tableau feedback hors borne")
    return root
def revision_list(item,root_id,version):
    if version==1: valid_text(item["raw_text"]); return [("fr0_"+root_id,item["observed_at"],item["raw_text"])]
    values=item["revisions"]
    if not isinstance(values,list) or not values or len(values)>4096:fail("révisions absentes/hors borne")
    result=[];seen=set()
    for revision in values:
        if not isinstance(revision,dict) or set(revision)!={"revision_id","created_at","raw_text"}:fail("révision invalide")
        rid=revision["revision_id"]
        if not isinstance(rid,str) or not (FR.fullmatch(rid) or rid=="fr0_"+root_id) or rid in seen:fail("revision_id invalide/dupliqué")
        seen.add(rid);instant(revision["created_at"]);valid_text(revision["raw_text"]);result.append((rid,revision["created_at"],revision["raw_text"]))
    return result
def merge_revisions(db,table,parent_column,parent_id,revisions):
    added=skipped=0
    for revision in revisions:
        old=db.execute(f"SELECT {parent_column},created_at,raw_text FROM {table} WHERE revision_id=?",(revision[0],)).fetchone(); expected=(parent_id,revision[1],revision[2])
        if old is None:db.execute(f"INSERT INTO {table}(revision_id,{parent_column},created_at,raw_text) VALUES(?,?,?,?)",(revision[0],*expected));added+=1
        elif old==expected:skipped+=1
        else:fail("HARD CONFLICT "+revision[0])
    current=max(db.execute(f"SELECT revision_id,created_at,raw_text FROM {table} WHERE {parent_column}=?",(parent_id,)).fetchall(),key=lambda r:(instant(r[1]),r[0].encode()))
    return added,skipped,current[2]
def main():
    parser=argparse.ArgumentParser();parser.add_argument("artifact",type=Path);parser.add_argument("--database",type=Path,required=True);args=parser.parse_args();root=load(args.artifact);db=sqlite3.connect(args.database)
    roots_added=roots_skipped=revisions_added=revisions_skipped=0
    try:
      db.execute("PRAGMA foreign_keys=ON")
      if db.execute("PRAGMA user_version").fetchone()[0]!=15:fail("schema desktop v15 requis")
      db.execute("BEGIN IMMEDIATE");seen=set()
      for index,item in enumerate(root["exercise_feedback"]):
        keys={"feedback_id","session_id","entry_id","exercise_id","observed_at","raw_text"} if root["version"]==1 else {"feedback_id","session_id","entry_id","exercise_id","observed_at","revisions"}
        if not isinstance(item,dict) or set(item)!=keys:fail(f"exercise_feedback[{index}] invalide")
        fid=item["feedback_id"]
        if not isinstance(fid,str) or not FB.fullmatch(fid) or fid in seen:fail("feedback_id invalide/dupliqué")
        seen.add(fid);instant(item["observed_at"]);occurrence=db.execute("SELECT se.id,e.exercise_id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id WHERE s.session_id=? AND se.entry_id=?",(item["session_id"],item["entry_id"])).fetchone()
        if not occurrence:fail("session/entry introuvable")
        canonical=db.execute("SELECT exercise_id FROM exercises WHERE exercise_id=? UNION ALL SELECT canonical_exercise_id FROM exercise_aliases WHERE source_exercise_id=? LIMIT 1",(item["exercise_id"],item["exercise_id"])).fetchone()
        if not canonical or canonical[0]!=occurrence[1]:fail("exercise_id incompatible avec l'occurrence")
        old=db.execute("SELECT session_exercise_row_id,observed_at FROM exercise_feedback WHERE feedback_id=?",(fid,)).fetchone();expected=(occurrence[0],item["observed_at"]);revisions=revision_list(item,fid,root["version"])
        if old is None:db.execute("INSERT INTO exercise_feedback(feedback_id,session_exercise_row_id,observed_at,raw_text) VALUES(?,?,?,?)",(fid,*expected,revisions[0][2]));roots_added+=1
        elif old==expected:roots_skipped+=1
        else:fail("HARD CONFLICT "+fid)
        added,skipped,current=merge_revisions(db,"exercise_feedback_revisions","feedback_id",fid,revisions);revisions_added+=added;revisions_skipped+=skipped;db.execute("UPDATE exercise_feedback SET raw_text=? WHERE feedback_id=?",(current,fid))
      seen=set()
      for index,item in enumerate(root["session_followups"]):
        keys={"followup_id","session_id","observed_at","raw_text"} if root["version"]==1 else {"followup_id","session_id","observed_at","revisions"}
        if not isinstance(item,dict) or set(item)!=keys:fail(f"session_followups[{index}] invalide")
        fid=item["followup_id"]
        if not isinstance(fid,str) or not FU.fullmatch(fid) or fid in seen:fail("followup_id invalide/dupliqué")
        seen.add(fid);instant(item["observed_at"]);session=db.execute("SELECT id FROM sessions WHERE session_id=?",(item["session_id"],)).fetchone()
        if not session:fail("session introuvable")
        old=db.execute("SELECT session_row_id,observed_at FROM session_followups WHERE followup_id=?",(fid,)).fetchone();expected=(session[0],item["observed_at"]);revisions=revision_list(item,fid,root["version"])
        if old is None:db.execute("INSERT INTO session_followups(followup_id,session_row_id,observed_at,raw_text) VALUES(?,?,?,?)",(fid,*expected,revisions[0][2]));roots_added+=1
        elif old==expected:roots_skipped+=1
        else:fail("HARD CONFLICT "+fid)
        added,skipped,current=merge_revisions(db,"session_followup_revisions","followup_id",fid,revisions);revisions_added+=added;revisions_skipped+=skipped;db.execute("UPDATE session_followups SET raw_text=? WHERE followup_id=?",(current,fid))
      db.commit()
    except Exception:db.rollback();raise
    finally:db.close()
    print(f"TRAINING_FEEDBACK_IMPORT=PASS version={root['version']} roots_added={roots_added} roots_skipped={roots_skipped} revisions_added={revisions_added} revisions_skipped={revisions_skipped}")
if __name__=="__main__":main()
