#include "trainlog/web_dashboard.h"

#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

#include "trainlog/body_zone_catalog.h"
#include "database_internal.h"
#include "timestamp.h"

#define SECONDS_PER_DAY INT64_C(86400)

static bool copy_column(sqlite3_stmt *s, int column, char *out, size_t cap) {
    const unsigned char *text;
    int bytes;
    if (sqlite3_column_type(s, column) != SQLITE_TEXT) {
        return false;
    }
    text = sqlite3_column_text(s, column);
    bytes = sqlite3_column_bytes(s, column);
    if (text == NULL || bytes < 0 || (size_t)bytes >= cap) {
        return false;
    }
    (void)memcpy(out, text, (size_t)bytes);
    out[bytes] = '\0';
    return true;
}
static bool read_size(sqlite3_stmt *s, int c, size_t *out) {
    sqlite3_int64 v;
    if (sqlite3_column_type(s, c) != SQLITE_INTEGER) {
        return false;
    }
    v = sqlite3_column_int64(s, c);
    if (v < 0 || (uint64_t)v > (uint64_t)SIZE_MAX) {
        return false;
    }
    *out = (size_t)v;
    return true;
}
static bool prepare(TrainlogDatabase *d, const char *sql, sqlite3_stmt **s) {
    return sqlite3_prepare_v2(d->connection, sql, -1, s, NULL) == SQLITE_OK;
}

