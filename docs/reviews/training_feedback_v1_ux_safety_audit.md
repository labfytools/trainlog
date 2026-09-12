# Training Feedback V1 — UX and destructive-action audit

Repository audit performed for the schema-v15 closeout. “No action” means the
current product exposes no such user operation; no functionality was invented.

| Action | Client | Persisted data affected | Confirmation before this tranche | Required correction / resulting gate |
|---|---|---|---|---|
| Remove active-draft occurrence | Android | occurrence, performed sets/continuous/MAX, plan and feedback roots/revisions | No | Shared action-specific modal with exact nonzero child counts |
| Remove one performed set | Android | one durable draft performed-set fact | No | Shared series-specific modal |
| Discard active draft/session | Android | complete durable draft graph | Yes, inline second action | Replaced by non-dismissible-outside shared modal; Back/Cancel safe |
| Change occurrence to incompatible profile | Android | existing actuals/MAX/plan may be replaced | No | Loss-specific modal; same `entry_id` and feedback retained |
| Detach/replace completed occurrence equipment | Android | persisted equipment association | No | Association-loss modal before detach/replacement |
| Delete feedback directly | Android | feedback root/revisions | No action | None added; correction appends a revision |
| Delete custom exercise | Android | catalog/history | No reachable delete action | No correction |
| Delete custom legacy equipment | Android | catalog association | No catalog-delete action; completed association editor can detach | Detach/replace gate above |
| Delete body observation | Android | body observation | No reachable delete action | No correction |
| Merge/retire exercise | Android | catalog identity graph | No reachable action | No correction |
| Settings reset/clear | Android | database/settings | No reachable destructive reset | No correction |
| Remove session occurrence | TUI | transient/corrected occurrence graph and children | Yes, numeric confirmation | Modal state normalized to default Non; arrows/Tab focus; Enter explicit; Escape/q cancel |
| Remove performed set | TUI | one occurrence performed-set fact | No | Existing event loop now enters the same safe confirmation state |
| Abandon in-process session draft | TUI | complete process-owned draft | Yes, numeric confirmation | Default Non modal semantics; no `q` confirmation |
| Merge/retire exercise | TUI | source identity, occurrence ownership, equipment/zones | Yes | Existing impact preview and default-safe two-choice overlay retained |
| Completed-session correction removes occurrence | TUI | occurrence plus its child facts and feedback revisions | Indirectly through explicit correction/save workflow | Replacement stays transactional; revisions retained only for same `entry_id`; removal follows confirmed occurrence choice |
| Delete custom exercise/equipment | TUI | catalog rows | No reachable direct delete action | No correction |
| Delete body observation | TUI | body row | No reachable delete action | No correction |
| Settings reset/clear | TUI | database/settings | No reachable action | No correction |

Generator proposal removal/discard and dirty-form abandon concern transient,
not-yet-persisted editor state. Existing keep/discard guards remain explicit;
they do not delete durable training facts. Synchronization cleanup of temporary
documents is system-owned publication rollback/housekeeping, not a user data
deletion action.

Undo was rejected for occurrence deletion. Current persistence reconstructs
draft occurrence rows; a byte-for-byte logical restore would require capturing
and reinstating every root, revision, timestamp, child row and position through
a second durable transaction. A cosmetic Snackbar could not prove that
identity contract. The confirmation modal is therefore the truthful V1 safety
mechanism.
