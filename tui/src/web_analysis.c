#include "trainlog/web_analysis.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"
#include "trainlog/body_zone_catalog.h"

#define SECONDS_PER_DAY INT64_C(86400)

typedef struct MeasurementDefinition {
    const char *id;
    const char *column;
    const char *unit;
} MeasurementDefinition;

static const MeasurementDefinition MEASUREMENTS[] = {
    {"weight", "body_weight_kg", "kg"},
    {"neck", "neck_cm", "cm"},
    {"shoulders", "shoulders_cm", "cm"},
    {"chest", "chest_cm", "cm"},
    {"waist", "waist_cm", "cm"},
    {"hips", "hips_cm", "cm"},
    {"left_arm", "left_arm_cm", "cm"},
    {"right_arm", "right_arm_cm", "cm"},
    {"left_forearm", "left_forearm_cm", "cm"},
    {"right_forearm", "right_forearm_cm", "cm"},
    {"left_thigh", "left_thigh_cm", "cm"},
    {"right_thigh", "right_thigh_cm", "cm"},
    {"left_calf", "left_calf_cm", "cm"},
    {"right_calf", "right_calf_cm", "cm"},
};

static const MeasurementDefinition *measurement_definition(const char *id) {
    size_t index;
    for (index = 0U; index < sizeof(MEASUREMENTS) / sizeof(MEASUREMENTS[0]); ++index) {
        if (strcmp(id, MEASUREMENTS[index].id) == 0) {
            return &MEASUREMENTS[index];
        }
    }
    return NULL;
}

static bool prepare(TrainlogDatabase *database, const char *sql, sqlite3_stmt **statement) {
    return sqlite3_prepare_v2(database->connection, sql, -1, statement, NULL) == SQLITE_OK;
}

static bool bind_interval(sqlite3_stmt *statement, const TrainlogWebAnalysisQuery *query) {
    if (query->period == TRAINLOG_WEB_ANALYSIS_ALL) {
        if (sqlite3_bind_null(statement, 1) != SQLITE_OK) {
            return false;
        }
    } else {
        int64_t days = (int64_t)query->period;
        int64_t start = query->reference_unix_second - ((days - 1) * SECONDS_PER_DAY);
        if (sqlite3_bind_int64(statement, 1, start) != SQLITE_OK) {
            return false;
        }
    }
    return sqlite3_bind_int64(statement, 2, query->reference_unix_second) == SQLITE_OK;
}

static bool add_nullable_real(yyjson_mut_doc *document,
                              yyjson_mut_val *object,
                              const char *key,
                              sqlite3_stmt *statement,
                              int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return yyjson_mut_obj_add_null(document, object, key);
    }
    return yyjson_mut_obj_add_real(document, object, key, sqlite3_column_double(statement, column));
}

static bool add_overview(TrainlogDatabase *database,
                         const TrainlogWebAnalysisQuery *query,
                         yyjson_mut_doc *document,
                         yyjson_mut_val *root) {
    static const char SQL[] =
        "WITH eligible AS (SELECT s.id,s.started_at,s.ended_at FROM sessions s WHERE "
        "(?1 IS NULL OR unixepoch(s.started_at)>=?1) AND unixepoch(s.started_at)<=?2 AND "
        "(EXISTS(SELECT 1 FROM session_exercises x JOIN performed_sets p ON "
        "p.session_exercise_row_id=x.id WHERE x.session_row_id=s.id) OR EXISTS(SELECT 1 FROM "
        "session_exercises x JOIN continuous_activity c ON c.session_exercise_row_id=x.id WHERE "
        "x.session_row_id=s.id) OR EXISTS(SELECT 1 FROM session_exercises x JOIN max_results m ON "
        "m.session_exercise_row_id=x.id WHERE x.session_row_id=s.id))) SELECT COUNT(*),"
        "(SELECT COUNT(*) FROM performed_sets ps JOIN session_exercises se ON "
        "se.id=ps.session_exercise_row_id JOIN eligible e ON e.id=se.session_row_id),"
        "CASE WHEN COUNT(*)>0 AND COUNT(*)=SUM(CASE WHEN ended_at IS NOT NULL AND "
        "unixepoch(ended_at)>=unixepoch(started_at) THEN 1 ELSE 0 END) THEN "
        "SUM(unixepoch(ended_at)-unixepoch(started_at)) ELSE NULL END FROM eligible";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *overview = yyjson_mut_obj(document);
    bool ok;
    if (overview == NULL || !prepare(database, SQL, &statement) ||
        !bind_interval(statement, query) || sqlite3_step(statement) != SQLITE_ROW) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    ok = yyjson_mut_obj_add_sint(
             document, overview, "sessions", sqlite3_column_int64(statement, 0)) &&
         yyjson_mut_obj_add_sint(document, overview, "sets", sqlite3_column_int64(statement, 1)) &&
         (sqlite3_column_type(statement, 2) == SQLITE_NULL
              ? yyjson_mut_obj_add_null(document, overview, "duration_seconds")
              : yyjson_mut_obj_add_sint(
                    document, overview, "duration_seconds", sqlite3_column_int64(statement, 2))) &&
         yyjson_mut_obj_add_val(document, root, "overview", overview) &&
         sqlite3_step(statement) == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK;
    return ok;
}

