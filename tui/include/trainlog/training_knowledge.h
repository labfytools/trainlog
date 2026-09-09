#ifndef TRAINLOG_TRAINING_KNOWLEDGE_H
#define TRAINLOG_TRAINING_KNOWLEDGE_H

/**
 * @file training_knowledge.h
 * @brief Immutable, evidence-linked TRAINING KNOWLEDGE V1 catalog API.
 *
 * All returned records and strings are borrowed from generated const storage
 * and remain valid for process lifetime. Newline-separated ID fields contain
 * exact stable IDs; labels are never used as identities.
 */

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/status.h"

typedef enum TrainlogKnowledgeMuscleRole {
    TRAINLOG_KNOWLEDGE_ROLE_ANY = 0,
    TRAINLOG_KNOWLEDGE_ROLE_PRIMARY,
    TRAINLOG_KNOWLEDGE_ROLE_SECONDARY,
    TRAINLOG_KNOWLEDGE_ROLE_STABILIZER
} TrainlogKnowledgeMuscleRole;

typedef struct TrainlogKnowledgeReference {
    const char *ref_id;
    const char *title;
    const char *authors_or_organization;
    int year; /* zero means the catalog explicitly records an unknown year */
    const char *type;
    const char *url;
    const char *doi;
    const char *pmid;
    const char *topics;
    const char *notes;
    const char *limitations;
    const char *accessed_on;
    const char *publication_note;
} TrainlogKnowledgeReference;

typedef struct TrainlogKnowledgeMuscle {
    const char *muscle_id;
    const char *display_name;
    const char *display_name_fr;
    const char *entity_type;
    const char *anatomical_group;
    const char *aggregate_group_id;
    const char *member_muscle_ids;
    const char *body_zone_ids;
    const char *joint_action_ids;
    const char *primary_actions;
    const char *primary_actions_semantics;
    const char *functional_notes;
    const char *confidence;
    const char *evidence_type;
    const char *source_refs;
    const char *overlap_warning;
} TrainlogKnowledgeMuscle;

typedef struct TrainlogKnowledgeJointAction {
    const char *action_id;
    const char *display_name_fr;
    const char *definition;
    const char *anatomical_region;
    const char *joint_complex;
    const char *principal_plane;
    const char *plane_notes;
    const char *contributing_muscle_ids;
    const char *contributor_semantics;
    const char *confidence;
    const char *evidence_type;
    const char *source_refs;
    const char *notes;
} TrainlogKnowledgeJointAction;

typedef struct TrainlogKnowledgeMovementPattern {
    const char *pattern_id;
    const char *display_name_fr;
    const char *definition;
    const char *parent_pattern_id;
    const char *typical_action_ids;
    const char *typical_body_zone_ids;
    const char *body_zone_semantics;
    const char *confidence;
    const char *evidence_type;
    const char *source_refs;
    const char *notes;
} TrainlogKnowledgeMovementPattern;

typedef struct TrainlogKnowledgeInterpretation {
    const char *family_description;
    const char *action_ids;
    const char *pattern_ids;
    const char *primary_muscle_ids;
    const char *secondary_muscle_ids;
    const char *stabilizer_muscle_ids;
    const char *primary_zone_id;
    const char *secondary_zone_ids;
    const char *confidence;
    const char *evidence_type;
    const char *source_refs;
    const char *variant_notes;
    const char *role_notes;
    const char *required_confirmation;
} TrainlogKnowledgeInterpretation;

typedef struct TrainlogExerciseKnowledge {
    const char *exercise_id;
    const char *exercise_name;
    const char *resolution_status;
    const char *confidence;
    const char *equipment_ids;
    const char *identity_evidence;
    const char *identity_evidence_type;
    const char *source_refs;
    const char *limitations;
    const char *equipment_link_status;
    const TrainlogKnowledgeInterpretation *interpretation;
    const TrainlogKnowledgeInterpretation *conditional_interpretation;
} TrainlogExerciseKnowledge;

/* WHY: the authored BODY ZONES review is part of the immutable scientific
 * catalog contract. Exposing every field prevents clients from substituting
 * the resolved interpretation for the review decision. All pointers are
 * borrowed from generated storage and remain valid for process lifetime. */
typedef struct TrainlogKnowledgeBodyZoneAudit {
    const char *exercise_id;
    const char *status;
    const char *severity;
    const char *rationale;
    const char *confidence;
    const char *source_refs;
    const char *existing_primary_zone_id;
    const char *existing_secondary_zone_ids;
    const char *scientific_primary_zone_id;
    const char *scientific_secondary_zone_ids;
    const char *proposed_mutation;
} TrainlogKnowledgeBodyZoneAudit;

