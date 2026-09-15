/**
 * @file test_statistics.c
 * @brief Read-model regressions for factual desktop statistics.
 */

#include <sqlite3.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "trainlog/database.h"
#include "trainlog/statistics.h"

#define NOW_UTC INT64_C(1789473600)

#define CHECK(condition) do { if (!(condition)) {                           \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
        __FILE__, __LINE__, #condition); return false; } } while (0)

static bool exec_sql(sqlite3 *database, const char *sql)
{
    char *error = NULL;
    int status = sqlite3_exec(database, sql, NULL, NULL, &error);
    if (status != SQLITE_OK) {
        (void)fprintf(stderr, "SQLite fixture error: %s\n",
            error != NULL ? error : "unknown");
        sqlite3_free(error);
        return false;
    }
    return true;
}

static void fixture_profile_revision(sqlite3_context *context, int count,
                                     sqlite3_value **values)
{
    static unsigned sequence = 0U;
    char value[32];
    (void)count; (void)values;
    ++sequence;
    (void)snprintf(value, sizeof(value), "pr_fixture_%u", sequence);
    sqlite3_result_text(context, value, -1, SQLITE_TRANSIENT);
}

static bool build_fixture(const char *path)
{
    TrainlogDatabase *database = NULL;
    sqlite3 *raw = NULL;
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    trainlog_database_close(database);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_create_function(raw, "trainlog_profile_revision", -1,
        SQLITE_UTF8, NULL, fixture_profile_revision, NULL, NULL) == SQLITE_OK);
    CHECK(exec_sql(raw,
        "PRAGMA foreign_keys=ON;"
        "INSERT INTO exercises(exercise_id,name,normalized_name,tracking_mode,recording_mode,data_fields,load_semantics) VALUES"
        "('ex_stat_load','Charge canonique','charge canonique','reps','sets',0,'external'),"
        "('ex_stat_alias','Ancien alias','ancien alias','reps','sets',0,'external'),"
        "('ex_stat_cont','Continu','continu','duration','continuous',0,'cardio');"
        "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) SELECT id,'chest','primary' FROM exercises WHERE exercise_id='ex_stat_load';"
        "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) SELECT id,'arms','secondary' FROM exercises WHERE exercise_id='ex_stat_load';"
        "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) SELECT id,'back','primary' FROM exercises WHERE exercise_id='ex_stat_cont';"
        "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES"
        "('se_stat_1','2026-09-10T10:00:00Z','2026-09-10T11:00:00Z','training'),"
        "('se_stat_2','2026-09-14T10:00:00Z','2026-09-14T10:30:00Z','training'),"
        "('se_stat_old','2026-06-17T10:00:00Z','2026-06-17T10:20:00Z','training'),"
        "('se_stat_max','2026-09-15T10:00:00Z','2026-09-15T10:10:00Z','max_test');"));
    CHECK(exec_sql(raw,
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds,target_sets,target_reps,target_weight_kg) "
        "SELECT 'sxe_stat_load',s.id,e.id,'sets','reps',0,0,'external',60,2,10,50 FROM sessions s,exercises e WHERE s.session_id='se_stat_1' AND e.exercise_id='ex_stat_load';"
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds) "
        "SELECT 'sxe_stat_cont',s.id,e.id,'continuous','duration',0,1,'none',0 FROM sessions s,exercises e WHERE s.session_id='se_stat_1' AND e.exercise_id='ex_stat_cont';"
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds,target_sets,target_reps,target_weight_kg) "
        "SELECT 'sxe_stat_alias',s.id,e.id,'sets','reps',0,0,'external',60,1,12,60 FROM sessions s,exercises e WHERE s.session_id='se_stat_2' AND e.exercise_id='ex_stat_alias';"
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds,target_sets,target_reps,target_weight_kg) "
        "SELECT 'sxe_stat_old',s.id,e.id,'sets','reps',0,0,'external',60,1,5,5 FROM sessions s,exercises e WHERE s.session_id='se_stat_old' AND e.exercise_id='ex_stat_load';"
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds) "
        "SELECT 'sxe_stat_max',s.id,e.id,'sets','reps',0,0,'external',60 FROM sessions s,exercises e WHERE s.session_id='se_stat_max' AND e.exercise_id='ex_stat_load';"
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,0,10,50 FROM session_exercises WHERE entry_id='sxe_stat_load';"
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,1,8,50 FROM session_exercises WHERE entry_id='sxe_stat_load';"
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,0,12,60 FROM session_exercises WHERE entry_id='sxe_stat_alias';"
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,0,5,5 FROM session_exercises WHERE entry_id='sxe_stat_old';"
        "INSERT INTO continuous_activity(session_exercise_row_id,duration_seconds) SELECT id,1800 FROM session_exercises WHERE entry_id='sxe_stat_cont';"
        "INSERT INTO max_results(session_exercise_row_id,max_weight_kg) SELECT id,100 FROM session_exercises WHERE entry_id='sxe_stat_max';"
        "INSERT INTO exercise_feedback(feedback_id,session_exercise_row_id,observed_at,raw_text) SELECT 'fb_stat',id,'2026-09-10T11:01:00Z','texte libre' FROM session_exercises WHERE entry_id='sxe_stat_load';"
        "INSERT INTO session_followups(followup_id,session_row_id,observed_at,raw_text) SELECT 'fu_stat',id,'2026-09-11T08:00:00Z','texte libre global' FROM sessions WHERE session_id='se_stat_1';"));
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    return true;
}

