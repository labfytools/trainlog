#include "trainlog/web_sleep.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <yyjson.h>

#include "trainlog/id.h"
#include "trainlog/sleep_diary.h"
#include "timestamp.h"

typedef struct SleepListContext {
    yyjson_mut_doc *document;
    yyjson_mut_val *items;
    size_t count;
    int64_t sleep_seconds;
    int64_t long_awake_seconds;
    int64_t nap_seconds;
    size_t long_awake_count;
    size_t nap_count;
    size_t sleepiness_count;
    size_t intake_count;
    size_t quality[3][5];
    int64_t bed_minute_total;
    int64_t get_up_minute_total;
    size_t bed_count;
    size_t get_up_count;
} SleepListContext;

static bool event_duration(const TrainlogSleepEvent *event, int64_t *seconds) {
    TrainlogTimestampKey start;
    TrainlogTimestampKey end;
    if (event->end_at[0] == '\0' ||
        !trainlog_timestamp_parse(event->start_at, strlen(event->start_at), &start) ||
        !trainlog_timestamp_parse(event->end_at, strlen(event->end_at), &end) ||
        end.utc_second <= start.utc_second) {
        return false;
    }
    *seconds = end.utc_second - start.utc_second;
    return true;
}

/* WHY: ordinary HH:MM arithmetic maps 23:30 and 00:30 to noon. CONTRACT:
 * analysis projects clock readings onto the diary's 18:00 -> 18:00 axis.
 * INVARIANT: storage remains an absolute offset-bearing timestamp. */
static bool diary_minute(const char *timestamp, int64_t *minute) {
    const char *separator = strchr(timestamp, 'T');
    int hour;
    int value;
    if (separator == NULL || sscanf(separator + 1, "%2d:%2d", &hour, &value) != 2 || hour < 0 ||
        hour > 23 || value < 0 || value > 59) {
        return false;
    }
    *minute = (int64_t)((hour < 18 ? hour + 24 : hour) * 60 + value) - 18 * 60;
    return true;
}

static const char *quality_text(TrainlogSleepQuality quality) {
    static const char *const values[] = {NULL, "TB", "B", "Moy", "M", "TM"};
    return quality >= TRAINLOG_SLEEP_QUALITY_UNSET && quality <= TRAINLOG_SLEEP_QUALITY_TM
               ? values[quality]
               : NULL;
}

static const char *publication_text(TrainlogSleepPublicationStatus status) {
    static const char *const values[] = {"draft", "ready", "synchronized", "modified"};
    return status >= TRAINLOG_SLEEP_PUBLICATION_DRAFT &&
                   status <= TRAINLOG_SLEEP_PUBLICATION_MODIFIED
               ? values[status]
               : NULL;
}

static bool quality_value(yyjson_val *value, TrainlogSleepQuality *output) {
    int index;
    if (yyjson_is_null(value)) {
        *output = TRAINLOG_SLEEP_QUALITY_UNSET;
        return true;
    }
    if (!yyjson_is_str(value)) {
        return false;
    }
    for (index = 1; index <= (int)TRAINLOG_SLEEP_QUALITY_TM; ++index) {
        if (strcmp(yyjson_get_str(value), quality_text((TrainlogSleepQuality)index)) == 0) {
            *output = (TrainlogSleepQuality)index;
            return true;
        }
    }
    return false;
}

static const char *event_text(TrainlogSleepEventType type) {
    static const char *const values[] = {"bed_time",
                                         "final_get_up",
                                         "night_get_up",
                                         "sleep",
                                         "nap",
                                         "long_awake",
                                         "half_sleep",
                                         "daytime_sleepiness"};
    return type >= TRAINLOG_SLEEP_EVENT_BED_TIME && type <= TRAINLOG_SLEEP_EVENT_DAYTIME_SLEEPINESS
               ? values[type]
               : NULL;
}

