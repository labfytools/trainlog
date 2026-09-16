/* Trainlog Web Dashboard V1 typed read model. */
#ifndef TRAINLOG_WEB_DASHBOARD_H
#define TRAINLOG_WEB_DASHBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/dashboard.h"

#define TRAINLOG_WEB_ACTIVITY_DAYS 90U
#define TRAINLOG_WEB_MAX_RECORDS 8U
#define TRAINLOG_WEB_ZONE_CAPACITY TRAINLOG_DASHBOARD_ZONE_CAPACITY
#define TRAINLOG_WEB_JSON_CAPACITY (128U * 1024U)

typedef struct TrainlogWebActivityDay {
    char date[11];
    bool active;
    size_t session_count;
    size_t set_count;
} TrainlogWebActivityDay;

typedef struct TrainlogWebWorkedZone {
    char zone_id[TRAINLOG_ZONE_ID_MAX + 1U];
    char label[TRAINLOG_NAME_MAX + 1U];
    size_t session_count;
    size_t occurrence_count;
    size_t set_count;
} TrainlogWebWorkedZone;

typedef struct TrainlogWebLastSession {
    bool available;
    char session_id[TRAINLOG_ID_MAX + 1U];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    bool has_ended_at;
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    bool has_duration;
    uint64_t duration_seconds;
    size_t exercise_count;
    size_t set_count;
    size_t continuous_count;
    size_t max_count;
    TrainlogWebWorkedZone zones[TRAINLOG_WEB_ZONE_CAPACITY];
    size_t zone_count;
} TrainlogWebLastSession;

typedef struct TrainlogWebMaxRecord {
    char session_id[TRAINLOG_ID_MAX + 1U];
    char entry_id[TRAINLOG_ID_MAX + 1U];
    char exercise_id[TRAINLOG_ID_MAX + 1U];
    char exercise_name[TRAINLOG_NAME_MAX + 1U];
    char equipment_id[TRAINLOG_ID_MAX + 1U];
    double weight_kg;
    char timestamp[TRAINLOG_TIMESTAMP_MAX + 1U];
} TrainlogWebMaxRecord;

typedef struct TrainlogWebDashboardSnapshot {
    bool partial;
    bool invalid_data;
    char generated_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char user_display_name[TRAINLOG_NAME_MAX + 1U];
    /* CONTRACT: no persisted executable plan exists in schema v18. */
    bool next_session_available;
    const char *next_session_reason;
    TrainlogWebActivityDay activity[TRAINLOG_WEB_ACTIVITY_DAYS];
    size_t activity_count;
    TrainlogDashboardSnapshot progression;
    TrainlogWebLastSession last_session;
    TrainlogWebMaxRecord max_records[TRAINLOG_WEB_MAX_RECORDS];
    size_t max_record_count;
    TrainlogWebWorkedZone muscle_zones[TRAINLOG_WEB_ZONE_CAPACITY];
    size_t muscle_zone_count;
    bool cardio_available;
    const char *cardio_reason;
} TrainlogWebDashboardSnapshot;

typedef struct TrainlogWebDashboardQuery {
    int64_t reference_unix_second;
} TrainlogWebDashboardQuery;

/* WHY: React must consume business facts, never reconstruct them from rows.
 * CONTRACT: all pointers are borrowed for the call; output is fully initialized,
 * bounded, deterministic and retains no SQLite statement. One outer read
 * snapshot covers every domain. reference_unix_second is an inclusive UTC
 * cutoff and also selects the current system-local calendar day.
 * INVARIANT: observable work means a performed set, continuous activity or
 * explicit max_results row; targets/catalogue rows alone never qualify. */
TrainlogStatus trainlog_web_dashboard_load(TrainlogDatabase *database,
    const TrainlogWebDashboardQuery *query,
    TrainlogWebDashboardSnapshot *output);

/* Caller owns output. Success guarantees valid NUL-terminated JSON. */
TrainlogStatus trainlog_web_dashboard_serialize(
    const TrainlogWebDashboardSnapshot *snapshot,
    char *output, size_t capacity, size_t *output_size);

#endif
