/**
 * @file dashboard.c
 * @brief Canonical bounded Dashboard read model.
 */

#include "trainlog/dashboard.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "trainlog/body_zone_catalog.h"
#include "timestamp.h"
#include "database_internal.h"

#define DASHBOARD_BODY_ZONE_RELATION_CAPACITY 16U

typedef enum TrainlogDashboardFactKind {
    TRAINLOG_DASHBOARD_WORK,
    TRAINLOG_DASHBOARD_MAX,
    TRAINLOG_DASHBOARD_CONTINUOUS
} TrainlogDashboardFactKind;

typedef struct TrainlogDashboardFact {
    TrainlogDashboardFactKind kind;
    TrainlogTimestampKey time;
    char session_id[TRAINLOG_ID_MAX + 1U];
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
    TrainlogLoadMode load_mode;
    size_t set_position;
    int dose;
    bool has_weight;
    double weight_kg;
} TrainlogDashboardFact;

static const TrainlogBodyMetric DASHBOARD_BODY_METRICS[] = {
    TRAINLOG_BODY_METRIC_WEIGHT, TRAINLOG_BODY_METRIC_NECK,
    TRAINLOG_BODY_METRIC_SHOULDERS, TRAINLOG_BODY_METRIC_CHEST,
    TRAINLOG_BODY_METRIC_WAIST, TRAINLOG_BODY_METRIC_HIPS,
    TRAINLOG_BODY_METRIC_LEFT_ARM, TRAINLOG_BODY_METRIC_RIGHT_ARM,
    TRAINLOG_BODY_METRIC_LEFT_FOREARM, TRAINLOG_BODY_METRIC_RIGHT_FOREARM,
    TRAINLOG_BODY_METRIC_LEFT_THIGH, TRAINLOG_BODY_METRIC_RIGHT_THIGH,
    TRAINLOG_BODY_METRIC_LEFT_CALF, TRAINLOG_BODY_METRIC_RIGHT_CALF
};

_Static_assert(sizeof(DASHBOARD_BODY_METRICS) /
    sizeof(DASHBOARD_BODY_METRICS[0]) ==
    TRAINLOG_DASHBOARD_BODY_METRIC_COUNT,
    "Dashboard body metric contract must remain complete");

static bool dashboard_copy_sql_text(sqlite3_stmt *statement, int column,
                                    char *output, size_t capacity)
{
    const unsigned char *value;
    int bytes;
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) return false;
    value = sqlite3_column_text(statement, column);
    bytes = sqlite3_column_bytes(statement, column);
    if (value == NULL || bytes < 0 || (size_t)bytes >= capacity) return false;
    (void)memcpy(output, value, (size_t)bytes);
    output[bytes] = '\0';
    return true;
}

/* WHY: the global dashboard compares occurrence-owned facts, while the public
 * performance reader deliberately collapses each session to one representative.
 * CONTRACT: this private projection emits every performed set, every explicit
 * MAX exactly once, and every continuous occurrence exactly once. INVARIANT:
 * malformed legacy timestamps are omitted before sorting and reported through
 * invalid_data; they can never reach a comparator with uninitialized keys. */
