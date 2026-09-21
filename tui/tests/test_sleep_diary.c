#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trainlog/database.h"
#include "trainlog/id.h"
#include "trainlog/sleep_diary.h"
#include "trainlog/web_sleep.h"

#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #value);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static TrainlogSleepDiaryEntry sample(void) {
    TrainlogSleepDiaryEntry entry = {0};
    (void)snprintf(entry.night_start_date, sizeof(entry.night_start_date), "2026-10-24");
    (void)snprintf(entry.night_end_date, sizeof(entry.night_end_date), "2026-10-25");
    (void)snprintf(entry.created_at, sizeof(entry.created_at), "2026-10-25T08:10:00+01:00");
    (void)snprintf(entry.updated_at, sizeof(entry.updated_at), "%s", entry.created_at);
    entry.sleep_quality = TRAINLOG_SLEEP_QUALITY_B;
    entry.wake_quality = TRAINLOG_SLEEP_QUALITY_MOY;
    (void)snprintf(
        entry.treatment_and_notes, sizeof(entry.treatment_and_notes), "Synthetic fixture");
    entry.event_count = 4U;
    (void)trainlog_id_generate("sle", entry.events[0].event_id, sizeof(entry.events[0].event_id));
    entry.events[0].type = TRAINLOG_SLEEP_EVENT_BED_TIME;
    (void)snprintf(
        entry.events[0].start_at, sizeof(entry.events[0].start_at), "2026-10-24T23:30:00+02:00");
    (void)trainlog_id_generate("sle", entry.events[1].event_id, sizeof(entry.events[1].event_id));
    entry.events[1].type = TRAINLOG_SLEEP_EVENT_SLEEP;
    (void)snprintf(
        entry.events[1].start_at, sizeof(entry.events[1].start_at), "2026-10-25T00:30:00+02:00");
    (void)snprintf(
        entry.events[1].end_at, sizeof(entry.events[1].end_at), "2026-10-25T02:30:00+02:00");
    (void)trainlog_id_generate("sle", entry.events[2].event_id, sizeof(entry.events[2].event_id));
    entry.events[2].type = TRAINLOG_SLEEP_EVENT_LONG_AWAKE;
    (void)snprintf(
        entry.events[2].start_at, sizeof(entry.events[2].start_at), "2026-10-25T02:30:00+02:00");
    (void)snprintf(
        entry.events[2].end_at, sizeof(entry.events[2].end_at), "2026-10-25T02:30:00+01:00");
    (void)trainlog_id_generate("sle", entry.events[3].event_id, sizeof(entry.events[3].event_id));
    entry.events[3].type = TRAINLOG_SLEEP_EVENT_FINAL_GET_UP;
    (void)snprintf(
        entry.events[3].start_at, sizeof(entry.events[3].start_at), "2026-10-25T08:00:00+01:00");
    return entry;
}

static TrainlogStatus count_visitor(void *context, const TrainlogSleepDiaryEntry *entry) {
    size_t *count = context;
    if (entry == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    ++*count;
    return TRAINLOG_STATUS_OK;
}

int main(void) {
    TrainlogDatabase *database = NULL;
    TrainlogSleepDiaryEntry entry = sample();
    TrainlogSleepDiaryEntry loaded;
    TrainlogSleepDiaryEntry stale;
    TrainlogSleepDiaryEntry second;
    char *json = NULL;
    size_t json_size = 0U;
    char first_revision[TRAINLOG_SLEEP_REVISION_ID_CAPACITY];
    char deleted_revision[TRAINLOG_SLEEP_REVISION_ID_CAPACITY];
    size_t count = 0U;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_sleep_diary_validate(&entry));
    CHECK(trainlog_sleep_diary_create(database, &entry) == TRAINLOG_STATUS_OK);
    second = sample();
    (void)snprintf(second.night_start_date, sizeof(second.night_start_date), "2026-10-25");
    (void)snprintf(second.night_end_date, sizeof(second.night_end_date), "2026-10-26");
    (void)snprintf(
        second.events[0].start_at, sizeof(second.events[0].start_at), "2026-10-26T00:30:00+01:00");
    CHECK(trainlog_sleep_diary_create(database, &second) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_web_sleep_list_json(database, NULL, NULL, 7U, &json, &json_size) ==
          TRAINLOG_STATUS_OK);
    CHECK(json_size > 0U && strstr(json, "\"average_bed_minute\":360") != NULL);
    free(json);
    json = NULL;
    CHECK(trainlog_sleep_diary_get(database, entry.entry_id, false, &loaded) == TRAINLOG_STATUS_OK);
    CHECK(loaded.event_count == 4U && loaded.sleep_quality == TRAINLOG_SLEEP_QUALITY_B);
    (void)snprintf(first_revision, sizeof(first_revision), "%s", loaded.revision_id);
    stale = loaded;
    loaded.day_form = TRAINLOG_SLEEP_QUALITY_TB;
    (void)snprintf(loaded.updated_at, sizeof(loaded.updated_at), "2026-10-25T19:00:00+01:00");
    CHECK(trainlog_sleep_diary_update(database, first_revision, &loaded) == TRAINLOG_STATUS_OK);
    stale.day_form = TRAINLOG_SLEEP_QUALITY_TM;
    (void)snprintf(stale.updated_at, sizeof(stale.updated_at), "2026-10-25T20:00:00+01:00");
    CHECK(trainlog_sleep_diary_update(database, first_revision, &stale) ==
          TRAINLOG_STATUS_CONFLICT);
    CHECK(trainlog_sleep_diary_list(
              database, "2026-10-24", "2026-10-24", 7U, count_visitor, &count) ==
              TRAINLOG_STATUS_OK &&
          count == 1U);
    CHECK(trainlog_sleep_diary_delete(database,
                                      entry.entry_id,
                                      loaded.revision_id,
                                      "2026-10-26T09:00:00+01:00",
                                      deleted_revision) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_sleep_diary_get(database, entry.entry_id, false, &loaded) ==
          TRAINLOG_STATUS_NOT_FOUND);
    CHECK(trainlog_sleep_diary_get(database, entry.entry_id, true, &loaded) == TRAINLOG_STATUS_OK &&
          loaded.deleted);
    CHECK(trainlog_sleep_diary_delete(database,
                                      entry.entry_id,
                                      first_revision,
                                      "2026-10-26T10:00:00+01:00",
                                      deleted_revision) != TRAINLOG_STATUS_OK);

    /* Same local clock across the DST fallback is a positive absolute interval. */
    entry = sample();
    (void)snprintf(
        entry.events[2].end_at, sizeof(entry.events[2].end_at), "2026-10-25T02:15:00+02:00");
    CHECK(!trainlog_sleep_diary_validate(&entry));
    trainlog_database_close(database);
    puts("PASS sleep diary domain");
    return 0;
}
