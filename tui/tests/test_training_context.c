#include "trainlog/training_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", \
    __FILE__, __LINE__, #test); return 1; } } while (0)

static void base_occurrence(TrainlogSessionExerciseInput *value, const char *entry_id,
    const char *exercise_id, const char *equipment_id, TrainlogLoadMode mode)
{
    (void)memset(value, 0, sizeof(*value));
    (void)snprintf(value->entry_id, sizeof(value->entry_id), "%s", entry_id);
    (void)snprintf(value->exercise_id, sizeof(value->exercise_id), "%s", exercise_id);
    (void)snprintf(value->equipment_id, sizeof(value->equipment_id), "%s", equipment_id);
    value->recording_mode = TRAINLOG_RECORDING_SETS;
    value->load_mode = mode;
    value->rest_seconds = 60;
    value->target_sets = 2;
    value->target_reps = 10;
    value->target_has_weight = mode != TRAINLOG_LOAD_NONE;
    value->target_weight_kg = 20.0;
}

static int insert_session(TrainlogDatabase *database, const char *session_id, const char *started_at,
    TrainlogSessionType type, TrainlogSessionExerciseInput *items, size_t count)
{
    TrainlogSessionInput session = {0};
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s", started_at);
    session.session_type = type;
    session.exercises = items;
    session.exercise_count = count;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

int main(void)
{
    const char *exercise_id = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde";
    const char *zones[] = {"glutes"};
    TrainlogDatabase *database = NULL;
    TrainlogSessionExerciseInput occurrences[2];
    TrainlogSessionExerciseInput max_occurrence;
    TrainlogSessionExerciseInput assistance_occurrence;
    TrainlogSessionExerciseInput continuous_occurrence;
    TrainlogSessionExerciseInput large_occurrence;
    TrainlogSessionExerciseInput temporal_occurrence;
    TrainlogSessionExerciseInput temporal_max;
    TrainlogSetInput first_sets[2] = {{8,0,false,0.0},{6,0,true,0.0}};
    TrainlogSetInput second_sets[1] = {{11,0,true,30.0}};
    TrainlogSetInput assistance_set[1] = {{9,0,true,15.0}};
    TrainlogSetInput large_sets[65];
    TrainlogTrainingExerciseContext context;
    TrainlogExerciseOccurrence page[1];
    TrainlogExerciseOccurrenceCursor next;
    TrainlogExerciseOccurrenceCursor cursor;
    bool has_more = false;
    size_t count = 0U;
    size_t index;
    TrainlogOccurrenceSet set_page[64];
    int next_position = -1;
    char database_path[] = "/tmp/trainlog-context-XXXXXX";
    int database_fd = mkstemp(database_path);
    sqlite3 *raw = NULL;

    CHECK(database_fd >= 0);
    CHECK(close(database_fd) == 0);
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database, exercise_id, "Leg press renamed",
        "leg press renamed", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database, exercise_id, "thighs", zones, 1U)
        == TRAINLOG_STATUS_OK);
    base_occurrence(&occurrences[0], "entry_a", exercise_id, "leg_press", TRAINLOG_LOAD_EXTERNAL);
    occurrences[0].sets=first_sets; occurrences[0].set_count=2U;
    base_occurrence(&occurrences[1], "entry_b", exercise_id, "plate_loaded_leg_press", TRAINLOG_LOAD_EXTERNAL);
    occurrences[1].sets=second_sets; occurrences[1].set_count=1U;
    CHECK(insert_session(database,"session_b","2026-09-08T12:00:00+02:00",TRAINLOG_SESSION_TRAINING,occurrences,2U));
    base_occurrence(&assistance_occurrence,"entry_assist",exercise_id,"leg_press",TRAINLOG_LOAD_ASSISTANCE);
    assistance_occurrence.sets=assistance_set; assistance_occurrence.set_count=1U;
    CHECK(insert_session(database,"session_a","2026-09-08T11:00:00+00:00",TRAINLOG_SESSION_TRAINING,&assistance_occurrence,1U));
    base_occurrence(&max_occurrence,"entry_max",exercise_id,"leg_press",TRAINLOG_LOAD_EXTERNAL);
    max_occurrence.has_max_weight=true; max_occurrence.max_weight_kg=120.0;
    max_occurrence.target_sets=0; max_occurrence.target_reps=0;
    max_occurrence.target_has_weight=false; max_occurrence.target_weight_kg=0.0;
    max_occurrence.rest_seconds=0;
    max_occurrence.load_mode=TRAINLOG_LOAD_NONE;
    CHECK(insert_session(database,"session_max","2026-09-09T10:00:00+02:00",TRAINLOG_SESSION_MAX_TEST,&max_occurrence,1U));

    CHECK(trainlog_database_latest_explicit_max_equipment_context(database,
        exercise_id, "leg_press", &context.latest_max) == TRAINLOG_STATUS_OK);
    CHECK(context.latest_max.found && context.latest_max.max_weight_kg == 120.0);
    CHECK(trainlog_database_latest_explicit_max_equipment_context(database,
        exercise_id, "plate_loaded_leg_press", &context.latest_max) == TRAINLOG_STATUS_OK);
    CHECK(!context.latest_max.found);
    CHECK(trainlog_database_latest_explicit_max_equipment_context(database,
        exercise_id, "", &context.latest_max) == TRAINLOG_STATUS_INVALID_ARGUMENT);

    CHECK(trainlog_training_exercise_context_load(database,exercise_id,4U,1U,&context)==TRAINLOG_STATUS_OK);
    CHECK(strcmp(context.exercise.name,"Leg press renamed")==0);
    CHECK(context.knowledge!=NULL && context.knowledge->interpretation!=NULL);
    CHECK(context.persisted_zone_count==2U);
    CHECK(context.compatible_equipment_count==2U);
    CHECK(context.latest_max.found && context.latest_max.max_weight_kg==120.0);
    CHECK(context.occurrence_count==4U);
    CHECK(strcmp(context.occurrences[0].occurrence.entry_id,"entry_max")==0);
    CHECK(context.occurrences[0].set_count==0U); /* explicit MAX is not a fake set */
    CHECK(context.occurrences[1].occurrence.load_mode==TRAINLOG_LOAD_ASSISTANCE);
    CHECK(strcmp(context.occurrences[2].occurrence.entry_id,"entry_b")==0);
    CHECK(context.occurrences[2].set_count==1U && !context.occurrences[2].sets_have_more);
    CHECK(strcmp(context.occurrences[3].occurrence.entry_id,"entry_a")==0);
    CHECK(context.occurrences[3].set_count==1U && context.occurrences[3].sets_have_more);
    CHECK(context.sets[context.occurrences[3].set_offset].has_weight==false);
    trainlog_training_exercise_context_release(&context);
    CHECK(trainlog_database_list_occurrence_sets_page(database,"entry_a",0,64U,set_page,&count,&has_more,
        &next_position)==TRAINLOG_STATUS_OK);
    CHECK(count==1U && !has_more && set_page[0].has_weight && set_page[0].weight_kg==0.0);

    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,NULL,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_OK);
    CHECK(count==1U && has_more);
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&next,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_OK);
    CHECK(count==1U);
    cursor=next; cursor.entry_id[0]='\0';
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
    cursor=next; (void)memset(cursor.started_at,'x',sizeof(cursor.started_at));
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
    cursor=next; (void)memset(cursor.session_id,'x',sizeof(cursor.session_id));
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
    cursor=next; (void)memset(cursor.entry_id,'x',sizeof(cursor.entry_id));
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
    cursor=next; (void)snprintf(cursor.started_at,sizeof(cursor.started_at),"not-a-date");
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
    cursor=next; (void)snprintf(cursor.started_at,sizeof(cursor.started_at),"2026-13-01T00:00:00+00:00");
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
    cursor=next; (void)snprintf(cursor.started_at,sizeof(cursor.started_at),"2026-02-30T00:00:00+00:00");
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
    {
        const char *invalid_timestamp[] = {
            "0000-01-01T00:00:00Z", "2026-09-05 18:34:12Z", "20260905T183412Z",
            "2026-W36-5T18:34:12Z", "2026-09-05T18:34:12,5Z",
            "2026-09-05T18:34:12+0200", "2026-09-05T18:34:12+02",
            "2026-09-05T18:34.5Z", "2026-09-05T18:34:60Z", "2026-09-05T18:34:12+24:00"
        };
        for(index=0U;index<sizeof(invalid_timestamp)/sizeof(invalid_timestamp[0]);++index){
            (void)memset(&cursor,0,sizeof(cursor));
            (void)snprintf(cursor.started_at,sizeof(cursor.started_at),"%s",invalid_timestamp[index]);
            (void)snprintf(cursor.session_id,sizeof(cursor.session_id),"session_b");
            (void)snprintf(cursor.entry_id,sizeof(cursor.entry_id),"entry_b");
            CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
                &has_more,&next)==TRAINLOG_STATUS_INVALID_ARGUMENT);
        }
    }
    cursor=next; (void)snprintf(cursor.started_at,sizeof(cursor.started_at),"2026-09-08T10:00:00+15:00");
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_OK);
    (void)memset(&cursor,0,sizeof(cursor));
    (void)snprintf(cursor.started_at,sizeof(cursor.started_at),"2026-09-08T10:00:00Z");
    (void)snprintf(cursor.session_id,sizeof(cursor.session_id),"session_b");
    (void)snprintf(cursor.entry_id,sizeof(cursor.entry_id),"entry_b");
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_OK);

    /* TEMPORAL_READER_V1: production paging must preserve exceptional source
     * spellings and use the same exact comparator for every exclusive cursor. */
    base_occurrence(&temporal_occurrence,"entry_frac_low",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_occurrence.target_has_weight=false; temporal_occurrence.target_weight_kg=0.0;
    CHECK(insert_session(database,"session_frac_low","2026-09-21T10:00:00.1234567890123456Z",
        TRAINLOG_SESSION_TRAINING,&temporal_occurrence,1U));
    base_occurrence(&temporal_occurrence,"entry_frac_high",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_occurrence.target_has_weight=false; temporal_occurrence.target_weight_kg=0.0;
    CHECK(insert_session(database,"session_frac_high","2026-09-21T10:00:00.1234567890123457Z",
        TRAINLOG_SESSION_TRAINING,&temporal_occurrence,1U));
    base_occurrence(&temporal_occurrence,"entry_lower",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_occurrence.target_has_weight=false; temporal_occurrence.target_weight_kg=0.0;
    CHECK(insert_session(database,"session_lower","2026-09-20t10:00:00z",TRAINLOG_SESSION_TRAINING,
        &temporal_occurrence,1U));
    base_occurrence(&temporal_occurrence,"entry_omit",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_occurrence.target_has_weight=false; temporal_occurrence.target_weight_kg=0.0;
    CHECK(insert_session(database,"session_omit","2026-09-22T10:00+15:00",TRAINLOG_SESSION_TRAINING,
        &temporal_occurrence,1U));
    base_occurrence(&temporal_occurrence,"entry_high_offset",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_occurrence.target_has_weight=false; temporal_occurrence.target_weight_kg=0.0;
    CHECK(insert_session(database,"session_high_offset","2026-09-22T10:00:00+23:59",TRAINLOG_SESSION_TRAINING,
        &temporal_occurrence,1U));
    base_occurrence(&temporal_occurrence,"entry_tie_a",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_occurrence.target_has_weight=false; temporal_occurrence.target_weight_kg=0.0;
    CHECK(insert_session(database,"session_tie_a","2026-09-20T12:00:00+02:00",TRAINLOG_SESSION_TRAINING,
        &temporal_occurrence,1U));
    base_occurrence(&temporal_occurrence,"entry_tie_z",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_occurrence.target_has_weight=false; temporal_occurrence.target_weight_kg=0.0;
    CHECK(insert_session(database,"session_tie_z","2026-09-20T10:00:00-00:00",TRAINLOG_SESSION_TRAINING,
        &temporal_occurrence,1U));
    {
        const char *expected[] = {"entry_omit","entry_high_offset","entry_frac_high","entry_frac_low",
            "entry_tie_z","entry_tie_a","entry_lower"};
        TrainlogExerciseOccurrenceCursor temporal_cursor;
        const TrainlogExerciseOccurrenceCursor *after_temporal = NULL;
        for(index=0U;index<7U;++index){
            CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,after_temporal,1U,
                page,&count,&has_more,&temporal_cursor)==TRAINLOG_STATUS_OK);
            CHECK(count==1U && strcmp(page[0].entry_id,expected[index])==0);
            CHECK(strcmp(page[0].started_at,temporal_cursor.started_at)==0);
            after_temporal=&temporal_cursor;
        }
    }
    base_occurrence(&temporal_max,"entry_max_offset",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_max.target_sets=0; temporal_max.target_reps=0; temporal_max.target_has_weight=false;
    temporal_max.rest_seconds=0; temporal_max.has_max_weight=true; temporal_max.max_weight_kg=151.0;
    CHECK(insert_session(database,"session_max_offset","2026-09-23T10:00:00+15:00",
        TRAINLOG_SESSION_MAX_TEST,&temporal_max,1U));
    base_occurrence(&temporal_max,"entry_max_true",exercise_id,"",TRAINLOG_LOAD_NONE);
    temporal_max.target_sets=0; temporal_max.target_reps=0; temporal_max.target_has_weight=false;
    temporal_max.rest_seconds=0; temporal_max.has_max_weight=true; temporal_max.max_weight_kg=152.0;
    CHECK(insert_session(database,"session_max_true","2026-09-22T20:00:00Z",
        TRAINLOG_SESSION_MAX_TEST,&temporal_max,1U));
    CHECK(trainlog_database_latest_explicit_max_context(database,exercise_id,&context.latest_max)==TRAINLOG_STATUS_OK);
    CHECK(context.latest_max.found && strcmp(context.latest_max.entry_id,"entry_max_true")==0 &&
        context.latest_max.max_weight_kg==152.0);
    (void)memset(&cursor,0,sizeof(cursor));
    (void)snprintf(cursor.started_at,sizeof(cursor.started_at),"2026-09-08T12:00:00+02:00");
    (void)snprintf(cursor.session_id,sizeof(cursor.session_id),"session_b");
    (void)snprintf(cursor.entry_id,sizeof(cursor.entry_id),"entry_b");
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,&cursor,1U,page,&count,
        &has_more,&next)==TRAINLOG_STATUS_OK);

    CHECK(trainlog_database_insert_exercise(database,"ex_unknown_runtime","Custom renamed","custom renamed",
        TRAINLOG_TRACKING_REPS)==TRAINLOG_STATUS_OK);
    CHECK(trainlog_training_exercise_context_load(database,"ex_unknown_runtime",1U,1U,&context)==TRAINLOG_STATUS_OK);
    CHECK(context.knowledge==NULL && context.occurrence_count==0U);
    trainlog_training_exercise_context_release(&context);

    CHECK(trainlog_database_insert_exercise_profiled(database,"ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d",
        "Marche renommée","marche renommee",TRAINLOG_TRACKING_DURATION,TRAINLOG_RECORDING_CONTINUOUS,
        TRAINLOG_EXERCISE_DATA_SPEED_KMH)==TRAINLOG_STATUS_OK);
    (void)memset(&continuous_occurrence,0,sizeof(continuous_occurrence));
    (void)snprintf(continuous_occurrence.entry_id,sizeof(continuous_occurrence.entry_id),"entry_walk");
    (void)snprintf(continuous_occurrence.exercise_id,sizeof(continuous_occurrence.exercise_id),"%s",
        "ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d");
    (void)snprintf(continuous_occurrence.equipment_id,sizeof(continuous_occurrence.equipment_id),"treadmill");
    continuous_occurrence.recording_mode=TRAINLOG_RECORDING_CONTINUOUS;
    continuous_occurrence.data_fields=TRAINLOG_EXERCISE_DATA_SPEED_KMH;
    continuous_occurrence.load_mode=TRAINLOG_LOAD_NONE;
    continuous_occurrence.continuous_duration_seconds=900;
    continuous_occurrence.continuous_has_speed=true;
    continuous_occurrence.continuous_speed_kmh=5.0;
    CHECK(insert_session(database,"session_walk","2026-09-06T10:00:00+02:00",TRAINLOG_SESSION_TRAINING,&continuous_occurrence,1U));
    CHECK(trainlog_training_exercise_context_load(database,continuous_occurrence.exercise_id,1U,1U,&context)==TRAINLOG_STATUS_OK);
    CHECK(context.occurrence_count==1U && context.occurrences[0].occurrence.continuous_duration_seconds==900);
    CHECK(context.occurrences[0].set_count==0U);
    trainlog_training_exercise_context_release(&context);

    for(index=0U;index<65U;++index){large_sets[index].reps=(int)(index+1U);large_sets[index].duration_seconds=0;
        large_sets[index].has_weight=false;large_sets[index].weight_kg=0.0;}
    base_occurrence(&large_occurrence,"entry_large","ex_unknown_runtime","",TRAINLOG_LOAD_NONE);
    large_occurrence.target_sets=65; large_occurrence.sets=large_sets; large_occurrence.set_count=65U;
    CHECK(insert_session(database,"session_large","2026-09-05T10:00:00+02:00",TRAINLOG_SESSION_TRAINING,&large_occurrence,1U));
    CHECK(trainlog_database_list_occurrence_sets_page(database,"entry_large",-1,64U,set_page,&count,&has_more,
        &next_position)==TRAINLOG_STATUS_OK);
    CHECK(count==64U && has_more && next_position==63);
    CHECK(trainlog_database_list_occurrence_sets_page(database,"entry_large",next_position,64U,set_page,&count,
        &has_more,&next_position)==TRAINLOG_STATUS_OK);
    CHECK(count==1U && !has_more && set_page[0].position==64U);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE sessions SET started_at='2026-09-20 10:00:00Z' WHERE session_id='session_lower';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,NULL,1U,page,&count,&has_more,
        &next)==TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE sessions SET started_at='2020-01-01T00:00:00.123456789012345678901234567890Z' WHERE session_id='session_lower';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,NULL,1U,page,&count,&has_more,
        &next)==TRAINLOG_STATUS_OK);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE sessions SET started_at='9999-12-31T23:59:59.123456789012345678901234567890Z' WHERE session_id='session_lower';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,NULL,1U,page,&count,&has_more,
        &next)==TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE sessions SET started_at='2026-09-20t10:00:00z' WHERE session_id='session_lower';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE sessions SET started_at='9999-12-31T23:59:59.123456789012345678901234567890Z' WHERE session_id='session_max_true';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_latest_explicit_max_context(database,exercise_id,&context.latest_max)
        == TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE sessions SET started_at='2026-09-22T20:00:00Z' WHERE session_id='session_max_true';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE continuous_activity SET duration_seconds=2147483648 WHERE "
        "session_exercise_row_id=(SELECT id FROM session_exercises WHERE entry_id='entry_walk');",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_occurrences_page(database,continuous_occurrence.exercise_id,NULL,1U,
        page,&count,&has_more,&next)==TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE performed_sets SET reps=2147483648 WHERE position=0 AND "
        "session_exercise_row_id=(SELECT id FROM session_exercises WHERE entry_id='entry_large');",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_occurrence_sets_page(database,"entry_large",-1,64U,set_page,&count,&has_more,
        &next_position)==TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE performed_sets SET reps=0,duration_seconds=NULL,weight_kg=NULL WHERE position=0 AND "
        "session_exercise_row_id=(SELECT id FROM session_exercises WHERE entry_id='entry_large');",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_occurrence_sets_page(database,"entry_large",-1,1U,set_page,&count,&has_more,
        &next_position)==TRAINLOG_STATUS_OK);
    CHECK(count==1U && set_page[0].has_reps && set_page[0].reps==0 && !set_page[0].has_duration &&
        !set_page[0].has_weight);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE performed_sets SET reps=NULL,duration_seconds='bad' WHERE position=0 AND "
        "session_exercise_row_id=(SELECT id FROM session_exercises WHERE entry_id='entry_large');",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_occurrence_sets_page(database,"entry_large",-1,64U,set_page,&count,&has_more,
        &next_position)==TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "PRAGMA ignore_check_constraints=ON; UPDATE performed_sets SET duration_seconds=NULL,position='bad'||position WHERE "
        "session_exercise_row_id=(SELECT id FROM session_exercises WHERE entry_id='entry_large');",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_occurrence_sets_page(database,"entry_large",-1,64U,set_page,&count,&has_more,
        &next_position)==TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "PRAGMA ignore_check_constraints=ON; UPDATE session_exercises SET data_fields='bad' WHERE "
        "exercise_row_id=(SELECT id FROM exercises WHERE exercise_id='ex_b432623f-bfe9-4daf-a653-60ec7fdffbde');",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_occurrences_page(database,exercise_id,NULL,1U,page,&count,&has_more,
        &next)==TRAINLOG_STATUS_DATABASE_ERROR);
    trainlog_database_close(database);
    CHECK(unlink(database_path) == 0);
    return 0;
}
