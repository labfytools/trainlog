#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "trainlog/database.h"
#include "trainlog/web_exercises.h"

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static int scalar(TrainlogDatabase *database, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int value = -1;
    if (sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int(statement, 0);
    }
    (void)sqlite3_finalize(statement);
    return value;
}

static bool json_string(const char *json, const char *key, char *output, size_t capacity) {
    char marker[96];
    const char *start;
    const char *end;
    size_t length;
    CHECK(snprintf(marker, sizeof(marker), "\"%s\":\"", key) > 0);
    start = strstr(json, marker);
    CHECK(start != NULL);
    start += strlen(marker);
    end = strchr(start, '"');
    CHECK(end != NULL);
    length = (size_t)(end - start);
    CHECK(length > 0U && length < capacity);
    (void)memcpy(output, start, length);
    output[length] = '\0';
    return true;
}

static bool lifecycle_preserves_history_and_is_causal(void) {
    static const char CREATE[] =
        "{\"name\":\"  Test   Web  \u00c9paules \" ,\"recording_mode\":\"sets\","
        "\"tracking_mode\":\"reps\",\"data_fields\":0,"
        "\"primary_zone_id\":\"shoulders\",\"secondary_zone_ids\":[\"arms\"]}";
    static const char UPDATE[] = "{\"name\":\"Test Web Renomm\u00e9\",\"recording_mode\":\"sets\","
                                 "\"tracking_mode\":\"duration\",\"data_fields\":0,"
                                 "\"primary_zone_id\":\"core\",\"secondary_zone_ids\":[]}";
    TrainlogDatabase *database = NULL;
    TrainlogWebExercisesQuery query = {0U, 24U, "test web", "sets_reps", "shoulders", false};
    char *created = NULL;
    char *page = NULL;
    char *updated = NULL;
    char *retired = NULL;
    size_t size = 0U;
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char revision[80];
    char updated_revision[80];
    char sql[2048];
    int initial_exercise_count;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    initial_exercise_count = scalar(database, "SELECT COUNT(*) FROM exercises");
    CHECK(trainlog_web_exercises_create_json(database, CREATE, strlen(CREATE), &created, &size) ==
          TRAINLOG_STATUS_OK);
    CHECK(json_string(created, "exercise_id", exercise_id, sizeof(exercise_id)));
    CHECK(json_string(created, "revision", revision, sizeof(revision)));
    CHECK(strstr(created, "\"primary_zone_id\":\"shoulders\"") != NULL);
    CHECK(strstr(created, "\"secondary_zone_ids\":[\"arms\"]") != NULL);
    CHECK(strstr(created, "\"retireable\":true") != NULL);
    CHECK(snprintf(sql,
                   sizeof(sql),
                   "SELECT COUNT(*) FROM exercise_profile_state s JOIN exercises e ON e.id=s."
                   "exercise_row_id WHERE e.exercise_id='%s'",
                   exercise_id) > 0);
    CHECK(scalar(database, sql) == 1);
    CHECK(snprintf(sql,
                   sizeof(sql),
                   "SELECT COUNT(*) FROM exercise_body_zones z JOIN exercises e ON e.id=z."
                   "exercise_row_id WHERE e.exercise_id='%s'",
                   exercise_id) > 0);
    CHECK(scalar(database, sql) == 2);
    CHECK(trainlog_web_exercises_list_json(database, &query, &page, &size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(page, exercise_id) != NULL);
    free(page);
    page = NULL;

    CHECK(snprintf(sql,
                   sizeof(sql),
                   "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES("
                   "'se_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','2026-09-20T10:00:00Z',"
                   "'2026-09-20T11:00:00Z','training');"
                   "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,"
                   "recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds) "
                   "SELECT 'sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',s.id,e.id,'sets','reps',0,"
                   "0,'external',0 FROM sessions s,exercises e WHERE e.exercise_id='%s';"
                   "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) "
                   "SELECT id,0,8,42.5 FROM session_exercises;",
                   exercise_id) > 0);
    CHECK(sqlite3_exec(database->connection, sql, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(trainlog_web_exercises_update_json(
              database, exercise_id, revision, UPDATE, strlen(UPDATE), &updated, &size) ==
          TRAINLOG_STATUS_OK);
    CHECK(json_string(updated, "revision", updated_revision, sizeof(updated_revision)));
    CHECK(strcmp(revision, updated_revision) != 0);
    CHECK(strstr(updated, "\"tracking_mode\":\"duration\"") != NULL);
    CHECK(snprintf(sql,
                   sizeof(sql),
                   "SELECT COUNT(*) FROM exercise_profile_revisions r JOIN exercises e ON e.id=r."
                   "exercise_row_id WHERE e.exercise_id='%s'",
                   exercise_id) > 0);
    CHECK(scalar(database, sql) >= 2);
    CHECK(snprintf(sql,
                   sizeof(sql),
                   "SELECT COUNT(*) FROM exercise_body_zones z JOIN exercises e ON e.id=z."
                   "exercise_row_id WHERE e.exercise_id='%s'",
                   exercise_id) > 0);
    CHECK(scalar(database, sql) == 1);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM session_exercises WHERE recording_mode='sets' AND "
                 "tracking_mode='reps' AND data_fields=0") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM performed_sets WHERE reps=8 AND weight_kg=42.5") ==
          1);
    CHECK(trainlog_web_exercises_update_json(
              database, exercise_id, revision, UPDATE, strlen(UPDATE), &page, &size) ==
          TRAINLOG_STATUS_CONFLICT);
    CHECK(page == NULL);
    CHECK(trainlog_web_exercises_retire_json(
              database, exercise_id, updated_revision, &retired, &size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(retired, "\"state\":\"retired\"") != NULL);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sync_causal_state WHERE target_kind='exercise' AND "
                 "deleted=1") == 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM exercises") == initial_exercise_count + 1);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sessions") == 1);
    query.search = "";
    query.profile = "";
    query.zone_id = NULL;
    CHECK(trainlog_web_exercises_list_json(database, &query, &page, &size) == TRAINLOG_STATUS_OK);
    CHECK(strstr(page, exercise_id) == NULL);
    free(page);
    page = NULL;
    free(retired);
    retired = NULL;
    CHECK(trainlog_web_exercises_retire_json(
              database, exercise_id, updated_revision, &retired, &size) == TRAINLOG_STATUS_OK);
    CHECK(scalar(database,
                 "SELECT COUNT(*) FROM sync_causal_operations WHERE target_kind='exercise'") == 1);
    free(created);
    free(updated);
    free(retired);
    trainlog_database_close(database);
    return true;
}

