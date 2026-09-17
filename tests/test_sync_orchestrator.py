import json, os, subprocess, tempfile, unittest, uuid
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
ORCHESTRATOR=ROOT/"tools/sync_orchestrator.py"
FIXTURE=ROOT/"tests/sync_orchestrator_peer_fixture.py"

class OrchestratorTest(unittest.TestCase):
  def test_correlated_worker_report_is_durable_and_reopenable(self):
   for trigger in ("web","android"):
    with self.subTest(trigger=trigger), tempfile.TemporaryDirectory() as directory:
      root=Path(directory);db=root/"trainlog.db";db.touch();state=Path(str(db)+".sync-run.json")
      run="sy_"+str(uuid.uuid4());request="sy_"+str(uuid.uuid4())
      state.write_text(json.dumps({"run_id":run,"request_id":request,"trigger":trigger,"phase":"requested","progress_revision":1,"result":"running"}))
      config=root/"config.json";config.write_text(json.dumps({"format":"trainlog-sync-orchestrator-config","version":1,"enabled":True,"mode":"directory-test-double","capabilities":["generation-manifest-v1","generation-ack-v1","causal-delete-v1","mobile-history-v4","execution-draft-v1"],"peer_command":["python3",str(FIXTURE)],"timeout_seconds":30}))
      result=subprocess.run(["python3",str(ORCHESTRATOR),"--database",str(db),"--state",str(state),"--config",str(config),"--run-id",run,"--request-id",request],capture_output=True,text=True)
      self.assertEqual(result.returncode,0,result.stderr)
      final=json.loads(state.read_text());self.assertEqual(final["phase"],"completed")
      self.assertEqual(final["result"],"completed");self.assertEqual(final["sessions_reconciled"],1)
      self.assertTrue(final["domains"]["causal-delete-v1"]);self.assertEqual(final["drafts"][0]["state"],"pending")

  def test_missing_capability_is_explicitly_degraded(self):
    with tempfile.TemporaryDirectory() as directory:
      root=Path(directory);db=root/"trainlog.db";db.touch();state=Path(str(db)+".sync-run.json")
      run="sy_"+str(uuid.uuid4());request="sy_"+str(uuid.uuid4())
      state.write_text(json.dumps({"run_id":run,"request_id":request,"trigger":"web","phase":"requested","progress_revision":1,"result":"running"}))
      config=root/"config.json";config.write_text(json.dumps({"format":"trainlog-sync-orchestrator-config","version":1,"enabled":True,"mode":"directory-test-double","capabilities":[],"peer_command":["python3",str(FIXTURE)],"timeout_seconds":30}))
      result=subprocess.run(["python3",str(ORCHESTRATOR),"--database",str(db),"--state",str(state),"--config",str(config),"--run-id",run,"--request-id",request])
      self.assertEqual(result.returncode,3);self.assertEqual(json.loads(state.read_text())["phase"],"explicitly_degraded")

if __name__=="__main__":unittest.main()
