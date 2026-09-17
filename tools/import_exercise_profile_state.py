#!/usr/bin/env python3
"""Apply direct-descendant current profiles without reinterpreting occurrences."""
import argparse
import json
import re
import sqlite3
from pathlib import Path

from validate_json import parse_timestamp
from trainlog_sqlite import connect_database, profile_revision

EX_ID=re.compile(r"^ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")
HISTORY_MAX=32
ADJ=("load_semantics","machine_variant","machine_provenance","scientific_profile_id","science_state","legacy_equipment_id")
REVISION_RE=re.compile(r"^pr2_[0-9a-f]{8}-[0-9a-f]{4}-5[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")

def fail(message): raise ValueError(message)

def validate(item):
    required={"exercise_id","recording_mode","tracking_mode","data_fields",*ADJ,
              "revision_id","parent_revision_id","legacy_seed","history"}
    if set(item)!=required or not isinstance(item["exercise_id"],str) or not EX_ID.fullmatch(item["exercise_id"]): fail("record profile invalide")
    r,t,f=item["recording_mode"],item["tracking_mode"],item["data_fields"]
    if r not in ("sets","continuous") or t not in ("reps","duration") or r=="continuous" and t!="duration" or type(f) is not int or f<0 or f&~3 or r=="sets" and f: fail("profil entrant invalide")
    if item["science_state"] not in ("resolved","unresolved") or any(item[k] is not None and not isinstance(item[k],str) for k in ADJ if k!="science_state") or type(item["legacy_seed"]) is not bool: fail("état profil invalide")
    if item["legacy_seed"]:
        if item["revision_id"]!="pr_legacy_v1" or item["parent_revision_id"] is not None: fail("racine legacy invalide")
    elif not isinstance(item["parent_revision_id"],str) or not isinstance(item["revision_id"],str) or not REVISION_RE.fullmatch(item["revision_id"]) or item["revision_id"]!=profile_revision(item["parent_revision_id"],r,t,f): fail("révision profil invalide")
    history=item["history"]
    keys={"revision_id","parent_revision_id","recording_mode","tracking_mode","data_fields","legacy_seed"}
    if not isinstance(history,list) or not 1<=len(history)<=HISTORY_MAX: fail("historique profil hors limite")
    previous=None;revision_ids=set()
    for index,revision in enumerate(history):
        if not isinstance(revision,dict) or set(revision)!=keys: fail("révision historique invalide")
        rr,rt,rf=revision["recording_mode"],revision["tracking_mode"],revision["data_fields"]
        legacy=revision["legacy_seed"];rid=revision["revision_id"];parent=revision["parent_revision_id"]
        if rr not in ("sets","continuous") or rt not in ("reps","duration") or rr=="continuous" and rt!="duration" or type(rf) is not int or rf<0 or rf&~3 or rr=="sets" and rf or type(legacy) is not bool: fail("profil historique invalide")
        if not isinstance(rid,str) or rid in revision_ids or index and parent!=previous: fail("chaîne historique invalide")
        if index == 0:
            if not legacy or rid!="pr_legacy_v1" or parent is not None: fail("racine historique invalide")
        elif legacy:
            fail("marqueur legacy hors racine")
        if legacy:
            if rid!="pr_legacy_v1" or parent is not None: fail("racine historique invalide")
        elif not isinstance(parent,str) or not isinstance(rid,str) or not REVISION_RE.fullmatch(rid) or rid!=profile_revision(parent,rr,rt,rf): fail("révision historique invalide")
        revision_ids.add(rid);previous=rid
    if any(history[-1][key]!=item[key] for key in keys): fail("pointe historique incohérente")

