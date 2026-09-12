#!/usr/bin/env python3
"""Generate deterministic C tables and read/query functions from knowledge V1."""

from __future__ import annotations

import json
import sys
from pathlib import Path

from validate_training_knowledge import ValidationError, validate


def c(value) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, list):
        value = "\n".join(value)
    return json.dumps(value, ensure_ascii=False)


def interp(value) -> str:
    if value is None:
        return "{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL}"
    keys = ("family_description", "action_ids", "pattern_ids", "primary_muscle_ids",
            "secondary_muscle_ids", "stabilizer_muscle_ids", "primary_zone_id",
            "secondary_zone_ids", "confidence", "evidence_type", "source_refs",
            "variant_notes", "role_notes", "required_confirmation")
    return "{" + ",".join(c(value.get(key)) for key in keys) + "}"


def load(path: Path, key: str):
    return json.loads(path.read_text(encoding="utf-8"))[key]


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_training_knowledge.py CATALOG_DIR OUTPUT_C")
    root, output = Path(sys.argv[1]), Path(sys.argv[2])
    try:
        validate(root)
    except ValidationError as error:
        raise SystemExit(str(error)) from error
    refs = load(root / "science-references-v1.json", "references")
    muscles = load(root / "muscles-v1.json", "muscles")
    actions = load(root / "joint-actions-v1.json", "joint_actions")
    patterns = load(root / "movement-patterns-v1.json", "movement_patterns")
    exercises = load(root / "exercise-knowledge-v1.json", "exercises")
    equipment = load(root / "equipment-knowledge-v1.json", "equipment")
    relation_groups = load(root / "equipment-exercise-relations-v2.json", "equipment_relations")
    lines = ['#include "trainlog/training_knowledge.h"',
             '#include "trainlog/body_zone_catalog.h"', "#include <string.h>", ""]
    lines.append("static const TrainlogKnowledgeReference references[] = {")
    for row in refs:
        lines.append("  {%s}," % ",".join((c(row["ref_id"]), c(row["title"]),
            c(row["authors_or_organization"]), str(row["year"] or 0), c(row["type"]),
            c(row["url"]), c(row["doi"]), c(row["pmid"]), c(row["topics"]),
            c(row["notes"]), c(row["limitations"]), c(row["accessed_on"]),
            c(row.get("publication_note")))))
    lines.append("};")
    lines.append("static const TrainlogKnowledgeBodyZoneAudit audits[] = {")
    for row in exercises:
        audit = row["body_zone_audit"]
        interpretation = row["interpretation"] or row["conditional_interpretation"]
        fields = (row["exercise_id"], audit["status"], audit["severity"], audit["rationale"],
                  audit["confidence"], audit["source_refs"], audit["existing_primary_zone_id"],
                  audit["existing_secondary_zone_ids"],
                  None if interpretation is None else interpretation["primary_zone_id"],
                  [] if interpretation is None else interpretation["secondary_zone_ids"],
                  audit["proposed_mutation"])
        lines.append("  {%s}," % ",".join(c(value) for value in fields))
    lines.append("};")
    lines.append("static const TrainlogKnowledgeMuscle muscles[] = {")
    for row in muscles:
        lines.append("  {%s}," % ",".join(c(row.get(key)) for key in (
            "muscle_id", "display_name", "display_name_fr", "entity_type", "anatomical_group",
            "aggregate_group_id", "member_muscle_ids", "body_zone_ids", "joint_action_ids",
            "primary_actions", "primary_actions_semantics", "functional_notes", "confidence", "evidence_type", "source_refs",
            "overlap_warning")))
    lines.append("};")
    lines.append("static const TrainlogKnowledgeJointAction actions[] = {")
    for row in actions:
        lines.append("  {%s}," % ",".join(c(row.get(key)) for key in (
            "action_id", "display_name_fr", "definition", "anatomical_region", "joint_complex",
            "principal_plane", "plane_notes", "contributing_muscle_ids", "contributor_semantics", "confidence",
            "evidence_type", "source_refs", "notes")))
    lines.append("};")
    lines.append("static const TrainlogKnowledgeMovementPattern patterns[] = {")
    for row in patterns:
        lines.append("  {%s}," % ",".join(c(row.get(key)) for key in (
            "pattern_id", "display_name_fr", "definition", "parent_pattern_id", "typical_action_ids",
            "typical_body_zone_ids", "body_zone_semantics", "confidence", "evidence_type", "source_refs", "notes")))
    lines.append("};")
    lines.append("static const TrainlogKnowledgeInterpretation exercise_interpretations[] = {")
    for row in exercises:
        lines.append("  %s," % interp(row["interpretation"]))
    lines.append("};")
    lines.append("static const TrainlogKnowledgeInterpretation exercise_conditionals[] = {")
    for row in exercises:
        lines.append("  %s," % interp(row["conditional_interpretation"]))
    lines.append("};")
    lines.append("static const TrainlogExerciseKnowledge exercises[] = {")
    for index, row in enumerate(exercises):
        resolved = "&exercise_interpretations[%d]" % index if row["interpretation"] else "NULL"
        conditional = "&exercise_conditionals[%d]" % index if row["conditional_interpretation"] else "NULL"
        fields = [c(row.get(key)) for key in ("exercise_id", "exercise_name", "resolution_status",
            "confidence", "equipment_ids", "identity_evidence", "identity_evidence_type", "source_refs",
            "limitations", "equipment_link_status")]
        lines.append("  {%s,%s,%s}," % (",".join(fields), resolved, conditional))
    lines.append("};")
    lines.append("static const TrainlogEquipmentKnowledge equipment[] = {")
    for row in equipment:
        fields = [c(row.get(key)) for key in ("equipment_id", "manufacturer", "model", "identification_status",
            "scientific_status", "scientific_status_scope", "catalog_type", "catalog_load_semantics", "mechanics", "confidence",
            "evidence_type", "source_refs", "limitations", "audit_note")]
        fields.append("true" if row["requires_actual_exercise"] else "false")
        lines.append("  {%s}," % ",".join(fields))
    lines.append("};")
    relations = [(row["equipment_id"], option) for row in relation_groups
                 for option in row["exercise_options"]]
    lines.append("static const TrainlogEquipmentExerciseRelation equipment_exercise_relations[] = {")
    for equipment_id, option in relations:
        lines.append("  {%s}," % ",".join(c(value) for value in (
            equipment_id, option["exercise_id"], option["relation_type"],
            option["configuration_label"], option["confidence"], option["source_refs"])))
    lines.append("};")
    capabilities = [(row["equipment_id"], cap) for row in equipment for cap in row["capabilities"]]
    lines.append("static const TrainlogKnowledgeInterpretation capability_interpretations[] = {")
    for _, cap in capabilities:
        lines.append("  %s," % interp(cap["interpretation"]))
    lines.append("};")
    lines.append("static const TrainlogEquipmentCapability capabilities[] = {")
    for index, (equipment_id, cap) in enumerate(capabilities):
        pointer = "&capability_interpretations[%d]" % index if cap["interpretation"] else "NULL"
        lines.append("  {%s,%s,%s,%s,%s,%s}," % (c(equipment_id), c(cap["display_name"]),
            c(cap["exercise_ids"]), c(cap["link_status"]), c(cap["requirements"]), pointer))
    lines.append("};")
    lines.append(r'''
static bool list_has(const char *list, const char *id) {
  size_t length;
  const char *at;
  if (list == NULL || id == NULL || id[0] == '\0') return false;
  length = strlen(id);
  at = list;
  while (*at != '\0') {
    const char *end = strchr(at, '\n');
    size_t item_length = end == NULL ? strlen(at) : (size_t)(end - at);
    if (item_length == length && memcmp(at, id, length) == 0) return true;
    if (end == NULL) break;
    at = end + 1;
  }
  return false;
}
#define DEFINE_ACCESSORS(prefix,type,array,key) \
size_t prefix##_count(void) { return sizeof(array) / sizeof(array[0]); } \
const type *prefix##_at(size_t index) { return index < prefix##_count() ? &array[index] : NULL; } \
const type *prefix##_lookup(const char *id) { size_t i; if (id == NULL || id[0] == '\0') return NULL; \
  for (i=0; i<prefix##_count(); ++i) { if (strcmp(array[i].key,id)==0) return &array[i]; } \
  return NULL; }
DEFINE_ACCESSORS(trainlog_knowledge_reference, TrainlogKnowledgeReference, references, ref_id)
DEFINE_ACCESSORS(trainlog_knowledge_muscle, TrainlogKnowledgeMuscle, muscles, muscle_id)
DEFINE_ACCESSORS(trainlog_knowledge_joint_action, TrainlogKnowledgeJointAction, actions, action_id)
DEFINE_ACCESSORS(trainlog_knowledge_movement_pattern, TrainlogKnowledgeMovementPattern, patterns, pattern_id)
DEFINE_ACCESSORS(trainlog_exercise_knowledge, TrainlogExerciseKnowledge, exercises, exercise_id)
DEFINE_ACCESSORS(trainlog_knowledge_body_zone_audit, TrainlogKnowledgeBodyZoneAudit, audits, exercise_id)
DEFINE_ACCESSORS(trainlog_equipment_knowledge, TrainlogEquipmentKnowledge, equipment, equipment_id)
size_t trainlog_equipment_capability_count(void) { return sizeof(capabilities)/sizeof(capabilities[0]); }
const TrainlogEquipmentCapability *trainlog_equipment_capability_at(size_t index) {
  return index < trainlog_equipment_capability_count() ? &capabilities[index] : NULL;
}
size_t trainlog_equipment_exercise_relation_count(void) {
  return sizeof(equipment_exercise_relations)/sizeof(equipment_exercise_relations[0]);
}
const TrainlogEquipmentExerciseRelation *trainlog_equipment_exercise_relation_at(size_t index) {
  return index < trainlog_equipment_exercise_relation_count() ? &equipment_exercise_relations[index] : NULL;
}
TrainlogStatus trainlog_knowledge_list_exercises_for_equipment(const char *equipment_id,
    const TrainlogExerciseKnowledge **output, size_t capacity, size_t *output_count) {
  size_t i, count=0;
  if (output_count == NULL || (capacity > 0 && output == NULL)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
  *output_count=0;
  if (trainlog_equipment_knowledge_lookup(equipment_id) == NULL) return TRAINLOG_STATUS_NOT_FOUND;
  for (i=0; i<trainlog_equipment_exercise_relation_count(); ++i) {
    const TrainlogEquipmentExerciseRelation *r=&equipment_exercise_relations[i];
    if (strcmp(r->equipment_id,equipment_id)!=0) continue;
    if (count<capacity) output[count]=trainlog_exercise_knowledge_lookup(r->exercise_id);
    ++count;
  }
  *output_count=count; return count>capacity ? TRAINLOG_STATUS_INVALID_ARGUMENT : TRAINLOG_STATUS_OK;
}
TrainlogStatus trainlog_knowledge_list_equipment_for_exercise(const char *exercise_id,
    const TrainlogEquipmentKnowledge **output, size_t capacity, size_t *output_count) {
  size_t i, count=0;
  if (output_count == NULL || (capacity > 0 && output == NULL)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
  *output_count=0;
  if (trainlog_exercise_knowledge_lookup(exercise_id) == NULL) return TRAINLOG_STATUS_NOT_FOUND;
  for (i=0; i<trainlog_equipment_exercise_relation_count(); ++i) {
    const TrainlogEquipmentExerciseRelation *r=&equipment_exercise_relations[i];
    if (strcmp(r->exercise_id,exercise_id)!=0) continue;
    if (count<capacity) output[count]=trainlog_equipment_knowledge_lookup(r->equipment_id);
    ++count;
  }
  *output_count=count; return count>capacity ? TRAINLOG_STATUS_INVALID_ARGUMENT : TRAINLOG_STATUS_OK;
}
const TrainlogKnowledgeInterpretation *trainlog_exercise_knowledge_conditional(const char *exercise_id) {
  const TrainlogExerciseKnowledge *record = trainlog_exercise_knowledge_lookup(exercise_id);
  return record == NULL ? NULL : record->conditional_interpretation;
}
static bool zone_matches(const TrainlogKnowledgeInterpretation *value, const char *zone, bool descendants) {
  if (strcmp(value->primary_zone_id, zone) == 0 || list_has(value->secondary_zone_ids, zone)) return true;
  if (!descendants) return false;
  if (trainlog_body_zone_catalog_is_descendant(value->primary_zone_id, zone)) return true;
  { const char *at = value->secondary_zone_ids;
    while (at != NULL && *at != '\0') { const char *end = strchr(at, '\n'); char id[64];
      size_t n = end == NULL ? strlen(at) : (size_t)(end-at); if (n >= sizeof(id)) return false;
      memcpy(id,at,n); id[n]='\0'; if (trainlog_body_zone_catalog_is_descendant(id,zone)) return true;
      if (end == NULL) break;
      at=end+1; }
  }
  return false;
}
TrainlogStatus trainlog_knowledge_list_equipment_for_body_zone(const char *zone_id, bool descendants,
    const TrainlogEquipmentKnowledge **output, size_t capacity, size_t *output_count) {
  size_t i, count=0;
  if (output_count == NULL || (capacity > 0 && output == NULL)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
  *output_count=0;
  if (trainlog_body_zone_catalog_lookup(zone_id) == NULL) return TRAINLOG_STATUS_NOT_FOUND;
  /* CONTRACT: zone -> resolved exercise anatomy -> physical relation. Machine
   * labels never participate in this derivation. */
  for (i=0; i<trainlog_equipment_knowledge_count(); ++i) {
    const TrainlogEquipmentKnowledge *eq=&equipment[i]; size_t j; bool matched=false;
    for (j=0; j<trainlog_equipment_exercise_relation_count(); ++j) {
      const TrainlogEquipmentExerciseRelation *r=&equipment_exercise_relations[j];
      const TrainlogExerciseKnowledge *e;
      if (strcmp(r->equipment_id,eq->equipment_id)!=0) continue;
      e=trainlog_exercise_knowledge_lookup(r->exercise_id);
      if (e!=NULL && e->interpretation!=NULL && zone_matches(e->interpretation,zone_id,descendants)) { matched=true; break; }
    }
    if (!matched) continue;
    if (count<capacity) output[count]=eq;
    ++count;
  }
  *output_count=count; return count>capacity ? TRAINLOG_STATUS_INVALID_ARGUMENT : TRAINLOG_STATUS_OK;
}
TrainlogStatus trainlog_exercise_knowledge_query(const TrainlogKnowledgeQuery *query,
    const TrainlogExerciseKnowledge **output, size_t capacity, size_t *output_count) {
  size_t i, count=0; const TrainlogKnowledgeQuery empty={0};
  if (output_count == NULL || (capacity > 0 && output == NULL)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
  *output_count=0; if (query == NULL) query=&empty;
  if (query->muscle_role < TRAINLOG_KNOWLEDGE_ROLE_ANY || query->muscle_role > TRAINLOG_KNOWLEDGE_ROLE_STABILIZER)
    return TRAINLOG_STATUS_INVALID_ARGUMENT;
  /* INVARIANT: a specific muscle role is meaningful only with a muscle ID.
   * Keep the C query contract aligned with Android's paired filters. */
  if (query->muscle_id == NULL && query->muscle_role != TRAINLOG_KNOWLEDGE_ROLE_ANY)
    return TRAINLOG_STATUS_INVALID_ARGUMENT;
  if (query->scientific_zone_id != NULL && trainlog_body_zone_catalog_lookup(query->scientific_zone_id) == NULL)
    return TRAINLOG_STATUS_NOT_FOUND;
  if (query->movement_pattern_id != NULL && trainlog_knowledge_movement_pattern_lookup(query->movement_pattern_id) == NULL)
    return TRAINLOG_STATUS_NOT_FOUND;
  if (query->muscle_id != NULL && trainlog_knowledge_muscle_lookup(query->muscle_id) == NULL)
    return TRAINLOG_STATUS_NOT_FOUND;
  if (query->available_equipment_id != NULL && trainlog_equipment_knowledge_lookup(query->available_equipment_id) == NULL)
    return TRAINLOG_STATUS_NOT_FOUND;
  for (i=0; i<trainlog_exercise_knowledge_count(); ++i) { const TrainlogExerciseKnowledge *e=&exercises[i];
    const TrainlogKnowledgeInterpretation *v=e->interpretation; bool muscle_ok=true;
    if (v == NULL) continue;
    if (query->scientific_zone_id != NULL && !zone_matches(v,query->scientific_zone_id,query->include_zone_descendants)) continue;
    if (query->movement_pattern_id != NULL && !list_has(v->pattern_ids,query->movement_pattern_id)) continue;
    if (query->available_equipment_id != NULL && !list_has(e->equipment_ids,query->available_equipment_id)) continue;
    if (query->muscle_id != NULL) {
      switch (query->muscle_role) {
        case TRAINLOG_KNOWLEDGE_ROLE_PRIMARY: muscle_ok=list_has(v->primary_muscle_ids,query->muscle_id); break;
        case TRAINLOG_KNOWLEDGE_ROLE_SECONDARY: muscle_ok=list_has(v->secondary_muscle_ids,query->muscle_id); break;
        case TRAINLOG_KNOWLEDGE_ROLE_STABILIZER: muscle_ok=list_has(v->stabilizer_muscle_ids,query->muscle_id); break;
        case TRAINLOG_KNOWLEDGE_ROLE_ANY: muscle_ok=list_has(v->primary_muscle_ids,query->muscle_id) ||
          list_has(v->secondary_muscle_ids,query->muscle_id) || list_has(v->stabilizer_muscle_ids,query->muscle_id); break;
      }
    }
    if (!muscle_ok) continue;
    if (count < capacity) output[count]=e;
    ++count;
  }
  *output_count=count; return count > capacity ? TRAINLOG_STATUS_INVALID_ARGUMENT : TRAINLOG_STATUS_OK;
}
''')
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