typedef struct TrainlogEquipmentKnowledge {
    const char *equipment_id;
    const char *manufacturer;
    const char *model;
    const char *identification_status;
    const char *scientific_status;
    const char *scientific_status_scope;
    const char *catalog_type;
    const char *catalog_load_semantics;
    const char *mechanics;
    const char *confidence;
    const char *evidence_type;
    const char *source_refs;
    const char *limitations;
    const char *audit_note;
    bool requires_actual_exercise;
} TrainlogEquipmentKnowledge;

typedef struct TrainlogEquipmentCapability {
    const char *equipment_id;
    const char *display_name;
    const char *exercise_ids;
    const char *link_status;
    const char *requirements;
    const TrainlogKnowledgeInterpretation *interpretation;
} TrainlogEquipmentCapability;

typedef struct TrainlogKnowledgeQuery {
    const char *scientific_zone_id;
    bool include_zone_descendants;
    const char *movement_pattern_id;
    const char *muscle_id;
    TrainlogKnowledgeMuscleRole muscle_role;
    const char *available_equipment_id;
} TrainlogKnowledgeQuery;

size_t trainlog_knowledge_reference_count(void);
const TrainlogKnowledgeReference *trainlog_knowledge_reference_at(size_t index);
const TrainlogKnowledgeReference *trainlog_knowledge_reference_lookup(const char *ref_id);
size_t trainlog_knowledge_muscle_count(void);
const TrainlogKnowledgeMuscle *trainlog_knowledge_muscle_at(size_t index);
const TrainlogKnowledgeMuscle *trainlog_knowledge_muscle_lookup(const char *muscle_id);
size_t trainlog_knowledge_joint_action_count(void);
const TrainlogKnowledgeJointAction *trainlog_knowledge_joint_action_at(size_t index);
const TrainlogKnowledgeJointAction *trainlog_knowledge_joint_action_lookup(const char *action_id);
size_t trainlog_knowledge_movement_pattern_count(void);
const TrainlogKnowledgeMovementPattern *trainlog_knowledge_movement_pattern_at(size_t index);
const TrainlogKnowledgeMovementPattern *trainlog_knowledge_movement_pattern_lookup(const char *pattern_id);
size_t trainlog_exercise_knowledge_count(void);
const TrainlogExerciseKnowledge *trainlog_exercise_knowledge_at(size_t index);
const TrainlogExerciseKnowledge *trainlog_exercise_knowledge_lookup(const char *exercise_id);
/* at() returns NULL outside the immutable snapshot; lookup() returns NULL for
 * NULL, empty, or unknown IDs. Neither function transfers ownership. */
size_t trainlog_knowledge_body_zone_audit_count(void);
const TrainlogKnowledgeBodyZoneAudit *trainlog_knowledge_body_zone_audit_at(size_t index);
const TrainlogKnowledgeBodyZoneAudit *trainlog_knowledge_body_zone_audit_lookup(const char *exercise_id);
/* Only this explicitly named accessor exposes conditional candidate data. */
const TrainlogKnowledgeInterpretation *trainlog_exercise_knowledge_conditional(const char *exercise_id);
size_t trainlog_equipment_knowledge_count(void);
const TrainlogEquipmentKnowledge *trainlog_equipment_knowledge_at(size_t index);
const TrainlogEquipmentKnowledge *trainlog_equipment_knowledge_lookup(const char *equipment_id);
size_t trainlog_equipment_capability_count(void);
const TrainlogEquipmentCapability *trainlog_equipment_capability_at(size_t index);

/**
 * AND-combine optional resolved-knowledge filters in stable exercise-ID order.
 * muscle_id may be NULL only when muscle_role is ROLE_ANY; a specific role
 * and muscle_id must be supplied together. Unknown filter IDs return NOT_FOUND. Malformed arguments or insufficient
 * capacity return INVALID_ARGUMENT. output_count always receives the required
 * count after valid filters are resolved, so truncation is never reported as
 * success. Conditional and unresolved records can never match this function.
 */
TrainlogStatus trainlog_exercise_knowledge_query(
    const TrainlogKnowledgeQuery *query,
    const TrainlogExerciseKnowledge **output,
    size_t capacity,
    size_t *output_count
);

#endif
