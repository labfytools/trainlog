#include "trainlog/web_dashboard.h"
#include "json_writer.h"

#include <string.h>

#define RAW(value)                                                                                 \
    do {                                                                                           \
        if (!trainlog_json_raw(&w, (value)))                                                       \
            goto overflow;                                                                         \
    } while (0)
#define STR(value)                                                                                 \
    do {                                                                                           \
        if (!trainlog_json_string(&w, (value)))                                                    \
            goto overflow;                                                                         \
    } while (0)
#define NUM(value)                                                                                 \
    do {                                                                                           \
        if (!trainlog_json_size(&w, (value)))                                                      \
            goto overflow;                                                                         \
    } while (0)

static const char *tracking_name(TrainlogTrackingMode value) {
    return value == TRAINLOG_TRACKING_DURATION ? "duration" : "reps";
}
static const char *load_name(TrainlogLoadMode value) {
    if (value == TRAINLOG_LOAD_EXTERNAL) {
        return "external";
    }
    if (value == TRAINLOG_LOAD_ASSISTANCE) {
        return "assistance";
    }
    return "none";
}
static bool point_improved(const TrainlogDashboardSnapshot *p, size_t i) {
    const TrainlogExercisePerformancePoint *a, *b;
    if (i == 0U) {
        return false;
    }
    a = &p->performance[i - 1U];
    b = &p->performance[i];
    if (!a->has_weight && !b->has_weight) {
        return b->metric_value > a->metric_value;
    }
    if (a->has_weight != b->has_weight) {
        return false;
    }
    if (p->performance_load_mode == TRAINLOG_LOAD_ASSISTANCE) {
        return b->weight_kg < a->weight_kg ||
               (b->weight_kg == a->weight_kg && b->metric_value > a->metric_value);
    }
    return b->weight_kg > a->weight_kg ||
           (b->weight_kg == a->weight_kg && b->metric_value > a->metric_value);
}

static bool write_zone(TrainlogJsonWriter *w0, const TrainlogWebWorkedZone *z) {
    TrainlogJsonWriter w = *w0;
    RAW("{\"zone_id\":");
    STR(z->zone_id);
    RAW(",\"label\":");
    STR(z->label);
    RAW(",\"session_count\":");
    NUM(z->session_count);
    RAW(",\"occurrence_count\":");
    NUM(z->occurrence_count);
    RAW(",\"set_count\":");
    NUM(z->set_count);
    RAW("}");
    *w0 = w;
    return true;
overflow:
    *w0 = w;
    return false;
}

