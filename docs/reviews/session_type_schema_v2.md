# Session type schema v2

## Status

```text
DATABASE_SCHEMA_V2=IMPLEMENTED
SESSION_TYPE_PERSISTENCE=IMPLEMENTED
TRAINLOG_FORMAT_V1=FROZEN
SESSION_TYPE_TUI=NEXT
```

Schema v2 adds one local SQLite field to `sessions`:

```text
session_type = training | max_test
```

Rules:

- `training` is the default;
- all rows migrated from schema v1 become `training`;
- zero-initialized `TrainlogSessionInput` values remain `training`;
- invalid database values are rejected by a SQLite `CHECK`;
- `max_test` is explicit and is never inferred from performance.

Migration is transactional. Fresh databases are created directly as schema v2.

The frozen Trainlog JSON v1 exchange format is unchanged.
