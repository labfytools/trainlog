# Web Sessions V1 evidence matrix

| Requirement | Production owner | Decisive test or check | Result |
|---|---|---|---|
| Manual preparation, stable occurrences and causal revisions | `web_sessions.c`, desktop schema v23 | `web_sessions` | PASS |
| Lost response replay and stale multi-tab conflict | durable request ledger and transactional head compare | `web_sessions` | PASS |
| Complete lists beyond Dashboard summary | paged Sessions Core query and exhaustive Web consumer | `web_sessions`, `sessions.test.ts` (41 objects over two pages) | PASS |
| Proposal remains immutable during explicit derivation | provenance-validated creation command | Core transaction and isolated Web flow | PASS |
| Separate Android pending preparation | Android schema v22 and repository | `SessionPreparationExchangeTest` | PASS |
| Occupied singleton is never overwritten | `startPreparedSession()` transaction | `SessionPreparationExchangeTest` | PASS |
| Generation, manifest and exact ACK delivery evidence | generation participant and delivery ledger | `SyncGenerationServiceTest` | PASS |
| Repeated bidirectional exchange and restart | production generation implementations | `SyncDeploymentConversationTest` (24 conversations) | PASS |
| Existing transport, deletion and archive behavior | unchanged production suites | canonical Meson suite, 85/85 | PASS |
| Android migrations and backup | SQLiteOpenHelper v22 and backup verifier | Android unit suite, 221 tests, 0 failures, 5 skips | PASS |
| Deep links, persisted browser creation and embedded assets | production HTTP server and React route | `test_web_sessions_browser.py`, `web_server`, stored-value inspection | PASS |
| Catppuccin responsive UI and embedded assets | React Sessions route and existing tokens | typecheck, Vitest 82/82, Vite/Meson builds; Chromium `[390, 844, DPR 1]` captures | PASS |

Physical USB delivery, the installed APK and daily data migration are
intentionally pending the single controlled-rollout consent gate. Isolated
real-Firefox functional captures and exact 390 CSS-pixel Chromium captures are
retained under `docs/reviews/evidence/`.
