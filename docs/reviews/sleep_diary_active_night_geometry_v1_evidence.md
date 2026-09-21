# Sleep Diary active-night and Agenda geometry evidence

Date: 2026-09-21

Branch: `trainlog-0.1.4-sleep-diary-v1`

## Scope

The Web Sleep Diary uses a single timestamp projection from 18:00 on the
selected start date to 18:00 on the following date. Hour ticks, interval edges,
point events and grouped medication markers use that same rendered timeline
box. The editor clears date-dependent state immediately when the selected
night changes and applies an asynchronous diary response only while its request
identity and requested date still match the active night.

The local editor retains user-facing Draft, Day validated and Modified states.
Synchronization transport state remains available to the existing backend
contracts but is not presented as a per-night action or status.

## Retained Firefox evidence

- [Populated aligned night](evidence/sleep-diary-alignment-firefox.png): a
  synthetic night with events at 22:30, 03:30, 04:45 and 15:30. Browser
  assertions compare the rendered marker/interval coordinates with the shared
  axis at a maximum tolerance of one pixel.
- [Empty selected night](evidence/sleep-diary-empty-night-firefox.png): after
  changing dates without reload, the Agenda, chronology, appreciations,
  medication intakes, notes and factual summary contain no data from the
  preceding night. No empty entry is persisted by viewing the date.

The same scenario returns to the populated night without reload and verifies
that its persisted events reappear.
