# Changelog

All notable changes to Trainlog will be documented in this file.

The project uses a simple pre-release changelog during early development.

## Unreleased

### Added

- Initial repository structure.
- Development contract.
- Architecture documentation.
- Coding-style rules.
- Exchange-format v1 draft.
- JSON Schema draft for Trainlog v1.
- Initial example workout export.
- Database, TUI, Android, testing, and roadmap documentation.
- Trainlog semantic JSON validator.
- Positive and negative exchange-format fixtures.
- Gate 0 review #1 report.

### Changed

- `ended_at` is optional for active or interrupted sessions.
- Repetition and timed exercise modes are now mutually exclusive.
- Exercise display-name anti-duplication semantics are defined.
- Session/catalog cross-reference rules are executable.
- Timestamp offset and chronology rules are executable.
- Unknown fields are explicitly rejected in v1.
