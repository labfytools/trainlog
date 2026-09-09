#include "trainlog/training_context.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t id_list_count(const char *list)
{
    size_t count = 0U;
    const char *at = list;
    if (at == NULL || *at == '\0') return 0U;
    count = 1U;
    while (*at != '\0') { if (*at == '\n') ++count; ++at; }
    return count;
}

static const TrainlogEquipmentKnowledge *equipment_from_list(const char **at)
{
    const char *end;
    char id[TRAINLOG_ID_MAX + 1U];
    size_t length;
    if (at == NULL || *at == NULL || **at == '\0') return NULL;
    end = strchr(*at, '\n');
    length = end == NULL ? strlen(*at) : (size_t)(end - *at);
    if (length == 0U || length >= sizeof(id)) return NULL;
    (void)memcpy(id, *at, length); id[length] = '\0';
    *at = end == NULL ? *at + length : end + 1;
    return trainlog_equipment_knowledge_lookup(id);
}

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
    const char *equipment_at;
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
    status = trainlog_database_get_exercise_profile(database, exercise_id, &result.exercise);
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
    result.knowledge=trainlog_exercise_knowledge_lookup(exercise_id);
    if(result.knowledge!=NULL){
        result.compatible_equipment_count=id_list_count(result.knowledge->equipment_ids);
        if(result.compatible_equipment_count>SIZE_MAX/sizeof(*result.compatible_equipment)){status=TRAINLOG_STATUS_INVALID_ARGUMENT;goto done;}
        if(result.compatible_equipment_count>0U){
            result.compatible_equipment=calloc(result.compatible_equipment_count,sizeof(*result.compatible_equipment));
            if(result.compatible_equipment==NULL){status=TRAINLOG_STATUS_SYSTEM_ERROR;goto done;}
            equipment_at=result.knowledge->equipment_ids;
            for(index=0U;index<result.compatible_equipment_count;++index){
                result.compatible_equipment[index]=equipment_from_list(&equipment_at);
                if(result.compatible_equipment[index]==NULL){status=TRAINLOG_STATUS_DATABASE_ERROR;goto done;}
            }
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
