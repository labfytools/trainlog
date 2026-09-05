# Trainlog Format

This directory contains machine-readable definitions of the Trainlog exchange format.

Current draft:

- `trainlog-v1.schema.json`

Canonical human-readable semantics live in:

- `docs/exchange_format.md`

The schema is a structural validator.

Semantic rules that JSON Schema cannot safely express remain documented and must be tested in application code.

Before Trainlog v1 is frozen, schema changes are allowed.

After `TRAINLOG_FORMAT_V1=FROZEN`, incompatible changes require a new version.
