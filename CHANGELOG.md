# Changelog

All notable changes to Trainlog will be documented in this file.

The project uses a simple pre-release changelog during early development.

## Unreleased

### Added

- Initial repository structure and development contract.
- Architecture, coding-style, database, Android, TUI, testing, and roadmap documentation.
- Trainlog v1 JSON Schema draft.
- Structural and semantic Trainlog validator.
- Positive and negative exchange-format fixtures.
- Gate 0 review and closure records.
- Gate 1 exchange-format freeze-candidate review.
- Gate 1 exercise-identity collision review.
- Stable exercise tracking modes.
- Explicit load modes for no load, external resistance, and assistance.
- Optional bounded session and exercise notes.
- Extended body measurement list.
- Executable local catalog reconciliation contract.
- UUIDv4 generation policy for new Trainlog exercise and session IDs.

### Changed

- Gate 0 project contract is complete.
- Gate 1 completed and Trainlog exchange format v1 frozen.
- Actual repetition count may be zero for a failed attempt.
- Planned exercises may contain zero actual sets.
- Top-level exercise metadata must exactly match session exercise references.
- Assistance kilograms have distinct semantics from external resistance.
- Different exercise IDs with equivalent normalized names are hard import conflicts.
- Same exercise ID with incompatible tracking mode is a hard import conflict.
- `TRAINLOG_FORMAT_V1=FROZEN`; incompatible changes require a new format version.
