#!/usr/bin/env python3
"""Bounded isolated peer transport fixture for orchestrator/Web tests."""
import json, os, uuid

run_id=os.environ["TRAINLOG_SYNC_RUN_ID"]
def identity(prefix): return prefix+str(uuid.uuid4())
print(json.dumps({
  "format":"trainlog-sync-worker-report","version":1,"run_id":run_id,
  "producer_peer_id":identity("peer_"),"consumer_peer_id":identity("peer_"),
  "inbound_generation_id":identity("gen_"),"outbound_generation_id":identity("gen_"),
  "manifest_sha256":"a"*64,"result":"completed","sessions_reconciled":1,
  "domains":{"history-v4":True,"execution-draft-v1":True,"causal-delete-v1":True,
             "equipment":True,"aliases":True,"profiles":True,"body-zones":True,"feedback":True},
  "drafts":[{"draft_id":identity("draft_"),"state":"pending","session_type":"strength","occurrence_count":2}],
  "ai_midpoint":{"result":"not_configured"},"ai_post_sync":{"result":"not_configured"}
},sort_keys=True,separators=(",",":")))