static bool validation_collision_and_builtin_protection(void) {
    static const char INVALID[] = "{\"name\":\"Invalide\",\"recording_mode\":\"continuous\","
                                  "\"tracking_mode\":\"reps\",\"data_fields\":0,"
                                  "\"primary_zone_id\":null,\"secondary_zone_ids\":[]}";
    static const char DUPLICATE[] =
        "{\"name\":\"  LEG    EXTENSION  \" ,\"recording_mode\":\"sets\","
        "\"tracking_mode\":\"reps\",\"data_fields\":0,"
        "\"primary_zone_id\":\"chest\",\"secondary_zone_ids\":[]}";
    static const char DUPLICATE_KEY[] =
        "{\"name\":\"A\",\"name\":\"B\",\"recording_mode\":\"sets\","
        "\"tracking_mode\":\"reps\",\"data_fields\":0,"
        "\"primary_zone_id\":\"chest\",\"secondary_zone_ids\":[]}";
    static const char SETS_FIELDS[] = "{\"name\":\"Fields\",\"recording_mode\":\"sets\","
                                      "\"tracking_mode\":\"reps\",\"data_fields\":1,"
                                      "\"primary_zone_id\":\"chest\",\"secondary_zone_ids\":[]}";
    TrainlogDatabase *database = NULL;
    char *json = NULL;
    size_t size = 0U;
    char revision[80];

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     "ex_1872246a-39ae-44dc-b58d-f87e90ca49ab",
                                                     "Leg Extension",
                                                     "leg extension",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_web_exercises_create_json(database, INVALID, strlen(INVALID), &json, &size) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_web_exercises_create_json(
              database, DUPLICATE_KEY, strlen(DUPLICATE_KEY), &json, &size) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_web_exercises_create_json(
              database, SETS_FIELDS, strlen(SETS_FIELDS), &json, &size) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_web_exercises_create_json(
              database, DUPLICATE, strlen(DUPLICATE), &json, &size) == TRAINLOG_STATUS_CONFLICT);
    CHECK(trainlog_web_exercises_detail_json(
              database, "ex_1872246a-39ae-44dc-b58d-f87e90ca49ab", &json, &size) ==
          TRAINLOG_STATUS_OK);
    CHECK(json_string(json, "revision", revision, sizeof(revision)));
    CHECK(strstr(json, "\"retireable\":false") != NULL);
    free(json);
    json = NULL;
    CHECK(trainlog_web_exercises_retire_json(
              database, "ex_1872246a-39ae-44dc-b58d-f87e90ca49ab", revision, &json, &size) ==
          TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(scalar(database, "SELECT COUNT(*) FROM sync_causal_operations") == 0);
    trainlog_database_close(database);
    return true;
}

int main(void) {
    if (!lifecycle_preserves_history_and_is_causal() ||
        !validation_collision_and_builtin_protection()) {
        return EXIT_FAILURE;
    }
    (void)puts("web exercises tests passed");
    return EXIT_SUCCESS;
}
