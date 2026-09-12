#include "trainlog/training_context.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void trainlog_training_exercise_context_release(TrainlogTrainingExerciseContext *context)
{
    if (context == NULL) return;
    free(context->persisted_zones);
    free(context->compatible_equipment);
    free(context->occurrences);
    free(context->sets);
    (void)memset(context, 0, sizeof(*context));
}

TrainlogStatus trainlog_training_exercise_context_load(
    TrainlogDatabase *database, const char *exercise_id, size_t occurrence_limit,
    size_t set_preview_limit, TrainlogTrainingExerciseContext *output)
{
    TrainlogTrainingExerciseContext result = {0};
    TrainlogStatus status;
    char canonical_exercise_id[TRAINLOG_ID_MAX + 1U];
    size_t index;
    size_t zone_count = 0U;
    bool snapshot = false;
    if (database == NULL || exercise_id == NULL || exercise_id[0] == '\0' || output == NULL ||
            occurrence_limit == 0U || occurrence_limit > TRAINLOG_OCCURRENCE_PAGE_MAX ||
            set_preview_limit == 0U || set_preview_limit > TRAINLOG_OCCURRENCE_SET_PAGE_MAX)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)memset(output, 0, sizeof(*output));
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) return status;
    snapshot = true;
    /* INVARIANT: aliases canonicalize exactly once before immutable knowledge
     * joins. Persisted history/MAX APIs keep their existing canonical behavior. */
    status = trainlog_database_resolve_exercise_id(database, exercise_id,
        canonical_exercise_id, sizeof(canonical_exercise_id));
    if (status != TRAINLOG_STATUS_OK) goto done;
    status = trainlog_database_get_exercise_profile(database, canonical_exercise_id, &result.exercise);
    if (status != TRAINLOG_STATUS_OK) goto done;
    status = trainlog_database_list_exercise_body_zones(database, exercise_id, NULL, 0U, &zone_count);
    if (status != TRAINLOG_STATUS_OK && !(status == TRAINLOG_STATUS_INVALID_ARGUMENT && zone_count > 0U)) goto done;
    if (zone_count > SIZE_MAX / sizeof(*result.persisted_zones)) { status=TRAINLOG_STATUS_INVALID_ARGUMENT; goto done; }
    if (zone_count > 0U) {
        result.persisted_zones = calloc(zone_count, sizeof(*result.persisted_zones));
        if (result.persisted_zones == NULL) { status=TRAINLOG_STATUS_SYSTEM_ERROR; goto done; }
        status=trainlog_database_list_exercise_body_zones(database,exercise_id,result.persisted_zones,zone_count,&result.persisted_zone_count);
        if(status!=TRAINLOG_STATUS_OK)goto done;
    }
    result.knowledge=trainlog_exercise_knowledge_lookup(canonical_exercise_id);
    if(result.knowledge!=NULL){
        status=trainlog_knowledge_list_equipment_for_exercise(canonical_exercise_id,NULL,0U,
            &result.compatible_equipment_count);
        if(status!=TRAINLOG_STATUS_OK && !(status==TRAINLOG_STATUS_INVALID_ARGUMENT &&
                result.compatible_equipment_count>0U))goto done;
        if(result.compatible_equipment_count>SIZE_MAX/sizeof(*result.compatible_equipment)){status=TRAINLOG_STATUS_INVALID_ARGUMENT;goto done;}
        if(result.compatible_equipment_count>0U){
            result.compatible_equipment=calloc(result.compatible_equipment_count,sizeof(*result.compatible_equipment));
            if(result.compatible_equipment==NULL){status=TRAINLOG_STATUS_SYSTEM_ERROR;goto done;}
            status=trainlog_knowledge_list_equipment_for_exercise(canonical_exercise_id,
                result.compatible_equipment,result.compatible_equipment_count,
                &result.compatible_equipment_count);
            if(status!=TRAINLOG_STATUS_OK)goto done;
        }
    }
    status=trainlog_database_latest_explicit_max_context(database,exercise_id,&result.latest_max);
    if(status!=TRAINLOG_STATUS_OK)goto done;
    result.occurrences=calloc(occurrence_limit,sizeof(*result.occurrences));
    if(result.occurrences==NULL){status=TRAINLOG_STATUS_SYSTEM_ERROR;goto done;}
    {
        TrainlogExerciseOccurrence *raw=calloc(occurrence_limit,sizeof(*raw));
        if(raw==NULL){status=TRAINLOG_STATUS_SYSTEM_ERROR;goto done;}
        status=trainlog_database_list_exercise_occurrences_page(database,exercise_id,NULL,occurrence_limit,raw,
            &result.occurrence_count,&result.occurrences_have_more,&result.next_occurrence);
        if(status==TRAINLOG_STATUS_OK){for(index=0U;index<result.occurrence_count;++index)result.occurrences[index].occurrence=raw[index];}
        free(raw); if(status!=TRAINLOG_STATUS_OK)goto done;
    }
    if(result.occurrence_count>0U && set_preview_limit>SIZE_MAX/result.occurrence_count){status=TRAINLOG_STATUS_INVALID_ARGUMENT;goto done;}
    result.sets=calloc(result.occurrence_count*set_preview_limit,sizeof(*result.sets));
    if(result.occurrence_count>0U && result.sets==NULL){status=TRAINLOG_STATUS_SYSTEM_ERROR;goto done;}
    for(index=0U;index<result.occurrence_count;++index){
        TrainlogTrainingOccurrenceView *view=&result.occurrences[index]; size_t count=0U;
        view->set_offset=result.set_count; view->next_set_position=-1;
        if(view->occurrence.recording_mode==TRAINLOG_RECORDING_CONTINUOUS)continue;
        status=trainlog_database_list_occurrence_sets_page(database,view->occurrence.entry_id,-1,set_preview_limit,
            result.sets+result.set_count,&count,&view->sets_have_more,&view->next_set_position);
        if (status != TRAINLOG_STATUS_OK) {
            goto done;
        }
        view->set_count = count;
        result.set_count += count;
    }
done:
    if (snapshot) {
        TrainlogStatus end_status=trainlog_database_read_snapshot_end(database,status==TRAINLOG_STATUS_OK);
        if(status==TRAINLOG_STATUS_OK)status=end_status;
    }
    if(status!=TRAINLOG_STATUS_OK){trainlog_training_exercise_context_release(&result);return status;}
    *output=result; return TRAINLOG_STATUS_OK;
}
