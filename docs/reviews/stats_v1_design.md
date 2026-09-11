# STATS_V1 preparation

STATS_V1 is not implemented and needs no schema change. Queries read completed
sessions only: `sessions.ended_at IS NOT NULL`, `session_exercises`,
`performed_sets`, `continuous_activity`, `max_results`, `exercise_body_zones`,
and `body_observations`. Drafts and planned targets are never frequency or
volume facts.

Proposed query boundary: date-windowed session count; completed exercise
occurrence count; performed-set count; continuous duration; per-exercise
working-weight observations; explicit MAX chronology; body-observation series;
and BODY ZONE/movement-pattern projections through the read-only knowledge
catalog. Windows use local calendar week/month derived from RFC3339 timestamps;
missing actual sets, duration, equipment, or load are omitted from that metric,
never coerced to zero.

Comparisons group first by `exercise_id`, then equipment context and load mode.
External resistance on different equipment IDs is not comparable; assistance is
not external resistance. A trend may label equipment-specific series but must
not blend them. Body-zone frequency counts completed occurrences/sets through
the existing primary/secondary hierarchy without double-counting parent and
child zones. Movement patterns are read-only knowledge projections; unresolved
exercises remain unclassified.

Android starts with overview, frequency, progress, exercise, zones,
mensurations and MAX cards; TUI uses the same query contract with tables and
small text sparklines. Validate empty windows, DST/date boundaries, duplicate
aliases, heterogeneous sets, continuous-only records, NULL equipment, planned
but unperformed targets, and fixture parity. Caches are future-only and require
an explicit schema tranche.