static bool event_value(yyjson_val *value, TrainlogSleepEventType *output) {
    int index;
    if (!yyjson_is_str(value)) {
        return false;
    }
    for (index = 0; index <= (int)TRAINLOG_SLEEP_EVENT_DAYTIME_SLEEPINESS; ++index) {
        if (strcmp(yyjson_get_str(value), event_text((TrainlogSleepEventType)index)) == 0) {
            *output = (TrainlogSleepEventType)index;
            return true;
        }
    }
    return false;
}

static bool add_nullable_quality(yyjson_mut_doc *document,
                                 yyjson_mut_val *object,
                                 const char *key,
                                 TrainlogSleepQuality quality) {
    return quality == TRAINLOG_SLEEP_QUALITY_UNSET
               ? yyjson_mut_obj_add_null(document, object, key)
               : yyjson_mut_obj_add_strcpy(document, object, key, quality_text(quality));
}

static bool
add_entry(yyjson_mut_doc *document, yyjson_mut_val *items, const TrainlogSleepDiaryEntry *entry) {
    yyjson_mut_val *object = yyjson_mut_obj(document);
    yyjson_mut_val *events = yyjson_mut_arr(document);
    yyjson_mut_val *intakes = yyjson_mut_arr(document);
    size_t index;
    if (object == NULL || events == NULL || intakes == NULL ||
        !yyjson_mut_obj_add_strcpy(document, object, "entry_id", entry->entry_id) ||
        !yyjson_mut_obj_add_strcpy(document, object, "night_start_date", entry->night_start_date) ||
        !yyjson_mut_obj_add_strcpy(document, object, "night_end_date", entry->night_end_date) ||
        !yyjson_mut_obj_add_strcpy(document, object, "created_at", entry->created_at) ||
        !yyjson_mut_obj_add_strcpy(document, object, "updated_at", entry->updated_at) ||
        !yyjson_mut_obj_add_strcpy(document, object, "revision_id", entry->revision_id) ||
        !yyjson_mut_obj_add_strcpy(
            document, object, "publication_status", publication_text(entry->publication_status)) ||
        !add_nullable_quality(document, object, "sleep_quality", entry->sleep_quality) ||
        !add_nullable_quality(document, object, "wake_quality", entry->wake_quality) ||
        !add_nullable_quality(document, object, "day_form", entry->day_form) ||
        !yyjson_mut_obj_add_strcpy(
            document, object, "treatment_and_notes", entry->treatment_and_notes)) {
        return false;
    }
    for (index = 0U; index < entry->event_count; ++index) {
        const TrainlogSleepEvent *event = &entry->events[index];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        if (item == NULL ||
            !yyjson_mut_obj_add_strcpy(document, item, "event_id", event->event_id) ||
            !yyjson_mut_obj_add_strcpy(document, item, "type", event_text(event->type)) ||
            !yyjson_mut_obj_add_strcpy(document, item, "start_at", event->start_at) ||
            !(event->end_at[0] == '\0'
                  ? yyjson_mut_obj_add_null(document, item, "end_at")
                  : yyjson_mut_obj_add_strcpy(document, item, "end_at", event->end_at)) ||
            !yyjson_mut_arr_add_val(events, item)) {
            return false;
        }
    }
    for (index = 0U; index < entry->intake_count; ++index) {
        const TrainlogMedicationIntake *intake = &entry->intakes[index];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        if (item == NULL ||
            !yyjson_mut_obj_add_strcpy(document, item, "intake_id", intake->intake_id) ||
            !yyjson_mut_obj_add_strcpy(document, item, "medication_id", intake->medication_id) ||
            !yyjson_mut_obj_add_strcpy(
                document, item, "medication_name", intake->medication_name) ||
            !yyjson_mut_obj_add_strcpy(document, item, "taken_at", intake->taken_at) ||
            !(intake->has_dose
                  ? yyjson_mut_obj_add_real(document, item, "dose_value", intake->dose_value)
                  : yyjson_mut_obj_add_null(document, item, "dose_value")) ||
            !(intake->has_dose
                  ? yyjson_mut_obj_add_strcpy(document, item, "dose_unit", intake->dose_unit)
                  : yyjson_mut_obj_add_null(document, item, "dose_unit")) ||
            !yyjson_mut_obj_add_strcpy(document, item, "note", intake->note) ||
            !yyjson_mut_obj_add_strcpy(document, item, "created_at", intake->created_at) ||
            !yyjson_mut_arr_add_val(intakes, item)) {
            return false;
        }
    }
    return yyjson_mut_obj_add_val(document, object, "events", events) &&
           yyjson_mut_obj_add_val(document, object, "intakes", intakes) &&
           yyjson_mut_arr_add_val(items, object);
}

