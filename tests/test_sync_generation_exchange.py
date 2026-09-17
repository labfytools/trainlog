#!/usr/bin/env python3
"""Production generation/manifest/ACK service regressions."""
import json, sqlite3, sys, tempfile, unittest
from contextlib import closing
from pathlib import Path
from unittest import mock
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import sync_generation_exchange as generation

PEER_A="peer_11111111-1111-4111-8111-111111111111"
PEER_B="peer_22222222-2222-4222-8222-222222222222"
RUN="sy_33333333-3333-4333-8333-333333333333"
GEN="gen_44444444-4444-4444-8444-444444444444"
SCHEMA="""
PRAGMA foreign_keys=ON;
CREATE TABLE facts(id TEXT PRIMARY KEY,value TEXT NOT NULL);
CREATE TABLE sync_causal_operations(operation_id TEXT PRIMARY KEY,target_kind TEXT,target_id TEXT,creator_id TEXT,predecessor_revision_id TEXT,created_at TEXT,payload_sha256 TEXT,publication_context TEXT);
CREATE TABLE sync_peer_identity(singleton INTEGER PRIMARY KEY CHECK(singleton=1),peer_id TEXT NOT NULL UNIQUE,kind TEXT NOT NULL);
CREATE TABLE sync_generations(generation_id TEXT PRIMARY KEY,run_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,producer_kind TEXT,generated_at TEXT,parent_generation_id TEXT,manifest_sha256 TEXT,status TEXT,manifest_json TEXT,staging_path TEXT,acknowledged_at TEXT);
CREATE TABLE sync_generation_artifacts(generation_id TEXT,logical_name TEXT,format TEXT,version INTEGER,filename TEXT,size_bytes INTEGER,sha256 TEXT,required INTEGER,PRIMARY KEY(generation_id,logical_name),UNIQUE(generation_id,filename));
CREATE TABLE sync_consumed_generations(generation_id TEXT PRIMARY KEY,run_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,parent_generation_id TEXT,manifest_sha256 TEXT,consumed_at TEXT,result TEXT,durability TEXT,diagnostic TEXT,ack_json TEXT,UNIQUE(producer_peer_id,generation_id));
CREATE TABLE sync_acknowledgements(ack_id TEXT PRIMARY KEY,generation_id TEXT,run_id TEXT,producer_peer_id TEXT,consumer_peer_id TEXT,manifest_sha256 TEXT,result TEXT,durability TEXT,created_at TEXT,diagnostic TEXT,payload_sha256 TEXT);
CREATE TABLE sync_generation_archives(generation_id TEXT PRIMARY KEY,archive_path TEXT UNIQUE,manifest_sha256 TEXT,archive_sha256 TEXT,archived_at TEXT,audit_json TEXT);
CREATE TABLE sync_causal_publications(operation_id TEXT,generation_id TEXT,first_emission INTEGER,PRIMARY KEY(operation_id,generation_id));
PRAGMA user_version=23;
"""

class GenerationTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.root=Path(self.temp.name)
        self.database=self.root/"source.sqlite"
        with closing(sqlite3.connect(self.database)) as db:
            db.executescript(SCHEMA); db.execute("INSERT INTO facts VALUES('state','A')"); db.commit()
            db.execute("INSERT INTO sync_peer_identity VALUES(1,?,'desktop')",(PEER_A,))
    def tearDown(self): self.temp.cleanup()
    @staticmethod
    def artifacts():
        return (("history","trainlog-mobile-export",4,"history.json",True,"controlled.py",()),)
    def capture(self):
        def exporter(_tool,_extra,output,snapshot):
            with closing(sqlite3.connect(self.database)) as live: live.execute("UPDATE facts SET value='B'"); live.commit()
            with closing(sqlite3.connect(snapshot)) as frozen: value=frozen.execute("SELECT value FROM facts").fetchone()[0]
            output.write_text(json.dumps({"format":"trainlog-mobile-export","version":4,"state":value}))
        with mock.patch.object(generation,"ARTIFACTS",self.artifacts()),mock.patch.object(generation,"SUPPORTED",{("trainlog-mobile-export",4)}),mock.patch.object(generation,"run_export",exporter):
            return generation.capture_desktop(self.database,self.root/"owned",PEER_B,RUN,GEN)
    def validate(self,directory):
        with mock.patch.object(generation,"ARTIFACTS",self.artifacts()),mock.patch.object(generation,"SUPPORTED",{("trainlog-mobile-export",4)}):
            return generation.validate_published(directory,PEER_B)
    def publish(self):
        with mock.patch.object(generation,"ARTIFACTS",self.artifacts()),mock.patch.object(generation,"SUPPORTED",{("trainlog-mobile-export",4)}):
            return generation.publish(self.database,GEN,self.root/"objects")
    def test_coherent_capture_publish_replay_and_ack_restart(self):
        stage,manifest,checksum=self.capture()
        self.assertEqual("A",json.loads((stage/"history.json").read_text())["state"])
        with closing(sqlite3.connect(self.database)) as db:self.assertEqual("B",db.execute("SELECT value FROM facts").fetchone()[0])
        published=self.publish()
        before={p.name:p.read_bytes() for p in published.iterdir()}
        self.assertEqual(published,self.publish())
        self.assertEqual(before,{p.name:p.read_bytes() for p in published.iterdir()})
        checked,actual=self.validate(published);self.assertEqual(manifest["generation_id"],checked["generation_id"]);self.assertEqual(checksum,actual)
        destination=self.root/"destination.sqlite"
        with closing(sqlite3.connect(destination)) as db:
            db.executescript(SCHEMA);db.execute("INSERT INTO sync_peer_identity VALUES(1,?,'desktop')",(PEER_B,));db.commit();db.execute("BEGIN IMMEDIATE")
            db.execute("INSERT INTO facts VALUES('imported','yes')");ack=generation.record_consumed(db,manifest,checksum);db.commit()
        ack_path=self.root/"ack.json";ack_path.write_bytes(generation.canonical(ack))
        self.assertEqual("acknowledged",generation.accept_ack(self.database,ack_path));self.assertEqual("unchanged",generation.accept_ack(self.database,ack_path))
        with closing(sqlite3.connect(destination)) as reopened:
            self.assertEqual(ack,generation.record_consumed(reopened,manifest,checksum));self.assertEqual(1,reopened.execute("SELECT count(*) FROM facts WHERE id='imported'").fetchone()[0])
    def test_substitution_and_transaction_failure_leave_no_consumption(self):
        _,manifest,checksum=self.capture();published=self.publish()
        (published/"history.json").write_text("{}")
        with self.assertRaisesRegex(generation.GenerationError,"size/digest"):self.validate(published)
        destination=self.root/"rollback.sqlite"
        with closing(sqlite3.connect(destination)) as db:
            db.executescript(SCHEMA);db.execute("INSERT INTO sync_peer_identity VALUES(1,?,'desktop')",(PEER_B,));db.commit();db.execute("BEGIN IMMEDIATE")
            db.execute("INSERT INTO facts VALUES('partial','rollback')");generation.record_consumed(db,manifest,checksum);db.rollback()
        with closing(sqlite3.connect(destination)) as db:
            self.assertEqual(0,db.execute("SELECT count(*) FROM facts WHERE id='partial'").fetchone()[0]);self.assertEqual(0,db.execute("SELECT count(*) FROM sync_consumed_generations").fetchone()[0])
    def test_paths_duplicates_limits_and_wrong_ack_are_rejected(self):
        _,manifest,checksum=self.capture()
        with mock.patch.object(generation,"ARTIFACTS",self.artifacts()),mock.patch.object(generation,"SUPPORTED",{("trainlog-mobile-export",4)}):
            for filename in ("/absolute",f"generations/{GEN}/../escape",f"generations/{GEN}/deep/too-deep",f"generations\\{GEN}\\x"):
                bad=json.loads(json.dumps(manifest));bad["artifacts"][0]["filename"]=filename
                with self.assertRaises(generation.GenerationError):generation.validate_manifest_bytes(generation.canonical(bad),PEER_B)
            duplicate=json.loads(json.dumps(manifest));duplicate["artifacts"].append(dict(duplicate["artifacts"][0]))
            with self.assertRaises(generation.GenerationError):generation.validate_manifest_bytes(generation.canonical(duplicate),PEER_B)
            exact=b" "*generation.MAX_MANIFEST
            with self.assertRaisesRegex(generation.GenerationError,"invalid"):generation.validate_manifest_bytes(exact,PEER_B)
            with self.assertRaisesRegex(generation.GenerationError,"byte bound"):generation.validate_manifest_bytes(exact+b" ",PEER_B)
        self.publish()
        published=self.root/"objects"/"generations"/GEN
        original=(published/"manifest.json").read_bytes();(published/"manifest.json").write_bytes(original+b" ")
        with self.assertRaisesRegex(generation.GenerationError,"different published bytes"):self.publish()
        (published/"manifest.json").write_bytes(original)
        wrong=generation.ack_document(RUN,GEN,PEER_A,PEER_B,checksum,"consumed",generation.now());wrong["consumer_peer_id"]="peer_55555555-5555-4555-8555-555555555555"
        payload=dict(wrong);payload.pop("payload_sha256");wrong["payload_sha256"]=generation.digest_bytes(generation.canonical(payload))
        path=self.root/"wrong.json";path.write_bytes(generation.canonical(wrong))
        with self.assertRaisesRegex(generation.GenerationError,"does not match"):generation.accept_ack(self.database,path)
        with closing(sqlite3.connect(self.database)) as db:self.assertEqual("waiting_acknowledgement",db.execute("SELECT status FROM sync_generations").fetchone()[0])

    def test_missing_marker_unsupported_required_capability_and_aggregate_bounds(self):
        empty=self.root/"partial";empty.mkdir()
        with self.assertRaisesRegex(generation.GenerationError,"marker"):generation.validate_published(empty,PEER_B)
        _,manifest,_=self.capture()
        with mock.patch.object(generation,"ARTIFACTS",self.artifacts()),mock.patch.object(generation,"SUPPORTED",set()):
            with self.assertRaisesRegex(generation.GenerationError,"unsupported required"):
                generation.validate_manifest_bytes(generation.canonical(manifest),PEER_B)
        descriptors=[]
        for index in range(4):
            descriptors.append({"logical_name":f"part-{index}","format":"test","version":1,"filename":f"generations/{GEN}/p{index}","size":generation.MAX_ARTIFACT,"sha256":"0"*64,"required":False})
        bounded=dict(manifest);bounded["artifacts"]=descriptors
        with mock.patch.object(generation,"ARTIFACTS",()),mock.patch.object(generation,"SUPPORTED",set()):
            generation.validate_manifest_bytes(generation.canonical(bounded),PEER_B)
            bounded["artifacts"].append({"logical_name":"overflow","format":"test","version":1,"filename":f"generations/{GEN}/overflow","size":1,"sha256":"0"*64,"required":False})
            with self.assertRaisesRegex(generation.GenerationError,"generation exceeds"):generation.validate_manifest_bytes(generation.canonical(bounded),PEER_B)

    def test_causal_operation_bytes_survive_multiple_generation_associations(self):
        kinds=("session","execution_draft","exercise","body_observation","custom_equipment","feedback","body_zone_relation")
        operations=[]
        for index,kind in enumerate(kinds,1):
            operations.append({"operation_id":f"op_{index:08x}-aaaa-4aaa-8aaa-aaaaaaaaaaaa","target_kind":kind,"target_id":f"target-{index}","creator_id":"peer_cccccccc-cccc-4ccc-8ccc-cccccccccccc","predecessor_revision_id":f"lv_deadbeef{index}","created_at":"2026-09-17T10:00:00+02:00","payload_sha256":f"{index:x}"*64,"publication_context":None})
        with closing(sqlite3.connect(self.database)) as db:
            db.executemany("INSERT INTO sync_causal_operations VALUES(?,?,?,?,?,?,?,?)",[tuple(operation.values()) for operation in operations]);db.commit()
        artifacts=(("causal-deletions","trainlog-causal-deletions",1,"causal-deletions-v1.json",True,"controlled.py",()),)
        def exporter(_tool,_extra,output,_snapshot):output.write_text(json.dumps({"format":"trainlog-causal-deletions","version":1,"generated_at":"2026-09-17T10:00:00+02:00","operations":operations},sort_keys=True))
        with mock.patch.object(generation,"ARTIFACTS",artifacts),mock.patch.object(generation,"SUPPORTED",{("trainlog-causal-deletions",1)}),mock.patch.object(generation,"run_export",exporter):
            first=generation.capture_desktop(self.database,self.root/"causal",PEER_B,RUN,GEN)[0]
            second_id="gen_55555555-5555-4555-8555-555555555555"
            second=generation.capture_desktop(self.database,self.root/"causal",PEER_B,"sy_66666666-6666-4666-8666-666666666666",second_id)[0]
        self.assertEqual(json.loads((first/"causal-deletions-v1.json").read_text())["operations"],json.loads((second/"causal-deletions-v1.json").read_text())["operations"])
        with closing(sqlite3.connect(self.database)) as db:
            self.assertEqual((7,7),tuple(db.execute("SELECT SUM(first_emission),COUNT(*)-SUM(first_emission) FROM sync_causal_publications").fetchone()))
            self.assertEqual([operation["payload_sha256"] for operation in operations],[row[0] for row in db.execute("SELECT payload_sha256 FROM sync_causal_operations ORDER BY operation_id")])

    def test_real_consumer_rolls_back_earlier_domain_write_on_late_failure(self):
        names=("catalog","history","execution-drafts","exercise-aliases","exercise-profile-state","equipment-definitions","equipment-associations","body-zones","feedback","causal-deletions")
        directory=self.root/"incoming";directory.mkdir()
        artifacts=[]
        for name in names:
            filename=name+".json";(directory/filename).write_text("{}")
            artifacts.append({"logical_name":name,"format":"test","version":1,"filename":f"generations/{GEN}/{filename}","size":2,"sha256":"0"*64,"required":True})
        manifest={"format":generation.MANIFEST,"version":1,"generation_id":GEN,"run_id":RUN,"producer":{"peer_id":PEER_A,"kind":"android"},"consumer_peer_id":PEER_B,"generated_at":"2026-09-17T10:00:00+02:00","parent_generation_id":None,"artifacts":artifacts}
        def write_fact(db,_payload,_trace=False):db.execute("INSERT INTO facts VALUES('partial','must roll back')");return {}
        with mock.patch.object(generation,"validate_published",return_value=(manifest,"a"*64)), \
             mock.patch.object(generation.causal_delete_exchange,"load",return_value={"operations":[]}), \
             mock.patch.object(generation.execution_draft_exchange,"load",return_value={}), \
             mock.patch.object(generation.import_exercise_aliases,"load",return_value=[]), \
             mock.patch.object(generation,"strict_json",return_value={}), \
             mock.patch.object(generation.import_equipment_definitions,"supplied_ids",return_value=set()), \
             mock.patch.object(generation.import_equipment_definitions,"validate",return_value=[]), \
             mock.patch.object(generation.import_mobile_export,"load_payload",return_value={}), \
             mock.patch.object(generation.import_mobile_export,"validate_payload"), \
             mock.patch.object(generation.import_equipment_associations,"load_mobile_occurrences",return_value={}), \
             mock.patch.object(generation.import_exercise_body_zones,"parse_payload",return_value=[]), \
             mock.patch.object(generation.import_training_feedback,"load",return_value={}), \
             mock.patch.object(generation.import_exercise_profile_state,"apply_profile_state"), \
             mock.patch.object(generation.import_equipment_definitions,"apply_definitions"), \
             mock.patch.object(generation.import_mobile_export,"apply_payload",side_effect=write_fact), \
             mock.patch.object(generation.import_exercise_aliases,"apply_aliases"), \
             mock.patch.object(generation.import_equipment_associations,"apply_associations"), \
             mock.patch.object(generation.import_exercise_body_zones,"apply_body_zones"), \
             mock.patch.object(generation.import_training_feedback,"apply_feedback",side_effect=ValueError("late semantic failure")):
            rejected=generation.consume_desktop(self.database,directory)
            self.assertEqual("rejected",rejected["result"]);self.assertIn("late semantic",rejected["diagnostic"])
        with closing(sqlite3.connect(self.database)) as db:
            self.assertEqual(0,db.execute("SELECT COUNT(*) FROM facts WHERE id='partial'").fetchone()[0])
            self.assertEqual(1,db.execute("SELECT COUNT(*) FROM sync_consumed_generations WHERE result='rejected'").fetchone()[0])

if __name__=="__main__":unittest.main()
