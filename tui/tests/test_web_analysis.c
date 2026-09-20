#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"
#include "trainlog/web_analysis.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);  \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool read_snapshot(TrainlogDatabase *database,
                          TrainlogWebAnalysisPeriod period,
                          const char *exercise_id,
                          const char *metric,
                          yyjson_doc **document) {
    TrainlogWebAnalysisQuery query = {
        .period = period,
        .reference_unix_second = INT64_C(1789560000),
        .exercise_id = exercise_id,
        .measurement_metric = metric,
    };
    char json[TRAINLOG_WEB_ANALYSIS_JSON_CAPACITY];
    size_t size = 0U;
    TrainlogStatus status;

    status = trainlog_web_analysis_json(database, &query, json, sizeof(json), &size);
    if (status != TRAINLOG_STATUS_OK || size == 0U) {
        (void)fprintf(stderr, "analysis status=%d size=%zu\n", (int)status, size);
        return false;
    }
    *document = yyjson_read(json, size, 0U);
    return *document != NULL;
}

static bool insert_session(TrainlogDatabase *database,
                           const char *session_id,
                           const char *entry_id,
                           const char *exercise_id,
                           const char *started_at,
                           TrainlogTrackingMode tracking,
                           TrainlogRecordingMode recording,
                           TrainlogLoadMode load,
                           bool explicit_max) {
    TrainlogSessionInput session;
    TrainlogSessionExerciseInput occurrence;
    TrainlogSetInput sets[2] = {
        {.reps = 8, .has_weight = load == TRAINLOG_LOAD_EXTERNAL, .weight_kg = 40.0},
        {.reps = 10, .has_weight = load == TRAINLOG_LOAD_EXTERNAL, .weight_kg = 42.0},
    };

    (void)memset(&session, 0, sizeof(session));
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    (void)snprintf(occurrence.entry_id, sizeof(occurrence.entry_id), "%s", entry_id);
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s", exercise_id);
    session.session_type = explicit_max ? TRAINLOG_SESSION_MAX_TEST : TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    occurrence.tracking_mode = tracking;
    occurrence.recording_mode = recording;
    occurrence.load_mode = load;
    if (explicit_max) {
        occurrence.has_max_weight = true;
        occurrence.max_weight_kg = 100.0;
    } else if (recording == TRAINLOG_RECORDING_CONTINUOUS) {
        occurrence.continuous_duration_seconds = 1200;
        occurrence.continuous_distance_km = 3.0;
        occurrence.continuous_speed_kmh = 9.0;
    } else {
        occurrence.sets = sets;
        occurrence.set_count = 2U;
    }
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool test_empty_and_periods(void) {
    static const TrainlogWebAnalysisPeriod PERIODS[] = {
        TRAINLOG_WEB_ANALYSIS_7_DAYS,
        TRAINLOG_WEB_ANALYSIS_30_DAYS,
        TRAINLOG_WEB_ANALYSIS_90_DAYS,
        TRAINLOG_WEB_ANALYSIS_ALL,
    };
    TrainlogDatabase *database = NULL;
    size_t index;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    for (index = 0U; index < sizeof(PERIODS) / sizeof(PERIODS[0]); ++index) {
        yyjson_doc *document = NULL;
        yyjson_val *root;
        CHECK(read_snapshot(database, PERIODS[index], NULL, "weight", &document));
        root = yyjson_doc_get_root(document);
        CHECK(yyjson_get_int(yyjson_obj_get(yyjson_obj_get(root, "overview"), "sessions")) == 0);
        CHECK(yyjson_is_null(yyjson_obj_get(root, "exercise")));
        CHECK(yyjson_is_null(yyjson_obj_get(root, "active_program")));
        yyjson_doc_free(document);
    }
    trainlog_database_close(database);
    return true;
}

static bool test_factual_models(void) {
    static const char LOADED[] = "ex_10000000-0000-4000-8000-000000000001";
    static const char UNLOADED[] = "ex_10000000-0000-4000-8000-000000000002";
    static const char DURATION[] = "ex_10000000-0000-4000-8000-000000000003";
    const char *none[] = {NULL};
    TrainlogDatabase *database = NULL;
    TrainlogBodyObservationInput observation;
    yyjson_doc *document = NULL;
    yyjson_val *root;
    yyjson_val *exercise;
    yyjson_val *measurements;
    yyjson_val *summaries;
    yyjson_val *waist;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     LOADED,
                                                     "Loaded",
                                                     "loaded",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     UNLOADED,
                                                     "Unloaded",
                                                     "unloaded",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     DURATION,
                                                     "Duration",
                                                     "duration",
                                                     TRAINLOG_TRACKING_DURATION,
                                                     TRAINLOG_RECORDING_CONTINUOUS,
                                                     3) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database, LOADED, "chest", none, 0U) ==
          TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database, UNLOADED, "arms", none, 0U) ==
          TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database, DURATION, "thighs", none, 0U) ==
          TRAINLOG_STATUS_OK);
    CHECK(insert_session(database,
                         "se_10000000-0000-4000-8000-000000000001",
                         "en_10000000-0000-4000-8000-000000000001",
                         LOADED,
                         "2026-09-15T10:00:00Z",
                         TRAINLOG_TRACKING_REPS,
                         TRAINLOG_RECORDING_SETS,
                         TRAINLOG_LOAD_EXTERNAL,
                         false));
    CHECK(insert_session(database,
                         "se_10000000-0000-4000-8000-000000000002",
                         "en_10000000-0000-4000-8000-000000000002",
                         UNLOADED,
                         "2026-09-14T10:00:00Z",
                         TRAINLOG_TRACKING_REPS,
                         TRAINLOG_RECORDING_SETS,
                         TRAINLOG_LOAD_NONE,
                         false));
    CHECK(insert_session(database,
                         "se_10000000-0000-4000-8000-000000000003",
                         "en_10000000-0000-4000-8000-000000000003",
                         DURATION,
                         "2026-09-13T10:00:00Z",
                         TRAINLOG_TRACKING_DURATION,
                         TRAINLOG_RECORDING_CONTINUOUS,
                         TRAINLOG_LOAD_NONE,
                         false));
    CHECK(insert_session(database,
                         "se_10000000-0000-4000-8000-000000000004",
                         "en_10000000-0000-4000-8000-000000000004",
                         LOADED,
                         "2026-09-12T10:00:00Z",
                         TRAINLOG_TRACKING_REPS,
                         TRAINLOG_RECORDING_SETS,
                         TRAINLOG_LOAD_EXTERNAL,
                         true));

    (void)memset(&observation, 0, sizeof(observation));
    (void)snprintf(observation.observation_id, sizeof(observation.observation_id), "bo_analysis_1");
    (void)snprintf(
        observation.observed_at, sizeof(observation.observed_at), "2026-09-10T08:00:00Z");
    observation.has_body_weight = true;
    observation.body_weight_kg = 80.0;
    observation.has_waist = true;
    observation.waist_cm = 90.0;
    CHECK(trainlog_database_insert_body_observation(database, &observation) == TRAINLOG_STATUS_OK);
    (void)snprintf(observation.observation_id, sizeof(observation.observation_id), "bo_analysis_2");
    (void)snprintf(
        observation.observed_at, sizeof(observation.observed_at), "2026-09-15T08:00:00Z");
    observation.body_weight_kg = 79.5;
    observation.waist_cm = 88.0;
    CHECK(trainlog_database_insert_body_observation(database, &observation) == TRAINLOG_STATUS_OK);

    CHECK(
        sqlite3_exec(database->connection,
                     "INSERT INTO programs VALUES('pg_analysis','Program',NULL,'active',NULL,NULL,"
                     "'2026-09-01T00:00:00Z','2026-09-01T00:00:00Z','rev','test',1,"
                     "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',NULL);"
                     "INSERT INTO program_sessions VALUES('ps_analysis','pg_analysis',0,'Next',"
                     "'training','2026-09-21',NULL);",
                     NULL,
                     NULL,
                     NULL) == SQLITE_OK);

    CHECK(read_snapshot(database, TRAINLOG_WEB_ANALYSIS_30_DAYS, LOADED, "waist", &document));
    root = yyjson_doc_get_root(document);
    CHECK(yyjson_get_int(yyjson_obj_get(yyjson_obj_get(root, "overview"), "sessions")) == 4);
    exercise = yyjson_obj_get(root, "exercise");
    CHECK(strcmp(yyjson_get_str(yyjson_obj_get(exercise, "exercise_id")), LOADED) == 0);
    CHECK(yyjson_arr_size(yyjson_obj_get(exercise, "points")) == 2U);
    CHECK(yyjson_arr_size(yyjson_obj_get(root, "body_zones")) == 3U);
    CHECK(!yyjson_is_null(yyjson_obj_get(root, "active_program")));
    measurements = yyjson_obj_get(root, "measurements");
    CHECK(yyjson_arr_size(yyjson_obj_get(measurements, "points")) == 2U);
    summaries = yyjson_obj_get(measurements, "summaries");
    waist = yyjson_arr_get(summaries, 4U);
    CHECK(yyjson_get_int(yyjson_obj_get(waist, "count")) == 2);
    CHECK(yyjson_get_real(yyjson_obj_get(waist, "delta")) == -2.0);
    yyjson_doc_free(document);
    trainlog_database_close(database);
    return true;
}

int main(void) {
    return test_empty_and_periods() && test_factual_models() ? 0 : 1;
}