static TrainlogStatus list_visitor(void *opaque, const TrainlogSleepDiaryEntry *entry) {
    SleepListContext *context = opaque;
    size_t index;
    if (!add_entry(context->document, context->items, entry)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    ++context->count;
    context->intake_count += entry->intake_count;
    if (entry->sleep_quality != TRAINLOG_SLEEP_QUALITY_UNSET) {
        ++context->quality[0][entry->sleep_quality - 1];
    }
    if (entry->wake_quality != TRAINLOG_SLEEP_QUALITY_UNSET) {
        ++context->quality[1][entry->wake_quality - 1];
    }
    if (entry->day_form != TRAINLOG_SLEEP_QUALITY_UNSET) {
        ++context->quality[2][entry->day_form - 1];
    }
    for (index = 0U; index < entry->event_count; ++index) {
        const TrainlogSleepEvent *event = &entry->events[index];
        int64_t duration;
        int64_t minute;
        if (event->type == TRAINLOG_SLEEP_EVENT_LONG_AWAKE) {
            ++context->long_awake_count;
        }
        if (event->type == TRAINLOG_SLEEP_EVENT_NAP) {
            ++context->nap_count;
        }
        if (event->type == TRAINLOG_SLEEP_EVENT_DAYTIME_SLEEPINESS) {
            ++context->sleepiness_count;
        }
        if (event_duration(event, &duration)) {
            if (event->type == TRAINLOG_SLEEP_EVENT_SLEEP) {
                context->sleep_seconds += duration;
            }
            if (event->type == TRAINLOG_SLEEP_EVENT_LONG_AWAKE) {
                context->long_awake_seconds += duration;
            }
            if (event->type == TRAINLOG_SLEEP_EVENT_NAP) {
                context->nap_seconds += duration;
            }
        }
        if (event->type == TRAINLOG_SLEEP_EVENT_BED_TIME &&
            diary_minute(event->start_at, &minute)) {
            context->bed_minute_total += minute;
            ++context->bed_count;
        }
        if (event->type == TRAINLOG_SLEEP_EVENT_FINAL_GET_UP &&
            diary_minute(event->start_at, &minute)) {
            context->get_up_minute_total += minute;
            ++context->get_up_count;
        }
    }
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus
write_document(yyjson_mut_doc *document, char **output_json, size_t *output_size) {
    yyjson_write_err error;
    char *json = yyjson_mut_write_opts(document, YYJSON_WRITE_NOFLAG, NULL, output_size, &error);
    yyjson_mut_doc_free(document);
    if (json == NULL || *output_size > TRAINLOG_WEB_SLEEP_BYTES_MAX) {
        free(json);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    *output_json = json;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_sleep_list_json(TrainlogDatabase *database,
                                            const char *start_date,
                                            const char *end_date,
                                            size_t limit,
                                            char **output_json,
                                            size_t *output_size) {
    static const char *const quality_names[] = {"TB", "B", "Moy", "M", "TM"};
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *summary;
    yyjson_mut_val *distribution;
    SleepListContext context = {0};
    TrainlogStatus status;
    size_t category, quality;
    if (database == NULL || output_json == NULL || output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_json = NULL;
    *output_size = 0U;
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    context.items = document == NULL ? NULL : yyjson_mut_arr(document);
    summary = document == NULL ? NULL : yyjson_mut_obj(document);
    distribution = document == NULL ? NULL : yyjson_mut_obj(document);
    if (document == NULL || root == NULL || context.items == NULL || summary == NULL ||
        distribution == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    context.document = document;
    yyjson_mut_doc_set_root(document, root);
    status =
        trainlog_sleep_diary_list(database, start_date, end_date, limit, list_visitor, &context);
    if (status != TRAINLOG_STATUS_OK ||
        !yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_uint(document, summary, "nights", context.count) ||
        !yyjson_mut_obj_add_uint(document, summary, "long_awake_count", context.long_awake_count) ||
        !yyjson_mut_obj_add_uint(document, summary, "nap_count", context.nap_count) ||
        !yyjson_mut_obj_add_uint(document, summary, "sleepiness_count", context.sleepiness_count) ||
        !yyjson_mut_obj_add_uint(document, summary, "intake_count", context.intake_count) ||
        !yyjson_mut_obj_add_sint(
            document, summary, "sleep_duration_seconds", context.sleep_seconds) ||
        !yyjson_mut_obj_add_sint(
            document, summary, "long_awake_duration_seconds", context.long_awake_seconds) ||
        !yyjson_mut_obj_add_sint(document, summary, "nap_duration_seconds", context.nap_seconds) ||
        !(context.bed_count == 0U
              ? yyjson_mut_obj_add_null(document, summary, "average_bed_minute")
              : yyjson_mut_obj_add_sint(document,
                                        summary,
                                        "average_bed_minute",
                                        context.bed_minute_total / (int64_t)context.bed_count)) ||
        !(context.get_up_count == 0U
              ? yyjson_mut_obj_add_null(document, summary, "average_get_up_minute")
              : yyjson_mut_obj_add_sint(document,
                                        summary,
                                        "average_get_up_minute",
                                        context.get_up_minute_total /
                                            (int64_t)context.get_up_count))) {
        yyjson_mut_doc_free(document);
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_SYSTEM_ERROR : status;
    }
    for (category = 0U; category < 3U; ++category) {
        yyjson_mut_val *values = yyjson_mut_obj(document);
        const char *key = category == 0U   ? "sleep_quality"
                          : category == 1U ? "wake_quality"
                                           : "day_form";
        if (values == NULL) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
        for (quality = 0U; quality < 5U; ++quality) {
            if (!yyjson_mut_obj_add_uint(
                    document, values, quality_names[quality], context.quality[category][quality])) {
                yyjson_mut_doc_free(document);
                return TRAINLOG_STATUS_SYSTEM_ERROR;
            }
        }
        if (!yyjson_mut_obj_add_val(document, distribution, key, values)) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    }
    if (!yyjson_mut_obj_add_val(document, summary, "quality_distribution", distribution) ||
        !yyjson_mut_obj_add_val(document, root, "summary", summary) ||
        !yyjson_mut_obj_add_val(document, root, "entries", context.items)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    return write_document(document, output_json, output_size);
}

static bool
copy_string(yyjson_val *object, const char *key, char *output, size_t capacity, bool nullable) {
    yyjson_val *value = yyjson_obj_get(object, key);
    size_t length;
    if (nullable && yyjson_is_null(value)) {
        output[0] = '\0';
        return true;
    }
    if (!yyjson_is_str(value)) {
        return false;
    }
    length = yyjson_get_len(value);
    if (length >= capacity) {
        return false;
    }
    memcpy(output, yyjson_get_str(value), length + 1U);
    return true;
}

static bool parse_entry(const char *body,
                        size_t body_size,
                        TrainlogSleepDiaryEntry *entry,
                        char expected_revision[TRAINLOG_SLEEP_REVISION_ID_CAPACITY]) {
    yyjson_read_err error;
    yyjson_doc *document =
        yyjson_read_opts((char *)body, body_size, YYJSON_READ_NOFLAG, NULL, &error);
    yyjson_val *root = document == NULL ? NULL : yyjson_doc_get_root(document);
    yyjson_val *events;
    yyjson_val *intakes;
    size_t index, count;
    bool valid =
        root != NULL && yyjson_is_obj(root) &&
        copy_string(root, "entry_id", entry->entry_id, sizeof(entry->entry_id), true) &&
        copy_string(root,
                    "expected_revision",
                    expected_revision,
                    TRAINLOG_SLEEP_REVISION_ID_CAPACITY,
                    true) &&
        copy_string(root,
                    "night_start_date",
                    entry->night_start_date,
                    sizeof(entry->night_start_date),
                    false) &&
        copy_string(
            root, "night_end_date", entry->night_end_date, sizeof(entry->night_end_date), false) &&
        copy_string(root, "created_at", entry->created_at, sizeof(entry->created_at), false) &&
        copy_string(root, "updated_at", entry->updated_at, sizeof(entry->updated_at), false) &&
        copy_string(root,
                    "treatment_and_notes",
                    entry->treatment_and_notes,
                    sizeof(entry->treatment_and_notes),
                    false) &&
        quality_value(yyjson_obj_get(root, "sleep_quality"), &entry->sleep_quality) &&
        quality_value(yyjson_obj_get(root, "wake_quality"), &entry->wake_quality) &&
        quality_value(yyjson_obj_get(root, "day_form"), &entry->day_form);
    events = valid ? yyjson_obj_get(root, "events") : NULL;
    count = yyjson_is_arr(events) ? yyjson_arr_size(events) : TRAINLOG_SLEEP_EVENTS_MAX + 1U;
    if (count > TRAINLOG_SLEEP_EVENTS_MAX) {
        valid = false;
    }
    for (index = 0U; valid && index < count; ++index) {
        yyjson_val *value = yyjson_arr_get(events, index);
        TrainlogSleepEvent *event = &entry->events[index];
        valid = yyjson_is_obj(value) &&
                copy_string(value, "event_id", event->event_id, sizeof(event->event_id), true) &&
                event_value(yyjson_obj_get(value, "type"), &event->type) &&
                copy_string(value, "start_at", event->start_at, sizeof(event->start_at), false) &&
                copy_string(value, "end_at", event->end_at, sizeof(event->end_at), true);
        if (valid && event->event_id[0] == '\0') {
            valid = trainlog_id_generate("sle", event->event_id, sizeof(event->event_id)) ==
                    TRAINLOG_STATUS_OK;
        }
    }
    entry->event_count = count;
    intakes = valid ? yyjson_obj_get(root, "intakes") : NULL;
    count = yyjson_is_arr(intakes) ? yyjson_arr_size(intakes) : TRAINLOG_SLEEP_INTAKES_MAX + 1U;
    if (count > TRAINLOG_SLEEP_INTAKES_MAX) {
        valid = false;
    }
    for (index = 0U; valid && index < count; ++index) {
        yyjson_val *value = yyjson_arr_get(intakes, index);
        yyjson_val *dose_value = yyjson_obj_get(value, "dose_value");
        yyjson_val *dose_unit = yyjson_obj_get(value, "dose_unit");
        TrainlogMedicationIntake *intake = &entry->intakes[index];
        valid =
            yyjson_is_obj(value) &&
            copy_string(value, "intake_id", intake->intake_id, sizeof(intake->intake_id), true) &&
            copy_string(value,
                        "medication_id",
                        intake->medication_id,
                        sizeof(intake->medication_id),
                        false) &&
            copy_string(value,
                        "medication_name",
                        intake->medication_name,
                        sizeof(intake->medication_name),
                        false) &&
            copy_string(value, "taken_at", intake->taken_at, sizeof(intake->taken_at), false) &&
            copy_string(value, "note", intake->note, sizeof(intake->note), false) &&
            copy_string(
                value, "created_at", intake->created_at, sizeof(intake->created_at), false) &&
            ((yyjson_is_null(dose_value) && yyjson_is_null(dose_unit)) ||
             (yyjson_is_num(dose_value) && yyjson_get_real(dose_value) > 0.0 &&
              copy_string(
                  value, "dose_unit", intake->dose_unit, sizeof(intake->dose_unit), false)));
        intake->has_dose = yyjson_is_num(dose_value);
        intake->dose_value = intake->has_dose ? yyjson_get_real(dose_value) : 0.0;
        if (valid && intake->intake_id[0] == '\0') {
            valid = trainlog_id_generate("mdi", intake->intake_id, sizeof(intake->intake_id)) ==
                    TRAINLOG_STATUS_OK;
        }
    }
    entry->intake_count = count;
    yyjson_doc_free(document);
    return valid;
}

TrainlogStatus trainlog_web_sleep_save_json(TrainlogDatabase *database,
                                            const char *body,
                                            size_t body_size,
                                            char **output_json,
                                            size_t *output_size) {
    TrainlogSleepDiaryEntry entry = {0};
    char expected[TRAINLOG_SLEEP_REVISION_ID_CAPACITY] = "";
    TrainlogStatus status;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    if (database == NULL || body == NULL || body_size == 0U ||
        body_size > TRAINLOG_WEB_SLEEP_BYTES_MAX || output_json == NULL || output_size == NULL ||
        !parse_entry(body, body_size, &entry, expected)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (expected[0] == '\0') {
        status = trainlog_sleep_diary_create(database, &entry);
    } else {
        (void)snprintf(entry.revision_id, sizeof(entry.revision_id), "%s", expected);
        status = trainlog_sleep_diary_update(database, expected, &entry);
    }
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    if (document == NULL || root == NULL ||
        !yyjson_mut_obj_add_strcpy(document, root, "entry_id", entry.entry_id) ||
        !yyjson_mut_obj_add_strcpy(document, root, "revision_id", entry.revision_id)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    return write_document(document, output_json, output_size);
}

TrainlogStatus trainlog_web_sleep_delete_json(TrainlogDatabase *database,
                                              const char *entry_id,
                                              const char *expected_revision,
                                              const char *deleted_at,
                                              char **output_json,
                                              size_t *output_size) {
    char revision[TRAINLOG_SLEEP_REVISION_ID_CAPACITY];
    TrainlogStatus status =
        trainlog_sleep_diary_delete(database, entry_id, expected_revision, deleted_at, revision);
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    if (document == NULL || root == NULL ||
        !yyjson_mut_obj_add_strcpy(document, root, "entry_id", entry_id) ||
        !yyjson_mut_obj_add_strcpy(document, root, "revision_id", revision) ||
        !yyjson_mut_obj_add_bool(document, root, "deleted", true)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    return write_document(document, output_json, output_size);
}

TrainlogStatus trainlog_web_sleep_delete_request_json(TrainlogDatabase *database,
                                                      const char *body,
                                                      size_t body_size,
                                                      char **output_json,
                                                      size_t *output_size) {
    yyjson_read_err error;
    yyjson_doc *document;
    yyjson_val *root;
    const char *entry_id;
    const char *revision;
    const char *deleted_at;
    TrainlogStatus status;
    if (body == NULL || body_size == 0U || body_size > TRAINLOG_WEB_SLEEP_BYTES_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    document = yyjson_read_opts((char *)body, body_size, YYJSON_READ_NOFLAG, NULL, &error);
    root = document == NULL ? NULL : yyjson_doc_get_root(document);
    entry_id = yyjson_is_obj(root) && yyjson_is_str(yyjson_obj_get(root, "entry_id"))
                   ? yyjson_get_str(yyjson_obj_get(root, "entry_id"))
                   : NULL;
    revision = yyjson_is_obj(root) && yyjson_is_str(yyjson_obj_get(root, "expected_revision"))
                   ? yyjson_get_str(yyjson_obj_get(root, "expected_revision"))
                   : NULL;
    deleted_at = yyjson_is_obj(root) && yyjson_is_str(yyjson_obj_get(root, "deleted_at"))
                     ? yyjson_get_str(yyjson_obj_get(root, "deleted_at"))
                     : NULL;
    if (entry_id == NULL || revision == NULL || deleted_at == NULL || yyjson_obj_size(root) != 3U) {
        yyjson_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    status = trainlog_web_sleep_delete_json(
        database, entry_id, revision, deleted_at, output_json, output_size);
    yyjson_doc_free(document);
    return status;
}

TrainlogStatus trainlog_web_sleep_validate_json(TrainlogDatabase *database,
                                                const char *body,
                                                size_t body_size,
                                                char **output_json,
                                                size_t *output_size) {
    yyjson_read_err error;
    yyjson_doc *input;
    yyjson_val *root;
    const char *entry_id;
    const char *revision;
    const char *validated_at;
    char entry_id_copy[TRAINLOG_SLEEP_ENTRY_ID_CAPACITY];
    char revision_copy[TRAINLOG_SLEEP_REVISION_ID_CAPACITY];
    TrainlogStatus status;
    yyjson_mut_doc *output;
    yyjson_mut_val *object;
    if (database == NULL || body == NULL || body_size == 0U ||
        body_size > TRAINLOG_WEB_SLEEP_BYTES_MAX || output_json == NULL || output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    input = yyjson_read_opts((char *)body, body_size, YYJSON_READ_NOFLAG, NULL, &error);
    root = input == NULL ? NULL : yyjson_doc_get_root(input);
    entry_id = yyjson_is_obj(root) && yyjson_is_str(yyjson_obj_get(root, "entry_id"))
                   ? yyjson_get_str(yyjson_obj_get(root, "entry_id"))
                   : NULL;
    revision = yyjson_is_obj(root) && yyjson_is_str(yyjson_obj_get(root, "expected_revision"))
                   ? yyjson_get_str(yyjson_obj_get(root, "expected_revision"))
                   : NULL;
    validated_at = yyjson_is_obj(root) && yyjson_is_str(yyjson_obj_get(root, "validated_at"))
                       ? yyjson_get_str(yyjson_obj_get(root, "validated_at"))
                       : NULL;
    if (entry_id == NULL || revision == NULL || validated_at == NULL ||
        yyjson_obj_size(root) != 3U) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    status = trainlog_sleep_diary_validate_revision(database, entry_id, revision, validated_at);
    (void)snprintf(entry_id_copy, sizeof(entry_id_copy), "%s", entry_id);
    (void)snprintf(revision_copy, sizeof(revision_copy), "%s", revision);
    yyjson_doc_free(input);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    output = yyjson_mut_doc_new(NULL);
    object = output == NULL ? NULL : yyjson_mut_obj(output);
    if (output == NULL || object == NULL ||
        !yyjson_mut_obj_add_strcpy(output, object, "entry_id", entry_id_copy) ||
        !yyjson_mut_obj_add_strcpy(output, object, "revision_id", revision_copy) ||
        !yyjson_mut_obj_add_strcpy(output, object, "publication_status", "ready")) {
        yyjson_mut_doc_free(output);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(output, object);
    return write_document(output, output_json, output_size);
}

typedef struct MedicationListContext {
    yyjson_mut_doc *document;
    yyjson_mut_val *items;
} MedicationListContext;

static TrainlogStatus medication_list_visitor(void *opaque, const TrainlogMedication *medication) {
    MedicationListContext *context = opaque;
    yyjson_mut_val *item = yyjson_mut_obj(context->document);
    if (item == NULL ||
        !yyjson_mut_obj_add_strcpy(
            context->document, item, "medication_id", medication->medication_id) ||
        !yyjson_mut_obj_add_strcpy(
            context->document, item, "revision_id", medication->revision_id) ||
        !yyjson_mut_obj_add_strcpy(context->document, item, "name", medication->name) ||
        !(medication->has_default_dose
              ? yyjson_mut_obj_add_real(
                    context->document, item, "default_dose_value", medication->default_dose_value)
              : yyjson_mut_obj_add_null(context->document, item, "default_dose_value")) ||
        !(medication->has_default_dose
              ? yyjson_mut_obj_add_strcpy(
                    context->document, item, "default_dose_unit", medication->default_dose_unit)
              : yyjson_mut_obj_add_null(context->document, item, "default_dose_unit")) ||
        !yyjson_mut_obj_add_strcpy(context->document, item, "form", medication->form) ||
        !yyjson_mut_obj_add_strcpy(context->document, item, "note", medication->note) ||
        !yyjson_mut_obj_add_bool(context->document, item, "active", medication->active) ||
        !yyjson_mut_arr_add_val(context->items, item)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_web_medication_list_json(TrainlogDatabase *database,
                                                 char **output_json,
                                                 size_t *output_size) {
    yyjson_mut_doc *document = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = document == NULL ? NULL : yyjson_mut_obj(document);
    MedicationListContext context = {document, document == NULL ? NULL : yyjson_mut_arr(document)};
    TrainlogStatus status;
    if (database == NULL || output_json == NULL || output_size == NULL || root == NULL ||
        context.items == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    yyjson_mut_doc_set_root(document, root);
    status = trainlog_medication_list(database, true, 1000U, medication_list_visitor, &context);
    if (status != TRAINLOG_STATUS_OK ||
        !yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_val(document, root, "medications", context.items)) {
        yyjson_mut_doc_free(document);
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_SYSTEM_ERROR : status;
    }
    return write_document(document, output_json, output_size);
}

TrainlogStatus trainlog_web_medication_save_json(TrainlogDatabase *database,
                                                 const char *body,
                                                 size_t body_size,
                                                 char **output_json,
                                                 size_t *output_size) {
    yyjson_read_err error;
    yyjson_doc *input;
    yyjson_val *root;
    yyjson_val *dose;
    yyjson_val *active;
    TrainlogMedication medication = {0};
    char expected[sizeof(medication.revision_id)] = "";
    TrainlogStatus status;
    yyjson_mut_doc *document;
    yyjson_mut_val *result;
    if (database == NULL || body == NULL || body_size == 0U ||
        body_size > TRAINLOG_WEB_SLEEP_BYTES_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    input = yyjson_read_opts((char *)body, body_size, YYJSON_READ_NOFLAG, NULL, &error);
    root = input == NULL ? NULL : yyjson_doc_get_root(input);
    dose = yyjson_is_obj(root) ? yyjson_obj_get(root, "default_dose_value") : NULL;
    active = yyjson_is_obj(root) ? yyjson_obj_get(root, "active") : NULL;
    if (!yyjson_is_obj(root) ||
        !copy_string(root,
                     "medication_id",
                     medication.medication_id,
                     sizeof(medication.medication_id),
                     true) ||
        !copy_string(root, "expected_revision", expected, sizeof(expected), true) ||
        !copy_string(
            root, "created_at", medication.created_at, sizeof(medication.created_at), false) ||
        !copy_string(
            root, "updated_at", medication.updated_at, sizeof(medication.updated_at), false) ||
        !copy_string(root, "name", medication.name, sizeof(medication.name), false) ||
        !copy_string(root, "form", medication.form, sizeof(medication.form), false) ||
        !copy_string(root, "note", medication.note, sizeof(medication.note), false) ||
        !yyjson_is_bool(active) ||
        !((yyjson_is_null(dose) && copy_string(root,
                                               "default_dose_unit",
                                               medication.default_dose_unit,
                                               sizeof(medication.default_dose_unit),
                                               true)) ||
          (yyjson_is_num(dose) && yyjson_get_real(dose) > 0.0 &&
           copy_string(root,
                       "default_dose_unit",
                       medication.default_dose_unit,
                       sizeof(medication.default_dose_unit),
                       false)))) {
        yyjson_doc_free(input);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    medication.has_default_dose = yyjson_is_num(dose);
    medication.default_dose_value = medication.has_default_dose ? yyjson_get_real(dose) : 0.0;
    medication.active = yyjson_get_bool(active);
    if (expected[0] == '\0') {
        status = trainlog_medication_create(database, &medication);
    } else {
        (void)snprintf(medication.revision_id, sizeof(medication.revision_id), "%s", expected);
        status = trainlog_medication_update(database, expected, &medication);
    }
    yyjson_doc_free(input);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    document = yyjson_mut_doc_new(NULL);
    result = document == NULL ? NULL : yyjson_mut_obj(document);
    if (result == NULL ||
        !yyjson_mut_obj_add_strcpy(document, result, "medication_id", medication.medication_id) ||
        !yyjson_mut_obj_add_strcpy(document, result, "revision_id", medication.revision_id)) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, result);
    return write_document(document, output_json, output_size);
}
