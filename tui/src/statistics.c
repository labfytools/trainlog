/**
 * @file statistics.c
 * @brief Read-only SQLite aggregation for auditable TUI statistics.
 */

#include "trainlog/statistics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "database_internal.h"
#include "timestamp.h"

#define SECONDS_PER_DAY INT64_C(86400)
#define SECONDS_PER_WEEK (INT64_C(7) * SECONDS_PER_DAY)

static TrainlogStatus prepare(sqlite3 *database, const char *sql,
                              sqlite3_stmt **statement)
{
    return database != NULL && sql != NULL && statement != NULL &&
        sqlite3_prepare_v2(database, sql, -1, statement, NULL) == SQLITE_OK
        ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

static bool size_column(sqlite3_stmt *statement, int column, size_t *output)
{
    sqlite3_int64 value = sqlite3_column_int64(statement, column);
    if (value < 0 || (uint64_t)value > (uint64_t)SIZE_MAX) return false;
    *output = (size_t)value;
    return true;
}

static bool u64_column(sqlite3_stmt *statement, int column, uint64_t *output)
{
    sqlite3_int64 value = sqlite3_column_int64(statement, column);
    if (value < 0) return false;
    *output = (uint64_t)value;
    return true;
}

static bool copy_text(sqlite3_stmt *statement, int column, char *output,
                      size_t capacity)
{
    const unsigned char *value = sqlite3_column_text(statement, column);
    int bytes = sqlite3_column_bytes(statement, column);
    if (output == NULL || capacity == 0U || value == NULL || bytes < 0 ||
        (size_t)bytes >= capacity) return false;
    (void)memcpy(output, value, (size_t)bytes);
    output[bytes] = '\0';
    return true;
}

static TrainlogStatus bind_window(sqlite3_stmt *statement, int64_t start,
                                  int64_t now)
{
    if (sqlite3_bind_int64(statement, 1, start) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, now) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    return TRAINLOG_STATUS_OK;
}

static bool local_date_timestamp(const char *date, int64_t *output)
{
    char timestamp[32];
    TrainlogTimestampKey key;
    TrainlogTimestampKey unix_epoch;
    int written;
    if (date == NULL || output == NULL || strlen(date) != 10U) return false;
    written = snprintf(timestamp, sizeof(timestamp), "%sT00:00:00Z", date);
    if (written != 20 || !trainlog_timestamp_parse(timestamp, 20U, &key) ||
        !trainlog_timestamp_parse("1970-01-01T00:00:00Z", 20U, &unix_epoch))
        return false;
    /* TrainlogTimestampKey uses a proleptic year-1 epoch for ordering. Chart
     * coordinates use Unix seconds, so the epochs must never be conflated. */
    *output = key.utc_second - unix_epoch.utc_second;
    return true;
}

static bool current_local_week_timestamp(int64_t now, int64_t *output)
{
    time_t seconds = (time_t)now;
    struct tm local;
    char date[11];
    int offset;
    if ((int64_t)seconds != now || localtime_r(&seconds, &local) == NULL) return false;
    offset = (local.tm_wday + 6) % 7;
    local.tm_mday -= offset;
    local.tm_hour = 12; local.tm_min = 0; local.tm_sec = 0;
    if (mktime(&local) == (time_t)-1 || strftime(date, sizeof(date), "%Y-%m-%d", &local) != 10U)
        return false;
    return local_date_timestamp(date, output);
}

static bool current_local_date(int64_t now, char output[11],
    int64_t *timestamp)
{
    time_t seconds = (time_t)now;
    struct tm local;
    if ((int64_t)seconds != now || localtime_r(&seconds, &local) == NULL ||
        strftime(output, 11U, "%Y-%m-%d", &local) != 10U)
        return false;
    return local_date_timestamp(output, timestamp);
}

static bool calendar_window_start(TrainlogStatisticsWindow window,
    int64_t now, int64_t *output)
{
    char date[11];
    int64_t ignored;
    if (output == NULL) return false;
    if (window == TRAINLOG_STATISTICS_ALL) {
        *output = INT64_MIN;
        return true;
    }
    if (window == TRAINLOG_STATISTICS_7_DAYS)
        return current_local_week_timestamp(now, output);
    if (!current_local_date(now, date, &ignored)) return false;
    date[8] = '0';
    date[9] = '1';
    return local_date_timestamp(date, output);
}

static const char *actual_occurrences_cte(void)
{
    return "WITH actual_occurrences AS ("
        "SELECT se.id,se.session_row_id,se.exercise_row_id,se.entry_id,"
        "se.load_mode,se.target_sets,se.target_reps,se.target_weight_kg "
        "FROM session_exercises se WHERE EXISTS(SELECT 1 FROM performed_sets p WHERE p.session_exercise_row_id=se.id) "
        "OR EXISTS(SELECT 1 FROM continuous_activity c WHERE c.session_exercise_row_id=se.id) "
        "OR EXISTS(SELECT 1 FROM max_results m WHERE m.session_exercise_row_id=se.id)),"
        "actual_sessions AS (SELECT DISTINCT s.id,s.session_id,s.started_at,s.ended_at "
        "FROM sessions s JOIN actual_occurrences ao ON ao.session_row_id=s.id) ";
}

static TrainlogStatus load_summary_row(TrainlogDatabase *database,
    int64_t start, int64_t now, TrainlogStatisticsSummary *output)
{
    sqlite3_stmt *statement = NULL;
    char sql[8192];
    int written = snprintf(sql, sizeof(sql),
        "%s SELECT "
        "(SELECT COUNT(*) FROM actual_sessions s WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(DISTINCT ao.exercise_row_id) FROM actual_occurrences ao JOIN sessions s ON s.id=ao.session_row_id WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(*) FROM actual_occurrences ao JOIN sessions s ON s.id=ao.session_row_id WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(*) FROM performed_sets p JOIN actual_occurrences ao ON ao.id=p.session_exercise_row_id JOIN sessions s ON s.id=ao.session_row_id WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM(p.reps),0) FROM performed_sets p JOIN actual_occurrences ao ON ao.id=p.session_exercise_row_id JOIN sessions s ON s.id=ao.session_row_id WHERE p.reps IS NOT NULL AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM(COALESCE(p.duration_seconds,0)),0) FROM performed_sets p JOIN actual_occurrences ao ON ao.id=p.session_exercise_row_id JOIN sessions s ON s.id=ao.session_row_id WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2) + "
        "(SELECT COALESCE(SUM(c.duration_seconds),0) FROM continuous_activity c JOIN actual_occurrences ao ON ao.id=c.session_exercise_row_id JOIN sessions s ON s.id=ao.session_row_id WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM(unixepoch(s.ended_at)-unixepoch(s.started_at)),0) FROM actual_sessions s WHERE s.ended_at IS NOT NULL AND unixepoch(s.ended_at)>=unixepoch(s.started_at) AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(*) FROM actual_sessions s WHERE s.ended_at IS NOT NULL AND unixepoch(s.ended_at)>=unixepoch(s.started_at) AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM(p.reps*p.weight_kg),0.0) FROM performed_sets p JOIN actual_occurrences ao ON ao.id=p.session_exercise_row_id JOIN sessions s ON s.id=ao.session_row_id JOIN exercises e ON e.id=ao.exercise_row_id WHERE p.reps IS NOT NULL AND p.weight_kg IS NOT NULL AND ao.load_mode='external' AND e.load_semantics='external' AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(DISTINCT date(s.started_at)) FROM actual_sessions s WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(DISTINCT CAST(unixepoch(s.started_at)/604800 AS INTEGER)) FROM actual_sessions s WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(MAX(unixepoch(s.started_at)),0) FROM actual_sessions s WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(*) FROM exercise_feedback f JOIN session_exercises se ON se.id=f.session_exercise_row_id JOIN sessions s ON s.id=se.session_row_id WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(*) FROM session_followups f JOIN sessions s ON s.id=f.session_row_id WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COUNT(*) FROM actual_occurrences ao JOIN sessions s ON s.id=ao.session_row_id WHERE ao.target_sets IS NOT NULL AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM(ao.target_sets),0) FROM actual_occurrences ao JOIN sessions s ON s.id=ao.session_row_id WHERE ao.target_sets IS NOT NULL AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM((SELECT COUNT(*) FROM performed_sets p WHERE p.session_exercise_row_id=ao.id)),0) FROM actual_occurrences ao JOIN sessions s ON s.id=ao.session_row_id WHERE ao.target_sets IS NOT NULL AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM(ao.target_sets*ao.target_reps),0) FROM actual_occurrences ao JOIN sessions s ON s.id=ao.session_row_id WHERE ao.target_sets IS NOT NULL AND ao.target_reps IS NOT NULL AND unixepoch(s.started_at) BETWEEN ?1 AND ?2),"
        "(SELECT COALESCE(SUM((SELECT COALESCE(SUM(p.reps),0) FROM performed_sets p WHERE p.session_exercise_row_id=ao.id)),0) FROM actual_occurrences ao JOIN sessions s ON s.id=ao.session_row_id WHERE ao.target_sets IS NOT NULL AND ao.target_reps IS NOT NULL AND unixepoch(s.started_at) BETWEEN ?1 AND ?2)",
        actual_occurrences_cte());
    if (written < 0 || (size_t)written >= sizeof(sql) ||
        prepare(database->connection, sql, &statement) != TRAINLOG_STATUS_OK ||
        bind_window(statement, start, now) != TRAINLOG_STATUS_OK) goto fail;
    if (sqlite3_step(statement) != SQLITE_ROW ||
        !size_column(statement, 0, &output->sessions) ||
        !size_column(statement, 1, &output->distinct_exercises) ||
        !size_column(statement, 2, &output->occurrences) ||
        !size_column(statement, 3, &output->sets) ||
        !u64_column(statement, 4, &output->repetitions) ||
        !u64_column(statement, 5, &output->activity_duration_seconds) ||
        !u64_column(statement, 6, &output->session_duration_seconds) ||
        !size_column(statement, 7, &output->sessions_with_duration) ||
        !isfinite(sqlite3_column_double(statement, 8)) ||
        !size_column(statement, 9, &output->active_days) ||
        !size_column(statement, 10, &output->active_weeks) ||
        !size_column(statement, 12, &output->immediate_feedback_count) ||
        !size_column(statement, 13, &output->followup_count) ||
        !size_column(statement, 14, &output->planned_actual_occurrences) ||
        !u64_column(statement, 15, &output->planned_sets) ||
        !u64_column(statement, 16, &output->actual_sets_for_plans) ||
        !u64_column(statement, 17, &output->planned_repetitions) ||
        !u64_column(statement, 18, &output->actual_repetitions_for_plans))
        goto fail;
    output->loaded_volume_kg = sqlite3_column_double(statement, 8);
    output->latest_session_timestamp = sqlite3_column_int64(statement, 11);
    if (sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    statement = NULL;
    return TRAINLOG_STATUS_OK;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static bool month_key_parse(const char *month, int64_t *key)
{
    int year;
    int number;
    char trailing;
    if (month == NULL || key == NULL ||
        sscanf(month, "%d-%d%c", &year, &number, &trailing) != 2 ||
        year < 1 || number < 1 || number > 12)
        return false;
    *key = (int64_t)year * INT64_C(12) + (int64_t)(number - 1);
    return true;
}

static bool month_key_date(int64_t key, char output[11])
{
    int64_t year;
    int64_t month;
    int written;
    if (key < INT64_C(12)) return false;
    year = key / INT64_C(12);
    month = key % INT64_C(12) + 1;
    if (year > 9999) return false;
    written = snprintf(output, 11U, "%04lld-%02lld-01",
        (long long)year, (long long)month);
    return written == 10;
}

static bool format_month_label(int64_t key, char *output, size_t capacity)
{
    static const char *const names[12] = {
        "janv.", "févr.", "mars", "avr.", "mai", "juin",
        "juil.", "août", "sept.", "oct.", "nov.", "déc."
    };
    int64_t year = key / INT64_C(12);
    int64_t month = key % INT64_C(12);
    int written;
    if (output == NULL || capacity == 0U || month < 0 || month >= 12 ||
        year < 1 || year > 9999) return false;
    written = snprintf(output, capacity, "%s %04lld", names[month],
        (long long)year);
    return written >= 0 && (size_t)written < capacity;
}

static TrainlogStatus load_month_buckets(TrainlogDatabase *database,
    int64_t now, bool recent_only, TrainlogStatisticsSummary *output)
{
    sqlite3_stmt *statement = NULL;
    TrainlogStatisticsBucket active[TRAINLOG_STATISTICS_BUCKET_MAX];
    int64_t active_keys[TRAINLOG_STATISTICS_BUCKET_MAX];
    size_t active_count = 0U;
    int64_t current_key;
    int64_t first_key = INT64_MAX;
    char current_date[11];
    char current_month[8];
    int64_t ignored_timestamp;
    char sql[4096];
    int written = snprintf(sql, sizeof(sql),
        "%s, months AS (SELECT substr(s.started_at,1,7) month,"
        "COUNT(DISTINCT s.id) sessions,COUNT(p.id) sets,COALESCE(SUM(p.reps),0) reps,"
        "COALESCE(SUM(CASE WHEN p.reps IS NOT NULL AND p.weight_kg IS NOT NULL AND ao.load_mode='external' AND e.load_semantics='external' THEN p.reps*p.weight_kg ELSE 0 END),0.0) volume "
        "FROM actual_sessions s LEFT JOIN actual_occurrences ao ON ao.session_row_id=s.id "
        "LEFT JOIN exercises e ON e.id=ao.exercise_row_id LEFT JOIN performed_sets p ON p.session_exercise_row_id=ao.id "
        "WHERE unixepoch(s.started_at)<=?1 GROUP BY month) "
        "SELECT month,sessions,sets,reps,volume,"
        "(SELECT COUNT(*) FROM exercise_feedback f JOIN session_exercises sf ON sf.id=f.session_exercise_row_id JOIN sessions ss ON ss.id=sf.session_row_id WHERE substr(ss.started_at,1,7)=months.month AND unixepoch(ss.started_at)<=?1) "
        "FROM months ORDER BY month DESC LIMIT %u;",
        actual_occurrences_cte(), TRAINLOG_STATISTICS_BUCKET_MAX + 1U);
    int step;
    if (!current_local_date(now, current_date, &ignored_timestamp))
        return TRAINLOG_STATUS_DATABASE_ERROR;
    (void)memcpy(current_month, current_date, 7U);
    current_month[7] = '\0';
    if (!month_key_parse(current_month, &current_key))
        return TRAINLOG_STATUS_DATABASE_ERROR;
    if (written < 0 || (size_t)written >= sizeof(sql) ||
        prepare(database->connection, sql, &statement) != TRAINLOG_STATUS_OK ||
        sqlite3_bind_int64(statement, 1, now) != SQLITE_OK) goto fail;
    output->buckets_partial = false;
    output->bucket_kind = TRAINLOG_STATISTICS_BUCKET_CALENDAR_MONTH;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogStatisticsBucket *bucket;
        const char *month = (const char *)sqlite3_column_text(statement, 0);
        int64_t key;
        if (!month_key_parse(month, &key)) goto fail;
        if (recent_only && (key < current_key - 3 || key > current_key))
            continue;
        if (active_count == TRAINLOG_STATISTICS_BUCKET_MAX) {
            output->buckets_partial = true; continue;
        }
        bucket = &active[active_count];
        (void)memset(bucket, 0, sizeof(*bucket));
        active_keys[active_count++] = key;
        if (!size_column(statement, 1, &bucket->sessions) ||
            !size_column(statement, 2, &bucket->sets) ||
            !u64_column(statement, 3, &bucket->repetitions) ||
            !isfinite(sqlite3_column_double(statement, 4)) ||
            !size_column(statement, 5, &bucket->immediate_feedback_count)) goto fail;
        bucket->loaded_volume_kg = sqlite3_column_double(statement, 4);
        if (key < first_key) first_key = key;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    output->bucket_count = 0U;
    if (first_key == INT64_MAX) return TRAINLOG_STATUS_OK;
    if (recent_only && first_key < current_key - 3) first_key = current_key - 3;
    if (current_key - first_key + 1 >
        (int64_t)TRAINLOG_STATISTICS_BUCKET_MAX) {
        first_key = current_key -
            ((int64_t)TRAINLOG_STATISTICS_BUCKET_MAX - 1);
        output->buckets_partial = true;
    }
    /* INVARIANT: Gregorian month keys are consecutive and non-overlapping.
     * Filling every key preserves recorded inactivity and naturally handles
     * February, leap years and year boundaries without day arithmetic. */
    for (int64_t key = first_key; key <= current_key; ++key) {
        TrainlogStatisticsBucket *bucket =
            &output->buckets[output->bucket_count++];
        char start_date[11];
        char end_date[11];
        (void)memset(bucket, 0, sizeof(*bucket));
        if (!month_key_date(key, start_date) ||
            !month_key_date(key + 1, end_date) ||
            !local_date_timestamp(start_date, &bucket->timestamp) ||
            !local_date_timestamp(end_date, &bucket->end_timestamp) ||
            !format_month_label(key, bucket->label, sizeof(bucket->label)))
            goto fail;
        for (size_t index = 0U; index < active_count; ++index)
            if (active_keys[index] == key) {
                int64_t timestamp = bucket->timestamp;
                int64_t end_timestamp = bucket->end_timestamp;
                char label[sizeof(bucket->label)];
                (void)memcpy(label, bucket->label, sizeof(label));
                *bucket = active[index];
                bucket->timestamp = timestamp;
                bucket->end_timestamp = end_timestamp;
                (void)memcpy(bucket->label, label, sizeof(label));
                break;
            }
    }
    return TRAINLOG_STATUS_OK;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus load_buckets(TrainlogDatabase *database,
    TrainlogStatisticsWindow window, int64_t now,
    TrainlogStatisticsSummary *output)
{
    sqlite3_stmt *statement = NULL;
    if (window == TRAINLOG_STATISTICS_30_DAYS)
        return load_month_buckets(database, now, true, output);
    output->bucket_kind = TRAINLOG_STATISTICS_BUCKET_7_DAYS;
    char sql[4096];
    int written = snprintf(sql, sizeof(sql),
        "%s, weeks AS (SELECT date(substr(s.started_at,1,10),printf('-%%d days',(CAST(strftime('%%w',substr(s.started_at,1,10)) AS INTEGER)+6)%%7)) week,"
        "COUNT(DISTINCT s.id) sessions,COUNT(p.id) sets,COALESCE(SUM(p.reps),0) reps,"
        "COALESCE(SUM(CASE WHEN p.reps IS NOT NULL AND p.weight_kg IS NOT NULL AND ao.load_mode='external' AND e.load_semantics='external' THEN p.reps*p.weight_kg ELSE 0 END),0.0) volume "
        "FROM actual_sessions s LEFT JOIN actual_occurrences ao ON ao.session_row_id=s.id "
        "LEFT JOIN exercises e ON e.id=ao.exercise_row_id LEFT JOIN performed_sets p ON p.session_exercise_row_id=ao.id "
        "WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2 GROUP BY week) "
        "SELECT week,sessions,sets,reps,volume,"
        "(SELECT COUNT(*) FROM exercise_feedback f JOIN session_exercises sf ON sf.id=f.session_exercise_row_id JOIN sessions ss ON ss.id=sf.session_row_id WHERE date(substr(ss.started_at,1,10),printf('-%%d days',(CAST(strftime('%%w',substr(ss.started_at,1,10)) AS INTEGER)+6)%%7))=weeks.week AND unixepoch(ss.started_at) BETWEEN ?1 AND ?2) "
        "FROM weeks ORDER BY week DESC LIMIT %u;",
        actual_occurrences_cte(), TRAINLOG_STATISTICS_BUCKET_MAX + 1U);
    if (written < 0 || (size_t)written >= sizeof(sql) ||
        prepare(database->connection, sql, &statement) != TRAINLOG_STATUS_OK ||
        bind_window(statement, INT64_MIN, now) != TRAINLOG_STATUS_OK) goto fail;
    int step;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogStatisticsBucket *bucket;
        const char *week;
        time_t seconds;
        struct tm date;
        if (output->bucket_count == TRAINLOG_STATISTICS_BUCKET_MAX) {
            output->buckets_partial = true;
            break;
        }
        bucket = &output->buckets[output->bucket_count++];
        week = (const char *)sqlite3_column_text(statement, 0);
        if (!local_date_timestamp(week, &bucket->timestamp) ||
            !size_column(statement, 1, &bucket->sessions) ||
            !size_column(statement, 2, &bucket->sets) ||
            !u64_column(statement, 3, &bucket->repetitions) ||
            !isfinite(sqlite3_column_double(statement, 4)) ||
            !size_column(statement, 5, &bucket->immediate_feedback_count)) goto fail;
        bucket->loaded_volume_kg = sqlite3_column_double(statement, 4);
        seconds = (time_t)bucket->timestamp;
        if ((int64_t)seconds != bucket->timestamp ||
            gmtime_r(&seconds, &date) == NULL ||
            strftime(bucket->label, sizeof(bucket->label), "%d/%m", &date) == 0U)
            (void)snprintf(bucket->label, sizeof(bucket->label), "semaine");
    }
    if (step != SQLITE_DONE && !output->buckets_partial) goto fail;
    if (sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    statement = NULL;
    /* SQL reads newest-first for bounded history; charts require chronology. */
    for (size_t left = 0U, right = output->bucket_count;
         left < right && left < --right; ++left) {
        TrainlogStatisticsBucket copy = output->buckets[left];
        output->buckets[left] = output->buckets[right];
        output->buckets[right] = copy;
    }
    /* WHY: active-row count alone misses sparse but very long histories. The
     * all-history chart chooses one scale from total duration, so it neither
     * drops an old sparse bucket nor mixes bucket sizes in a single graph. */
    if (window == TRAINLOG_STATISTICS_ALL && output->bucket_count > 0U) {
        int64_t current_week;
        int64_t week_span;
        if (!current_local_week_timestamp(now, &current_week)) goto fail;
        week_span = (current_week - output->buckets[0].timestamp) /
            SECONDS_PER_WEEK + 1;
        if (output->buckets_partial ||
            week_span > (int64_t)TRAINLOG_STATISTICS_BUCKET_MAX)
            return load_month_buckets(database, now, false, output);
    }
    if (output->bucket_count > 0U) {
        TrainlogStatisticsBucket filled[TRAINLOG_STATISTICS_BUCKET_MAX];
        int64_t first_week = output->buckets[0].timestamp;
        int64_t last_week;
        int64_t span;
        size_t filled_count = 0U;
        size_t source = 0U;
        if (!current_local_week_timestamp(now, &last_week)) goto fail;
        if (window == TRAINLOG_STATISTICS_7_DAYS) {
            int64_t comparison_start = last_week - INT64_C(3) * SECONDS_PER_WEEK;
            if (first_week < comparison_start) first_week = comparison_start;
        }
        span = (last_week - first_week) / SECONDS_PER_WEEK + 1;
        if (span > (int64_t)TRAINLOG_STATISTICS_BUCKET_MAX) {
            first_week = last_week -
                ((int64_t)TRAINLOG_STATISTICS_BUCKET_MAX - 1) * SECONDS_PER_WEEK;
            output->buckets_partial = true;
        }
        /* INVARIANT: weekly facts include explicit zero weeks. Connecting only
         * active weeks would visually invent activity inside a chronological gap. */
        for (int64_t week = first_week; week <= last_week;
             week += SECONDS_PER_WEEK) {
            int64_t timestamp = week;
            time_t seconds = (time_t)timestamp;
            struct tm date;
            while (source < output->bucket_count &&
                   output->buckets[source].timestamp < timestamp) ++source;
            (void)memset(&filled[filled_count], 0, sizeof(filled[filled_count]));
            filled[filled_count].timestamp = timestamp;
            filled[filled_count].end_timestamp = timestamp + SECONDS_PER_WEEK;
            if (source < output->bucket_count &&
                output->buckets[source].timestamp == timestamp) {
                int64_t end_timestamp = filled[filled_count].end_timestamp;
                filled[filled_count] = output->buckets[source];
                filled[filled_count].end_timestamp = end_timestamp;
            }
            if ((int64_t)seconds == timestamp && gmtime_r(&seconds, &date) != NULL) {
                char start_label[6];
                char end_label[6];
                time_t end_seconds = seconds + (time_t)(INT64_C(6) * SECONDS_PER_DAY);
                struct tm end;
                if (gmtime_r(&end_seconds, &end) != NULL &&
                    strftime(start_label, sizeof(start_label), "%d/%m", &date) == 5U &&
                    strftime(end_label, sizeof(end_label), "%d/%m", &end) == 5U)
                    (void)snprintf(filled[filled_count].label,
                        sizeof(filled[filled_count].label), "%s-%s",
                        start_label, end_label);
            }
            ++filled_count;
        }
        (void)memcpy(output->buckets, filled,
            filled_count * sizeof(output->buckets[0]));
        output->bucket_count = filled_count;
    }
    return TRAINLOG_STATUS_OK;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus load_top_feedback(TrainlogDatabase *database,
    int64_t start, int64_t now, TrainlogStatisticsSummary *output)
{
    sqlite3_stmt *statement = NULL;
    const char *sql =
        "SELECT e.name,COUNT(*) amount FROM exercise_feedback f "
        "JOIN session_exercises se ON se.id=f.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id "
        "JOIN exercises e ON e.id=se.exercise_row_id "
        "WHERE unixepoch(s.started_at) BETWEEN ?1 AND ?2 "
        "GROUP BY e.id ORDER BY amount DESC,e.name COLLATE NOCASE,e.exercise_id LIMIT 1;";
    int step;
    if (prepare(database->connection, sql, &statement) != TRAINLOG_STATUS_OK ||
        bind_window(statement, start, now) != TRAINLOG_STATUS_OK) goto fail;
    step = sqlite3_step(statement);
    if (step == SQLITE_ROW &&
        (!copy_text(statement, 0, output->top_feedback_exercise,
            sizeof(output->top_feedback_exercise)) ||
         !size_column(statement, 1, &output->top_feedback_count))) goto fail;
    if (step != SQLITE_ROW && step != SQLITE_DONE) goto fail;
    if (sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    return TRAINLOG_STATUS_OK;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_statistics_load_summary(TrainlogDatabase *database,
    TrainlogStatisticsWindow window, int64_t now_utc,
    TrainlogStatisticsSummary *output)
{
    TrainlogStatus status;
    int64_t start;
    if (database == NULL || database->connection == NULL || output == NULL ||
        window < TRAINLOG_STATISTICS_7_DAYS || window > TRAINLOG_STATISTICS_ALL)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)memset(output, 0, sizeof(*output));
    if (!calendar_window_start(window, now_utc, &start))
        return TRAINLOG_STATUS_DATABASE_ERROR;
    status = trainlog_database_read_snapshot_begin(database);
    if (status != TRAINLOG_STATUS_OK) return status;
    status = load_summary_row(database, start, now_utc, output);
    if (status == TRAINLOG_STATUS_OK && output->latest_session_timestamp > 0 &&
        output->latest_session_timestamp <= now_utc) {
        uint64_t days = (uint64_t)(now_utc - output->latest_session_timestamp) /
            (uint64_t)SECONDS_PER_DAY;
        if (days > (uint64_t)SIZE_MAX) status = TRAINLOG_STATUS_DATABASE_ERROR;
        else output->days_since_last_session = (size_t)days;
    }
    if (status == TRAINLOG_STATUS_OK)
        status = load_top_feedback(database, start, now_utc, output);
    if (status == TRAINLOG_STATUS_OK)
        status = load_buckets(database, window, now_utc, output);
    if (trainlog_database_read_snapshot_end(database,
            status == TRAINLOG_STATUS_OK) != TRAINLOG_STATUS_OK)
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    return status;
}

TrainlogStatus trainlog_statistics_load_zones(TrainlogDatabase *database,
    int64_t now_utc, TrainlogZoneStatistics *output, size_t capacity,
    size_t *output_count, bool *output_partial)
{
    sqlite3_stmt *statement = NULL;
    const char *sql =
        "WITH actual_occurrences AS (SELECT se.id,se.session_row_id,se.exercise_row_id FROM session_exercises se "
        "WHERE EXISTS(SELECT 1 FROM performed_sets p WHERE p.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM continuous_activity c WHERE c.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM max_results m WHERE m.session_exercise_row_id=se.id)) "
        "SELECT z.zone_id,COUNT(DISTINCT s.id),"
        "COUNT(DISTINCT CASE WHEN z.role='primary' AND unixepoch(s.started_at)>=?1 THEN s.id END),"
        "COUNT(DISTINCT CASE WHEN z.role='secondary' AND unixepoch(s.started_at)>=?1 THEN s.id END),"
        "COUNT(DISTINCT CASE WHEN z.role='primary' AND unixepoch(s.started_at)>=?2 THEN s.id END),"
        "COUNT(DISTINCT CASE WHEN z.role='secondary' AND unixepoch(s.started_at)>=?2 THEN s.id END),"
        "COUNT(DISTINCT ao.exercise_row_id),"
        "MAX(CASE WHEN z.role='primary' THEN s.started_at END),"
        "MAX(CASE WHEN z.role='secondary' THEN s.started_at END) "
        "FROM exercise_body_zones z JOIN actual_occurrences ao ON ao.exercise_row_id=z.exercise_row_id "
        "JOIN sessions s ON s.id=ao.session_row_id WHERE unixepoch(s.started_at)<=?3 "
        "GROUP BY z.zone_id ORDER BY z.zone_id;";
    TrainlogStatus status = TRAINLOG_STATUS_OK;
    int step;
    if (database == NULL || database->connection == NULL || output_count == NULL ||
        output_partial == NULL || (capacity > 0U && output == NULL))
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_count = 0U; *output_partial = false;
    if (prepare(database->connection, sql, &statement) != TRAINLOG_STATUS_OK ||
        sqlite3_bind_int64(statement, 1, now_utc - INT64_C(7) * SECONDS_PER_DAY) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, now_utc - INT64_C(30) * SECONDS_PER_DAY) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 3, now_utc) != SQLITE_OK) goto fail;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *zone = sqlite3_column_text(statement, 0);
        size_t index = *output_count;
        TrainlogZoneStatistics *value;
        if (zone == NULL) goto fail;
        if (index == capacity) { *output_partial = true; continue; }
        value = &output[index];
        (void)memset(value, 0, sizeof(*value));
        if (!copy_text(statement, 0, value->zone_id,
                       sizeof(value->zone_id))) goto fail;
        {
            const unsigned char *latest_primary = sqlite3_column_text(statement, 7);
            const unsigned char *latest_secondary = sqlite3_column_text(statement, 8);
            if (!size_column(statement, 1, &value->sessions_any) ||
                !size_column(statement, 2, &value->primary_7) ||
                !size_column(statement, 3, &value->secondary_7) ||
                !size_column(statement, 4, &value->primary_30) ||
                !size_column(statement, 5, &value->secondary_30) ||
                !size_column(statement, 6, &value->distinct_exercises)) goto fail;
            if (latest_primary != NULL && !copy_text(statement, 7,
                value->latest_primary, sizeof(value->latest_primary))) goto fail;
            if (latest_secondary != NULL && !copy_text(statement, 8,
                value->latest_secondary, sizeof(value->latest_secondary))) goto fail;
        }
        ++(*output_count);
    }
    if (step != SQLITE_DONE) goto fail;
    if (sqlite3_finalize(statement) != SQLITE_OK) status = TRAINLOG_STATUS_DATABASE_ERROR;
    return status;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static bool append_point(TrainlogExerciseStatistics *output,
    TrainlogExerciseSeriesKind kind, int64_t timestamp, const char *label,
    double value)
{
    size_t index = output->series_count[kind];
    TrainlogStatisticPoint *point;
    if (!isfinite(value)) return false;
    if (index == TRAINLOG_STATISTICS_SERIES_MAX) {
        output->series_partial[kind] = true;
        return true;
    }
    point = &output->series[kind][index];
    point->timestamp = timestamp; point->value = value;
    if (snprintf(point->timestamp_label, sizeof(point->timestamp_label), "%s",
            label) < 0) return false;
    output->series_count[kind] = index + 1U;
    return true;
}

TrainlogStatus trainlog_statistics_load_exercise(TrainlogDatabase *database,
    const char *exercise_id, int64_t now_utc, TrainlogExerciseStatistics *output)
{
    sqlite3_stmt *statement = NULL;
    TrainlogStatus status = TRAINLOG_STATUS_OK;
    const char *profile_sql = "SELECT name,tracking_mode,recording_mode,COALESCE(load_semantics,'') FROM exercises WHERE exercise_id=?1;";
    const char *series_sql =
        "SELECT s.started_at,unixepoch(s.started_at),se.id,"
        "COUNT(p.id),COALESCE(SUM(p.reps),0),COALESCE(SUM(p.duration_seconds),0)+COALESCE(c.duration_seconds,0),"
        "MAX(CASE WHEN p.weight_kg IS NOT NULL THEN p.weight_kg END),"
        "COALESCE(SUM(CASE WHEN p.reps IS NOT NULL AND p.weight_kg IS NOT NULL AND se.load_mode='external' AND e.load_semantics='external' THEN p.reps*p.weight_kg ELSE 0 END),0.0),"
        "m.max_weight_kg,se.target_sets,se.target_reps,se.target_weight_kg,"
        "(SELECT COUNT(*) FROM exercise_feedback f WHERE f.session_exercise_row_id=se.id) "
        "FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id "
        "LEFT JOIN performed_sets p ON p.session_exercise_row_id=se.id LEFT JOIN continuous_activity c ON c.session_exercise_row_id=se.id "
        "LEFT JOIN max_results m ON m.session_exercise_row_id=se.id WHERE e.exercise_id=?1 "
        "AND (p.id IS NOT NULL OR c.id IS NOT NULL OR m.max_weight_kg IS NOT NULL) AND unixepoch(s.started_at)<=?2 "
        "GROUP BY s.id,se.id ORDER BY unixepoch(s.started_at),s.session_id,se.entry_id;";
    int step;
    if (database == NULL || database->connection == NULL || exercise_id == NULL ||
        exercise_id[0] == '\0' || output == NULL)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)memset(output, 0, sizeof(*output));
    (void)snprintf(output->exercise_id, sizeof(output->exercise_id), "%s", exercise_id);
    if (prepare(database->connection, profile_sql, &statement) != TRAINLOG_STATUS_OK ||
        sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK)
        goto fail;
    if (sqlite3_step(statement) != SQLITE_ROW ||
        !copy_text(statement, 0, output->name, sizeof(output->name))) {
        status = TRAINLOG_STATUS_NOT_FOUND; goto done;
    }
    {
        const char *tracking = (const char *)sqlite3_column_text(statement, 1);
        const char *recording = (const char *)sqlite3_column_text(statement, 2);
        if (tracking == NULL || recording == NULL ||
            !copy_text(statement, 3, output->load_semantics,
                       sizeof(output->load_semantics))) goto fail;
        output->tracking_mode = strcmp(tracking, "duration") == 0
            ? TRAINLOG_TRACKING_DURATION : TRAINLOG_TRACKING_REPS;
        output->recording_mode = strcmp(recording, "continuous") == 0
            ? TRAINLOG_RECORDING_CONTINUOUS : TRAINLOG_RECORDING_SETS;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) { statement = NULL; goto fail; }
    statement = NULL;
    if (prepare(database->connection, series_sql, &statement) != TRAINLOG_STATUS_OK ||
        sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, now_utc) != SQLITE_OK) goto fail;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *at = sqlite3_column_text(statement, 0);
        int64_t timestamp = sqlite3_column_int64(statement, 1);
        size_t sets;
        uint64_t reps, duration;
        double volume = sqlite3_column_double(statement, 7);
        if (at == NULL || !size_column(statement, 3, &sets) ||
            !u64_column(statement, 4, &reps) || !u64_column(statement, 5, &duration) ||
            !isfinite(volume)) goto fail;
        ++output->occurrences;
        ++output->sessions; /* corrected to distinct below after ordered scan */
        if (sets > SIZE_MAX - output->sets || reps > UINT64_MAX - output->repetitions ||
            duration > UINT64_MAX - output->duration_seconds) goto fail;
        output->sets += sets; output->repetitions += reps;
        output->duration_seconds += duration; output->loaded_volume_kg += volume;
        if (output->first_at[0] == '\0')
            (void)snprintf(output->first_at, sizeof(output->first_at), "%s", at);
        (void)snprintf(output->last_at, sizeof(output->last_at), "%s", at);
        if (sqlite3_column_type(statement, 6) != SQLITE_NULL) {
            double load = sqlite3_column_double(statement, 6);
            if (!isfinite(load)) goto fail;
            if (!output->has_highest_load || load > output->highest_load_kg) {
                output->highest_load_kg = load; output->has_highest_load = true;
            }
            if (!append_point(output, TRAINLOG_EXERCISE_SERIES_LOAD,
                    timestamp, (const char *)at, load)) goto fail;
        }
        if (sets > 0U && !append_point(output, TRAINLOG_EXERCISE_SERIES_SETS,
                timestamp, (const char *)at, (double)sets)) goto fail;
        if (reps > 0U && !append_point(output, TRAINLOG_EXERCISE_SERIES_REPETITIONS,
                timestamp, (const char *)at, (double)reps)) goto fail;
        if (volume > 0.0 && !append_point(output, TRAINLOG_EXERCISE_SERIES_VOLUME,
                timestamp, (const char *)at, volume)) goto fail;
        if (sqlite3_column_type(statement, 8) != SQLITE_NULL) {
            double maximum = sqlite3_column_double(statement, 8);
            if (!isfinite(maximum) || maximum <= 0.0) goto fail;
            if (output->has_latest_max) {
                output->previous_max_kg = output->latest_max_kg;
                output->has_previous_max = true;
            }
            output->latest_max_kg = maximum; output->has_latest_max = true;
            if (output->max_count == 0U || maximum > output->best_max_kg)
                output->best_max_kg = maximum;
            ++output->max_count;
            (void)snprintf(output->latest_max_at, sizeof(output->latest_max_at),
                           "%s", at);
            if (!append_point(output, TRAINLOG_EXERCISE_SERIES_MAX,
                    timestamp, (const char *)at, maximum)) goto fail;
        }
        if (sqlite3_column_type(statement, 9) != SQLITE_NULL) {
            uint64_t target_sets = (uint64_t)sqlite3_column_int64(statement, 9);
            if (target_sets > UINT64_MAX - output->planned_sets ||
                (uint64_t)sets > UINT64_MAX - output->actual_sets_for_plans)
                goto fail;
            output->planned_sets += target_sets;
            output->actual_sets_for_plans += (uint64_t)sets;
            if (sqlite3_column_type(statement, 10) != SQLITE_NULL) {
                uint64_t target_reps = (uint64_t)sqlite3_column_int64(statement, 10);
                if (target_reps != 0U && target_sets > UINT64_MAX / target_reps)
                    goto fail;
                target_reps *= target_sets;
                if (target_reps > UINT64_MAX - output->planned_repetitions ||
                    reps > UINT64_MAX - output->actual_repetitions_for_plans)
                    goto fail;
                output->planned_repetitions += target_reps;
                output->actual_repetitions_for_plans += reps;
            }
            if (sqlite3_column_type(statement, 11) != SQLITE_NULL &&
                sqlite3_column_type(statement, 6) != SQLITE_NULL) {
                double planned_weight = sqlite3_column_double(statement, 11);
                double actual_weight = sqlite3_column_double(statement, 6);
                if (!isfinite(planned_weight) || !isfinite(actual_weight)) goto fail;
                output->latest_planned_weight_kg = planned_weight;
                output->latest_actual_weight_kg = actual_weight;
                output->has_latest_planned_actual_weight = true;
            }
        }
        {
            size_t feedback;
            if (!size_column(statement, 12, &feedback) ||
                feedback > SIZE_MAX - output->immediate_feedback_count) goto fail;
            output->immediate_feedback_count += feedback;
        }
    }
    if (step != SQLITE_DONE) goto fail;
    /* Occurrences of one exercise can repeat inside a session. This aggregate
     * query is chronological but not session-ID preserving, so obtain the exact
     * distinct count in one constant supplementary query. */
    if (sqlite3_finalize(statement) != SQLITE_OK) { statement = NULL; goto fail; }
    statement = NULL;
    if (prepare(database->connection,
        "SELECT COUNT(DISTINCT CASE WHEN (EXISTS(SELECT 1 FROM performed_sets pa WHERE pa.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM continuous_activity ca WHERE ca.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM max_results ma WHERE ma.session_exercise_row_id=se.id)) THEN s.id END),"
        "COUNT(DISTINCT CASE WHEN unixepoch(s.started_at)>=?2-604800 AND (EXISTS(SELECT 1 FROM performed_sets pa WHERE pa.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM continuous_activity ca WHERE ca.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM max_results ma WHERE ma.session_exercise_row_id=se.id)) THEN s.id END),"
        "COUNT(DISTINCT CASE WHEN unixepoch(s.started_at)>=?2-2592000 AND (EXISTS(SELECT 1 FROM performed_sets pa WHERE pa.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM continuous_activity ca WHERE ca.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM max_results ma WHERE ma.session_exercise_row_id=se.id)) THEN s.id END),"
        "(SELECT p2.reps FROM performed_sets p2 JOIN session_exercises se2 ON se2.id=p2.session_exercise_row_id JOIN sessions s2 ON s2.id=se2.session_row_id JOIN exercises e2 ON e2.id=se2.exercise_row_id WHERE e2.exercise_id=?1 AND p2.reps IS NOT NULL AND p2.weight_kg IS NOT NULL AND unixepoch(s2.started_at)<=?2 ORDER BY p2.reps DESC,p2.weight_kg DESC,s2.started_at DESC LIMIT 1),"
        "(SELECT p2.weight_kg FROM performed_sets p2 JOIN session_exercises se2 ON se2.id=p2.session_exercise_row_id JOIN sessions s2 ON s2.id=se2.session_row_id JOIN exercises e2 ON e2.id=se2.exercise_row_id WHERE e2.exercise_id=?1 AND p2.reps IS NOT NULL AND p2.weight_kg IS NOT NULL AND unixepoch(s2.started_at)<=?2 ORDER BY p2.reps DESC,p2.weight_kg DESC,s2.started_at DESC LIMIT 1) "
        "FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id WHERE e.exercise_id=?1 AND unixepoch(s.started_at)<=?2;",
        &statement) != TRAINLOG_STATUS_OK ||
        sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, now_utc) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW ||
        !size_column(statement, 0, &output->sessions) ||
        !size_column(statement, 1, &output->frequency_7) ||
        !size_column(statement, 2, &output->frequency_30)) goto fail;
    if (sqlite3_column_type(statement, 3) != SQLITE_NULL) {
        output->highest_repetitions = sqlite3_column_int(statement, 3);
        output->has_repetitions_at_load = sqlite3_column_type(statement, 4) != SQLITE_NULL;
        if (output->has_repetitions_at_load)
            output->highest_repetitions_load_kg = sqlite3_column_double(statement, 4);
    }
done:
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK)
        status = TRAINLOG_STATUS_DATABASE_ERROR;
    return status;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_statistics_list_maxima(TrainlogDatabase *database,
    TrainlogMaxListItem *output, size_t capacity, size_t *output_count,
    bool *output_partial)
{
    sqlite3_stmt *statement = NULL;
    const char *sql =
        "SELECT exercise_id,max_weight_kg,started_at FROM ("
        "SELECT e.exercise_id,m.max_weight_kg,s.started_at,"
        "ROW_NUMBER() OVER (PARTITION BY e.id ORDER BY unixepoch(s.started_at) DESC,s.session_id DESC,se.entry_id DESC) rank "
        "FROM max_results m JOIN session_exercises se ON se.id=m.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id) "
        "WHERE rank=1 ORDER BY started_at DESC,exercise_id LIMIT ?1;";
    int step;
    if (database == NULL || database->connection == NULL || output_count == NULL ||
        output_partial == NULL || (capacity > 0U && output == NULL) ||
        capacity == SIZE_MAX) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_count = 0U; *output_partial = false;
    if (prepare(database->connection, sql, &statement) != TRAINLOG_STATUS_OK ||
        sqlite3_bind_int64(statement, 1, (sqlite3_int64)(capacity + 1U)) != SQLITE_OK)
        goto fail;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogMaxListItem *item;
        if (*output_count == capacity) { *output_partial = true; continue; }
        item = &output[*output_count];
        (void)memset(item, 0, sizeof(*item));
        if (!copy_text(statement, 0, item->exercise_id, sizeof(item->exercise_id)) ||
            !isfinite(sqlite3_column_double(statement, 1)) ||
            !copy_text(statement, 2, item->latest_at, sizeof(item->latest_at))) goto fail;
        item->latest_max_kg = sqlite3_column_double(statement, 1);
        ++(*output_count);
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    return TRAINLOG_STATUS_OK;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_statistics_list_exercises(TrainlogDatabase *database,
    TrainlogExerciseListStatistic *output, size_t capacity,
    size_t *output_count, bool *output_partial)
{
    sqlite3_stmt *statement = NULL;
    char sql[2048];
    int written = snprintf(sql, sizeof(sql),
        "%s SELECT e.exercise_id,COUNT(*),MAX(s.started_at) FROM actual_occurrences ao "
        "JOIN sessions s ON s.id=ao.session_row_id JOIN exercises e ON e.id=ao.exercise_row_id "
        "GROUP BY e.id ORDER BY MAX(unixepoch(s.started_at)) DESC,e.exercise_id LIMIT ?1;",
        actual_occurrences_cte());
    int step;
    if (database == NULL || database->connection == NULL || output_count == NULL ||
        output_partial == NULL || (capacity > 0U && output == NULL) ||
        capacity == SIZE_MAX || written < 0 || (size_t)written >= sizeof(sql))
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *output_count = 0U; *output_partial = false;
    if (prepare(database->connection, sql, &statement) != TRAINLOG_STATUS_OK ||
        sqlite3_bind_int64(statement, 1, (sqlite3_int64)(capacity + 1U)) != SQLITE_OK)
        goto fail;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogExerciseListStatistic *item;
        if (*output_count == capacity) { *output_partial = true; continue; }
        item = &output[*output_count];
        (void)memset(item, 0, sizeof(*item));
        if (!copy_text(statement, 0, item->exercise_id, sizeof(item->exercise_id)) ||
            !size_column(statement, 1, &item->occurrences) ||
            !copy_text(statement, 2, item->latest_at, sizeof(item->latest_at))) goto fail;
        ++(*output_count);
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    return TRAINLOG_STATUS_OK;
fail:
    if (statement != NULL) (void)sqlite3_finalize(statement);
    return TRAINLOG_STATUS_DATABASE_ERROR;
}
