/* Trainlog Sleep Diary V1 domain API. */
#ifndef TRAINLOG_SLEEP_DIARY_H
#define TRAINLOG_SLEEP_DIARY_H

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

#define TRAINLOG_SLEEP_EVENTS_MAX 64U
#define TRAINLOG_SLEEP_NOTES_MAX 16384U
#define TRAINLOG_SLEEP_ENTRY_ID_CAPACITY 40U
#define TRAINLOG_SLEEP_REVISION_ID_CAPACITY 41U
#define TRAINLOG_SLEEP_EVENT_ID_CAPACITY 41U
#define TRAINLOG_SLEEP_TIMESTAMP_CAPACITY 64U

typedef enum TrainlogSleepQuality {
    TRAINLOG_SLEEP_QUALITY_UNSET = 0,
    TRAINLOG_SLEEP_QUALITY_TB,
    TRAINLOG_SLEEP_QUALITY_B,
    TRAINLOG_SLEEP_QUALITY_MOY,
    TRAINLOG_SLEEP_QUALITY_M,
    TRAINLOG_SLEEP_QUALITY_TM
} TrainlogSleepQuality;

typedef enum TrainlogSleepEventType {
    TRAINLOG_SLEEP_EVENT_BED_TIME = 0,
    TRAINLOG_SLEEP_EVENT_FINAL_GET_UP,
    TRAINLOG_SLEEP_EVENT_NIGHT_GET_UP,
    TRAINLOG_SLEEP_EVENT_SLEEP,
    TRAINLOG_SLEEP_EVENT_NAP,
    TRAINLOG_SLEEP_EVENT_LONG_AWAKE,
    TRAINLOG_SLEEP_EVENT_HALF_SLEEP,
    TRAINLOG_SLEEP_EVENT_DAYTIME_SLEEPINESS
} TrainlogSleepEventType;

typedef struct TrainlogSleepEvent {
    char event_id[TRAINLOG_SLEEP_EVENT_ID_CAPACITY];
    TrainlogSleepEventType type;
    char start_at[TRAINLOG_SLEEP_TIMESTAMP_CAPACITY];
    char end_at[TRAINLOG_SLEEP_TIMESTAMP_CAPACITY];
} TrainlogSleepEvent;

typedef struct TrainlogSleepDiaryEntry {
    char entry_id[TRAINLOG_SLEEP_ENTRY_ID_CAPACITY];
    char night_start_date[11];
    char night_end_date[11];
    char created_at[TRAINLOG_SLEEP_TIMESTAMP_CAPACITY];
    char updated_at[TRAINLOG_SLEEP_TIMESTAMP_CAPACITY];
    char revision_id[TRAINLOG_SLEEP_REVISION_ID_CAPACITY];
    char parent_revision_id[TRAINLOG_SLEEP_REVISION_ID_CAPACITY];
    TrainlogSleepQuality sleep_quality;
    TrainlogSleepQuality wake_quality;
    TrainlogSleepQuality day_form;
    char treatment_and_notes[TRAINLOG_SLEEP_NOTES_MAX + 1U];
    TrainlogSleepEvent events[TRAINLOG_SLEEP_EVENTS_MAX];
    size_t event_count;
    bool deleted;
} TrainlogSleepDiaryEntry;

typedef TrainlogStatus (*TrainlogSleepDiaryVisitor)(void *context,
                                                    const TrainlogSleepDiaryEntry *entry);

/* CONTRACT: timestamps are ISO-8601 instants carrying Z or a numeric offset.
 * Intervals are ordered by absolute time, never lexically or by HH:MM. */
bool trainlog_sleep_diary_validate(const TrainlogSleepDiaryEntry *entry);

/* Creates one stable identity and initial immutable revision transactionally. */
TrainlogStatus trainlog_sleep_diary_create(TrainlogDatabase *database,
                                           TrainlogSleepDiaryEntry *entry);

/* Replaces the complete current snapshot iff expected_revision is current.
 * Exact replay of an already-current resulting revision is idempotent. */
TrainlogStatus trainlog_sleep_diary_update(TrainlogDatabase *database,
                                           const char *expected_revision,
                                           TrainlogSleepDiaryEntry *entry);

TrainlogStatus trainlog_sleep_diary_get(TrainlogDatabase *database,
                                        const char *entry_id,
                                        bool include_deleted,
                                        TrainlogSleepDiaryEntry *output);

/* Dates are inclusive YYYY-MM-DD bounds; NULL means unbounded. */
TrainlogStatus trainlog_sleep_diary_list(TrainlogDatabase *database,
                                         const char *start_date,
                                         const char *end_date,
                                         size_t limit,
                                         TrainlogSleepDiaryVisitor visitor,
                                         void *context);

/* Logical deletion is revision guarded and retained for sync replay safety. */
TrainlogStatus
trainlog_sleep_diary_delete(TrainlogDatabase *database,
                            const char *entry_id,
                            const char *expected_revision,
                            const char *deleted_at,
                            char output_revision[TRAINLOG_SLEEP_REVISION_ID_CAPACITY]);

#endif
