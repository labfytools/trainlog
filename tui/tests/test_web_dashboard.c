#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/web_dashboard.h"

#define CHECK(x) do { if (!(x)) { (void)fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return false; } } while (0)

static bool insert_fact(TrainlogDatabase*d,const char*sid,const char*eid,const char*when,int kind){
    TrainlogSessionExerciseInput occurrence;TrainlogSessionInput session;TrainlogSetInput set={8,0,true,42.0};
    (void)memset(&occurrence,0,sizeof(occurrence));(void)memset(&session,0,sizeof(session));
    (void)snprintf(occurrence.entry_id,sizeof(occurrence.entry_id),"%s",eid);
    (void)snprintf(occurrence.exercise_id,sizeof(occurrence.exercise_id),"%s",kind==1?"ex_90000000-0000-4000-8000-000000000002":"ex_90000000-0000-4000-8000-000000000001");
    occurrence.tracking_mode=kind==1?TRAINLOG_TRACKING_DURATION:TRAINLOG_TRACKING_REPS;occurrence.recording_mode=kind==1?TRAINLOG_RECORDING_CONTINUOUS:TRAINLOG_RECORDING_SETS;occurrence.load_mode=kind==1?TRAINLOG_LOAD_NONE:TRAINLOG_LOAD_EXTERNAL;
    if(kind==0){occurrence.sets=&set;occurrence.set_count=1U;}else if(kind==1){occurrence.continuous_duration_seconds=600;}else{occurrence.has_max_weight=true;occurrence.max_weight_kg=100.0+(double)kind;}
    (void)snprintf(session.session_id,sizeof(session.session_id),"%s",sid);(void)snprintf(session.started_at,sizeof(session.started_at),"%s",when);session.session_type=kind>=2?TRAINLOG_SESSION_MAX_TEST:TRAINLOG_SESSION_TRAINING;session.exercises=&occurrence;session.exercise_count=1U;
    return trainlog_database_insert_session(d,&session)==TRAINLOG_STATUS_OK;
}

static bool test_empty_and_real_facts(void){
    TrainlogDatabase*d=NULL;TrainlogWebDashboardQuery q={INT64_C(1789560000)};TrainlogWebDashboardSnapshot s;const char*none[1]={NULL};size_t i;char id[64],entry[64];
    CHECK(trainlog_database_open(":memory:",&d)==TRAINLOG_STATUS_OK);CHECK(trainlog_web_dashboard_load(d,&q,&s)==TRAINLOG_STATUS_OK);
    CHECK(!s.partial&&!s.invalid_data&&!s.next_session_available&&!s.cardio_available);CHECK(strcmp(s.next_session_reason,"no_persisted_executable_plan")==0);CHECK(s.activity_count==90U&&!s.last_session.available&&s.muscle_zone_count==0U);
    CHECK(trainlog_database_insert_exercise_profiled(d,"ex_90000000-0000-4000-8000-000000000001","Échappé \"dos\"","echappe dos",TRAINLOG_TRACKING_REPS,TRAINLOG_RECORDING_SETS,(TrainlogExerciseDataFields)0)==TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(d,"ex_90000000-0000-4000-8000-000000000001","back",none,0U)==TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(d,"ex_90000000-0000-4000-8000-000000000002","Continu","continu",TRAINLOG_TRACKING_DURATION,TRAINLOG_RECORDING_CONTINUOUS,(TrainlogExerciseDataFields)0)==TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(d,"ex_90000000-0000-4000-8000-000000000002","back",none,0U)==TRAINLOG_STATUS_OK);
    CHECK(insert_fact(d,"se_90000000-0000-4000-8000-000000000001","en_90000000-0000-4000-8000-000000000001","2026-09-15T10:00:00+02:00",0));
    CHECK(insert_fact(d,"se_90000000-0000-4000-8000-000000000002","en_90000000-0000-4000-8000-000000000002","2026-09-15T18:00:00+02:00",1));
    for(i=0U;i<9U;++i){(void)snprintf(id,sizeof(id),"se_91000000-0000-4000-8000-%012zu",i);(void)snprintf(entry,sizeof(entry),"en_91000000-0000-4000-8000-%012zu",i);CHECK(insert_fact(d,id,entry,"2026-09-14T12:00:00+02:00",(int)i+2));}
    CHECK(trainlog_web_dashboard_load(d,&q,&s)==TRAINLOG_STATUS_OK);CHECK(s.partial);CHECK(s.last_session.available);CHECK(strcmp(s.last_session.session_id,"se_90000000-0000-4000-8000-000000000002")==0);CHECK(!s.last_session.has_ended_at&&!s.last_session.has_duration);CHECK(s.last_session.continuous_count==1U&&s.last_session.zone_count==1U);CHECK(s.max_record_count==8U&&s.muscle_zone_count==1U);CHECK(s.muscle_zones[0].session_count==11U&&s.muscle_zones[0].occurrence_count==11U&&s.muscle_zones[0].set_count==1U);
    for(i=0U;i<s.activity_count;++i)if(strcmp(s.activity[i].date,"2026-09-15")==0)CHECK(s.activity[i].active&&s.activity[i].session_count==2U&&s.activity[i].set_count==1U);
    trainlog_database_close(d);return true;
}

static bool test_json_escaping_and_limit(void){TrainlogWebDashboardSnapshot s;char json[TRAINLOG_WEB_JSON_CAPACITY];char tiny[8];size_t size;(void)memset(&s,0,sizeof(s));(void)snprintf(s.user_display_name,sizeof(s.user_display_name),"A\"B\\C\nÉ");(void)snprintf(s.generated_at,sizeof(s.generated_at),"2026-09-16T12:00:00Z");s.next_session_reason="no_persisted_executable_plan";s.cardio_reason="no_cardio_data_source";CHECK(trainlog_web_dashboard_serialize(&s,json,sizeof(json),&size)==TRAINLOG_STATUS_OK&&size>0U);CHECK(strstr(json,"A\\\"B\\\\C\\nÉ")!=NULL);CHECK(trainlog_web_dashboard_serialize(&s,tiny,sizeof(tiny),&size)==TRAINLOG_STATUS_SYSTEM_ERROR);s.user_display_name[0]=(char)0xc3;s.user_display_name[1]='\0';CHECK(trainlog_web_dashboard_serialize(&s,json,sizeof(json),&size)==TRAINLOG_STATUS_SYSTEM_ERROR);return true;}

int main(void){return test_empty_and_real_facts()&&test_json_escaping_and_limit()?0:1;}
