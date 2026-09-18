#include "trainlog/web_preferences.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <yyjson.h>

void trainlog_web_preferences_default(TrainlogWebPreferences *preferences) {
    if (preferences == NULL) {
        return;
    }
    preferences->revision = 0U;
    preferences->date_format = TRAINLOG_WEB_DATE_FORMAT_FRENCH;
}

static bool preferences_valid(const TrainlogWebPreferences *preferences) {
    return preferences != NULL &&
           preferences->revision <= TRAINLOG_WEB_PREFERENCES_MAX_REVISION &&
           (preferences->date_format == TRAINLOG_WEB_DATE_FORMAT_FRENCH ||
            preferences->date_format == TRAINLOG_WEB_DATE_FORMAT_ISO);
}

TrainlogWebPreferencesResult trainlog_web_preferences_parse(
    const char *json, size_t size, TrainlogWebPreferences *preferences) {
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *revision;
    yyjson_val *date_format;
    const char *date_format_text;

    if (json == NULL || size == 0U || size >= TRAINLOG_WEB_PREFERENCES_JSON_CAPACITY ||
        preferences == NULL) {
        return TRAINLOG_WEB_PREFERENCES_INVALID;
    }
    document = yyjson_read(json, size, YYJSON_READ_NOFLAG);
    if (document == NULL) {
        return TRAINLOG_WEB_PREFERENCES_INVALID;
    }
    root = yyjson_doc_get_root(document);
    revision = yyjson_obj_get(root, "revision");
    date_format = yyjson_obj_get(root, "date_format");
    if (!yyjson_is_obj(root) || yyjson_obj_size(root) != 4U ||
        !yyjson_equals_str(yyjson_obj_get(root, "format"), "trainlog-web-preferences") ||
        !yyjson_is_uint(yyjson_obj_get(root, "version")) ||
        yyjson_get_uint(yyjson_obj_get(root, "version")) != 1U || !yyjson_is_uint(revision) ||
        !yyjson_is_str(date_format)) {
        yyjson_doc_free(document);
        return TRAINLOG_WEB_PREFERENCES_INVALID;
    }
    preferences->revision = yyjson_get_uint(revision);
    date_format_text = yyjson_get_str(date_format);
    if (strcmp(date_format_text, "fr") == 0) {
        preferences->date_format = TRAINLOG_WEB_DATE_FORMAT_FRENCH;
    } else if (strcmp(date_format_text, "iso") == 0) {
        preferences->date_format = TRAINLOG_WEB_DATE_FORMAT_ISO;
    } else {
        yyjson_doc_free(document);
        return TRAINLOG_WEB_PREFERENCES_INVALID;
    }
    yyjson_doc_free(document);
    return preferences_valid(preferences) ? TRAINLOG_WEB_PREFERENCES_OK
                                          : TRAINLOG_WEB_PREFERENCES_INVALID;
}

bool trainlog_web_preferences_path(char *output, size_t capacity) {
    const char *base = getenv("XDG_CONFIG_HOME");
    const char *home;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (base != NULL && base[0] != '\0') {
        length = snprintf(output, capacity, "%s/trainlog/web/preferences-v1.json", base);
    } else {
        home = getenv("HOME");
        if (home == NULL || home[0] == '\0') {
            return false;
        }
        length = snprintf(output,
                          capacity,
                          "%s/.config/trainlog/web/preferences-v1.json",
                          home);
    }
    return length > 0 && (size_t)length < capacity;
}

