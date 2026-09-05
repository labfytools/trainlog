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
- Stable exercise tracking modes.
- Explicit load modes for no load, external resistance, and assistance.
- Optional bounded session and exercise notes.
- Extended body measurement list.

### Changed

- Gate 0 project contract is complete.
- Gate 1 is the active format-freeze gate.
- Actual repetition count may be zero for a failed attempt.
- Planned exercises may contain zero actual sets.
- Top-level exercise metadata must exactly match the session exercise references.
- Assistance kilograms have distinct semantics from external resistance.
- `TRAINLOG_FORMAT_V1` remains draft pending Gate 1 validation and mirrored review.
