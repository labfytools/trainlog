#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "trainlog/database.h"
#include "trainlog/web_prepared_items.h"
#include "database_internal.h"

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            (void)fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return false;                                                                      \
        }                                                                                      \
    } while (0)

static bool execute(TrainlogDatabase *database, const char *sql) {
    char *error = NULL;
    int status = sqlite3_exec(database->connection, sql, NULL, NULL, &error);
    if (status != SQLITE_OK) {
        (void)fprintf(stderr, "SQLite fixture failed: %s\n", error == NULL ? "unknown" : error);
    }
    sqlite3_free(error);
    return status == SQLITE_OK;
}

static bool test_distinct_durable_items(void) {
    TrainlogDatabase *database = NULL;
    TrainlogWebPreparedItems items;
    char json[TRAINLOG_WEB_PREPARED_ITEMS_JSON_CAPACITY];
    size_t json_size = 0U;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(execute(database,
        "INSERT INTO exercises(exercise_id,name,normalized_name,tracking_mode,recording_mode,"
        "data_fields) VALUES('ex_00000000-0000-4000-8000-000000000001','Test','test','reps',"
        "'sets',0);"
        "INSERT INTO ai_session_drafts(draft_id,created_at,planned_for,session_type,title,notes,"
        "published_at) VALUES('aid_00000000-0000-4000-8000-000000000001',"
        "'2026-09-16T10:00:00Z','2026-09-17','training','Prepared title',NULL,"
        "'2026-09-16T11:00:00Z');"
        "INSERT INTO ai_session_draft_entries(draft_row_id,entry_id,position,exercise_row_id,"
        "source_exercise_id,recording_mode,tracking_mode,data_fields,equipment_id,load_mode,"
        "target_sets,target_reps,target_duration_seconds,target_weight_kg,rest_seconds) "
        "SELECT d.id,'sxe_00000000-0000-4000-8000-000000000001',0,e.id,e.exercise_id,'sets',"
        "'reps',0,NULL,'none',2,8,NULL,NULL,60 FROM ai_session_drafts d,exercises e WHERE "
        "e.exercise_id='ex_00000000-0000-4000-8000-000000000001';"
        "INSERT INTO execution_drafts(session_id,session_type,source_session_id,started_at,"
        "revision_id,parent_revision_id,state,payload_json) VALUES("
        "'se_00000000-0000-4000-8000-000000000001','training',NULL,'2026-09-17T12:00:00Z',"
        "'dr_one',NULL,'active','{\"exercises\":[{},{}]}');"));
    {
        TrainlogStatus status = trainlog_web_prepared_items_load(database, &items);
        if (status != TRAINLOG_STATUS_OK) {
            (void)fprintf(stderr, "prepared load failed: %d sqlite=%s\n", (int)status,
                          sqlite3_errmsg(database->connection));
        }
        CHECK(status == TRAINLOG_STATUS_OK);
    }
    CHECK(items.item_count == 2U);
    CHECK(items.items[0].kind == TRAINLOG_WEB_PREPARED_AI_PROPOSAL);
    CHECK(strcmp(items.items[0].state, "published") == 0);
    CHECK(strcmp(items.items[0].title, "Prepared title") == 0);
    CHECK(items.items[0].occurrence_count == 1U);
    CHECK(items.items[1].kind == TRAINLOG_WEB_PREPARED_EXECUTION_DRAFT);
    CHECK(strcmp(items.items[1].state, "active") == 0);
    CHECK(items.items[1].occurrence_count == 2U);
    CHECK(trainlog_web_prepared_items_serialize(
              &items, json, sizeof(json), &json_size) == TRAINLOG_STATUS_OK);
    CHECK(json_size > 0U);
    CHECK(strstr(json, "\"kind\":\"ai_proposal\"") != NULL);
    CHECK(strstr(json, "\"kind\":\"execution_draft\"") != NULL);
    items.items[0].title[0] = (char)0xc3;
    items.items[0].title[1] = '\0';
    CHECK(trainlog_web_prepared_items_serialize(
              &items, json, sizeof(json), &json_size) == TRAINLOG_STATUS_SYSTEM_ERROR);
    CHECK(json_size == 0U && json[0] == '\0');
    trainlog_database_close(database);
    return true;
}

