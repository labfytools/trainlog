# Trainlog documentation

This file is the canonical documentation index. Canonical documents describe
the current contract or state; `reviews/` and `CHANGELOG.md` retain historical
evidence and chronology.

## Start here

- [Current state](current_state.md) owns present schemas, capabilities,
  validation, and active limitations.
- [Architecture](architecture.md) owns component, process, storage, transport,
  and responsibility boundaries.
- [Roadmap](roadmap.md) owns the current cursor and future work.

## Interfaces

- [Web Exercises V1](web_exercises.md) owns desktop catalogue administration,
  optimistic editing, historical preservation, causal retirement, and its
  existing-artifact synchronization boundary.
- [Web Sessions V1](web_sessions.md) owns the implemented preparation, resume,
  history and Android-delivery workflow.
- [Program format V1](program_format_v1.md) owns the strict desktop planning
  import contract and program lifecycle used by the Sessions Programs subtab.

- [Android](android.md) owns field-companion capture, local persistence,
  glanceable summaries, permissions, and Android interaction rules.
- [Desktop TUI](tui.md) owns Notcurses navigation, correction, detailed
  consultation, analytics, graphs, and long-term follow-up behavior.
- [Architecture](architecture.md) owns the implemented and future boundaries of
  the local Web sibling, including Core/API boundaries, loopback networking, shell,
  Dashboard, layout, build/runtime separation, and security invariants.

## Data model

- [Exercise data model](exercise_data_model.md) owns exercise profiles,
  occurrence identity, planned-versus-actual work, load, and MAX semantics.
- [Database](database.md) owns desktop SQLite tables, constraints, transactions,
  and migrations.
- [Training feedback](training_feedback.md) owns immediate feedback, immutable
  revisions, and J+1/session follow-up semantics.
- [Domain documentation](domain/) owns cited anatomy, biomechanics, equipment
  interpretation, training knowledge, and session-generation science.

## Synchronization and exchange

- [Synchronization exchange](sync_exchange.md) owns the active MTP workflow,
  artifact direction, replay, publication, receipts, and daemon behavior.
- [Complete synchronization gap contract](design/sync_gap_contract_v1.md)
  freezes the future lifecycle, causal deletion, generation, consumption,
  acknowledgement and Web-service requirements. It is explicitly not an
  implemented wire format.
- [Exchange formats](exchange_format.md) owns frozen Trainlog JSON V1 and
  separately versioned contracts, including AI exchange definitions.

## Analytics

- [Desktop TUI](tui.md) owns visible statistics, MAX, body analytics, calendar
  buckets, and graph behavior.
- [Current state](current_state.md) records which analytics are implemented.
- [Exercise data model](exercise_data_model.md) owns the distinctions between
  measured MAX, performed work, plans, and estimates.

## Development

- [Tests and validation](tests.md) owns durable build, test, sanitizer,
  contract, and hardware-validation commands.
- [Coding style](coding_style.md) owns C17/Kotlin style and comment rules.
- [Development contract](../AGENTS.md) owns frozen boundaries, invariants,
  validation policy, and safe device-testing policy.
- [Android build notes](../android/README.md) are the Android setup entry point.
- [Format material](../format/README.md) indexes frozen schema assets.

## Design

- [Design records](design/) retain proposals and design-gate evidence. The
  synchronization gap contract is normative for future implementation; design
  records do not override `current_state.md` for implemented status.

## Reviews and history

- [Review records](reviews/) retain historical audits, incidents, checkpoints,
  and validation evidence. Their status and paths are historical.
- [Programs presentation / Android delete evidence](reviews/programs_presentation_android_delete_v1_evidence.md)
  records the implemented, not-deployed validation checkpoint and real-browser
  screenshot evidence.
- [Changelog](../CHANGELOG.md) owns chronological change history.
- [Release notes](releases/) retain reusable, tag-scoped English source text
  for public GitHub and Forgejo releases.

## Ownership rule

Do not copy long normative explanations between documents. The repository
`README.md` is the entry point, this file is the index,
`current_state.md` is the snapshot, and `roadmap.md` is future-oriented.