TrainlogStatus trainlog_web_dashboard_serialize(const TrainlogWebDashboardSnapshot *s,
                                                char *out,
                                                size_t cap,
                                                size_t *out_size) {
    TrainlogJsonWriter w;
    size_t i;
    if (s == NULL || out == NULL || out_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *out_size = 0U;
    trainlog_json_init(&w, out, cap);
    RAW("{\"api_version\":1,\"data\":{\"footer\":{\"user\":");
    STR(s->user_display_name);
    RAW(",\"last_session_date\":");
    if (s->last_session.available) {
        char date[11];
        (void)memcpy(date, s->last_session.started_at, 10U);
        date[10] = '\0';
        STR(date);
    } else {
        RAW("null");
    }
    RAW(",\"last_zones\":[");
    for (i = 0U; i < s->last_session.zone_count; ++i) {
        if (i > 0U) {
            RAW(",");
        }
        STR(s->last_session.zones[i].label);
    }
    RAW("]},");
    RAW("\"next_session\":{\"available\":false,\"reason\":");
    STR(s->next_session_reason);
    RAW("},");
    RAW("\"activity\":{\"available\":true,\"window_days\":90,\"days\":[");
    for (i = 0U; i < s->activity_count; ++i) {
        const TrainlogWebActivityDay *d = &s->activity[i];
        if (i > 0U) {
            RAW(",");
        }
        RAW("{\"date\":");
        STR(d->date);
        RAW(",\"active\":");
        RAW(d->active ? "true" : "false");
        RAW(",\"session_count\":");
        NUM(d->session_count);
        RAW(",\"set_count\":");
        NUM(d->set_count);
        RAW("}");
    }
    RAW("]},");
    RAW("\"progression\":{");
    if (!s->progression.has_performance) {
        RAW("\"available\":false,\"reason\":\"no_comparable_performance\"");
    } else {
        RAW("\"available\":true,\"identity\":{\"exercise_id\":");
        STR(s->progression.exercise.exercise_id);
        RAW(",\"exercise_name\":");
        STR(s->progression.exercise.name);
        RAW(",\"equipment_id\":");
        STR(s->progression.performance_equipment_id);
        RAW(",\"equipment_label\":");
        STR(s->progression.performance_equipment_label);
        RAW(",\"tracking_mode\":");
        STR(tracking_name(s->progression.performance_tracking_mode));
        RAW(",\"load_mode\":");
        STR(load_name(s->progression.performance_load_mode));
        RAW(",\"dose\":");
        if (!trainlog_json_i64(&w, (long long)s->progression.performance_dose)) {
            goto overflow;
        }
        RAW("},\"points\":[");
        for (i = 0U; i < s->progression.performance_count; ++i) {
            const TrainlogExercisePerformancePoint *p = &s->progression.performance[i];
            if (i > 0U) {
                RAW(",");
            }
            RAW("{\"session_id\":");
            STR(p->session_id);
            RAW(",\"timestamp\":");
            STR(p->started_at);
            RAW(",\"metric_value\":");
            if (!trainlog_json_i64(&w, (long long)p->metric_value)) {
                goto overflow;
            }
            RAW(",\"weight_kg\":");
            if (p->has_weight) {
                if (!trainlog_json_double(&w, p->weight_kg)) {
                    goto overflow;
                }
            } else {
                RAW("null");
            }
            RAW(",\"improved\":");
            RAW(point_improved(&s->progression, i) ? "true" : "false");
            RAW("}");
        }
        RAW("]");
    }
    RAW("},");
    RAW("\"last_session\":{");
    if (!s->last_session.available) {
        RAW("\"available\":false,\"reason\":\"no_observable_session\"");
    } else {
        const TrainlogWebLastSession *l = &s->last_session;
        RAW("\"available\":true,\"session_id\":");
        STR(l->session_id);
        RAW(",\"started_at\":");
        STR(l->started_at);
        RAW(",\"ended_at\":");
        if (l->has_ended_at) {
            STR(l->ended_at);
        } else {
            RAW("null");
        }
        RAW(",\"duration_seconds\":");
        if (l->has_duration) {
            if (!trainlog_json_i64(&w, (long long)l->duration_seconds)) {
                goto overflow;
            }
        } else {
            RAW("null");
        }
        RAW(",\"exercise_count\":");
        NUM(l->exercise_count);
        RAW(",\"set_count\":");
        NUM(l->set_count);
        RAW(",\"continuous_count\":");
        NUM(l->continuous_count);
        RAW(",\"max_count\":");
        NUM(l->max_count);
        RAW(",\"primary_zones\":[");
        for (i = 0U; i < l->zone_count; ++i) {
            if (i > 0U) {
                RAW(",");
            }
            if (!write_zone(&w, &l->zones[i])) {
                goto overflow;
            }
        }
        RAW("]");
    }
    RAW("},");
    RAW("\"max_records\":{\"available\":");
    RAW(s->max_record_count > 0U ? "true" : "false");
    if (s->max_record_count == 0U) {
        RAW(",\"reason\":\"no_explicit_max_results\"");
    }
    RAW(",\"records\":[");
    for (i = 0U; i < s->max_record_count; ++i) {
        const TrainlogWebMaxRecord *r = &s->max_records[i];
        if (i > 0U) {
            RAW(",");
        }
        RAW("{\"session_id\":");
        STR(r->session_id);
        RAW(",\"entry_id\":");
        STR(r->entry_id);
        RAW(",\"exercise_id\":");
        STR(r->exercise_id);
        RAW(",\"exercise_name\":");
        STR(r->exercise_name);
        RAW(",\"equipment_id\":");
        STR(r->equipment_id);
        RAW(",\"weight_kg\":");
        if (!trainlog_json_double(&w, r->weight_kg)) {
            goto overflow;
        }
        RAW(",\"timestamp\":");
        STR(r->timestamp);
        RAW("}");
    }
    RAW("]},");
    RAW("\"muscle_distribution\":{\"available\":");
    RAW(s->muscle_zone_count > 0U ? "true" : "false");
    if (s->muscle_zone_count == 0U) {
        RAW(",\"reason\":\"no_worked_primary_zones\"");
    }
    RAW(",\"window_days\":30,\"primary_zones\":[");
    for (i = 0U; i < s->muscle_zone_count; ++i) {
        if (i > 0U) {
            RAW(",");
        }
        if (!write_zone(&w, &s->muscle_zones[i])) {
            goto overflow;
        }
    }
    RAW("]},");
    RAW("\"cardio\":{\"available\":false,\"reason\":");
    STR(s->cardio_reason);
    RAW("}},\"meta\":{\"partial\":");
    RAW(s->partial ? "true" : "false");
    RAW(",\"invalid_data\":");
    RAW(s->invalid_data ? "true" : "false");
    RAW(",\"generated_at\":");
    STR(s->generated_at);
    RAW("}}\n");
    if (w.failed) {
        goto overflow;
    }
    *out_size = w.size;
    return TRAINLOG_STATUS_OK;
overflow:
    if (cap > 0U) {
        out[0] = '\0';
    }
    return TRAINLOG_STATUS_SYSTEM_ERROR;
}