static bool add_activity(TrainlogDatabase *database,
                         const TrainlogWebAnalysisQuery *query,
                         yyjson_mut_doc *document,
                         yyjson_mut_val *root,
                         bool *partial) {
    static const char SQL[] =
        "SELECT substr(s.started_at,1,10),COUNT(DISTINCT s.id),COUNT(DISTINCT ps.id) FROM "
        "sessions s LEFT JOIN session_exercises se ON se.session_row_id=s.id LEFT JOIN "
        "performed_sets ps ON ps.session_exercise_row_id=se.id WHERE (?1 IS NULL OR "
        "unixepoch(s.started_at)>=?1) AND unixepoch(s.started_at)<=?2 AND (ps.id IS NOT NULL OR "
        "EXISTS(SELECT 1 FROM session_exercises x JOIN continuous_activity c ON "
        "c.session_exercise_row_id=x.id WHERE x.session_row_id=s.id) OR EXISTS(SELECT 1 FROM "
        "session_exercises x JOIN max_results m ON m.session_exercise_row_id=x.id WHERE "
        "x.session_row_id=s.id)) GROUP BY substr(s.started_at,1,10) ORDER BY 1 DESC LIMIT ?3";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *activity = yyjson_mut_arr(document);
    size_t count = 0U;
    int step;
    if (activity == NULL || !prepare(database, SQL, &statement) ||
        !bind_interval(statement, query) ||
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)(TRAINLOG_WEB_ANALYSIS_POINTS_MAX + 1U)) !=
            SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        yyjson_mut_val *day;
        if (count == TRAINLOG_WEB_ANALYSIS_POINTS_MAX) {
            *partial = true;
            continue;
        }
        day = yyjson_mut_obj(document);
        if (day == NULL ||
            !yyjson_mut_obj_add_strcpy(
                document, day, "date", (const char *)sqlite3_column_text(statement, 0)) ||
            !yyjson_mut_obj_add_sint(
                document, day, "sessions", sqlite3_column_int64(statement, 1)) ||
            !yyjson_mut_obj_add_sint(document, day, "sets", sqlite3_column_int64(statement, 2)) ||
            !yyjson_mut_arr_add_val(activity, day)) {
            (void)sqlite3_finalize(statement);
            return false;
        }
        ++count;
    }
    return step == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK &&
           yyjson_mut_obj_add_val(document, root, "activity", activity);
}

