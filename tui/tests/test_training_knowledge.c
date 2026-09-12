#include "trainlog/training_knowledge.h"

#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", \
    __FILE__, __LINE__, #test); return 1; } } while (0)

static int contains(const TrainlogExerciseKnowledge *const *rows, size_t count, const char *id)
{
    size_t index;
    for (index = 0; index < count; ++index) {
        if (strcmp(rows[index]->exercise_id, id) == 0) return 1;
    }
    return 0;
}

int main(void)
{
    const char *const rear_delt = "ex_4cd2433e-80b1-478a-b8df-73fc6ef80962";
    const char *const chest_press = "ex_8552dd77-fcd7-4f06-a1cc-d956eb1009af";
    const char *const leg_press = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde";
    const char *const rotary = "ex_1a34814c-2e46-40fc-b1f4-6d60b8e5a3e0";
    const TrainlogExerciseKnowledge *rows[32];
    const TrainlogExerciseKnowledge *record;
    const TrainlogKnowledgeInterpretation *conditional;
    TrainlogKnowledgeQuery query = {0};
    size_t count = 0U;
    size_t index;
    int rotary_capability_found = 0;
    const TrainlogKnowledgeBodyZoneAudit *audit;
    const TrainlogEquipmentKnowledge *equipment_rows[32];

    CHECK(trainlog_knowledge_reference_count() == 23U);
    CHECK(trainlog_knowledge_muscle_count() == 53U);
    CHECK(trainlog_knowledge_joint_action_count() == 35U);
    CHECK(trainlog_knowledge_movement_pattern_count() == 26U);
    CHECK(trainlog_exercise_knowledge_count() == 23U);
    CHECK(trainlog_equipment_knowledge_count() == 41U);
    CHECK(trainlog_knowledge_body_zone_audit_count() == trainlog_exercise_knowledge_count());
    audit = trainlog_knowledge_body_zone_audit_lookup(leg_press);
    CHECK(audit != NULL && strcmp(audit->status, "confirmed") == 0);
    CHECK(audit->rationale[0] != '\0' && audit->source_refs[0] != '\0');
    audit = trainlog_knowledge_body_zone_audit_lookup(chest_press);
    CHECK(audit != NULL && strcmp(audit->status, "questionable") == 0);
    audit = trainlog_knowledge_body_zone_audit_lookup("ex_2d488c08-194c-4051-a3c9-34471646c1d3");
    CHECK(audit != NULL && strcmp(audit->status, "unresolved") == 0);
    CHECK(trainlog_knowledge_body_zone_audit_at(trainlog_knowledge_body_zone_audit_count()) == NULL);
    CHECK(trainlog_knowledge_body_zone_audit_lookup(NULL) == NULL);
    CHECK(trainlog_knowledge_muscle_lookup("pectoralis_major") != NULL);
    CHECK(trainlog_knowledge_joint_action_lookup("shoulder_horizontal_abduction") != NULL);
    CHECK(trainlog_knowledge_movement_pattern_lookup("horizontal_pull") != NULL);
    CHECK(trainlog_knowledge_reference_lookup("openstax_upper") != NULL);
    CHECK(trainlog_equipment_knowledge_lookup("rear_delt_pec_fly") != NULL);
    CHECK(trainlog_exercise_knowledge_lookup("ex_unknown") == NULL);
    CHECK(trainlog_equipment_exercise_relation_count() == 17U);
    CHECK(trainlog_knowledge_list_exercises_for_equipment("leg_press", rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && strcmp(rows[0]->exercise_id, leg_press) == 0);
    CHECK(trainlog_knowledge_list_exercises_for_equipment("functional_trainer", rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 0U);
    CHECK(trainlog_knowledge_list_exercises_for_equipment("assisted_dip_chin_machine", rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 0U);
    CHECK(trainlog_knowledge_list_exercises_for_equipment("seated_leg_curl", rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && strcmp(rows[0]->exercise_id, "ex_a1ef5047-b44b-4c64-a6ed-c7a3bc13b163") == 0);
    CHECK(trainlog_knowledge_list_equipment_for_exercise(leg_press, equipment_rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 2U && strcmp(equipment_rows[0]->equipment_id, "leg_press") == 0);
    CHECK(trainlog_knowledge_list_equipment_for_body_zone("chest", true, equipment_rows, 32U, &count) == TRAINLOG_STATUS_OK);
    for (index = 0U; index < count; ++index)
        CHECK(strcmp(equipment_rows[index]->equipment_id, "rear_delt_pec_fly") != 0);

    record = trainlog_exercise_knowledge_lookup(rear_delt);
    CHECK(record != NULL && record->interpretation != NULL);
    CHECK(record->conditional_interpretation == NULL);
    CHECK(strcmp(record->interpretation->primary_zone_id, "shoulders") == 0);

    record = trainlog_exercise_knowledge_lookup(chest_press);
    CHECK(record != NULL && record->interpretation == NULL);
    conditional = trainlog_exercise_knowledge_conditional(chest_press);
    CHECK(conditional != NULL);
    CHECK(strcmp(conditional->family_description, "machine_chest_press") == 0);

    query.muscle_id = "quadriceps";
    query.muscle_role = TRAINLOG_KNOWLEDGE_ROLE_PRIMARY;
    query.available_equipment_id = "leg_press";
    CHECK(trainlog_exercise_knowledge_query(&query, rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && strcmp(rows[0]->exercise_id, leg_press) == 0);

    query = (TrainlogKnowledgeQuery){0};
    query.muscle_role = TRAINLOG_KNOWLEDGE_ROLE_PRIMARY;
    CHECK(trainlog_exercise_knowledge_query(&query, rows, 32U, &count) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);

    query = (TrainlogKnowledgeQuery){0};
    query.scientific_zone_id = "upper_body";
    query.include_zone_descendants = true;
    CHECK(trainlog_exercise_knowledge_query(&query, rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(contains(rows, count, rear_delt));
    CHECK(!contains(rows, count, chest_press));

    query = (TrainlogKnowledgeQuery){0};
    query.movement_pattern_id = "horizontal_pull";
    CHECK(trainlog_exercise_knowledge_query(&query, rows, 0U, &count) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(count > 0U);
    CHECK(trainlog_exercise_knowledge_query(&query, rows, 32U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count >= 2U);
    query.movement_pattern_id = "not_a_pattern";
    CHECK(trainlog_exercise_knowledge_query(&query, rows, 32U, &count) == TRAINLOG_STATUS_NOT_FOUND);

    for (index = 0; index < trainlog_equipment_capability_count(); ++index) {
        const TrainlogEquipmentCapability *capability = trainlog_equipment_capability_at(index);
        if (strcmp(capability->equipment_id, "rotary_torso") == 0 &&
                strstr(capability->exercise_ids, rotary) != NULL) {
            rotary_capability_found = 1;
            CHECK(strcmp(capability->link_status,
                         "catalog_compatible_not_observed_occurrence") == 0);
        }
    }
    /* Catalog compatibility is static metadata; the API contains no occurrence
     * row and therefore cannot fabricate performance history. */
    CHECK(rotary_capability_found);
    return 0;
}