static TrainlogStatus dashboard_database_list_facts(TrainlogDatabase *database,
    TrainlogDashboardFact *output, size_t capacity, size_t *output_count,
    bool *truncated, bool *invalid_data)
{
    static const char *const SQL =
        "SELECT 0,s.started_at,s.session_id,se.entry_id,e.exercise_id,"
        "COALESCE(se.equipment_id,''),se.tracking_mode,se.load_mode,ps.position,"
        "CASE WHEN se.tracking_mode='reps' THEN ps.reps ELSE ps.duration_seconds END,"
        "ps.weight_kg FROM performed_sets ps "
        "JOIN session_exercises se ON se.id=ps.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id "
        "JOIN exercises e ON e.id=se.exercise_row_id "
        "UNION ALL "
        "SELECT 1,s.started_at,s.session_id,se.entry_id,e.exercise_id,"
        "COALESCE(se.equipment_id,''),se.tracking_mode,se.load_mode,0,0,"
        "mr.max_weight_kg FROM max_results mr "
        "JOIN session_exercises se ON se.id=mr.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id "
        "JOIN exercises e ON e.id=se.exercise_row_id "
        "UNION ALL "
        "SELECT 2,s.started_at,s.session_id,se.entry_id,e.exercise_id,"
        "COALESCE(se.equipment_id,''),se.tracking_mode,se.load_mode,0,"
        "ca.duration_seconds,NULL FROM continuous_activity ca "
        "JOIN session_exercises se ON se.id=ca.session_exercise_row_id "
        "JOIN sessions s ON s.id=se.session_row_id "
        "JOIN exercises e ON e.id=se.exercise_row_id;";
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int rc;
    if (output_count != NULL) *output_count = 0U;
    if (truncated != NULL) *truncated = false;
    if (invalid_data != NULL) *invalid_data = false;
    if (database == NULL || database->connection == NULL || output_count == NULL ||
        truncated == NULL || invalid_data == NULL ||
        (capacity > 0U && output == NULL)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    rc = sqlite3_prepare_v2(database->connection, SQL, -1, &statement, NULL);
    if (rc != SQLITE_OK) return TRAINLOG_STATUS_DATABASE_ERROR;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        TrainlogDashboardFact fact;
        const unsigned char *tracking;
        const unsigned char *load;
        int kind;
        (void)memset(&fact, 0, sizeof(fact));
        if (sqlite3_column_type(statement, 0) != SQLITE_INTEGER ||
            !dashboard_copy_sql_text(statement, 1, fact.started_at,
                sizeof(fact.started_at)) ||
            !dashboard_copy_sql_text(statement, 2, fact.session_id,
                sizeof(fact.session_id)) ||
            !dashboard_copy_sql_text(statement, 3, fact.entry_id,
                sizeof(fact.entry_id)) ||
            !dashboard_copy_sql_text(statement, 4, fact.exercise_id,
                sizeof(fact.exercise_id)) ||
            !dashboard_copy_sql_text(statement, 5, fact.equipment_id,
                sizeof(fact.equipment_id)) ||
            sqlite3_column_type(statement, 6) != SQLITE_TEXT ||
            sqlite3_column_type(statement, 7) != SQLITE_TEXT ||
            !trainlog_timestamp_parse(fact.started_at, strlen(fact.started_at),
                &fact.time)) {
            *invalid_data = true;
            continue;
        }
        kind = sqlite3_column_int(statement, 0);
        if (kind < 0 || kind > 2) {
            *invalid_data = true;
            continue;
        }
        fact.kind = (TrainlogDashboardFactKind)kind;
        tracking = sqlite3_column_text(statement, 6);
        load = sqlite3_column_text(statement, 7);
        if (strcmp((const char *)tracking, "reps") == 0)
            fact.tracking_mode = TRAINLOG_TRACKING_REPS;
        else if (strcmp((const char *)tracking, "duration") == 0)
            fact.tracking_mode = TRAINLOG_TRACKING_DURATION;
        else { *invalid_data = true; continue; }
        if (strcmp((const char *)load, "none") == 0)
            fact.load_mode = TRAINLOG_LOAD_NONE;
        else if (strcmp((const char *)load, "external") == 0)
            fact.load_mode = TRAINLOG_LOAD_EXTERNAL;
        else if (strcmp((const char *)load, "assistance") == 0)
            fact.load_mode = TRAINLOG_LOAD_ASSISTANCE;
        else { *invalid_data = true; continue; }
        if (fact.kind == TRAINLOG_DASHBOARD_WORK) {
            sqlite3_int64 position;
            if (sqlite3_column_type(statement, 8) != SQLITE_INTEGER ||
                sqlite3_column_type(statement, 9) != SQLITE_INTEGER) {
                *invalid_data = true; continue;
            }
            position = sqlite3_column_int64(statement, 8);
            if (position < 0 || (uint64_t)position > (uint64_t)SIZE_MAX) {
                *invalid_data = true; continue;
            }
            fact.set_position = (size_t)position;
            fact.dose = sqlite3_column_int(statement, 9);
        } else if (fact.kind == TRAINLOG_DASHBOARD_CONTINUOUS) {
            if (sqlite3_column_type(statement, 9) != SQLITE_INTEGER) {
                *invalid_data = true; continue;
            }
            fact.dose = sqlite3_column_int(statement, 9);
        }
        if (fact.kind != TRAINLOG_DASHBOARD_CONTINUOUS) {
            int type = sqlite3_column_type(statement, 10);
            if (type == SQLITE_NULL && fact.kind == TRAINLOG_DASHBOARD_WORK) {
                fact.has_weight = false;
            } else if ((type != SQLITE_FLOAT && type != SQLITE_INTEGER) ||
                       !isfinite(sqlite3_column_double(statement, 10))) {
                *invalid_data = true; continue;
            } else {
                fact.has_weight = true;
                fact.weight_kg = sqlite3_column_double(statement, 10);
            }
            if (fact.kind == TRAINLOG_DASHBOARD_MAX && fact.weight_kg <= 0.0) {
                *invalid_data = true; continue;
            }
        }
        if (count == capacity) { *truncated = true; continue; }
        output[count++] = fact;
    }
    if (rc != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK)
        return TRAINLOG_STATUS_DATABASE_ERROR;
    *output_count = count;
    return TRAINLOG_STATUS_OK;
}

