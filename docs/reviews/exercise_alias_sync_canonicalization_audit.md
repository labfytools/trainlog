# Exercise alias sync canonicalization audit

Date: 2026-09-11

Scope: actual Android/desktop sync artifacts and the production import/export
paths which carry an exercise identity. This is an engineering review artifact,
not a format or architecture change.

## Identity rule

`exercise_aliases` contains flattened compatibility identities. A source ID is
accepted as input identity and resolves to its live canonical exercise before
identity/conflict comparison. It must not recreate an exercise row. Catalogue,
session, body-zone, and equipment exports publish live canonical exercise IDs;
the alias companion is the only output which publishes retired source IDs.

## Surface matrix

| Direction / artifact | Producer or consumer | Incoming/working treatment | Exported identity | Audit result |
|---|---|---|---|---|
| Android → PC `trainlog-exercise-aliases-v1.json` | `tools/import_exercise_aliases.py` | Validates sorted, flattened A→B mappings; moves references from a compatible live A to B and deletes A before persisting the alias | n/a | Canonical; establishes durable identity before other imports |
| Android → PC `trainlog-mobile-export-v1/v2/v3.json` exercise catalogue | `tools/import_mobile_export.py:import_exercises` | Resolves a persisted source alias before normalized-name reconciliation and builds raw-input-ID → canonical-ID mapping | n/a | Canonical; no source resurrection |
| Android → PC mobile session occurrences | `tools/import_mobile_export.py:import_sessions` and `session_semantically_matches` | Uses the catalogue mapping before occurrence insertion and replay identity/content comparison | n/a | Canonical; preserves session ID and entry ID |
| Android → PC `trainlog-exercise-body-zones-v1.json` | `tools/import_exercise_body_zones.py` | Resolves each input ID to one exercise row, groups equal A/B claims by canonical row, rejects divergent claims before writes | n/a | Canonical and order-independent |
| Android → PC `trainlog-equipment-associations-v2.json` | `tools/import_equipment_associations.py` | **Previously compared raw incoming A with stored B.** Now resolves both through the flattened alias before corroborating `(session_id, entry_id)`; distinct live canonical IDs and equipment divergence remain conflicts | n/a | Repaired |
| PC → Android `trainlog-exercise-aliases-v1.json` | `tools/export_exercise_aliases.py` | Reads the flattened compatibility table | A→B by definition | Correct exception: sole retired-ID publication |
| PC → Android `trainlog-pc-catalog-v1.json` | `tools/export_pc_catalog.py` | Reads live `exercises` rows after merge removes A | B | Canonical |
| PC → Android `trainlog-pc-mobile-export-v2/v3.json` catalogue and occurrences | `tools/export_pc_mobile.py` | Reads live exercise rows and occurrence FKs joined to those rows | B | Canonical; entry/session identities unchanged |
| Bidirectional `trainlog-exercise-body-zones-v1.json` export | `tools/export_exercise_body_zones.py` | Reads body-zone state joined to live exercises | B | Canonical |
| Bidirectional `trainlog-equipment-associations-v2.json` export | `tools/export_equipment_associations.py` | Reads occurrences joined to live exercises | B | Canonical; equipment identity unchanged |
| PC → Android alias companion | `TrainlogRepository.applyExerciseAliasesJson` | Imports/merges aliases before the catalogue; moves session/draft/metadata references and removes retired source | n/a | Canonical; no source resurrection |
| PC → Android catalogue | `TrainlogRepository.applyPcCatalogJson` | Calls `resolveExerciseId` before lookup/reconciliation | n/a | Canonical |
| PC → Android mobile session V2/V3 | `TrainlogRepository.applyPcMobileExportJson` | Resolves catalogue and occurrence exercise IDs before row lookup, replay checks, and resumed-MAX checks | n/a | Canonical; entry/session identities unchanged |
| PC → Android body-zone companion | `TrainlogRepository.applyExerciseBodyZonesJson` | Resolves input ID before row/baseline lookup | n/a | Canonical |
| PC → Android equipment companion V1/V2 | `TrainlogRepository.applyPcEquipmentAssociationsJson` | Resolves input ID before occurrence lookup/corroboration | n/a | Canonical; genuine exercise/equipment conflicts rejected |
| Android → PC mobile/session, body-zone, and equipment producers | `TrainlogRepository.buildMobileExportV3Json`, `buildExerciseBodyZonesJson`, and `buildEquipmentAssociationsJson` | Read live exercise rows referenced by catalogue/occurrence FKs | B | Canonical |
| Android → PC alias producer | `TrainlogRepository.buildExerciseAliasesJson` | Reads the flattened compatibility table | A→B by definition | Correct exception: sole retired-ID publication |

## Root cause and repaired precondition

The desktop equipment V2 consumer looked up the occurrence correctly by stable
`(session_id, entry_id)`, but then compared `exists[0] != exercise_id` using raw
exercise-ID strings. Its only fallback required a selected V2 mobile proof and
the incoming exercise row to have disappeared. Production `trainlog_sync_run()`
does not pass that optional proof to this importer, and current Android publishes
V3, so a valid persistent A→B alias still failed as `conflit exercice
association`.

The repaired precondition is equality after durable alias resolution. The V2
proof fallback remains bounded to historic reconciliation without a persistent
alias. No import path changes a session ID, entry ID, equipment ID, occurrence,
schema, or artifact version.

## Regression coverage

`tests/test_equipment_associations_exchange.py` covers direct A→B acceptance
without V2 proof, no source recreation, and rejection of an unrelated live
canonical exercise.

`tui/tests/test_sync_body_zone_wiring.c` drives the real
`trainlog_sync_run(TRAINLOG_SYNC_BIDIRECTIONAL)` orchestration against mocked
MTP and temporary storage. It uses live
`ex_2d488c08-194c-4051-a3c9-34471646c1d3` plus persistent alias
`ex_43c7375f-934c-4650-930c-45807d2f2929` → live ID, imports historical A/B
session and companion rows, replays the run, checks both stable entries remain
bound to B, and verifies that all non-alias outbound artifacts contain B and do
not contain A.