static bool test_empty(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogStatisticsSummary summary;
    TrainlogZoneStatistics zones[4];
    size_t count = 99U;
    bool partial = true;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    for (TrainlogStatisticsWindow window = TRAINLOG_STATISTICS_7_DAYS;
         window <= TRAINLOG_STATISTICS_ALL; ++window) {
        CHECK(trainlog_statistics_load_summary(database, window,
            NOW_UTC, &summary) == TRAINLOG_STATUS_OK);
        CHECK(summary.sessions == 0U && summary.sets == 0U &&
              summary.loaded_volume_kg == 0.0 && summary.bucket_count == 0U);
    }
    CHECK(trainlog_statistics_load_zones(database, NOW_UTC, zones, 4U,
        &count, &partial) == TRAINLOG_STATUS_OK);
    CHECK(count == 0U && !partial);
    CHECK(trainlog_statistics_load_exercise(database, "ex_absent", NOW_UTC,
        &(TrainlogExerciseStatistics){0}) == TRAINLOG_STATUS_NOT_FOUND);
    trainlog_database_close(database);
    return true;
}

static bool test_calendar_boundaries(void)
{
    char path[] = "/tmp/trainlog-statistics-calendar-XXXXXX";
    int descriptor = mkstemp(path);
    TrainlogDatabase *database = NULL;
    TrainlogStatisticsSummary summary;
    sqlite3 *raw = NULL;
    const int64_t march_2024_now = INT64_C(1709726400); /* 06/03/2024 13:00 CET */
    const int64_t april_2024_now = INT64_C(1713175200); /* 15/04/2024 12:00 CEST */
    const int64_t march_2023_now = INT64_C(1678881600); /* 15/03/2023 13:00 CET */
    CHECK(descriptor >= 0 && close(descriptor) == 0);
    CHECK(build_fixture(path));
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_create_function(raw, "trainlog_profile_revision", -1,
        SQLITE_UTF8, NULL, fixture_profile_revision, NULL, NULL) == SQLITE_OK);
    CHECK(exec_sql(raw,
        "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES"
        "('se_cal_2023_feb_first','2023-02-01T08:00:00+14:00',NULL,'training'),"
        "('se_cal_2023_feb_last','2023-02-28T23:00:00-10:00',NULL,'training'),"
        "('se_cal_2023_mar_first','2023-03-01T08:00:00Z',NULL,'training'),"
        "('se_cal_2024_feb_first','2024-02-01T08:00:00Z',NULL,'training'),"
        "('se_cal_2024_feb_last','2024-02-29T08:00:00Z',NULL,'training'),"
        "('se_cal_sunday','2024-03-03T08:00:00Z',NULL,'training'),"
        "('se_cal_monday','2024-03-04T08:00:00Z',NULL,'training'),"
        "('se_cal_april','2024-04-15T08:00:00Z',NULL,'training');"
        "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds) "
        "SELECT 'sxe_'||s.session_id,s.id,e.id,'sets','reps',0,0,'external',0 FROM sessions s,exercises e WHERE s.session_id LIKE 'se_cal_%' AND e.exercise_id='ex_stat_load';"
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,0,1,1 FROM session_exercises WHERE entry_id LIKE 'sxe_se_cal_%';"));
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);

    CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_7_DAYS,
        march_2024_now, &summary) == TRAINLOG_STATUS_OK);
    CHECK(summary.bucket_kind == TRAINLOG_STATISTICS_BUCKET_7_DAYS);
    CHECK(summary.bucket_count >= 2U);
    CHECK(strcmp(summary.buckets[summary.bucket_count - 2U].label,
        "26/02-03/03") == 0);
    CHECK(summary.buckets[summary.bucket_count - 2U].sessions == 2U);
    CHECK(strcmp(summary.buckets[summary.bucket_count - 1U].label,
        "04/03-10/03") == 0);
    CHECK(summary.buckets[summary.bucket_count - 1U].sessions == 1U);
    CHECK(summary.buckets[summary.bucket_count - 1U].end_timestamp -
          summary.buckets[summary.bucket_count - 1U].timestamp ==
          INT64_C(7) * INT64_C(86400));

    CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_30_DAYS,
        april_2024_now, &summary) == TRAINLOG_STATUS_OK);
    CHECK(summary.bucket_kind == TRAINLOG_STATISTICS_BUCKET_CALENDAR_MONTH);
    CHECK(summary.bucket_count == 3U);
    CHECK(strcmp(summary.buckets[0].label, "févr. 2024") == 0 &&
          summary.buckets[0].sessions == 2U);
    CHECK(strcmp(summary.buckets[1].label, "mars 2024") == 0 &&
          summary.buckets[1].sessions == 2U);
    CHECK(strcmp(summary.buckets[2].label, "avr. 2024") == 0 &&
          summary.buckets[2].sessions == 1U);
    CHECK(summary.buckets[0].end_timestamp - summary.buckets[0].timestamp ==
          INT64_C(29) * INT64_C(86400));
    CHECK(summary.buckets[1].end_timestamp - summary.buckets[1].timestamp ==
          INT64_C(31) * INT64_C(86400));

    CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_30_DAYS,
        march_2023_now, &summary) == TRAINLOG_STATUS_OK);
    CHECK(summary.bucket_count == 2U);
    CHECK(strcmp(summary.buckets[0].label, "févr. 2023") == 0 &&
          summary.buckets[0].sessions == 2U);
    CHECK(summary.buckets[0].end_timestamp - summary.buckets[0].timestamp ==
          INT64_C(28) * INT64_C(86400));
    CHECK(strcmp(summary.buckets[1].label, "mars 2023") == 0 &&
          summary.buckets[1].sessions == 1U);

    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