def apply_profile_state(con, root, allow_pending=False):
        if not isinstance(root,dict) or set(root)!={"format","version","generated_at","exercises"} or root["format"]!="trainlog-exercise-profile-state" or type(root["version"]) is not int or root["version"]!=1 or not isinstance(root["exercises"],list): fail("enveloppe profile-state invalide")
        parse_timestamp(root["generated_at"], "generated_at");con.row_factory=sqlite3.Row
        if con.execute("PRAGMA user_version").fetchone()[0] not in (17,18,19,20,21,22,23): fail("schema desktop v17-v23 requis")
        applied=equal=ancestor=pending=0;seen=set()
        for item in root["exercises"]:
            validate(item);eid=item["exercise_id"]
            if eid in seen: fail("exercise_id profile-state dupliqué")
            seen.add(eid)
            row=con.execute("""SELECT e.id,e.recording_mode,e.tracking_mode,e.data_fields,e.load_semantics,e.machine_variant,e.machine_provenance,e.scientific_profile_id,e.science_state,e.legacy_equipment_id,s.revision_id,s.parent_revision_id,s.legacy_seed FROM exercises e JOIN exercise_profile_state s ON s.exercise_row_id=e.id WHERE e.exercise_id=?""",(eid,)).fetchone()
            if row is None:
                if allow_pending: pending+=1;continue
                fail(f"identité exercice inconnue: {eid}")
            if any(row[k]!=item[k] for k in ADJ): fail(f"conflit identité/machine immutable: {eid}")
            local_profile=(row["recording_mode"],row["tracking_mode"],row["data_fields"]);incoming=(item["recording_mode"],item["tracking_mode"],item["data_fields"])
            incoming_ids={revision["revision_id"] for revision in item["history"]}
            known={r[0] for r in con.execute("SELECT revision_id FROM exercise_profile_revisions WHERE exercise_row_id=?",(row["id"],))}
            if row["revision_id"]==item["revision_id"]:
                if local_profile==incoming: equal+=1;continue
                if not (row["legacy_seed"] and item["legacy_seed"]): fail(f"révision identique avec profil différent: {eid}")
            elif row["revision_id"] in incoming_ids:
                pass
            elif item["revision_id"] in known:
                ancestor+=1;continue
            else: fail(f"conflit de descendants concurrents: {eid}")
            existing_count=con.execute("SELECT COUNT(*) FROM exercise_profile_revisions WHERE exercise_row_id=?",(row["id"],)).fetchone()[0]
            additions=sum(1 for revision in item["history"] if revision["revision_id"] not in known)
            if existing_count+additions>HISTORY_MAX: fail(f"historique profil dépasse {HISTORY_MAX}: {eid}")
            for revision in item["history"]:
                con.execute("INSERT OR IGNORE INTO exercise_profile_revisions(exercise_row_id,revision_id,parent_revision_id,recording_mode,tracking_mode,data_fields,legacy_seed) VALUES(?,?,?,?,?,?,?)",
                    (row["id"],revision["revision_id"],revision["parent_revision_id"],revision["recording_mode"],revision["tracking_mode"],revision["data_fields"],int(revision["legacy_seed"])))
            if item["legacy_seed"]:
                # The shared migration root deliberately permits the one initial
                # cross-peer repair; keep its stored tuple equal to the adopted tip.
                con.execute("UPDATE exercise_profile_revisions SET recording_mode=?,tracking_mode=?,data_fields=? WHERE exercise_row_id=? AND revision_id='pr_legacy_v1'",(*incoming,row["id"]))
            # CONTRACT: select the proven incoming tip before changing the catalogue;
            # the guarded local-edit trigger then recognizes an authoritative import.
            con.execute("UPDATE exercise_profile_state SET revision_id=?,parent_revision_id=?,legacy_seed=? WHERE exercise_row_id=?",(item["revision_id"],item["parent_revision_id"],int(item["legacy_seed"]),row["id"]))
            con.execute("UPDATE exercises SET recording_mode=?,tracking_mode=?,data_fields=? WHERE id=?",(*incoming,row["id"]))
            applied+=1
        return applied,equal,ancestor,pending

def main():
    p=argparse.ArgumentParser();p.add_argument("artifact",type=Path);p.add_argument("--database",required=True,type=Path);p.add_argument("--allow-pending",action="store_true");p.add_argument("--mobile-export",type=Path,help="sync pre-pass identity proof; permits identities introduced by the following catalog import");a=p.parse_args()
    try:
        root=json.loads(a.artifact.read_text(encoding="utf-8"));con=connect_database(a.database)
        con.execute("BEGIN IMMEDIATE")
        applied,equal,ancestor,pending=apply_profile_state(con,root,a.allow_pending or a.mobile_export is not None)
        con.commit();print(f"EXERCISE_PROFILE_STATE_IMPORT=PASS applied={applied} equal={equal} ancestor={ancestor} pending={pending}")
    except (OSError,json.JSONDecodeError,sqlite3.Error,ValueError) as e:
        try:
            if 'con' in locals(): con.rollback()
        except sqlite3.Error: pass
        raise SystemExit(f"EXERCISE_PROFILE_STATE_IMPORT=FAIL {e}")
    finally:
        if 'con' in locals(): con.close()

if __name__=="__main__": main()