static int dashboard_fact_time_compare(const void *left, const void *right)
{
    const TrainlogDashboardFact *a = left;
    const TrainlogDashboardFact *b = right;
    int result = trainlog_timestamp_compare(&a->time, &b->time);
    if (result != 0) return result;
    result = strcmp(a->session_id, b->session_id);
    if (result != 0) return result;
    result = strcmp(a->entry_id, b->entry_id);
    if (result != 0) return result;
    if (a->kind != b->kind) return a->kind < b->kind ? -1 : 1;
    return a->set_position == b->set_position ? 0 :
        (a->set_position < b->set_position ? -1 : 1);
}

static int dashboard_performance_time_compare(const void *left, const void *right)
{
    const TrainlogExercisePerformancePoint *a = left;
    const TrainlogExercisePerformancePoint *b = right;
    TrainlogTimestampKey a_key;
    TrainlogTimestampKey b_key;
    (void)trainlog_timestamp_parse(a->started_at, strlen(a->started_at), &a_key);
    (void)trainlog_timestamp_parse(b->started_at, strlen(b->started_at), &b_key);
    {
        int result = trainlog_timestamp_compare(&a_key, &b_key);
        return result != 0 ? result : strcmp(a->session_id, b->session_id);
    }
}

static int dashboard_body_time_compare(const void *left, const void *right)
{
    const TrainlogBodyMetricPoint *a = left;
    const TrainlogBodyMetricPoint *b = right;
    TrainlogTimestampKey a_key;
    TrainlogTimestampKey b_key;
    (void)trainlog_timestamp_parse(a->observed_at, strlen(a->observed_at), &a_key);
    (void)trainlog_timestamp_parse(b->observed_at, strlen(b->observed_at), &b_key);
    return trainlog_timestamp_compare(&a_key, &b_key);
}