static bool add_exercises(TrainlogDatabase *database,
                          const TrainlogWebAnalysisQuery *query,
                          yyjson_mut_doc *document,
                          yyjson_mut_val *root,
                          char selected_id[64],
                          bool *partial) {
    static const char LIST_SQL[] =
        "SELECT e.exercise_id,e.name,e.tracking_mode,e.recording_mode FROM exercises e ORDER BY "
        "e.name COLLATE NOCASE,e.exercise_id LIMIT ?1";
    static const char RECENT_SQL[] =
        "SELECT e.exercise_id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id "
        "JOIN exercises e ON e.id=se.exercise_row_id WHERE (?1 IS NULL OR "
        "unixepoch(s.started_at)>=?1) AND unixepoch(s.started_at)<=?2 AND "
        "(EXISTS(SELECT 1 FROM performed_sets p WHERE p.session_exercise_row_id=se.id) OR "
        "EXISTS(SELECT 1 FROM continuous_activity c WHERE c.session_exercise_row_id=se.id) OR "
        "EXISTS(SELECT 1 FROM max_results m WHERE m.session_exercise_row_id=se.id)) ORDER BY "
        "unixepoch(s.started_at) DESC,s.session_id DESC LIMIT 1";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *items = yyjson_mut_arr(document);
    size_t count = 0U;
    int step;
    if (query->exercise_id != NULL) {
        (void)snprintf(selected_id, 64U, "%s", query->exercise_id);
    } else if (prepare(database, RECENT_SQL, &statement) && bind_interval(statement, query)) {
        if (sqlite3_step(statement) == SQLITE_ROW) {
            (void)snprintf(selected_id, 64U, "%s", (const char *)sqlite3_column_text(statement, 0));
        }
        (void)sqlite3_finalize(statement);
        statement = NULL;
    } else {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    if (items == NULL || !prepare(database, LIST_SQL, &statement) ||
        sqlite3_bind_int64(statement, 1, TRAINLOG_WEB_ANALYSIS_EXERCISES_MAX + 1U) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        yyjson_mut_val *item;
        if (count == TRAINLOG_WEB_ANALYSIS_EXERCISES_MAX) {
            *partial = true;
            continue;
        }
        item = yyjson_mut_obj(document);
        if (item == NULL ||
            !yyjson_mut_obj_add_strcpy(
                document, item, "exercise_id", (const char *)sqlite3_column_text(statement, 0)) ||
            !yyjson_mut_obj_add_strcpy(
                document, item, "name", (const char *)sqlite3_column_text(statement, 1)) ||
            !yyjson_mut_obj_add_strcpy(
                document, item, "tracking_mode", (const char *)sqlite3_column_text(statement, 2)) ||
            !yyjson_mut_obj_add_strcpy(document,
                                       item,
                                       "recording_mode",
                                       (const char *)sqlite3_column_text(statement, 3)) ||
            !yyjson_mut_arr_add_val(items, item)) {
            (void)sqlite3_finalize(statement);
            return false;
        }
        ++count;
    }
    return step == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK &&
           yyjson_mut_obj_add_val(document, root, "exercises", items);
}

static bool add_exercise_detail(TrainlogDatabase *database,
                                const TrainlogWebAnalysisQuery *query,
                                const char *exercise_id,
                                yyjson_mut_doc *document,
                                yyjson_mut_val *root,
                                bool *partial) {
    static const char PROFILE_SQL[] =
        "SELECT name,tracking_mode,recording_mode FROM exercises WHERE exercise_id=?1";
    static const char POINT_SQL[] =
        "SELECT s.session_id,s.started_at,se.load_mode,COUNT(ps.id),SUM(ps.reps),"
        "SUM(ps.duration_seconds),CASE WHEN se.load_mode='external' THEN AVG(ps.weight_kg) ELSE "
        "NULL END,CASE WHEN se.load_mode='external' AND COUNT(ps.id)>0 AND "
        "COUNT(ps.id)=COUNT(ps.weight_kg) THEN SUM(ps.reps*ps.weight_kg) ELSE NULL END,"
        "SUM(c.duration_seconds),SUM(c.distance_km),AVG(c.speed_kmh),m.max_weight_kg FROM "
        "session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON "
        "e.id=se.exercise_row_id LEFT JOIN performed_sets ps ON ps.session_exercise_row_id=se.id "
        "LEFT JOIN continuous_activity c ON c.session_exercise_row_id=se.id LEFT JOIN max_results "
        "m ON m.session_exercise_row_id=se.id WHERE e.exercise_id=?3 AND (?1 IS NULL OR "
        "unixepoch(s.started_at)>=?1) AND unixepoch(s.started_at)<=?2 AND (ps.id IS NOT NULL OR "
        "c.id IS NOT NULL OR m.session_exercise_row_id IS NOT NULL) GROUP BY se.id ORDER BY "
        "unixepoch(s.started_at) DESC,s.session_id DESC LIMIT ?4";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *detail = yyjson_mut_obj(document);
    yyjson_mut_val *points = yyjson_mut_arr(document);
    size_t count = 0U;
    int step;
    if (detail == NULL || points == NULL) {
        return false;
    }
    if (exercise_id[0] == '\0') {
        return yyjson_mut_obj_add_null(document, root, "exercise");
    }
    if (!prepare(database, PROFILE_SQL, &statement) ||
        sqlite3_bind_text(statement, 1, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    step = sqlite3_step(statement);
    if (step == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return yyjson_mut_obj_add_null(document, root, "exercise");
    }
    if (step != SQLITE_ROW ||
        !yyjson_mut_obj_add_strcpy(document, detail, "exercise_id", exercise_id) ||
        !yyjson_mut_obj_add_strcpy(
            document, detail, "name", (const char *)sqlite3_column_text(statement, 0)) ||
        !yyjson_mut_obj_add_strcpy(
            document, detail, "tracking_mode", (const char *)sqlite3_column_text(statement, 1)) ||
        !yyjson_mut_obj_add_strcpy(
            document, detail, "recording_mode", (const char *)sqlite3_column_text(statement, 2)) ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return false;
    }
    statement = NULL;
    if (!prepare(database, POINT_SQL, &statement) || !bind_interval(statement, query) ||
        sqlite3_bind_text(statement, 3, exercise_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 4, TRAINLOG_WEB_ANALYSIS_POINTS_MAX + 1U) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        yyjson_mut_val *point;
        if (count == TRAINLOG_WEB_ANALYSIS_POINTS_MAX) {
            *partial = true;
            continue;
        }
        point = yyjson_mut_obj(document);
        if (point == NULL ||
            !yyjson_mut_obj_add_strcpy(
                document, point, "session_id", (const char *)sqlite3_column_text(statement, 0)) ||
            !yyjson_mut_obj_add_strcpy(
                document, point, "timestamp", (const char *)sqlite3_column_text(statement, 1)) ||
            !yyjson_mut_obj_add_strcpy(
                document, point, "load_mode", (const char *)sqlite3_column_text(statement, 2)) ||
            !yyjson_mut_obj_add_sint(document, point, "sets", sqlite3_column_int64(statement, 3)) ||
            !add_nullable_real(document, point, "reps", statement, 4) ||
            !add_nullable_real(document, point, "set_duration_seconds", statement, 5) ||
            !add_nullable_real(document, point, "external_load_kg", statement, 6) ||
            !add_nullable_real(document, point, "external_volume_kg", statement, 7) ||
            !add_nullable_real(document, point, "continuous_duration_seconds", statement, 8) ||
            !add_nullable_real(document, point, "distance_km", statement, 9) ||
            !add_nullable_real(document, point, "speed_kmh", statement, 10) ||
            !add_nullable_real(document, point, "explicit_max_kg", statement, 11) ||
            !yyjson_mut_arr_add_val(points, point)) {
            (void)sqlite3_finalize(statement);
            return false;
        }
        ++count;
    }
    return step == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK &&
           yyjson_mut_obj_add_val(document, detail, "points", points) &&
           yyjson_mut_obj_add_val(document, root, "exercise", detail);
}

static bool add_zones(TrainlogDatabase *database,
                      const TrainlogWebAnalysisQuery *query,
                      yyjson_mut_doc *document,
                      yyjson_mut_val *root) {
    static const char SQL[] =
        "SELECT z.zone_id,COUNT(DISTINCT se.id),COUNT(DISTINCT ps.id) FROM sessions s JOIN "
        "session_exercises se ON se.session_row_id=s.id JOIN exercise_body_zones z ON "
        "z.exercise_row_id=se.exercise_row_id AND z.role='primary' LEFT JOIN performed_sets ps ON "
        "ps.session_exercise_row_id=se.id WHERE (?1 IS NULL OR unixepoch(s.started_at)>=?1) AND "
        "unixepoch(s.started_at)<=?2 AND (ps.id IS NOT NULL OR EXISTS(SELECT 1 FROM "
        "continuous_activity c WHERE c.session_exercise_row_id=se.id) OR EXISTS(SELECT 1 FROM "
        "max_results m WHERE m.session_exercise_row_id=se.id)) GROUP BY z.zone_id ORDER BY "
        "COUNT(DISTINCT se.id) DESC,z.zone_id";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *zones = yyjson_mut_arr(document);
    int step;
    if (zones == NULL || !prepare(database, SQL, &statement) || !bind_interval(statement, query)) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(statement, 0);
        const TrainlogBodyZone *zone = trainlog_body_zone_catalog_lookup(id);
        yyjson_mut_val *item = yyjson_mut_obj(document);
        if (zone == NULL || item == NULL ||
            !yyjson_mut_obj_add_strcpy(document, item, "zone_id", id) ||
            !yyjson_mut_obj_add_strcpy(document, item, "label", zone->display_name) ||
            !yyjson_mut_obj_add_sint(
                document, item, "exposures", sqlite3_column_int64(statement, 1)) ||
            !yyjson_mut_obj_add_sint(
                document, item, "associated_sets", sqlite3_column_int64(statement, 2)) ||
            !yyjson_mut_arr_add_val(zones, item)) {
            (void)sqlite3_finalize(statement);
            return false;
        }
    }
    return step == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK &&
           yyjson_mut_obj_add_val(document, root, "body_zones", zones);
}

static bool add_measurements(TrainlogDatabase *database,
                             const TrainlogWebAnalysisQuery *query,
                             yyjson_mut_doc *document,
                             yyjson_mut_val *root,
                             bool *partial) {
    yyjson_mut_val *measurements = yyjson_mut_obj(document);
    yyjson_mut_val *summaries = yyjson_mut_arr(document);
    yyjson_mut_val *points = yyjson_mut_arr(document);
    const MeasurementDefinition *selected = measurement_definition(query->measurement_metric);
    size_t index;
    if (measurements == NULL || summaries == NULL || points == NULL || selected == NULL) {
        return false;
    }
    for (index = 0U; index < sizeof(MEASUREMENTS) / sizeof(MEASUREMENTS[0]); ++index) {
        char sql[512];
        sqlite3_stmt *statement = NULL;
        yyjson_mut_val *summary = yyjson_mut_obj(document);
        int written = snprintf(
            sql,
            sizeof(sql),
            "WITH filtered AS (SELECT observed_at,observation_id,%s AS value FROM "
            "body_observations WHERE %s IS NOT NULL AND (?1 IS NULL OR "
            "unixepoch(observed_at)>=?1) AND unixepoch(observed_at)<=?2) SELECT COUNT(*),"
            "(SELECT value FROM filtered ORDER BY unixepoch(observed_at),observation_id LIMIT 1),"
            "(SELECT value FROM filtered ORDER BY unixepoch(observed_at) DESC,observation_id DESC "
            "LIMIT 1) FROM filtered",
            MEASUREMENTS[index].column,
            MEASUREMENTS[index].column);
        if (written < 0 || (size_t)written >= sizeof(sql) || summary == NULL ||
            !prepare(database, sql, &statement) || !bind_interval(statement, query) ||
            sqlite3_step(statement) != SQLITE_ROW ||
            !yyjson_mut_obj_add_strcpy(document, summary, "metric", MEASUREMENTS[index].id) ||
            !yyjson_mut_obj_add_strcpy(document, summary, "unit", MEASUREMENTS[index].unit) ||
            !yyjson_mut_obj_add_sint(
                document, summary, "count", sqlite3_column_int64(statement, 0)) ||
            !add_nullable_real(document, summary, "first", statement, 1) ||
            !add_nullable_real(document, summary, "last", statement, 2) ||
            !(sqlite3_column_int64(statement, 0) < 2
                  ? yyjson_mut_obj_add_null(document, summary, "delta")
                  : yyjson_mut_obj_add_real(document,
                                            summary,
                                            "delta",
                                            sqlite3_column_double(statement, 2) -
                                                sqlite3_column_double(statement, 1))) ||
            !yyjson_mut_arr_add_val(summaries, summary) || sqlite3_step(statement) != SQLITE_DONE ||
            sqlite3_finalize(statement) != SQLITE_OK) {
            if (statement != NULL) {
                (void)sqlite3_finalize(statement);
            }
            return false;
        }
    }
    {
        char sql[512];
        sqlite3_stmt *statement = NULL;
        size_t count = 0U;
        int step;
        int written = snprintf(sql,
                               sizeof(sql),
                               "SELECT observed_at,%s FROM body_observations WHERE %s IS NOT NULL "
                               "AND (?1 IS NULL OR unixepoch(observed_at)>=?1) AND "
                               "unixepoch(observed_at)<=?2 ORDER BY unixepoch(observed_at) DESC,"
                               "observation_id DESC LIMIT ?3",
                               selected->column,
                               selected->column);
        if (written < 0 || (size_t)written >= sizeof(sql) || !prepare(database, sql, &statement) ||
            !bind_interval(statement, query) ||
            sqlite3_bind_int64(statement, 3, TRAINLOG_WEB_ANALYSIS_POINTS_MAX + 1U) != SQLITE_OK) {
            if (statement != NULL) {
                (void)sqlite3_finalize(statement);
            }
            return false;
        }
        while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
            yyjson_mut_val *point;
            if (count == TRAINLOG_WEB_ANALYSIS_POINTS_MAX) {
                *partial = true;
                continue;
            }
            point = yyjson_mut_obj(document);
            if (point == NULL ||
                !yyjson_mut_obj_add_strcpy(document,
                                           point,
                                           "timestamp",
                                           (const char *)sqlite3_column_text(statement, 0)) ||
                !yyjson_mut_obj_add_real(
                    document, point, "value", sqlite3_column_double(statement, 1)) ||
                !yyjson_mut_arr_add_val(points, point)) {
                (void)sqlite3_finalize(statement);
                return false;
            }
            ++count;
        }
        if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
            return false;
        }
    }
    return yyjson_mut_obj_add_val(document, measurements, "summaries", summaries) &&
           yyjson_mut_obj_add_strcpy(document, measurements, "selected_metric", selected->id) &&
           yyjson_mut_obj_add_strcpy(document, measurements, "unit", selected->unit) &&
           yyjson_mut_obj_add_val(document, measurements, "points", points) &&
           yyjson_mut_obj_add_val(document, root, "measurements", measurements);
}