static bool ensure_private_directories(const char *path) {
    char web[4096];
    char trainlog[4096];
    char *last;
    struct stat status;

    if (path == NULL || strlen(path) >= sizeof(web)) {
        return false;
    }
    (void)snprintf(web, sizeof(web), "%s", path);
    last = strrchr(web, '/');
    if (last == NULL) {
        return false;
    }
    *last = '\0';
    (void)snprintf(trainlog, sizeof(trainlog), "%s", web);
    last = strrchr(trainlog, '/');
    if (last == NULL) {
        return false;
    }
    *last = '\0';
    /* CONTRACT: refuse symlinks and chmod only Trainlog-owned directories. */
    if (mkdir(trainlog, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    if (lstat(trainlog, &status) != 0 || !S_ISDIR(status.st_mode) ||
        chmod(trainlog, 0700) != 0) {
        return false;
    }
    if (mkdir(web, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    if (lstat(web, &status) != 0 || !S_ISDIR(status.st_mode)) {
        return false;
    }
    return chmod(web, 0700) == 0;
}

bool trainlog_web_preferences_serialize(
    const TrainlogWebPreferences *preferences, TrainlogWebPreferencesSource source,
    bool include_source, char *output, size_t capacity, size_t *size) {
    const char *date_format;
    const char *source_name;
    int written;

    if (!preferences_valid(preferences) || output == NULL || size == NULL) {
        return false;
    }
    date_format = preferences->date_format == TRAINLOG_WEB_DATE_FORMAT_ISO ? "iso" : "fr";
    source_name = source == TRAINLOG_WEB_PREFERENCES_PERSISTED
                      ? "persisted"
                      : source == TRAINLOG_WEB_PREFERENCES_INVALID_PERSISTED ? "invalid_persisted"
                                                                            : "default";
    if (include_source) {
        written = snprintf(output,
                           capacity,
                           "{\"format\":\"trainlog-web-preferences\",\"version\":1,"
                           "\"revision\":%llu,\"date_format\":\"%s\",\"source\":\"%s\"}\n",
                           (unsigned long long)preferences->revision,
                           date_format,
                           source_name);
    } else {
        written = snprintf(output,
                           capacity,
                           "{\"format\":\"trainlog-web-preferences\",\"version\":1,"
                           "\"revision\":%llu,\"date_format\":\"%s\"}\n",
                           (unsigned long long)preferences->revision,
                           date_format);
    }
    if (written < 0 || (size_t)written >= capacity) {
        return false;
    }
    *size = (size_t)written;
    return true;
}

TrainlogWebPreferencesResult trainlog_web_preferences_load(
    TrainlogWebPreferences *preferences, TrainlogWebPreferencesSource *source) {
    char path[4096];
    char buffer[TRAINLOG_WEB_PREFERENCES_JSON_CAPACITY];
    struct stat status;
    ssize_t count;
    int descriptor;

    if (preferences == NULL || source == NULL ||
        !trainlog_web_preferences_path(path, sizeof(path))) {
        return TRAINLOG_WEB_PREFERENCES_IO_ERROR;
    }
    trainlog_web_preferences_default(preferences);
    *source = TRAINLOG_WEB_PREFERENCES_DEFAULT;
    descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) {
        if (errno == ENOENT || errno == ELOOP) {
            if (errno == ELOOP) {
                *source = TRAINLOG_WEB_PREFERENCES_INVALID_PERSISTED;
            }
            return TRAINLOG_WEB_PREFERENCES_OK;
        }
        return TRAINLOG_WEB_PREFERENCES_IO_ERROR;
    }
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size <= 0 ||
        status.st_size >= (off_t)sizeof(buffer)) {
        (void)close(descriptor);
        *source = TRAINLOG_WEB_PREFERENCES_INVALID_PERSISTED;
        return TRAINLOG_WEB_PREFERENCES_OK;
    }
    count = read(descriptor, buffer, (size_t)status.st_size);
    if (close(descriptor) != 0 || count != status.st_size ||
        trainlog_web_preferences_parse(buffer, (size_t)count, preferences) !=
            TRAINLOG_WEB_PREFERENCES_OK) {
        trainlog_web_preferences_default(preferences);
        *source = TRAINLOG_WEB_PREFERENCES_INVALID_PERSISTED;
        return TRAINLOG_WEB_PREFERENCES_OK;
    }
    *source = TRAINLOG_WEB_PREFERENCES_PERSISTED;
    return TRAINLOG_WEB_PREFERENCES_OK;
}

static bool write_all(int descriptor, const char *data, size_t size) {
    size_t used = 0U;
    while (used < size) {
        ssize_t count = write(descriptor, data + used, size - used);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        used += (size_t)count;
    }
    return true;
}

static void sync_parent(const char *path) {
    char parent[4096];
    char *slash;
    int descriptor;

    if (strlen(path) >= sizeof(parent)) {
        return;
    }
    (void)snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '/');
    if (slash == NULL) {
        return;
    }
    *slash = '\0';
    descriptor = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor >= 0) {
        (void)fsync(descriptor);
        (void)close(descriptor);
    }
}

TrainlogWebPreferencesResult trainlog_web_preferences_save(
    const TrainlogWebPreferences *preferences, uint64_t expected_revision,
    TrainlogWebPreferences *saved) {
    TrainlogWebPreferences current;
    TrainlogWebPreferencesSource source;
    char path[4096];
    char temporary[4128];
    char json[TRAINLOG_WEB_PREFERENCES_JSON_CAPACITY];
    size_t size;
    int descriptor;
    bool ok;

    if (!preferences_valid(preferences) || saved == NULL ||
        preferences->revision != expected_revision ||
        expected_revision >= TRAINLOG_WEB_PREFERENCES_MAX_REVISION) {
        return TRAINLOG_WEB_PREFERENCES_INVALID;
    }
    if (trainlog_web_preferences_load(&current, &source) != TRAINLOG_WEB_PREFERENCES_OK) {
        return TRAINLOG_WEB_PREFERENCES_IO_ERROR;
    }
    if (current.revision != expected_revision) {
        return TRAINLOG_WEB_PREFERENCES_CONFLICT;
    }
    *saved = *preferences;
    saved->revision = expected_revision + 1U;
    if (!trainlog_web_preferences_path(path, sizeof(path)) || !ensure_private_directories(path) ||
        !trainlog_web_preferences_serialize(saved,
                                            TRAINLOG_WEB_PREFERENCES_PERSISTED,
                                            false,
                                            json,
                                            sizeof(json),
                                            &size) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >=
            (int)sizeof(temporary)) {
        return TRAINLOG_WEB_PREFERENCES_IO_ERROR;
    }
    descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        return TRAINLOG_WEB_PREFERENCES_IO_ERROR;
    }
    ok = fchmod(descriptor, 0600) == 0 && fcntl(descriptor, F_SETFD, FD_CLOEXEC) == 0 &&
         write_all(descriptor, json, size) && fsync(descriptor) == 0;
    if (close(descriptor) != 0) {
        ok = false;
    }
    if (!ok || rename(temporary, path) != 0) {
        (void)unlink(temporary);
        return TRAINLOG_WEB_PREFERENCES_IO_ERROR;
    }
    /* INVARIANT: rename commits the new revision; directory fsync is best
     * effort because a visible commit cannot safely be retried as the old one. */
    sync_parent(path);
    return TRAINLOG_WEB_PREFERENCES_OK;
}
