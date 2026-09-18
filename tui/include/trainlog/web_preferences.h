#ifndef TRAINLOG_WEB_PREFERENCES_H
#define TRAINLOG_WEB_PREFERENCES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRAINLOG_WEB_PREFERENCES_JSON_CAPACITY 512U
#define TRAINLOG_WEB_PREFERENCES_MAX_REVISION 9007199254740991ULL

typedef enum TrainlogWebDateFormat {
    TRAINLOG_WEB_DATE_FORMAT_FRENCH = 0,
    TRAINLOG_WEB_DATE_FORMAT_ISO
} TrainlogWebDateFormat;

typedef struct TrainlogWebPreferences {
    uint64_t revision;
    TrainlogWebDateFormat date_format;
} TrainlogWebPreferences;

typedef enum TrainlogWebPreferencesSource {
    TRAINLOG_WEB_PREFERENCES_DEFAULT = 0,
    TRAINLOG_WEB_PREFERENCES_PERSISTED,
    TRAINLOG_WEB_PREFERENCES_INVALID_PERSISTED
} TrainlogWebPreferencesSource;

typedef enum TrainlogWebPreferencesResult {
    TRAINLOG_WEB_PREFERENCES_OK = 0,
    TRAINLOG_WEB_PREFERENCES_INVALID,
    TRAINLOG_WEB_PREFERENCES_CONFLICT,
    TRAINLOG_WEB_PREFERENCES_IO_ERROR
} TrainlogWebPreferencesResult;

/*
 * CONTRACT: this module owns only installation-local Web presentation
 * preferences below XDG_CONFIG_HOME/trainlog/web. It never reads or writes
 * business SQLite state. Revision zero is the French default when no valid
 * private file exists.
 */
void trainlog_web_preferences_default(TrainlogWebPreferences *preferences);
TrainlogWebPreferencesResult trainlog_web_preferences_parse(
    const char *json, size_t size, TrainlogWebPreferences *preferences);
TrainlogWebPreferencesResult trainlog_web_preferences_load(
    TrainlogWebPreferences *preferences, TrainlogWebPreferencesSource *source);
TrainlogWebPreferencesResult trainlog_web_preferences_save(
    const TrainlogWebPreferences *preferences, uint64_t expected_revision,
    TrainlogWebPreferences *saved);
bool trainlog_web_preferences_serialize(
    const TrainlogWebPreferences *preferences, TrainlogWebPreferencesSource source,
    bool include_source, char *output, size_t capacity, size_t *size);
bool trainlog_web_preferences_path(char *output, size_t capacity);

#endif