static bool
add_program(TrainlogDatabase *database, yyjson_mut_doc *document, yyjson_mut_val *root) {
    static const char SQL[] =
        "SELECT p.program_id,p.title,COUNT(ps.program_session_id),"
        "SUM(CASE WHEN pe.state='completed' THEN 1 ELSE 0 END),"
        "(SELECT ps2.title FROM program_sessions ps2 LEFT JOIN program_session_executions pe2 ON "
        "pe2.program_session_id=ps2.program_session_id WHERE ps2.program_id=p.program_id AND "
        "COALESCE(pe2.state,'todo') NOT IN('completed','deleted') ORDER BY "
        "CASE WHEN ps2.planned_for IS NULL THEN 1 ELSE 0 END,ps2.planned_for,ps2.position LIMIT 1) "
        "FROM programs p LEFT JOIN program_sessions ps ON ps.program_id=p.program_id LEFT JOIN "
        "program_session_executions pe ON pe.program_session_id=ps.program_session_id WHERE "
        "p.state='active' AND p.deleted_at IS NULL GROUP BY p.program_id ORDER BY p.updated_at "
        "DESC "
        "LIMIT 1";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *program = yyjson_mut_obj(document);
    int step;
    if (program == NULL || !prepare(database, SQL, &statement)) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    step = sqlite3_step(statement);
    if (step == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return yyjson_mut_obj_add_null(document, root, "active_program");
    }
    if (step != SQLITE_ROW ||
        !yyjson_mut_obj_add_strcpy(
            document, program, "program_id", (const char *)sqlite3_column_text(statement, 0)) ||
        !yyjson_mut_obj_add_strcpy(
            document, program, "title", (const char *)sqlite3_column_text(statement, 1)) ||
        !yyjson_mut_obj_add_sint(
            document, program, "total_sessions", sqlite3_column_int64(statement, 2)) ||
        !yyjson_mut_obj_add_sint(
            document, program, "completed_sessions", sqlite3_column_int64(statement, 3)) ||
        !(sqlite3_column_type(statement, 4) == SQLITE_NULL
              ? yyjson_mut_obj_add_null(document, program, "next_session_title")
              : yyjson_mut_obj_add_strcpy(document,
                                          program,
                                          "next_session_title",
                                          (const char *)sqlite3_column_text(statement, 4))) ||
        !yyjson_mut_obj_add_val(document, root, "active_program", program) ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return false;
    }
    return true;
}

