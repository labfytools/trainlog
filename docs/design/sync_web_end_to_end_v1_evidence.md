# Sync Web end-to-end V1 evidence

Status: `TRAINLOG_SYNC_WEB_END_TO_END_V1=PASS/FROZEN` on the development
branch. This status means implemented and validated with isolated stores and a
directory transport adapter. It does not mean physical MTP, Drive, deployment,
or a merge to `main`.

## Production call graph

```text
React SyncControl
 -> POST /api/v1/sync (Host + Origin + ephemeral CSRF)
 -> C Web admission (trusted opt-in config, one request identity)
 -> owned sync_orchestrator.py process
 -> common XDG sync.lock
 -> trusted peer adapter
 -> Android SyncGenerationService / desktop sync_generation_exchange.py
 -> durable generation and ACK evidence
 -> bounded run report
 -> GET /api/v1/sync/status polling
 -> Dashboard Core snapshot refresh after local commit
```

The deployed/default path remains V3. Full generation mode is unavailable
unless `TRAINLOG_SYNC_GENERATION_CONFIG` names an absolute, trusted local
configuration. HTTP input cannot select a database, executable, transport,
capability, path, timeout, or causal bypass.

## Requirement-to-proof matrix

| Requirement | Production entry point | Exact proof | Observed result |
|---|---|---|---|
| Web admission and passive status | `web_server.c` `/api/v1/sync`, `/api/v1/sync/status` | `web_server`, `sync API` | 202/409/403/405, passive GET and CSRF header pass |
| Async owned worker | `sync_orchestrator.py` | `sync_orchestrator`, Firefox scenario | HTTP remains responsive and child is reaped |
| Durable typed phases | atomic `.sync-run.json` replacement | `test_sync_orchestrator.py` | correlated completion survives reopen |
| Common contention | XDG `sync.lock` | orchestrator conflict test and legacy lock contract | second owner fails without a second import |
| Strict compatibility | orchestrator capability set | missing-capability test | `explicitly_degraded`, missing capabilities retained |
| Generation semantics | desktop and Android generation services | `sync_generation_exchange`, `SyncGenerationServiceTest` | coherent capture, whole transaction, ACK and replay pass |
| Independent producers | public C fixture and Android repository fixture | `androidGenerationCapturePublishConsumeAndAckSurviveReopen` | both direction chains pass after reopen |
| All mandatory domains | generation descriptors and apply order | generation suites and causal seven-kind test | required empty artifacts included and validated |
| Causal immutability | `sync_causal_publications` | causal multi-generation tests | operation bytes/digests unchanged |
| Late failure | transaction-neutral apply helpers | desktop and Android late-domain tests | prior writes roll back; no consumed ACK |
| Lost/late ACK | durable ACK tables | generation restart tests | exact ACK reconstructed; replay unchanged |
| UI click/reload | `SyncControl` | `test_web_sync_browser.py` | real Firefox click, completion, draft details and reload pass |
| Draft visibility | status `drafts` projection | `SyncControl.test.tsx`, Firefox scenario | stable ID/state/type/count shown read-only |
| Dashboard freshness | `onCommitted` callback | component test and browser flow | Core snapshot refetched after committed phase |
| Mutation security | existing Host/Origin/CSRF plus strict body | `web_server`, `sync API` | proxy origin retained; no CORS or client paths |
| Optional AI | separate `ai_midpoint` and `ai_post_sync` | orchestrator report validation | not-configured remains explicit, never main success |
| Default isolation | missing trusted config | C HTTP test | button disabled and POST returns explicit conflict |

The Firefox peer command in the browser scenario is the bounded directory
transport fixture. Cross-implementation Android/desktop behavior is proven by
the production generation suites in the same final validation run. The test
does not claim physical-device timing, MTP object behavior, or Drive access.

## Bounds

- request body: existing 4 KiB HTTP limit;
- headers: existing 8 KiB limit;
- durable report and each worker output stream: 64 KiB;
- trusted configuration: 16 KiB;
- diagnostic: 1024 UTF-8 bytes;
- draft summaries: 32;
- peer command: 16 fixed arguments, each at most 4096 bytes;
- trusted timeout: 1 to 900 seconds;
- frontend polling: one request at a time, 750 ms between completions;
- generation retention: existing eight outgoing generations per consumer.

## Real limitations

Android must participate through its foreground synchronization flow to create
a publication correlated to the accepted run. No always-on Android background
service was added. A waiting phase is therefore truthful and may require user
action. Physical MTP and Drive remain rollout gates. A browser disconnect does
not cancel the worker; server shutdown terminates only its owned process group.
