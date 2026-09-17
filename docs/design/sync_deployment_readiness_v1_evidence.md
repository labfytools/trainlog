# Sync deployment readiness V1 evidence

Status: **SUPERSEDED SOFTWARE BLOCKER — adapter and backup completed by the
MTP/Android-backup follow-up; physical and rollout gates remain**.

This corrective tranche replaces the earlier configurable peer fixture with a
fixed desktop worker and a production Android foreground coordinator. The real
Firefox scenario executes the C HTTP adapter, durable run state, both generation
services, transactional imports, real ACK creation/acceptance and store reopen.
It uses a directory object-I/O double and therefore does not claim USB/MTP.

Decisive targeted evidence:

- `SyncDeploymentConversationTest`: Android generation capture/publication,
  desktop consume/ACK, desktop return generation, Android consume/ACK and draft
  survival after reopen.
- `test_web_sync_browser.py`: Firefox click, observable desktop commit before the
  Android barrier is released, completion, active draft display and reload.
- `test_sync_orchestrator.py`: untrusted-command rejection, pre-accumulation
  64-KiB stdout enforcement, and SIGKILL escalation for an owned helper and
  descendant that ignore SIGTERM.
- `test_sync_deployment_tools.py`: SQLite backup captures committed WAL state and
  passes integrity/foreign-key checks; candidate inventory contains the fixed
  helper and hashes.

The tests also exposed and corrected two integration defects: execution-draft
`active`/`pending` state was incorrectly treated as immutable revision content,
and coordination files could become visible before their bytes were complete.

Not performed: physical MTP, real phone access, release signature verification,
real Drive, user backups, main merge, service switch or deployment. The Selenium
`ResourceWarning` remains visible and is associated with an HTTP 405 during the
external Firefox/geckodriver teardown path; no blanket warning suppression is
used. It remains a toolchain limitation until independently fixed or upgraded.
