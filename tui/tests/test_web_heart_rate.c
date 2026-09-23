#define _POSIX_C_SOURCE 200809L

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <yyjson.h>

#include "trainlog/database.h"
#include "trainlog/web_heart_rate.h"
#include "database_internal.h"

#define CHECK(value) do { if (!(value)) {     fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #value);     goto cleanup; } } while (0)

static const char *const SESSION_ID = "se_11111111-1111-4111-8111-111111111111";
static const char *const ENTRY_ID = "sxe_22222222-2222-4222-8222-222222222222";
static const char *const RUN_ID = "cgr_55555555-5555-4555-8555-555555555555";
static const char *const CALIBRATION_ID = "cal_77777777-7777-4777-8777-777777777777";
static const char *const SLEEP_ID = "sl_88888888-8888-4888-8888-888888888888";

static bool execute_sql(sqlite3 *database, const char *sql) {
    int rc = sqlite3_exec(database, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "seed SQLite rc=%d: %s\n", rc, sqlite3_errmsg(database));
        return false;
    }
    return true;
}

static bool seed(sqlite3 *database) {
    const char *const statements[] = {
        "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields)"
        " VALUES('ex_33333333-3333-4333-8333-333333333333','Vélo guidé','velo guide',"
        "'continuous','duration',0);"
        "INSERT INTO sessions(session_id,started_at,ended_at,session_type,session_kind)"
        " VALUES('se_11111111-1111-4111-8111-111111111111','2026-09-23T10:00:00+02:00',"
        "'2026-09-23T10:10:00+02:00','training','cardio');",

        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,"
        "tracking_mode,data_fields,position,load_mode,rest_seconds)"
        " SELECT 'sxe_22222222-2222-4222-8222-222222222222',s.id,e.id,'continuous','duration',"
        "0,0,'none',0 FROM sessions s,exercises e WHERE "
        "s.session_id='se_11111111-1111-4111-8111-111111111111' AND "
        "e.exercise_id='ex_33333333-3333-4333-8333-333333333333';"
        "INSERT INTO session_exercise_timeline(session_id,entry_id,exercise_id,started_at,ended_at,imported_at)"
        " VALUES('se_11111111-1111-4111-8111-111111111111',"
        "'sxe_22222222-2222-4222-8222-222222222222',"
        "'ex_33333333-3333-4333-8333-333333333333','2026-09-23T10:00:10+02:00',"
        "'2026-09-23T10:09:50+02:00','2026-09-23T10:11:00+02:00');",

        "INSERT INTO heart_rate_captures(capture_id,context_kind,context_id,started_at,ended_at,sensor_name,imported_at)"
        " VALUES('hrc_44444444-4444-4444-8444-444444444444','cardio',"
        "'se_11111111-1111-4111-8111-111111111111','2026-09-23T10:00:00+02:00',"
        "'2026-09-23T10:10:00+02:00','CYCPLUS H2','2026-09-23T10:11:00+02:00');"
        "INSERT INTO heart_rate_samples(capture_id,sequence,observed_at,bpm,exercise_entry_id,"
        "sensor_contact_detected,energy_expended) VALUES"
        "('hrc_44444444-4444-4444-8444-444444444444',0,'2026-09-23T10:00:20+02:00',110,"
        "'sxe_22222222-2222-4222-8222-222222222222',NULL,NULL),"
        "('hrc_44444444-4444-4444-8444-444444444444',1,'2026-09-23T10:00:40+02:00',130,"
        "'sxe_22222222-2222-4222-8222-222222222222',NULL,NULL);"
        "INSERT INTO heart_rate_rr_intervals(capture_id,sample_sequence,rr_index,value_1024)"
        " VALUES('hrc_44444444-4444-4444-8444-444444444444',0,0,1024);",

        "INSERT INTO cardio_calibrations(calibration_id,protocol_version,session_id,entry_id,"
        "started_at,effort_end_at,ended_at,heart_rate_capture_id,observed_peak_bpm,imported_at)"
        " VALUES('cal_77777777-7777-4777-8777-777777777777',1,"
        "'se_11111111-1111-4111-8111-111111111111','sxe_22222222-2222-4222-8222-222222222222',"
        "'2026-09-23T10:00:00+02:00','2026-09-23T10:05:00+02:00','2026-09-23T10:10:00+02:00',"
        "'hrc_44444444-4444-4444-8444-444444444444',160,'2026-09-23T10:11:00+02:00');"
        "INSERT INTO cardio_calibration_recovery(calibration_id,target_offset_seconds,observed_at,bpm)"
        " VALUES('cal_77777777-7777-4777-8777-777777777777',60,'2026-09-23T10:06:00+02:00',140);",

        "INSERT INTO cardio_guidance_runs(run_id,session_id,started_at,ended_at,imported_at)"
        " VALUES('cgr_55555555-5555-4555-8555-555555555555',"
        "'se_11111111-1111-4111-8111-111111111111','2026-09-23T10:00:20+02:00',"
        "'2026-09-23T10:03:20+02:00','2026-09-23T10:11:00+02:00');"
        "INSERT INTO cardio_guidance_phases(run_id,phase_id,entry_id,position,kind,target_min_bpm,"
        "target_max_bpm,calibration_id,calibration_observed_peak_bpm,minimum_percent,maximum_percent,"
        "exit_kind,exit_seconds,exit_bpm,started_at,ended_at,final_instruction)"
        " VALUES('cgr_55555555-5555-4555-8555-555555555555',"
        "'cgp_66666666-6666-4666-8666-666666666666','sxe_22222222-2222-4222-8222-222222222222',"
        "0,'work',120,140,'cal_77777777-7777-4777-8777-777777777777',160,75,88,"
        "'fixed_duration',180,NULL,'2026-09-23T10:00:20+02:00','2026-09-23T10:03:20+02:00','maintain');",

        "INSERT INTO cardio_guidance_events(run_id,phase_id,sequence,observed_at,instruction,bpm,"
        "target_min_bpm,target_max_bpm) VALUES"
        "('cgr_55555555-5555-4555-8555-555555555555','cgp_66666666-6666-4666-8666-666666666666',"
        "0,'2026-09-23T10:00:20+02:00','accelerate',110,120,140),"
        "('cgr_55555555-5555-4555-8555-555555555555','cgp_66666666-6666-4666-8666-666666666666',"
        "1,'2026-09-23T10:00:40+02:00','maintain',130,120,140);",

        "INSERT INTO sleep_diary_entries(entry_id,night_start_date,night_end_date,created_at,updated_at,"
        "current_revision_id,deleted) VALUES('sl_88888888-8888-4888-8888-888888888888','2026-09-22',"
        "'2026-09-23','2026-09-22T22:00:00+02:00','2026-09-23T06:30:00+02:00',"
        "'slr_99999999-9999-4999-8999-999999999999',0);"
        "INSERT INTO sleep_diary_revisions(revision_id,entry_id,parent_revision_id,created_at,"
        "sleep_quality,wake_quality,day_form,treatment_and_notes) VALUES("
        "'slr_99999999-9999-4999-8999-999999999999','sl_88888888-8888-4888-8888-888888888888',"
        "NULL,'2026-09-23T06:30:00+02:00',NULL,NULL,NULL,'');",

        "INSERT INTO sleep_diary_events(revision_id,event_id,event_type,start_at,end_at) VALUES"
        "('slr_99999999-9999-4999-8999-999999999999','sle_1','bed_time',"
        "'2026-09-22T22:30:00+02:00',NULL),"
        "('slr_99999999-9999-4999-8999-999999999999','sle_2','night_get_up',"
        "'2026-09-23T02:10:00+02:00',NULL),"
        "('slr_99999999-9999-4999-8999-999999999999','sle_3','final_get_up',"
        "'2026-09-23T06:20:00+02:00',NULL);"
        "INSERT INTO sleep_medication_intakes(revision_id,intake_id,medication_id,medication_name,"
        "taken_at,dose_value,dose_unit,note,created_at) VALUES("
        "'slr_99999999-9999-4999-8999-999999999999','mdi_1','med_1','Médicament test',"
        "'2026-09-22T22:40:00+02:00',NULL,NULL,NULL,'2026-09-22T22:40:00+02:00');",

        "INSERT INTO heart_rate_captures(capture_id,context_kind,context_id,started_at,ended_at,sensor_name,imported_at)"
        " VALUES('hrc_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','sleep',"
        "'sl_88888888-8888-4888-8888-888888888888','2026-09-22T22:30:00+02:00',"
        "'2026-09-23T06:20:00+02:00','CYCPLUS H2','2026-09-23T07:00:00+02:00');"
        "INSERT INTO heart_rate_samples(capture_id,sequence,observed_at,bpm,exercise_entry_id,"
        "sensor_contact_detected,energy_expended) VALUES("
        "'hrc_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',0,'2026-09-23T02:10:00+02:00',75,NULL,NULL,NULL);",
    };
    size_t index;

    for (index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        if (!execute_sql(database, statements[index])) {
            return false;
        }
    }
    return true;
}