static bool test_aggregates(void)
{
    char path[] = "/tmp/trainlog-statistics-XXXXXX";
    int descriptor = mkstemp(path);
    TrainlogDatabase *database = NULL;
    TrainlogStatisticsSummary summary;
    TrainlogExerciseStatistics exercise;
    TrainlogZoneStatistics zones[16];
    TrainlogMaxListItem maxima[16];
    TrainlogExerciseListStatistic exercise_list[16];
    size_t maximum_count = 0U;
    size_t exercise_list_count = 0U;
    size_t zone_count = 0U;
    bool partial = false;
    size_t index;
    CHECK(descriptor >= 0 && close(descriptor) == 0);
    CHECK(build_fixture(path));
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    /* Alias reconciliation repoints the persisted occurrence; statistics then
     * expose only the canonical identity, never the retired display name. */
    CHECK(trainlog_database_merge_exercises(database, "ex_stat_alias",
        "ex_stat_load") == TRAINLOG_STATUS_OK);
    CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_7_DAYS,
        NOW_UTC, &summary) == TRAINLOG_STATUS_OK);
    CHECK(summary.sessions == 2U && summary.distinct_exercises == 1U);
    CHECK(summary.occurrences == 2U && summary.sets == 1U);
    CHECK(summary.repetitions == 12U && summary.activity_duration_seconds == 0U);
    CHECK(summary.session_duration_seconds == 2400U &&
          summary.sessions_with_duration == 2U);
    CHECK(fabs(summary.loaded_volume_kg - 720.0) < 0.001);
    CHECK(summary.active_days == 2U && summary.active_weeks == 1U);
    CHECK(summary.days_since_last_session == 0U);
    CHECK(summary.immediate_feedback_count == 0U && summary.followup_count == 0U);
    CHECK(summary.top_feedback_count == 0U);
    CHECK(summary.planned_actual_occurrences == 1U &&
          summary.planned_sets == 1U && summary.actual_sets_for_plans == 1U);
    CHECK(summary.planned_repetitions == 12U &&
          summary.actual_repetitions_for_plans == 12U);
    CHECK(summary.bucket_count >= 1U);
    CHECK(summary.bucket_count == 4U);
    CHECK(summary.bucket_kind == TRAINLOG_STATISTICS_BUCKET_7_DAYS);
    CHECK(strcmp(summary.buckets[0].label, "24/08-30/08") == 0);
    CHECK(strcmp(summary.buckets[summary.bucket_count - 1U].label,
        "14/09-20/09") == 0);
    CHECK(summary.buckets[2].sessions == 1U &&
          summary.buckets[3].sessions == 2U);
    {
        size_t bucket_sessions = 0U;
        for (index = 0U; index < summary.bucket_count; ++index)
            bucket_sessions += summary.buckets[index].sessions;
        CHECK(bucket_sessions == 3U);
    }

    CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_30_DAYS,
        NOW_UTC, &summary) == TRAINLOG_STATUS_OK && summary.sessions == 3U);
    CHECK(summary.bucket_kind == TRAINLOG_STATISTICS_BUCKET_CALENDAR_MONTH);
    CHECK(summary.bucket_count == 4U);
    CHECK(strcmp(summary.buckets[0].label, "juin 2026") == 0);
    CHECK(strcmp(summary.buckets[1].label, "juil. 2026") == 0);
    CHECK(strcmp(summary.buckets[2].label, "août 2026") == 0);
    CHECK(strcmp(summary.buckets[3].label, "sept. 2026") == 0);
    CHECK(summary.buckets[0].sessions == 1U);
    CHECK(summary.buckets[1].sessions == 0U &&
          summary.buckets[2].sessions == 0U);
    CHECK(summary.buckets[3].sessions == 3U);
    {
        size_t bucket_sessions = 0U;
        TrainlogStatisticsSummary repeated;
        for (index = 0U; index < summary.bucket_count; ++index)
            bucket_sessions += summary.buckets[index].sessions;
        /* The persisted 17/06 date belongs to June exactly once. */
        CHECK(bucket_sessions == 4U);
        CHECK(trainlog_statistics_load_summary(database,
            TRAINLOG_STATISTICS_30_DAYS, NOW_UTC, &repeated) ==
            TRAINLOG_STATUS_OK);
        CHECK(repeated.bucket_count == summary.bucket_count &&
              memcmp(repeated.buckets, summary.buckets,
                  summary.bucket_count * sizeof(summary.buckets[0])) == 0);
    }
    CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_ALL,
        NOW_UTC, &summary) == TRAINLOG_STATUS_OK && summary.sessions == 4U &&
        summary.repetitions == 35U);
    CHECK(summary.bucket_kind == TRAINLOG_STATISTICS_BUCKET_7_DAYS);
    {
        bool has_zero_week = false;
        for (index = 0U; index < summary.bucket_count; ++index)
            if (summary.buckets[index].sessions == 0U) has_zero_week = true;
        CHECK(has_zero_week);
    }

    CHECK(trainlog_statistics_load_zones(database, NOW_UTC, zones, 16U,
        &zone_count, &partial) == TRAINLOG_STATUS_OK && !partial);
    for (index = 0U; index < zone_count; ++index) {
        if (strcmp(zones[index].zone_id, "chest") == 0)
            CHECK(zones[index].primary_7 == 3U &&
                  zones[index].secondary_7 == 0U);
        if (strcmp(zones[index].zone_id, "arms") == 0)
            CHECK(zones[index].secondary_30 == 3U);
        if (strcmp(zones[index].zone_id, "back") == 0)
            CHECK(zones[index].primary_7 == 1U);
    }

    CHECK(trainlog_statistics_load_exercise(database, "ex_stat_load", NOW_UTC,
        &exercise) == TRAINLOG_STATUS_OK);
    CHECK(exercise.occurrences == 4U && exercise.sessions == 4U);
    CHECK(exercise.frequency_7 == 3U && exercise.frequency_30 == 3U);
    CHECK(exercise.sets == 4U && exercise.repetitions == 35U);
    CHECK(fabs(exercise.loaded_volume_kg - 1645.0) < 0.001);
    CHECK(exercise.has_highest_load && exercise.highest_load_kg == 60.0);
    CHECK(exercise.has_repetitions_at_load && exercise.highest_repetitions == 12 &&
          exercise.highest_repetitions_load_kg == 60.0);
    CHECK(exercise.max_count == 1U && exercise.latest_max_kg == 100.0 &&
          !exercise.has_previous_max);
    CHECK(exercise.immediate_feedback_count == 1U);
    CHECK(exercise.has_latest_planned_actual_weight &&
          exercise.latest_planned_weight_kg == 60.0 &&
          exercise.latest_actual_weight_kg == 60.0);
    CHECK(exercise.series_count[TRAINLOG_EXERCISE_SERIES_MAX] == 1U);
    CHECK(trainlog_statistics_load_exercise(database, "ex_stat_alias", NOW_UTC,
        &exercise) == TRAINLOG_STATUS_NOT_FOUND);
    CHECK(trainlog_statistics_list_maxima(database, maxima, 16U,
        &maximum_count, &partial) == TRAINLOG_STATUS_OK && !partial &&
        maximum_count == 1U && strcmp(maxima[0].exercise_id, "ex_stat_load") == 0 &&
        maxima[0].latest_max_kg == 100.0);
    CHECK(trainlog_statistics_list_exercises(database, exercise_list, 16U,
        &exercise_list_count, &partial) == TRAINLOG_STATUS_OK && !partial &&
        exercise_list_count == 2U);
    /* A sparse old observation still expands total duration. Adaptive choice
     * is duration-based, not dependent on the number of active weeks. */
    {
        sqlite3 *raw = NULL;
        TrainlogStatisticsSummary repeated;
        CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
        CHECK(sqlite3_create_function(raw, "trainlog_profile_revision", -1,
            SQLITE_UTF8, NULL, fixture_profile_revision, NULL, NULL) == SQLITE_OK);
        CHECK(exec_sql(raw,
            "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES"
            "('se_stat_ancient','2020-01-01T10:00:00Z','2020-01-01T10:10:00Z','training');"
            "INSERT INTO session_exercises(entry_id,session_row_id,exercise_row_id,recording_mode,tracking_mode,data_fields,position,load_mode,rest_seconds) "
            "SELECT 'sxe_stat_ancient',s.id,e.id,'sets','reps',0,0,'external',0 FROM sessions s,exercises e WHERE s.session_id='se_stat_ancient' AND e.exercise_id='ex_stat_load';"
            "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,0,1,1 FROM session_exercises WHERE entry_id='sxe_stat_ancient';"));
        CHECK(sqlite3_close(raw) == SQLITE_OK);
        CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_ALL,
            NOW_UTC, &summary) == TRAINLOG_STATUS_OK);
        CHECK(summary.bucket_kind ==
              TRAINLOG_STATISTICS_BUCKET_CALENDAR_MONTH);
        CHECK(trainlog_statistics_load_summary(database, TRAINLOG_STATISTICS_ALL,
            NOW_UTC, &repeated) == TRAINLOG_STATUS_OK);
        CHECK(repeated.bucket_kind == summary.bucket_kind &&
              repeated.bucket_count == summary.bucket_count);
    }
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void)
{
    /* The production contract uses the user's local civil date. Pin the test
     * locale so exact bucket-boundary fixtures are deterministic in CI. */
    if (setenv("TZ", "Europe/Paris", 1) != 0) return 1;
    tzset();
    if (!test_empty() || !test_calendar_boundaries() || !test_aggregates())
        return 1;
    (void)printf("PASS statistics\n");
    return 0;
}