static bool format_generated(int64_t now, char out[TRAINLOG_TIMESTAMP_MAX + 1U]) {
    time_t value = (time_t)now;
    struct tm utc;
    if ((int64_t)value != now || gmtime_r(&value, &utc) == NULL) {
        return false;
    }
    return strftime(out, TRAINLOG_TIMESTAMP_MAX + 1U, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0U;
}
static bool format_local_date(time_t value, char out[11]) {
    struct tm local;
    return localtime_r(&value, &local) != NULL && strftime(out, 11U, "%Y-%m-%d", &local) == 10U;
}
static bool load_user(char out[TRAINLOG_NAME_MAX + 1U]) {
    struct passwd value;
    struct passwd *result = NULL;
    char buffer[4096];
    int rc = getpwuid_r(geteuid(), &value, buffer, sizeof(buffer), &result);
    if (rc != 0 || result == NULL || result->pw_name == NULL || result->pw_name[0] == '\0' ||
        strlen(result->pw_name) > TRAINLOG_NAME_MAX) {
        return false;
    }
    (void)snprintf(out, TRAINLOG_NAME_MAX + 1U, "%s", result->pw_name);
    return true;
}

static TrainlogStatus
load_activity(TrainlogDatabase *d, int64_t now, TrainlogWebDashboardSnapshot *out) {
    static const char SQL[] =
        "SELECT substr(s.started_at,1,10),COUNT(DISTINCT s.id),COUNT(ps.id) FROM sessions s "
        "LEFT JOIN session_exercises se ON se.session_row_id=s.id LEFT JOIN performed_sets ps ON "
        "ps.session_exercise_row_id=se.id "
        "WHERE unixepoch(s.started_at)<=?1 AND substr(s.started_at,1,10)>=?2 AND (EXISTS(SELECT 1 "
        "FROM session_exercises x JOIN performed_sets p ON p.session_exercise_row_id=x.id WHERE "
        "x.session_row_id=s.id) OR EXISTS(SELECT 1 FROM session_exercises x JOIN "
        "continuous_activity c ON c.session_exercise_row_id=x.id WHERE x.session_row_id=s.id) OR "
        "EXISTS(SELECT 1 FROM session_exercises x JOIN max_results m ON "
        "m.session_exercise_row_id=x.id WHERE x.session_row_id=s.id)) GROUP BY "
        "substr(s.started_at,1,10) ORDER BY 1;";
    sqlite3_stmt *s = NULL;
    int step;
    size_t i;
    time_t reference = (time_t)now;
    struct tm current;
    if ((int64_t)reference != now) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    if (localtime_r(&reference, &current) == NULL) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    out->activity_count = TRAINLOG_WEB_ACTIVITY_DAYS;
    for (i = 0U; i < TRAINLOG_WEB_ACTIVITY_DAYS; ++i) {
        struct tm day = current;
        time_t normalized;
        day.tm_mday -= (int)(TRAINLOG_WEB_ACTIVITY_DAYS - 1U - i);
        day.tm_isdst = -1;
        normalized = mktime(&day);
        if (normalized == (time_t)-1 || !format_local_date(normalized, out->activity[i].date)) {
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    if (!prepare(d, SQL, &s) || sqlite3_bind_int64(s, 1, now) != SQLITE_OK ||
        sqlite3_bind_text(s, 2, out->activity[0].date, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        goto fail;
    }
    while ((step = sqlite3_step(s)) == SQLITE_ROW) {
        char date[11];
        size_t sessions, sets;
        if (!copy_column(s, 0, date, sizeof(date)) || !read_size(s, 1, &sessions) ||
            !read_size(s, 2, &sets)) {
            goto fail;
        }
        for (i = 0U; i < out->activity_count; ++i) {
            if (strcmp(date, out->activity[i].date) == 0) {
                out->activity[i].active = true;
                out->activity[i].session_count = sessions;
                out->activity[i].set_count = sets;
                break;
            }
        }
    }
    if (step != SQLITE_DONE || sqlite3_finalize(s) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
fail:
    if (s != NULL) {
        (void)sqlite3_finalize(s);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus load_last(TrainlogDatabase *d, int64_t now, TrainlogWebLastSession *out) {
    static const char SESSION_SQL[] =
        "SELECT s.session_id,s.started_at,COALESCE(s.ended_at,''),COUNT(DISTINCT CASE WHEN ps.id "
        "IS NOT NULL OR ca.id IS NOT NULL OR mr.session_exercise_row_id IS NOT NULL THEN se.id "
        "END),COUNT(DISTINCT ps.id),COUNT(DISTINCT ca.id),COUNT(DISTINCT "
        "mr.session_exercise_row_id) FROM sessions s JOIN session_exercises se ON "
        "se.session_row_id=s.id LEFT JOIN performed_sets ps ON ps.session_exercise_row_id=se.id "
        "LEFT JOIN continuous_activity ca ON ca.session_exercise_row_id=se.id LEFT JOIN "
        "max_results mr ON mr.session_exercise_row_id=se.id WHERE unixepoch(s.started_at)<=?1 AND "
        "(ps.id IS NOT NULL OR ca.id IS NOT NULL OR mr.session_exercise_row_id IS NOT NULL) GROUP "
        "BY s.id ORDER BY unixepoch(s.started_at) DESC,s.session_id DESC LIMIT 1;";
    static const char ZONE_SQL[] =
        "SELECT z.zone_id,COUNT(DISTINCT se.id),COUNT(DISTINCT ps.id) FROM sessions s JOIN "
        "session_exercises se ON se.session_row_id=s.id JOIN exercises e ON "
        "e.id=se.exercise_row_id JOIN exercise_body_zones z ON z.exercise_row_id=e.id AND "
        "z.role='primary' LEFT JOIN performed_sets ps ON ps.session_exercise_row_id=se.id WHERE "
        "s.session_id=?1 AND (EXISTS(SELECT 1 FROM performed_sets p WHERE "
        "p.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM continuous_activity c WHERE "
        "c.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM max_results m WHERE "
        "m.session_exercise_row_id=se.id)) GROUP BY z.zone_id ORDER BY z.zone_id;";
    sqlite3_stmt *s = NULL;
    int step;
    TrainlogTimestampKey start, end;
    if (!prepare(d, SESSION_SQL, &s) || sqlite3_bind_int64(s, 1, now) != SQLITE_OK) {
        goto fail;
    }
    step = sqlite3_step(s);
    if (step == SQLITE_DONE) {
        (void)sqlite3_finalize(s);
        return TRAINLOG_STATUS_OK;
    }
    if (step != SQLITE_ROW || !copy_column(s, 0, out->session_id, sizeof(out->session_id)) ||
        !copy_column(s, 1, out->started_at, sizeof(out->started_at)) ||
        !copy_column(s, 2, out->ended_at, sizeof(out->ended_at)) ||
        !read_size(s, 3, &out->exercise_count) || !read_size(s, 4, &out->set_count) ||
        !read_size(s, 5, &out->continuous_count) || !read_size(s, 6, &out->max_count)) {
        goto fail;
    }
    out->available = true;
    out->has_ended_at = out->ended_at[0] != '\0';
    if (!trainlog_timestamp_parse(out->started_at, strlen(out->started_at), &start)) {
        out->available = false;
        goto invalid;
    }
    if (out->has_ended_at) {
        if (!trainlog_timestamp_parse(out->ended_at, strlen(out->ended_at), &end) ||
            end.utc_second < start.utc_second) {
            goto invalid;
        }
        out->has_duration = true;
        out->duration_seconds = (uint64_t)(end.utc_second - start.utc_second);
    }
    if (sqlite3_step(s) != SQLITE_DONE || sqlite3_finalize(s) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    s = NULL;
    if (!prepare(d, ZONE_SQL, &s) ||
        sqlite3_bind_text(s, 1, out->session_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        goto fail;
    }
    while ((step = sqlite3_step(s)) == SQLITE_ROW) {
        TrainlogWebWorkedZone *z;
        const TrainlogBodyZone *catalog;
        if (out->zone_count == TRAINLOG_WEB_ZONE_CAPACITY) {
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        z = &out->zones[out->zone_count];
        if (!copy_column(s, 0, z->zone_id, sizeof(z->zone_id)) ||
            !read_size(s, 1, &z->occurrence_count) || !read_size(s, 2, &z->set_count)) {
            goto fail;
        }
        z->session_count = 1U;
        catalog = trainlog_body_zone_catalog_lookup(z->zone_id);
        if (catalog == NULL || strlen(catalog->display_name) > TRAINLOG_NAME_MAX) {
            goto invalid;
        }
        (void)snprintf(z->label, sizeof(z->label), "%s", catalog->display_name);
        ++out->zone_count;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(s) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
invalid:
    if (s != NULL) {
        (void)sqlite3_finalize(s);
    }
    return TRAINLOG_STATUS_INVALID_ARGUMENT;
fail:
    if (s != NULL) {
        (void)sqlite3_finalize(s);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus
load_maxima(TrainlogDatabase *d, int64_t now, TrainlogWebDashboardSnapshot *out) {
    static const char SQL[] =
        "SELECT "
        "s.session_id,se.entry_id,e.exercise_id,e.name,COALESCE(se.equipment_id,''),m.max_weight_"
        "kg,s.started_at FROM max_results m JOIN session_exercises se ON "
        "se.id=m.session_exercise_row_id JOIN sessions s ON s.id=se.session_row_id JOIN exercises "
        "e ON e.id=se.exercise_row_id WHERE unixepoch(s.started_at)<=?1 ORDER BY "
        "unixepoch(s.started_at) DESC,s.session_id DESC,se.entry_id DESC LIMIT ?2;";
    sqlite3_stmt *s = NULL;
    int step;
    if (!prepare(d, SQL, &s) || sqlite3_bind_int64(s, 1, now) != SQLITE_OK ||
        sqlite3_bind_int64(s, 2, (sqlite3_int64)(TRAINLOG_WEB_MAX_RECORDS + 1U)) != SQLITE_OK) {
        goto fail;
    }
    while ((step = sqlite3_step(s)) == SQLITE_ROW) {
        TrainlogWebMaxRecord *r;
        if (out->max_record_count == TRAINLOG_WEB_MAX_RECORDS) {
            out->partial = true;
            continue;
        }
        r = &out->max_records[out->max_record_count];
        if (!copy_column(s, 0, r->session_id, sizeof(r->session_id)) ||
            !copy_column(s, 1, r->entry_id, sizeof(r->entry_id)) ||
            !copy_column(s, 2, r->exercise_id, sizeof(r->exercise_id)) ||
            !copy_column(s, 3, r->exercise_name, sizeof(r->exercise_name)) ||
            !copy_column(s, 4, r->equipment_id, sizeof(r->equipment_id)) ||
            !copy_column(s, 6, r->timestamp, sizeof(r->timestamp)) ||
            (sqlite3_column_type(s, 5) != SQLITE_FLOAT &&
             sqlite3_column_type(s, 5) != SQLITE_INTEGER) ||
            !isfinite(sqlite3_column_double(s, 5)) || sqlite3_column_double(s, 5) <= 0.0) {
            out->invalid_data = true;
            continue;
        }
        r->weight_kg = sqlite3_column_double(s, 5);
        ++out->max_record_count;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(s) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
fail:
    if (s != NULL) {
        (void)sqlite3_finalize(s);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus
load_zones(TrainlogDatabase *d, int64_t now, TrainlogWebDashboardSnapshot *out) {
    static const char SQL[] =
        "SELECT z.zone_id,COUNT(DISTINCT s.id),COUNT(DISTINCT se.id),COUNT(DISTINCT ps.id) FROM "
        "sessions s JOIN session_exercises se ON se.session_row_id=s.id JOIN exercises e ON "
        "e.id=se.exercise_row_id JOIN exercise_body_zones z ON z.exercise_row_id=e.id AND "
        "z.role='primary' LEFT JOIN performed_sets ps ON ps.session_exercise_row_id=se.id WHERE "
        "substr(s.started_at,1,10)>=?1 AND unixepoch(s.started_at)<=?2 AND (EXISTS(SELECT 1 FROM "
        "performed_sets p WHERE p.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM "
        "continuous_activity c WHERE c.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM "
        "max_results m WHERE m.session_exercise_row_id=se.id)) GROUP BY z.zone_id ORDER BY "
        "z.zone_id;";
    sqlite3_stmt *s = NULL;
    int step;
    if (!prepare(d, SQL, &s) ||
        sqlite3_bind_text(
            s, 1, out->activity[TRAINLOG_WEB_ACTIVITY_DAYS - 30U].date, -1, SQLITE_TRANSIENT) !=
            SQLITE_OK ||
        sqlite3_bind_int64(s, 2, now) != SQLITE_OK) {
        goto fail;
    }
    while ((step = sqlite3_step(s)) == SQLITE_ROW) {
        TrainlogWebWorkedZone *z;
        const TrainlogBodyZone *catalog;
        if (out->muscle_zone_count == TRAINLOG_WEB_ZONE_CAPACITY) {
            out->partial = true;
            continue;
        }
        z = &out->muscle_zones[out->muscle_zone_count];
        if (!copy_column(s, 0, z->zone_id, sizeof(z->zone_id)) ||
            !read_size(s, 1, &z->session_count) || !read_size(s, 2, &z->occurrence_count) ||
            !read_size(s, 3, &z->set_count)) {
            goto fail;
        }
        catalog = trainlog_body_zone_catalog_lookup(z->zone_id);
        if (catalog == NULL || strlen(catalog->display_name) > TRAINLOG_NAME_MAX) {
            out->invalid_data = true;
            continue;
        }
        (void)snprintf(z->label, sizeof(z->label), "%s", catalog->display_name);
        ++out->muscle_zone_count;
    }
    if (step != SQLITE_DONE || sqlite3_finalize(s) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
fail:
    if (s != NULL) {
        (void)sqlite3_finalize(s);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

static TrainlogStatus scan_quality(TrainlogDatabase *d, TrainlogWebDashboardSnapshot *out) {
    static const char SQL[] =
        "SELECT s.started_at,COALESCE(s.ended_at,'') FROM sessions s WHERE EXISTS(SELECT 1 FROM "
        "session_exercises x JOIN performed_sets p ON p.session_exercise_row_id=x.id WHERE "
        "x.session_row_id=s.id) OR EXISTS(SELECT 1 FROM session_exercises x JOIN "
        "continuous_activity c ON c.session_exercise_row_id=x.id WHERE x.session_row_id=s.id) OR "
        "EXISTS(SELECT 1 FROM session_exercises x JOIN max_results m ON "
        "m.session_exercise_row_id=x.id WHERE x.session_row_id=s.id) ORDER BY s.id LIMIT 4097;";
    sqlite3_stmt *s = NULL;
    int step;
    size_t count = 0U;
    if (!prepare(d, SQL, &s)) {
        goto fail;
    }
    while ((step = sqlite3_step(s)) == SQLITE_ROW) {
        char started[TRAINLOG_TIMESTAMP_MAX + 1U], ended[TRAINLOG_TIMESTAMP_MAX + 1U];
        TrainlogTimestampKey start_key, end_key;
        ++count;
        if (count > 4096U) {
            out->partial = true;
            continue;
        }
        if (!copy_column(s, 0, started, sizeof(started)) ||
            !copy_column(s, 1, ended, sizeof(ended)) ||
            !trainlog_timestamp_parse(started, strlen(started), &start_key) ||
            (ended[0] != '\0' && (!trainlog_timestamp_parse(ended, strlen(ended), &end_key) ||
                                  trainlog_timestamp_compare(&end_key, &start_key) < 0))) {
            out->invalid_data = true;
        }
    }
    if (step != SQLITE_DONE || sqlite3_finalize(s) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    return TRAINLOG_STATUS_OK;
fail:
    if (s != NULL) {
        (void)sqlite3_finalize(s);
    }
    return TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_web_dashboard_load(TrainlogDatabase *d,
                                           const TrainlogWebDashboardQuery *q,
                                           TrainlogWebDashboardSnapshot *out) {
    TrainlogStatus status, end_status;
    TrainlogDashboardQuery progression_query;
    if (d == NULL || q == NULL || out == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out, 0, sizeof(*out));
    out->next_session_reason = "no_persisted_executable_plan";
    out->cardio_reason = "no_cardio_data_source";
    if (!format_generated(q->reference_unix_second, out->generated_at) ||
        !load_user(out->user_display_name)) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = trainlog_database_read_snapshot_begin(d);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    status = load_activity(d, q->reference_unix_second, out);
    if (status == TRAINLOG_STATUS_OK) {
        status = scan_quality(d, out);
    }
    if (status == TRAINLOG_STATUS_OK) {
        progression_query.period = TRAINLOG_DASHBOARD_90_DAYS;
        progression_query.reference_unix_second = q->reference_unix_second;
        status = trainlog_dashboard_load(d, &progression_query, &out->progression);
        out->partial = out->partial || out->progression.partial;
        out->invalid_data = out->invalid_data || out->progression.invalid_data;
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = load_last(d, q->reference_unix_second, &out->last_session);
        if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
            out->invalid_data = true;
            status = TRAINLOG_STATUS_OK;
        }
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = load_maxima(d, q->reference_unix_second, out);
    }
    if (status == TRAINLOG_STATUS_OK) {
        status = load_zones(d, q->reference_unix_second, out);
    }
    end_status = trainlog_database_read_snapshot_end(d, status == TRAINLOG_STATUS_OK);
    if (end_status != TRAINLOG_STATUS_OK) {
        status = end_status;
    }
    return status;
}