TrainlogStatus trainlog_web_analysis_json(TrainlogDatabase *database,
                                          const TrainlogWebAnalysisQuery *query,
                                          char *output,
                                          size_t capacity,
                                          size_t *output_size) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *meta;
    char selected_id[64] = "";
    bool partial = false;
    char *json;
    size_t json_size;
    yyjson_write_err error;
    if (database == NULL || query == NULL || output == NULL || capacity == 0U ||
        output_size == NULL || query->reference_unix_second < 0 ||
        (query->period != TRAINLOG_WEB_ANALYSIS_7_DAYS &&
         query->period != TRAINLOG_WEB_ANALYSIS_14_DAYS &&
         query->period != TRAINLOG_WEB_ANALYSIS_21_DAYS &&
         query->period != TRAINLOG_WEB_ANALYSIS_30_DAYS &&
         query->period != TRAINLOG_WEB_ANALYSIS_90_DAYS &&
         query->period != TRAINLOG_WEB_ANALYSIS_ALL) ||
        query->measurement_metric == NULL ||
        measurement_definition(query->measurement_metric) == NULL ||
        (query->exercise_id != NULL && strlen(query->exercise_id) >= sizeof(selected_id))) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_size = 0U;
    output[0] = '\0';
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    meta = document == NULL ? NULL : yyjson_mut_obj(document);
    if (document == NULL || root == NULL || meta == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_strcpy(document,
                                   root,
                                   "period",
                                   query->period == TRAINLOG_WEB_ANALYSIS_ALL       ? "all"
                                   : query->period == TRAINLOG_WEB_ANALYSIS_7_DAYS  ? "7d"
                                   : query->period == TRAINLOG_WEB_ANALYSIS_14_DAYS ? "14d"
                                   : query->period == TRAINLOG_WEB_ANALYSIS_21_DAYS ? "21d"
                                   : query->period == TRAINLOG_WEB_ANALYSIS_30_DAYS ? "30d"
                                                                                    : "90d") ||
        !add_overview(database, query, document, root) ||
        !add_activity(database, query, document, root, &partial) ||
        !add_exercises(database, query, document, root, selected_id, &partial) ||
        !add_exercise_detail(database, query, selected_id, document, root, &partial) ||
        !add_zones(database, query, document, root) ||
        !add_measurements(database, query, document, root, &partial) ||
        !add_program(database, document, root) ||
        !yyjson_mut_obj_add_bool(document, meta, "partial", partial) ||
        !yyjson_mut_obj_add_sint(
            document, meta, "reference_unix_second", query->reference_unix_second) ||
        !yyjson_mut_obj_add_val(document, root, "meta", meta)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    json = yyjson_mut_write_opts(document, YYJSON_WRITE_NOFLAG, NULL, &json_size, &error);
    yyjson_mut_doc_free(document);
    if (json == NULL || json_size + 2U > capacity) {
        free(json);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    (void)memcpy(output, json, json_size);
    output[json_size] = '\n';
    output[json_size + 1U] = '\0';
    *output_size = json_size + 1U;
    free(json);
    return TRAINLOG_STATUS_OK;
}