static yyjson_doc *timeline(TrainlogDatabase *database, const char *context_id) {
    char *json = malloc(TRAINLOG_WEB_HEART_RATE_JSON_CAPACITY);
    size_t size = 0U;
    yyjson_doc *document = NULL;
    if (json == NULL) {
        return NULL;
    }
    if (trainlog_web_heart_rate_timeline_json(database,
                                              context_id,
                                              json,
                                              TRAINLOG_WEB_HEART_RATE_JSON_CAPACITY,
                                              &size) == TRAINLOG_STATUS_OK &&
        size > 0U) {
        document = yyjson_read(json, size, 0);
    }
    free(json);
    return document;
}

int main(void) {
    char path[] = "/tmp/trainlog-web-heart-rate-XXXXXX";
    int descriptor = mkstemp(path);
    TrainlogDatabase *database = NULL;
    yyjson_doc *document = NULL;
    yyjson_val *root;
    yyjson_val *capture;
    yyjson_val *samples;
    yyjson_val *events;
    yyjson_val *guidance;
    yyjson_val *phase;
    yyjson_val *calibration;
    int result = EXIT_FAILURE;

    CHECK(descriptor >= 0 && close(descriptor) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(seed(database->connection));

    document = timeline(database, SESSION_ID);
    CHECK(document != NULL);
    root = yyjson_doc_get_root(document);
    CHECK(yyjson_get_bool(yyjson_obj_get(root, "available")));
    capture = yyjson_obj_get(root, "capture");
    CHECK(strcmp(yyjson_get_str(yyjson_obj_get(capture, "context_kind")), "cardio") == 0);
    CHECK(yyjson_get_int(yyjson_obj_get(capture, "sample_count")) == 2);
    samples = yyjson_obj_get(capture, "samples");
    CHECK(yyjson_arr_size(samples) == 2U);
    CHECK(yyjson_arr_size(yyjson_obj_get(yyjson_arr_get(samples, 0U), "rr_1024")) == 1U);
    events = yyjson_obj_get(root, "events");
    CHECK(yyjson_arr_size(events) == 2U);
    guidance = yyjson_obj_get(root, "guidance");
    CHECK(yyjson_is_obj(guidance));
    CHECK(strcmp(yyjson_get_str(yyjson_obj_get(guidance, "run_id")), RUN_ID) == 0);
    phase = yyjson_arr_get(yyjson_obj_get(guidance, "phases"), 0U);
    CHECK(strcmp(yyjson_get_str(yyjson_obj_get(phase, "entry_id")), ENTRY_ID) == 0);
    CHECK(yyjson_get_int(yyjson_obj_get(yyjson_obj_get(phase, "target"), "minimum_bpm")) == 120);
    CHECK(yyjson_arr_size(yyjson_obj_get(phase, "events")) == 2U);
    calibration = yyjson_obj_get(root, "calibration");
    CHECK(yyjson_is_obj(calibration));
    CHECK(strcmp(yyjson_get_str(yyjson_obj_get(calibration, "calibration_id")), CALIBRATION_ID) == 0);
    CHECK(yyjson_get_int(yyjson_obj_get(calibration, "observed_peak_bpm")) == 160);
    CHECK(yyjson_arr_size(yyjson_obj_get(calibration, "recovery")) == 1U);
    yyjson_doc_free(document);
    document = NULL;

    document = timeline(database, SLEEP_ID);
    CHECK(document != NULL);
    root = yyjson_doc_get_root(document);
    CHECK(yyjson_get_bool(yyjson_obj_get(root, "available")));
    capture = yyjson_obj_get(root, "capture");
    CHECK(strcmp(yyjson_get_str(yyjson_obj_get(capture, "context_kind")), "sleep") == 0);
    CHECK(yyjson_get_int(yyjson_obj_get(capture, "sample_count")) == 1);
    events = yyjson_obj_get(root, "events");
    CHECK(yyjson_arr_size(events) == 4U);
    CHECK(yyjson_is_null(yyjson_obj_get(root, "guidance")));
    CHECK(yyjson_is_null(yyjson_obj_get(root, "calibration")));
    yyjson_doc_free(document);
    document = NULL;

    document = timeline(database, "se_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
    CHECK(document != NULL);
    root = yyjson_doc_get_root(document);
    CHECK(!yyjson_get_bool(yyjson_obj_get(root, "available")));
    CHECK(yyjson_is_null(yyjson_obj_get(root, "capture")));
    yyjson_doc_free(document);
    document = NULL;

    {
        char buffer[64];
        size_t size = 123U;
        CHECK(trainlog_web_heart_rate_timeline_json(
                  database, "bad", buffer, sizeof(buffer), &size) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    }

    result = EXIT_SUCCESS;

cleanup:
    if (document != NULL) yyjson_doc_free(document);
    if (database != NULL) trainlog_database_close(database);
    (void)unlink(path);
    if (result == EXIT_SUCCESS) puts("PASS factual Web heart-rate timeline");
    return result;
}
