#include "trainlog/sleep_diary.h"

#include <stdio.h>
#include <string.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "timestamp.h"
#include "trainlog/id.h"

static bool date_valid(const char *value) {
    TrainlogTimestampKey key;
    char timestamp[32];
    int written;
    if (value == NULL || strlen(value) != 10U) {
        return false;
    }
    written = snprintf(timestamp, sizeof(timestamp), "%sT00:00:00Z", value);
    return written == 20 && trainlog_timestamp_parse(timestamp, 20U, &key);
}

static bool id_valid(const char *value, const char *prefix, size_t capacity) {
    size_t prefix_size = strlen(prefix);
    return value != NULL && strlen(value) < capacity && strncmp(value, prefix, prefix_size) == 0 &&
           value[prefix_size] == '_' &&
           strlen(value + prefix_size + 1U) == TRAINLOG_UUID_TEXT_LENGTH;
}

static const char *quality_text(TrainlogSleepQuality value) {
    static const char *const values[] = {NULL, "TB", "B", "Moy", "M", "TM"};
    return value >= TRAINLOG_SLEEP_QUALITY_UNSET && value <= TRAINLOG_SLEEP_QUALITY_TM
               ? values[value]
               : NULL;
}

static bool quality_parse(const unsigned char *value, TrainlogSleepQuality *output) {
    size_t index;
    if (value == NULL) {
        *output = TRAINLOG_SLEEP_QUALITY_UNSET;
        return true;
    }
    for (index = 1U; index <= (size_t)TRAINLOG_SLEEP_QUALITY_TM; ++index) {
        if (strcmp((const char *)value, quality_text((TrainlogSleepQuality)index)) == 0) {
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

static bool event_parse(const unsigned char *value, TrainlogSleepEventType *output) {
    int index;
    if (value == NULL) {
        return false;
    }
    for (index = 0; index <= (int)TRAINLOG_SLEEP_EVENT_DAYTIME_SLEEPINESS; ++index) {
        if (strcmp((const char *)value, event_text((TrainlogSleepEventType)index)) == 0) {
            *output = (TrainlogSleepEventType)index;
            return true;
        }
    }
    return false;
}

static bool point_type(TrainlogSleepEventType type) {
    return type == TRAINLOG_SLEEP_EVENT_BED_TIME || type == TRAINLOG_SLEEP_EVENT_FINAL_GET_UP ||
           type == TRAINLOG_SLEEP_EVENT_NIGHT_GET_UP ||
           type == TRAINLOG_SLEEP_EVENT_DAYTIME_SLEEPINESS;
}

static bool timestamp_valid(const char *value, TrainlogTimestampKey *key) {
    size_t length = value == NULL ? 0U : strlen(value);
    return length > 0U && length < TRAINLOG_SLEEP_TIMESTAMP_CAPACITY &&
           trainlog_timestamp_parse(value, length, key);
}

bool trainlog_sleep_diary_validate(const TrainlogSleepDiaryEntry *entry) {
    size_t index, other;
    TrainlogTimestampKey created, updated;
    if (entry == NULL || !date_valid(entry->night_start_date) ||
        !date_valid(entry->night_end_date) ||
        strcmp(entry->night_start_date, entry->night_end_date) >= 0 ||
        !timestamp_valid(entry->created_at, &created) ||
        !timestamp_valid(entry->updated_at, &updated) ||
        trainlog_timestamp_compare(&updated, &created) < 0 ||
        entry->sleep_quality > TRAINLOG_SLEEP_QUALITY_TM ||
        entry->wake_quality > TRAINLOG_SLEEP_QUALITY_TM ||
        entry->day_form > TRAINLOG_SLEEP_QUALITY_TM ||
        strlen(entry->treatment_and_notes) > TRAINLOG_SLEEP_NOTES_MAX ||
        entry->event_count > TRAINLOG_SLEEP_EVENTS_MAX) {
        return false;
    }
    for (index = 0U; index < entry->event_count; ++index) {
        const TrainlogSleepEvent *event = &entry->events[index];
        TrainlogTimestampKey start, end;
        bool point = point_type(event->type);
        if (!id_valid(event->event_id, "sle", sizeof(event->event_id)) ||
            event_text(event->type) == NULL || !timestamp_valid(event->start_at, &start) ||
            (point ? event->end_at[0] != '\0'
                   : !timestamp_valid(event->end_at, &end) ||
                         trainlog_timestamp_compare(&end, &start) <= 0)) {
            return false;
        }
        for (other = index + 1U; other < entry->event_count; ++other) {
            if (strcmp(event->event_id, entry->events[other].event_id) == 0) {
                return false;
            }
        }
    }
    return true;
}

static void rollback(sqlite3 *database) {
    (void)sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
}

static TrainlogStatus insert_revision(TrainlogDatabase *database,
                                      const TrainlogSleepDiaryEntry *entry) {
    static const char revision_sql[] =
        "INSERT INTO sleep_diary_revisions(revision_id,entry_id,parent_revision_id,created_at,"
        "sleep_quality,wake_quality,day_form,treatment_and_notes) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)";
    static const char event_sql[] =
        "INSERT INTO sleep_diary_events(revision_id,event_id,event_type,start_at,end_at) "
        "VALUES(?1,?2,?3,?4,?5)";
    sqlite3_stmt *statement = NULL;
    size_t index;
    int result = sqlite3_prepare_v2(database->connection, revision_sql, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, entry->revision_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, entry->entry_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result =
            entry->parent_revision_id[0] == '\0'
                ? sqlite3_bind_null(statement, 3)
                : sqlite3_bind_text(statement, 3, entry->parent_revision_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, entry->updated_at, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = entry->sleep_quality == TRAINLOG_SLEEP_QUALITY_UNSET
                     ? sqlite3_bind_null(statement, 5)
                     : sqlite3_bind_text(
                           statement, 5, quality_text(entry->sleep_quality), -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result = entry->wake_quality == TRAINLOG_SLEEP_QUALITY_UNSET
                     ? sqlite3_bind_null(statement, 6)
                     : sqlite3_bind_text(
                           statement, 6, quality_text(entry->wake_quality), -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result =
            entry->day_form == TRAINLOG_SLEEP_QUALITY_UNSET
                ? sqlite3_bind_null(statement, 7)
                : sqlite3_bind_text(statement, 7, quality_text(entry->day_form), -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 8, entry->treatment_and_notes, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK
                                                        : sqlite3_errcode(database->connection);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_OK) {
        result = SQLITE_ERROR;
    }
    if (result != SQLITE_OK) {
        return result == SQLITE_CONSTRAINT ? TRAINLOG_STATUS_CONFLICT
                                           : TRAINLOG_STATUS_DATABASE_ERROR;
    }
    for (index = 0U; index < entry->event_count; ++index) {
        const TrainlogSleepEvent *event = &entry->events[index];
        statement = NULL;
        result = sqlite3_prepare_v2(database->connection, event_sql, -1, &statement, NULL);
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 1, entry->revision_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 2, event->event_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 3, event_text(event->type), -1, SQLITE_STATIC);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 4, event->start_at, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = point_type(event->type)
                         ? sqlite3_bind_null(statement, 5)
                         : sqlite3_bind_text(statement, 5, event->end_at, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK
                                                            : sqlite3_errcode(database->connection);
        }
        if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_OK) {
            result = SQLITE_ERROR;
        }
        if (result != SQLITE_OK) {
            return result == SQLITE_CONSTRAINT ? TRAINLOG_STATUS_CONFLICT
                                               : TRAINLOG_STATUS_DATABASE_ERROR;
        }
    }
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_sleep_diary_create(TrainlogDatabase *database,
                                           TrainlogSleepDiaryEntry *entry) {
    static const char sql[] = "INSERT INTO sleep_diary_entries VALUES(?1,?2,?3,?4,?5,?6,0)";
    sqlite3_stmt *statement = NULL;
    TrainlogStatus status;
    int result;
    if (database == NULL || entry == NULL || entry->deleted || entry->entry_id[0] != '\0' ||
        entry->revision_id[0] != '\0') {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (trainlog_id_generate("sl", entry->entry_id, sizeof(entry->entry_id)) !=
            TRAINLOG_STATUS_OK ||
        trainlog_id_generate("slr", entry->revision_id, sizeof(entry->revision_id)) !=
            TRAINLOG_STATUS_OK ||
        !trainlog_sleep_diary_validate(entry)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    result = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, entry->entry_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, entry->night_start_date, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, entry->night_end_date, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, entry->created_at, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 5, entry->updated_at, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 6, entry->revision_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK
                                                        : sqlite3_errcode(database->connection);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_OK) {
        result = SQLITE_ERROR;
    }
    status = result == SQLITE_OK ? insert_revision(database, entry)
                                 : (result == SQLITE_CONSTRAINT ? TRAINLOG_STATUS_CONFLICT
                                                                : TRAINLOG_STATUS_DATABASE_ERROR);
    if (status == TRAINLOG_STATUS_OK &&
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) {
        return status;
    }
    rollback(database->connection);
    return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
}

TrainlogStatus trainlog_sleep_diary_update(TrainlogDatabase *database,
                                           const char *expected_revision,
                                           TrainlogSleepDiaryEntry *entry) {
    static const char sql[] =
        "UPDATE sleep_diary_entries SET "
        "night_start_date=?1,night_end_date=?2,updated_at=?3,current_revision_id=?4 WHERE "
        "entry_id=?5 AND current_revision_id=?6 AND deleted=0";
    sqlite3_stmt *statement = NULL;
    TrainlogStatus status;
    char next[TRAINLOG_SLEEP_REVISION_ID_CAPACITY];
    int result;
    if (database == NULL || entry == NULL ||
        !id_valid(entry->entry_id, "sl", sizeof(entry->entry_id)) ||
        !id_valid(expected_revision, "slr", TRAINLOG_SLEEP_REVISION_ID_CAPACITY) ||
        entry->deleted) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (strcmp(entry->revision_id, expected_revision) != 0 ||
        trainlog_id_generate("slr", next, sizeof(next)) != TRAINLOG_STATUS_OK) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)snprintf(
        entry->parent_revision_id, sizeof(entry->parent_revision_id), "%s", expected_revision);
    (void)snprintf(entry->revision_id, sizeof(entry->revision_id), "%s", next);
    if (!trainlog_sleep_diary_validate(entry)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = insert_revision(database, entry);
    if (status == TRAINLOG_STATUS_OK) {
        result = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 1, entry->night_start_date, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 2, entry->night_end_date, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 3, entry->updated_at, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 4, entry->revision_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 5, entry->entry_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 6, expected_revision, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK
                                                            : sqlite3_errcode(database->connection);
        }
        if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_OK) {
            result = SQLITE_ERROR;
        }
        status = result != SQLITE_OK                          ? TRAINLOG_STATUS_DATABASE_ERROR
                 : sqlite3_changes(database->connection) == 1 ? TRAINLOG_STATUS_OK
                                                              : TRAINLOG_STATUS_CONFLICT;
    }
    if (status == TRAINLOG_STATUS_OK &&
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) {
        return status;
    }
    rollback(database->connection);
    return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
}

static TrainlogStatus load_entry(TrainlogDatabase *database,
                                 const char *entry_id,
                                 bool include_deleted,
                                 TrainlogSleepDiaryEntry *output) {
    static const char root_sql[] =
        "SELECT "
        "e.entry_id,e.night_start_date,e.night_end_date,e.created_at,e.updated_at,e.current_"
        "revision_id,e.deleted,r.parent_revision_id,r.sleep_quality,r.wake_quality,r.day_form,r."
        "treatment_and_notes FROM sleep_diary_entries e JOIN sleep_diary_revisions r ON "
        "r.revision_id=e.current_revision_id WHERE e.entry_id=?1 AND (?2 OR e.deleted=0)";
    static const char events_sql[] =
        "SELECT event_id,event_type,start_at,end_at FROM sleep_diary_events WHERE revision_id=?1 "
        "ORDER BY start_at COLLATE BINARY,event_id COLLATE BINARY";
    sqlite3_stmt *statement = NULL;
    int result;
    memset(output, 0, sizeof(*output));
    result = sqlite3_prepare_v2(database->connection, root_sql, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, entry_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 2, include_deleted ? 1 : 0);
    }
    if (result != SQLITE_OK || sqlite3_step(statement) != SQLITE_ROW) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return result == SQLITE_OK ? TRAINLOG_STATUS_NOT_FOUND : TRAINLOG_STATUS_DATABASE_ERROR;
    }
#define COPY_COLUMN(field, column)                                                                 \
    (void)snprintf(output->field,                                                                  \
                   sizeof(output->field),                                                          \
                   "%s",                                                                           \
                   sqlite3_column_text(statement, column) == NULL                                  \
                       ? ""                                                                        \
                       : (const char *)sqlite3_column_text(statement, column))
    COPY_COLUMN(entry_id, 0);
    COPY_COLUMN(night_start_date, 1);
    COPY_COLUMN(night_end_date, 2);
    COPY_COLUMN(created_at, 3);
    COPY_COLUMN(updated_at, 4);
    COPY_COLUMN(revision_id, 5);
    output->deleted = sqlite3_column_int(statement, 6) != 0;
    COPY_COLUMN(parent_revision_id, 7);
#undef COPY_COLUMN
    if (!quality_parse(sqlite3_column_text(statement, 8), &output->sleep_quality) ||
        !quality_parse(sqlite3_column_text(statement, 9), &output->wake_quality) ||
        !quality_parse(sqlite3_column_text(statement, 10), &output->day_form)) {
        (void)sqlite3_finalize(statement);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    (void)snprintf(output->treatment_and_notes,
                   sizeof(output->treatment_and_notes),
                   "%s",
                   sqlite3_column_text(statement, 11) == NULL
                       ? ""
                       : (const char *)sqlite3_column_text(statement, 11));
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    result = sqlite3_prepare_v2(database->connection, events_sql, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 1, output->revision_id, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    while (result == SQLITE_ROW) {
        TrainlogSleepEvent *event;
        if (output->event_count == TRAINLOG_SLEEP_EVENTS_MAX) {
            result = SQLITE_TOOBIG;
            break;
        }
        event = &output->events[output->event_count++];
        (void)snprintf(
            event->event_id, sizeof(event->event_id), "%s", sqlite3_column_text(statement, 0));
        if (!event_parse(sqlite3_column_text(statement, 1), &event->type)) {
            result = SQLITE_MISMATCH;
            break;
        }
        (void)snprintf(
            event->start_at, sizeof(event->start_at), "%s", sqlite3_column_text(statement, 2));
        (void)snprintf(event->end_at,
                       sizeof(event->end_at),
                       "%s",
                       sqlite3_column_text(statement, 3) == NULL
                           ? ""
                           : (const char *)sqlite3_column_text(statement, 3));
        result = sqlite3_step(statement);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_DONE) {
        result = SQLITE_ERROR;
    }
    return result == SQLITE_DONE && trainlog_sleep_diary_validate(output)
               ? TRAINLOG_STATUS_OK
               : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus trainlog_sleep_diary_get(TrainlogDatabase *database,
                                        const char *entry_id,
                                        bool include_deleted,
                                        TrainlogSleepDiaryEntry *output) {
    if (database == NULL || output == NULL ||
        !id_valid(entry_id, "sl", TRAINLOG_SLEEP_ENTRY_ID_CAPACITY)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    return load_entry(database, entry_id, include_deleted, output);
}

TrainlogStatus trainlog_sleep_diary_list(TrainlogDatabase *database,
                                         const char *start_date,
                                         const char *end_date,
                                         size_t limit,
                                         TrainlogSleepDiaryVisitor visitor,
                                         void *context) {
    static const char sql[] =
        "SELECT entry_id FROM sleep_diary_entries WHERE deleted=0 AND (?1 IS NULL OR "
        "night_start_date>=?1) AND (?2 IS NULL OR night_start_date<=?2) ORDER BY night_start_date "
        "DESC,entry_id COLLATE BINARY LIMIT ?3";
    sqlite3_stmt *statement = NULL;
    int result;
    if (database == NULL || visitor == NULL || limit == 0U || limit > 3660U ||
        (start_date != NULL && !date_valid(start_date)) ||
        (end_date != NULL && !date_valid(end_date)) ||
        (start_date != NULL && end_date != NULL && strcmp(start_date, end_date) > 0)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    result = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
    if (result == SQLITE_OK) {
        result = start_date == NULL
                     ? sqlite3_bind_null(statement, 1)
                     : sqlite3_bind_text(statement, 1, start_date, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = end_date == NULL ? sqlite3_bind_null(statement, 2)
                                  : sqlite3_bind_text(statement, 2, end_date, -1, SQLITE_TRANSIENT);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 3, (sqlite3_int64)limit);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement);
    }
    while (result == SQLITE_ROW) {
        TrainlogSleepDiaryEntry entry;
        TrainlogStatus status =
            load_entry(database, (const char *)sqlite3_column_text(statement, 0), false, &entry);
        if (status == TRAINLOG_STATUS_OK) {
            status = visitor(context, &entry);
        }
        if (status != TRAINLOG_STATUS_OK) {
            (void)sqlite3_finalize(statement);
            return status;
        }
        result = sqlite3_step(statement);
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_DONE) {
        result = SQLITE_ERROR;
    }
    return result == SQLITE_DONE ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
}

TrainlogStatus
trainlog_sleep_diary_delete(TrainlogDatabase *database,
                            const char *entry_id,
                            const char *expected_revision,
                            const char *deleted_at,
                            char output_revision[TRAINLOG_SLEEP_REVISION_ID_CAPACITY]) {
    static const char sql[] =
        "UPDATE sleep_diary_entries SET deleted=1,updated_at=?1,current_revision_id=?2 WHERE "
        "entry_id=?3 AND current_revision_id=?4 AND deleted=0";
    sqlite3_stmt *statement = NULL;
    TrainlogSleepDiaryEntry entry;
    TrainlogStatus status;
    int result;
    if (database == NULL || output_revision == NULL ||
        !id_valid(entry_id, "sl", TRAINLOG_SLEEP_ENTRY_ID_CAPACITY) ||
        !id_valid(expected_revision, "slr", TRAINLOG_SLEEP_REVISION_ID_CAPACITY)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    status = load_entry(database, entry_id, false, &entry);
    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }
    if (strcmp(entry.revision_id, expected_revision) != 0) {
        return TRAINLOG_STATUS_CONFLICT;
    }
    (void)snprintf(
        entry.parent_revision_id, sizeof(entry.parent_revision_id), "%s", expected_revision);
    if (trainlog_id_generate("slr", entry.revision_id, sizeof(entry.revision_id)) !=
            TRAINLOG_STATUS_OK ||
        !timestamp_valid(deleted_at, &(TrainlogTimestampKey){0})) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)snprintf(entry.updated_at, sizeof(entry.updated_at), "%s", deleted_at);
    entry.deleted = true;
    if (sqlite3_exec(database->connection, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }
    status = insert_revision(database, &entry);
    if (status == TRAINLOG_STATUS_OK) {
        result = sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL);
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 1, deleted_at, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 2, entry.revision_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 3, entry_id, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_bind_text(statement, 4, expected_revision, -1, SQLITE_TRANSIENT);
        }
        if (result == SQLITE_OK) {
            result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK
                                                            : sqlite3_errcode(database->connection);
        }
        if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && result == SQLITE_OK) {
            result = SQLITE_ERROR;
        }
        status = result != SQLITE_OK                          ? TRAINLOG_STATUS_DATABASE_ERROR
                 : sqlite3_changes(database->connection) == 1 ? TRAINLOG_STATUS_OK
                                                              : TRAINLOG_STATUS_CONFLICT;
    }
    if (status == TRAINLOG_STATUS_OK &&
        sqlite3_exec(database->connection, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) {
        (void)snprintf(
            output_revision, TRAINLOG_SLEEP_REVISION_ID_CAPACITY, "%s", entry.revision_id);
        return status;
    }
    rollback(database->connection);
    return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_DATABASE_ERROR : status;
}
