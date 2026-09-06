# Transaction-safe session editing

## Status

```text
SESSION_EDIT_PERSISTENCE=IMPLEMENTED
SESSION_EDIT_TUI=NEXT
TRAINLOG_FORMAT_V1=FROZEN
DATABASE_SCHEMA_V2=UNCHANGED
```

This slice adds the persistence foundation required to correct a recorded
workout without deleting the session itself.

`trainlog_database_replace_session_exercises(...)` replaces the
`session_exercises` and `performed_sets` children inside one explicit
transaction.

The parent `sessions` row is never deleted. Therefore these remain stable:

- `session_id`;
- start/end timestamps;
- `session_type`;
- session notes;
- any `body_observations.session_row_id` link.

On any lookup, constraint or insertion error, the whole replacement rolls back.

`trainlog_database_load_session_editable(...)` exposes the exact persisted
exercise/set values in bounded caller-owned buffers. It refuses insufficient
capacity instead of silently truncating editable data.

No schema migration and no JSON v1 change are required.
