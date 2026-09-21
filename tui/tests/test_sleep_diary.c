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

static TrainlogStatus medication_count_visitor(void *context,
                                               const TrainlogMedication *medication) {
    size_t *count = context;
    if (medication == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    ++*count;
    return TRAINLOG_STATUS_OK;
}

static int check_integer_medication_json(void) {
    static const char request[] = "{\"medication_id\":null,\"expected_revision\":null,"
                                  "\"created_at\":\"2026-10-24T18:00:00+02:00\","
                                  "\"updated_at\":\"2026-10-24T18:00:00+02:00\","
                                  "\"name\":\"Synthetic integer dose\",\"default_dose_value\":5,"
                                  "\"default_dose_unit\":\"mg\",\"form\":\"\",\"note\":\"\","
                                  "\"active\":true}";
    TrainlogDatabase *database = NULL;
    char *json = NULL;
    size_t json_size = 0U;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_web_medication_save_json(
              database, request, strlen(request), &json, &json_size) == TRAINLOG_STATUS_OK);
    CHECK(json != NULL && json_size > 0U && strstr(json, "\"medication_id\":\"med_") != NULL);
    free(json);
    trainlog_database_close(database);
    return 0;
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
    size_t medication_count = 0U;
    TrainlogMedication medication = {0};

    CHECK(check_integer_medication_json() == 0);

    (void)snprintf(
        medication.created_at, sizeof(medication.created_at), "2026-10-24T18:00:00+02:00");
    (void)snprintf(
        medication.updated_at, sizeof(medication.updated_at), "%s", medication.created_at);
    (void)snprintf(medication.name, sizeof(medication.name), "Synthetic medication");
    medication.has_default_dose = true;
    medication.default_dose_value = 5.0;
    (void)snprintf(medication.default_dose_unit, sizeof(medication.default_dose_unit), "mg");
    medication.active = true;

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_medication_create(database, &medication) == TRAINLOG_STATUS_OK);
    entry.intake_count = 2U;
    (void)trainlog_id_generate(
        "mdi", entry.intakes[0].intake_id, sizeof(entry.intakes[0].intake_id));
    (void)snprintf(entry.intakes[0].medication_id,
                   sizeof(entry.intakes[0].medication_id),
                   "%s",
                   medication.medication_id);
    (void)snprintf(entry.intakes[0].medication_name,
                   sizeof(entry.intakes[0].medication_name),
                   "%s",
                   medication.name);
    (void)snprintf(
        entry.intakes[0].taken_at, sizeof(entry.intakes[0].taken_at), "2026-10-24T22:30:00+02:00");
    (void)snprintf(entry.intakes[0].created_at,
                   sizeof(entry.intakes[0].created_at),
                   "%s",
                   medication.created_at);
    entry.intakes[0].has_dose = true;
    entry.intakes[0].dose_value = 5.0;
    (void)snprintf(entry.intakes[0].dose_unit, sizeof(entry.intakes[0].dose_unit), "mg");
    entry.intakes[1] = entry.intakes[0];
    (void)trainlog_id_generate(
        "mdi", entry.intakes[1].intake_id, sizeof(entry.intakes[1].intake_id));
    (void)snprintf(
        entry.intakes[1].taken_at, sizeof(entry.intakes[1].taken_at), "2026-10-25T05:15:00+01:00");
    entry.intakes[1].dose_value = 10.0;
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
    CHECK(loaded.event_count == 4U && loaded.intake_count == 2U &&
          loaded.intakes[0].dose_value == 5.0 && loaded.intakes[1].dose_value == 10.0 &&
          loaded.sleep_quality == TRAINLOG_SLEEP_QUALITY_B &&
          loaded.publication_status == TRAINLOG_SLEEP_PUBLICATION_DRAFT);
    CHECK(trainlog_sleep_diary_validate_revision(
              database, entry.entry_id, loaded.revision_id, "2026-10-25T18:00:00+01:00") ==
          TRAINLOG_STATUS_OK);
    CHECK(trainlog_sleep_diary_get(database, entry.entry_id, false, &loaded) ==
              TRAINLOG_STATUS_OK &&
          loaded.publication_status == TRAINLOG_SLEEP_PUBLICATION_READY);
    CHECK(trainlog_sleep_diary_validate_revision(database,
                                                 entry.entry_id,
                                                 "slr_00000000-0000-4000-8000-000000000000",
                                                 "2026-10-25T18:00:00+01:00") ==
          TRAINLOG_STATUS_CONFLICT);
    (void)snprintf(
        medication.updated_at, sizeof(medication.updated_at), "2026-10-26T12:00:00+01:00");
    (void)snprintf(medication.name, sizeof(medication.name), "Renamed medication");
    CHECK(trainlog_medication_update(database, medication.revision_id, &medication) ==
          TRAINLOG_STATUS_OK);
    CHECK(strcmp(loaded.intakes[0].medication_name, "Synthetic medication") == 0);
    medication.active = false;
    (void)snprintf(
        medication.updated_at, sizeof(medication.updated_at), "2026-10-26T13:00:00+01:00");
    CHECK(trainlog_medication_update(database, medication.revision_id, &medication) ==
          TRAINLOG_STATUS_OK);
    CHECK(trainlog_medication_list(
              database, false, 10U, medication_count_visitor, &medication_count) ==
              TRAINLOG_STATUS_OK &&
          medication_count == 0U);
    (void)snprintf(first_revision, sizeof(first_revision), "%s", loaded.revision_id);
    stale = loaded;
    loaded.day_form = TRAINLOG_SLEEP_QUALITY_TB;
    (void)snprintf(loaded.updated_at, sizeof(loaded.updated_at), "2026-10-25T19:00:00+01:00");
    CHECK(trainlog_sleep_diary_update(database, first_revision, &loaded) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_sleep_diary_get(database, entry.entry_id, false, &loaded) ==
              TRAINLOG_STATUS_OK &&
          loaded.publication_status == TRAINLOG_SLEEP_PUBLICATION_DRAFT);
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