static bool test_finalized_and_deleted_drafts_are_hidden(void) {
    TrainlogDatabase *database = NULL;
    TrainlogWebPreparedItems items;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(execute(database,
        "INSERT INTO execution_drafts(session_id,session_type,revision_id,state,payload_json) "
        "VALUES('se_00000000-0000-4000-8000-000000000002','training','dr_two','pending',"
        "'{\"exercises\":[]}'),('se_00000000-0000-4000-8000-000000000003','training',"
        "'dr_three','pending','{\"exercises\":[]}');"
        "INSERT INTO execution_draft_finalizations(session_id,final_revision_id,finalized_at) "
        "VALUES('se_00000000-0000-4000-8000-000000000002','dr_final',"
        "'2026-09-17T12:00:00Z');"
        "INSERT INTO sync_causal_state(target_kind,target_id,current_revision_id,deleted) "
        "VALUES('execution_draft','se_00000000-0000-4000-8000-000000000003','dr_delete',1);"));
    CHECK(trainlog_web_prepared_items_load(database, &items) == TRAINLOG_STATUS_OK);
    CHECK(items.item_count == 0U);
    trainlog_database_close(database);
    return true;
}

static bool test_executed_program_preparation_and_empty_draft_are_hidden(void) {
    TrainlogDatabase *database = NULL;
    TrainlogWebPreparedItems items;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(execute(database,
        "INSERT INTO programs VALUES('pg_00000000-0000-4000-8000-000000000001','Program',NULL,"
        "'active','2026-09-21','2026-10-18','2026-09-20T00:00:00Z',"
        "'2026-09-20T00:00:00Z','pgr_one','test',1,"
        "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',NULL);"
        "INSERT INTO program_sessions VALUES('pgs_00000000-0000-4000-8000-000000000001',"
        "'pg_00000000-0000-4000-8000-000000000001',0,'Done','training','2026-09-21',NULL);"
        "INSERT INTO session_preparations(preparation_id,current_revision_id,created_at,updated_at,"
        "editing_state,delivery_state,source_program_id,source_program_session_id) VALUES("
        "'sp_00000000-0000-4000-8000-000000000001','spr_one','2026-09-20T00:00:00Z',"
        "'2026-09-20T00:00:00Z','ready','acknowledged',"
        "'pg_00000000-0000-4000-8000-000000000001',"
        "'pgs_00000000-0000-4000-8000-000000000001');"
        "INSERT INTO session_preparation_revisions VALUES('spr_one',"
        "'sp_00000000-0000-4000-8000-000000000001',NULL,'Done','training','2026-09-21',NULL,"
        "'2026-09-20T00:00:00Z');"
        "INSERT INTO program_session_executions VALUES("
        "'pgs_00000000-0000-4000-8000-000000000001',"
        "'pg_00000000-0000-4000-8000-000000000001',"
        "'se_00000000-0000-4000-8000-000000000009','completed','2026-09-21T12:00:00Z');"
        "INSERT INTO execution_drafts(session_id,session_type,revision_id,state,payload_json) "
        "VALUES('se_00000000-0000-4000-8000-000000000010','training','dr_empty','active',"
        "'{\"entries\":[]}');"));
    CHECK(trainlog_web_prepared_items_load(database, &items) == TRAINLOG_STATUS_OK);
    CHECK(items.item_count == 0U);
    trainlog_database_close(database);
    return true;
}

int main(void) {
    if (!test_distinct_durable_items() || !test_finalized_and_deleted_drafts_are_hidden() ||
        !test_executed_program_preparation_and_empty_draft_are_hidden()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