static int64_t dashboard_floor_div(int64_t value, int64_t divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

static int64_t dashboard_period_days(TrainlogDashboardPeriod period)
{
    switch (period) {
        case TRAINLOG_DASHBOARD_7_DAYS: return 7;
        case TRAINLOG_DASHBOARD_30_DAYS: return 30;
        case TRAINLOG_DASHBOARD_90_DAYS: return 90;
        case TRAINLOG_DASHBOARD_YEAR: return 365;
        case TRAINLOG_DASHBOARD_ALL: return 0;
    }
    return 30;
}

static bool dashboard_in_period(const TrainlogTimestampKey *key,
    TrainlogDashboardPeriod period, int64_t reference_unix_second)
{
    const int64_t days = dashboard_period_days(period);
    /* Unix time and Trainlog's proleptic day zero differ by 719162 days. */
    const int64_t now = reference_unix_second +
        INT64_C(719162) * INT64_C(86400);
    return key->utc_second <= now &&
        (days == 0 || key->utc_second >= now - days * INT64_C(86400));
}

/* WHY: the selected period must change both dashboard charts, while the
 * canonical rolling membership test above remains the sole inclusion rule.
 * CONTRACT: 7/30/90/365 days use respectively daily, five-day, fifteen-day
 * and roughly monthly (31-day) presentation buckets; Tout uses at most twelve
 * equal day spans covering represented history. INVARIANT: every included
 * local date is clamped into exactly one leading-to-current bucket. */
static void dashboard_prepare_period_buckets(TrainlogDashboardSnapshot *dashboard,
    int64_t current_day, int64_t earliest_day)
{
    size_t count;
    int64_t span;
    size_t index;
    switch (dashboard->period) {
        case TRAINLOG_DASHBOARD_7_DAYS: count = 7U; span = 1; break;
        case TRAINLOG_DASHBOARD_30_DAYS: count = 6U; span = 5; break;
        case TRAINLOG_DASHBOARD_90_DAYS: count = 6U; span = 15; break;
        case TRAINLOG_DASHBOARD_YEAR: count = 12U; span = 31; break;
        case TRAINLOG_DASHBOARD_ALL: {
            int64_t represented = current_day >= earliest_day
                ? current_day - earliest_day + 1 : 1;
            count = represented < (int64_t)TRAINLOG_DASHBOARD_BUCKET_CAPACITY
                ? (size_t)represented : TRAINLOG_DASHBOARD_BUCKET_CAPACITY;
            if (count == 0U) count = 1U;
            span = (represented + (int64_t)count - 1) / (int64_t)count;
            break;
        }
    }
    dashboard->week_count = count;
    for (index = 0U; index < count; ++index) {
        int64_t distance = (int64_t)(count - 1U - index) * span;
        dashboard->weeks[index].start_day = current_day - distance - span + 1;
        dashboard->weeks[index].span_days = span;
    }
}

static size_t dashboard_period_bucket(const TrainlogDashboardSnapshot *dashboard,
    int64_t local_day)
{
    int64_t first;
    int64_t span;
    size_t slot;
    if (dashboard->week_count == 0U) return 0U;
    first = dashboard->weeks[0].start_day;
    span = dashboard->weeks[0].span_days;
    if (local_day <= first) return 0U;
    slot = (size_t)((local_day - first) / span);
    return slot < dashboard->week_count ? slot : dashboard->week_count - 1U;
}

static bool dashboard_fact_external_context_qualifies(TrainlogDatabase *database,
    const TrainlogDashboardFact *fact)
{
    TrainlogResolvedEquipment equipment;
    if (fact->equipment_id[0] == '\0' ||
        trainlog_database_resolve_equipment(database, fact->equipment_id,
            &equipment) != TRAINLOG_STATUS_OK) return false;
    return strcmp(equipment.load_semantics, "external") == 0;
}

static TrainlogExercise *dashboard_find_exercise(TrainlogExercise *exercises,
    size_t count, const char *exercise_id)
{
    size_t index;
    for (index = 0U; index < count; ++index)
        if (strcmp(exercises[index].exercise_id, exercise_id) == 0)
            return &exercises[index];
    return NULL;
}

static int dashboard_zone_bucket_compare(const void *left, const void *right)
{
    const TrainlogDashboardZoneBucket *a = left;
    const TrainlogDashboardZoneBucket *b = right;
    const TrainlogBodyZone *a_zone;
    const TrainlogBodyZone *b_zone;
    const char *a_label;
    const char *b_label;
    if (a->count != b->count) return a->count > b->count ? -1 : 1;
    a_zone = trainlog_body_zone_catalog_lookup(a->zone_id);
    b_zone = trainlog_body_zone_catalog_lookup(b->zone_id);
    a_label = a_zone == NULL ? "Non classés" : a_zone->display_name;
    b_label = b_zone == NULL ? "Non classés" : b_zone->display_name;
    return strcmp(a_label, b_label);
}

/* WHY: a body-zone dashboard must describe the current catalogue rather than
 * infer classifications from history or commercial names. CONTRACT: one
 * canonical exercise is counted once, using only its persisted primary body
 * zone. INVARIANT: missing primary relations remain visible as "Non classés";
 * secondary relations and aliases can neither create nor duplicate a bucket. */
static bool dashboard_load_zone_distribution(TrainlogDatabase *database, TrainlogDashboardSnapshot *dashboard,
    const TrainlogExercise *exercises, size_t exercise_count)
{
    size_t exercise_index;
    for (exercise_index = 0U; exercise_index < exercise_count; ++exercise_index) {
        TrainlogExerciseBodyZone
            relations[DASHBOARD_BODY_ZONE_RELATION_CAPACITY];
        const char *zone_id = "";
        size_t relation_count = 0U;
        size_t relation_index;
        size_t bucket;
        if (trainlog_database_list_exercise_body_zones(database,
                exercises[exercise_index].exercise_id, relations,
                DASHBOARD_BODY_ZONE_RELATION_CAPACITY,
                &relation_count) != TRAINLOG_STATUS_OK) return false;
        for (relation_index = 0U; relation_index < relation_count; ++relation_index) {
            if (relations[relation_index].role == TRAINLOG_BODY_ZONE_PRIMARY) {
                const TrainlogBodyZone *zone = trainlog_body_zone_catalog_lookup(
                    relations[relation_index].zone_id);
                if (zone != NULL && !zone->is_group) zone_id = zone->zone_id;
                break;
            }
        }
        for (bucket = 0U; bucket < dashboard->zone_count; ++bucket)
            if (strcmp(dashboard->zones[bucket].zone_id, zone_id) == 0) break;
        if (bucket == dashboard->zone_count) {
            if (bucket == TRAINLOG_DASHBOARD_ZONE_CAPACITY) return false;
            (void)memcpy(dashboard->zones[bucket].zone_id, zone_id,
                strlen(zone_id) + 1U);
            ++dashboard->zone_count;
        }
        ++dashboard->zones[bucket].count;
    }
    qsort(dashboard->zones, dashboard->zone_count, sizeof(dashboard->zones[0]),
        dashboard_zone_bucket_compare);
    return true;
}

/* WHY: the landing page must remain useful without rendering-time SQL.
 * CONTRACT: this bounded controller snapshot contains only actual persisted
 * facts. One working-performance series has an exact exercise/equipment/load
 * identity; explicit MAX is separate, and assistance/planned targets never
 * enter it. All chronology is decided by Trainlog's timestamp parser, not
 * SQLite text ordering. */
TrainlogStatus trainlog_dashboard_load(
    TrainlogDatabase *database, const TrainlogDashboardQuery *query,
    TrainlogDashboardSnapshot *dashboard)
{
    const int64_t epoch_offset = INT64_C(719162) * INT64_C(86400);
    TrainlogDashboardPeriod period;
    TrainlogExercise *exercises = NULL;
    TrainlogDashboardFact *facts = NULL;
    TrainlogBodyMetricPoint body[TRAINLOG_DASHBOARD_BODY_CAPACITY];
    TrainlogSessionSummary sessions[TRAINLOG_DASHBOARD_SESSION_CAPACITY];
    TrainlogTimestampKey newest_performance;
    char newest_body[TRAINLOG_TIMESTAMP_MAX + 1U] = "";
    size_t exercise_count = 0U;
    size_t fact_count = 0U;
    size_t session_count = 0U;
    size_t exercise_index;
    int64_t current_day;
    int64_t earliest_day = INT64_MAX;
    bool has_newest_performance = false;
    bool facts_truncated = false;
    bool facts_invalid = false;

    if (dashboard == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    (void)memset(dashboard, 0, sizeof(*dashboard));
    if (database == NULL || query == NULL ||
        query->period < TRAINLOG_DASHBOARD_30_DAYS ||
        query->period > TRAINLOG_DASHBOARD_ALL ||
        query->reference_unix_second > INT64_MAX - epoch_offset) {
        dashboard->error = true;
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    period = query->period;
    dashboard->period = period;
    facts = calloc(TRAINLOG_DASHBOARD_FACT_CAPACITY, sizeof(*facts));
    if (facts == NULL) {
        dashboard->error = true;
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (trainlog_database_read_snapshot_begin(database) != TRAINLOG_STATUS_OK) {
        dashboard->error = true;
        free(facts);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (trainlog_database_list_sessions(database, sessions, TRAINLOG_DASHBOARD_SESSION_CAPACITY,
            &session_count) != TRAINLOG_STATUS_OK) {
        dashboard->error = true;
        goto finish;
    }
    dashboard->partial = session_count == TRAINLOG_DASHBOARD_SESSION_CAPACITY;
    /* CONTRACT: frequency is one count per actual session; a MAX marker is an
     * attribute of that count and never a second session. Rolling membership
     * uses canonical instants, while buckets use represented local dates.
     * Presentation always ends at the canonical current day, so the bucket
     * containing that day remains explicit even when it is zero. */
    current_day = dashboard_floor_div(query->reference_unix_second +
        INT64_C(719162) * INT64_C(86400), INT64_C(86400));
    for (exercise_index = 0U; exercise_index < session_count; ++exercise_index) {
        TrainlogTimestampKey key;
        if (!trainlog_timestamp_parse(sessions[exercise_index].started_at,
                strlen(sessions[exercise_index].started_at), &key)) {
            dashboard->invalid_data = true;
            continue;
        }
        if (!dashboard_in_period(&key, dashboard->period, query->reference_unix_second)) continue;
        ++dashboard->completed_session_count;
        if (key.local_day < earliest_day) earliest_day = key.local_day;
    }
    dashboard_prepare_period_buckets(dashboard, current_day,
        earliest_day == INT64_MAX ? current_day : earliest_day);
    for (exercise_index = 0U; exercise_index < session_count; ++exercise_index) {
        TrainlogTimestampKey key;
        size_t slot;
        if (!trainlog_timestamp_parse(sessions[exercise_index].started_at,
                strlen(sessions[exercise_index].started_at), &key) ||
            !dashboard_in_period(&key, dashboard->period, query->reference_unix_second)) continue;
        slot = dashboard_period_bucket(dashboard, key.local_day);
        ++dashboard->weeks[slot].sessions;
        if (sessions[exercise_index].session_type == TRAINLOG_SESSION_MAX_TEST)
            ++dashboard->weeks[slot].maxima;
    }
    if (trainlog_database_list_exercises(database, NULL, 0U,
            &exercise_count) != TRAINLOG_STATUS_OK ||
        exercise_count > SIZE_MAX / sizeof(*exercises)) {
        dashboard->error = true;
        goto finish;
    }
    if (exercise_count > 0U) {
        exercises = calloc(exercise_count, sizeof(*exercises));
        if (exercises == NULL || trainlog_database_list_exercises(database,
                exercises, exercise_count, &exercise_count) != TRAINLOG_STATUS_OK) {
            dashboard->error = true;
            goto finish;
        }
    }
    if (!dashboard_load_zone_distribution(database, dashboard, exercises, exercise_count)) {
        dashboard->error = true;
        goto finish;
    }
    if (dashboard_database_list_facts(database, facts,
            TRAINLOG_DASHBOARD_FACT_CAPACITY, &fact_count, &facts_truncated,
            &facts_invalid) != TRAINLOG_STATUS_OK) {
        dashboard->error = true;
        goto finish;
    }
    dashboard->partial = dashboard->partial || facts_truncated;
    dashboard->invalid_data = dashboard->invalid_data || facts_invalid;
    qsort(facts, fact_count, sizeof(facts[0]), dashboard_fact_time_compare);
    for (exercise_index = 0U; exercise_index < fact_count; ++exercise_index) {
        TrainlogDashboardFact *fact = &facts[exercise_index];
        TrainlogExercise *exercise;
        bool external;
        bool comparable_work;
        bool comparable_max;
        size_t prior;
        bool has_prior = false;
        double prior_best = 0.0;
        exercise = dashboard_find_exercise(exercises, exercise_count,
            fact->exercise_id);
        if (!dashboard_in_period(&fact->time, dashboard->period, query->reference_unix_second)) continue;
        if (fact->kind == TRAINLOG_DASHBOARD_WORK)
            ++dashboard->performed_set_count;
        else if (fact->kind == TRAINLOG_DASHBOARD_MAX)
            ++dashboard->explicit_max_count;
        {
            size_t seen;
            for (seen = 0U; seen < exercise_index; ++seen)
                if (dashboard_in_period(&facts[seen].time, dashboard->period, query->reference_unix_second) &&
                    strcmp(facts[seen].exercise_id, fact->exercise_id) == 0)
                    break;
            if (seen == exercise_index) ++dashboard->distinct_exercise_count;
        }
        external = dashboard_fact_external_context_qualifies(database, fact);
        comparable_work = fact->kind == TRAINLOG_DASHBOARD_WORK && external &&
            fact->load_mode == TRAINLOG_LOAD_EXTERNAL && fact->dose > 0 &&
            fact->has_weight && isfinite(fact->weight_kg) &&
            fact->weight_kg >= 0.0;
        comparable_max = fact->kind == TRAINLOG_DASHBOARD_MAX && external &&
            fact->load_mode != TRAINLOG_LOAD_ASSISTANCE && fact->weight_kg > 0.0;

        /* CONTRACT: Android and TUI compare the same actual observations:
         * exact exercise + nonempty resolved external equipment + external
         * load semantics + exact performed dose. Only facts at a strictly
         * earlier canonical instant establish the prior best; bytewise IDs
         * order equal-instant presentation but never manufacture an event. */
        if (comparable_work || comparable_max) {
            for (prior = 0U; prior < exercise_index; ++prior) {
                TrainlogDashboardFact *old = &facts[prior];
                bool same = old->kind == fact->kind &&
                    strcmp(old->exercise_id, fact->exercise_id) == 0 &&
                    strcmp(old->equipment_id, fact->equipment_id) == 0 &&
                    old->tracking_mode == fact->tracking_mode &&
                    old->load_mode == fact->load_mode &&
                    (fact->kind != TRAINLOG_DASHBOARD_WORK ||
                        old->dose == fact->dose);
                if (!same || trainlog_timestamp_compare(&old->time,
                        &fact->time) >= 0) continue;
                if (!has_prior || old->weight_kg > prior_best)
                    prior_best = old->weight_kg;
                has_prior = true;
            }
            if (has_prior && fact->weight_kg > prior_best &&
                dashboard->week_count > 0U) {
                size_t slot = dashboard_period_bucket(dashboard,
                    fact->time.local_day);
                if (comparable_work)
                    ++dashboard->weeks[slot].working_improvements;
                else ++dashboard->weeks[slot].max_improvements;
            }
        }
        if (comparable_max && exercise != NULL) {
            TrainlogTimestampKey current_max;
            int order = 1;
            if (dashboard->has_explicit_max &&
                trainlog_timestamp_parse(dashboard->max_timestamp,
                    strlen(dashboard->max_timestamp), &current_max))
                order = trainlog_timestamp_compare(&fact->time, &current_max);
            if (!dashboard->has_explicit_max || order > 0 ||
                (order == 0 && strcmp(fact->exercise_id,
                    dashboard->max_exercise.exercise_id) < 0)) {
                dashboard->has_explicit_max = true;
                dashboard->max_exercise = *exercise;
                dashboard->max_weight_kg = fact->weight_kg;
                (void)snprintf(dashboard->max_timestamp,
                    sizeof(dashboard->max_timestamp), "%s", fact->started_at);
            }
        }
        if (comparable_work && exercise != NULL) {
            int order = has_newest_performance ?
                trainlog_timestamp_compare(&fact->time, &newest_performance) : 1;
            if (!has_newest_performance || order > 0 ||
                (order == 0 && strcmp(fact->exercise_id,
                    dashboard->exercise.exercise_id) < 0)) {
                dashboard->has_performance = true;
                dashboard->exercise = *exercise;
                dashboard->performance_load_mode = fact->load_mode;
                dashboard->performance_tracking_mode = fact->tracking_mode;
                dashboard->performance_dose = fact->dose;
                (void)snprintf(dashboard->performance_equipment_id,
                    sizeof(dashboard->performance_equipment_id), "%s",
                    fact->equipment_id);
                newest_performance = fact->time;
                has_newest_performance = true;
            }
        }
    }
    if (dashboard->has_performance) {
        TrainlogResolvedEquipment equipment;
        size_t index;
        if (trainlog_database_resolve_equipment(database,
                dashboard->performance_equipment_id, &equipment) ==
                TRAINLOG_STATUS_OK)
            (void)snprintf(dashboard->performance_equipment_label,
                sizeof(dashboard->performance_equipment_label), "%s",
                equipment.display_name);
        for (index = 0U; index < fact_count &&
             dashboard->performance_count < TRAINLOG_DASHBOARD_SESSION_CAPACITY; ++index) {
            TrainlogDashboardFact *fact = &facts[index];
            TrainlogExercisePerformancePoint *point;
            if (fact->kind != TRAINLOG_DASHBOARD_WORK || fact->dose <= 0 ||
                fact->load_mode != dashboard->performance_load_mode ||
                fact->tracking_mode != dashboard->performance_tracking_mode ||
                fact->dose != dashboard->performance_dose ||
                strcmp(fact->exercise_id, dashboard->exercise.exercise_id) != 0 ||
                strcmp(fact->equipment_id,
                    dashboard->performance_equipment_id) != 0 ||
                !dashboard_in_period(&fact->time, dashboard->period, query->reference_unix_second)) continue;
            point = &dashboard->performance[dashboard->performance_count++];
            (void)memset(point, 0, sizeof(*point));
            (void)snprintf(point->session_id, sizeof(point->session_id), "%s",
                fact->session_id);
            (void)snprintf(point->started_at, sizeof(point->started_at), "%s",
                fact->started_at);
            point->tracking_mode = fact->tracking_mode;
            point->load_mode = fact->load_mode;
            point->actual_set_count = 1U;
            point->has_performance = 1;
            point->metric_value = fact->dose;
            point->has_weight = 1;
            point->weight_kg = fact->weight_kg;
            (void)snprintf(point->equipment_id, sizeof(point->equipment_id), "%s",
                fact->equipment_id);
        }
        if (dashboard->performance_count == TRAINLOG_DASHBOARD_SESSION_CAPACITY)
            dashboard->partial = true;
        qsort(dashboard->performance, dashboard->performance_count,
            sizeof(dashboard->performance[0]), dashboard_performance_time_compare);
    }

    for (exercise_index = 0U; exercise_index < 14U; ++exercise_index) {
        size_t count = 0U;
        size_t point_index;
        if (trainlog_database_list_body_metric_points(database,
                DASHBOARD_BODY_METRICS[exercise_index], body,
                TRAINLOG_DASHBOARD_BODY_CAPACITY, &count) != TRAINLOG_STATUS_OK) {
            dashboard->error = true;
            continue;
        }
        if (count == TRAINLOG_DASHBOARD_BODY_CAPACITY) dashboard->partial = true;
        for (point_index = 0U; point_index < count; ++point_index) {
            TrainlogTimestampKey candidate;
            TrainlogTimestampKey current;
            int order = 1;
            if (!trainlog_timestamp_parse(body[point_index].observed_at,
                    strlen(body[point_index].observed_at), &candidate)) {
                dashboard->invalid_data = true;
                continue;
            }
            if (!dashboard_in_period(&candidate, dashboard->period, query->reference_unix_second)) continue;
            if (newest_body[0] != '\0') {
                (void)trainlog_timestamp_parse(newest_body, strlen(newest_body), &current);
                order = trainlog_timestamp_compare(&candidate, &current);
            }
            if (newest_body[0] == '\0' || order > 0 ||
                (order == 0 && exercise_index < dashboard->body_metric)) {
                dashboard->has_body = true;
                dashboard->body_metric = exercise_index;
                (void)snprintf(newest_body, sizeof(newest_body), "%s",
                    body[point_index].observed_at);
            }
        }
    }
    if (dashboard->has_body) {
        size_t count = 0U;
        size_t point_index;
        if (trainlog_database_list_body_metric_points(database,
                DASHBOARD_BODY_METRICS[dashboard->body_metric], body,
                TRAINLOG_DASHBOARD_BODY_CAPACITY, &count) != TRAINLOG_STATUS_OK) {
            dashboard->error = true;
            goto finish;
        }
        if (count == TRAINLOG_DASHBOARD_BODY_CAPACITY)
            dashboard->partial = true;
        for (point_index = 0U; point_index < count; ++point_index) {
            TrainlogTimestampKey key;
            if (!trainlog_timestamp_parse(body[point_index].observed_at,
                    strlen(body[point_index].observed_at), &key)) {
                dashboard->invalid_data = true;
                continue;
            }
            if (dashboard_in_period(&key, dashboard->period, query->reference_unix_second))
                dashboard->body[dashboard->body_count++] = body[point_index];
        }
        qsort(dashboard->body, dashboard->body_count, sizeof(dashboard->body[0]),
            dashboard_body_time_compare);
    }

finish:
    if (trainlog_database_read_snapshot_end(database, !dashboard->error) !=
        TRAINLOG_STATUS_OK) dashboard->error = true;
    free(exercises);
    free(facts);
    return dashboard->error ? TRAINLOG_STATUS_DATABASE_ERROR : TRAINLOG_STATUS_OK;
}
