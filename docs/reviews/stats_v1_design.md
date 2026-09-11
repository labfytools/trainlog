# STATS_V1 preparation

STATS_V1 needs no schema change. Observable history reads `session_exercises`,
`performed_sets`, `continuous_activity`, `max_results`, `exercise_body_zones`,
and `body_observations`. A stable session ID contributes once when it owns a
performed set, continuous activity, or explicit MAX, whether or not `ended_at`
is present. Drafts, planned targets, and empty occurrences are never frequency
or volume facts.

Implemented query boundary: date-windowed session count; completed exercise
occurrence count; performed-set count; continuous duration; per-exercise
working-weight observations; explicit MAX chronology; body-observation series;
and BODY ZONE/movement-pattern projections through the read-only knowledge
catalog. Windows use local calendar week/month derived from RFC3339 timestamps;
missing actual sets, duration, equipment, or load are omitted from that metric,
never coerced to zero.

Comparisons group first by canonical `exercise_id`, then equipment context and
load mode. Working classification additionally requires the exact performed
reps/duration dose and a later value above the prior best. Explicit MAX has its
own later-record-above-prior-MAX rule. Classes with fewer than two comparable
points produce no event, and the landing graph plots weekly event counts only.
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
