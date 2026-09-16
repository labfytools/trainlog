#include "trainlog/dashboard_layout.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <yyjson.h>

typedef struct TileContract {
    const char *id;
    uint16_t min_width;
    uint16_t max_width;
    uint16_t min_height;
    uint16_t max_height;
} TileContract;

static const TileContract contracts[TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT] = {
    {"next-session", 4U, 8U, 3U, 7U},
    {"activity", 5U, 12U, 3U, 7U},
    {"progression", 6U, 12U, 4U, 9U},
    {"last-session", 3U, 7U, 3U, 6U},
    {"max-records", 3U, 8U, 3U, 7U},
    {"muscle-distribution", 5U, 12U, 4U, 9U},
    {"cardio-recovery", 5U, 12U, 4U, 8U}
};

static const TrainlogDashboardTileLayout defaults[TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT] = {
    {"next-session", 0U, 0U, 5U, 4U}, {"activity", 5U, 0U, 7U, 4U},
    {"progression", 0U, 4U, 8U, 5U}, {"last-session", 8U, 4U, 4U, 3U},
    {"max-records", 8U, 7U, 4U, 3U},
    {"muscle-distribution", 0U, 9U, 6U, 5U},
    {"cardio-recovery", 6U, 10U, 6U, 4U}
};

static int contract_index(const char *id)
{
    size_t index;
    for (index = 0U; index < TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT; ++index)
        if (strcmp(id, contracts[index].id) == 0) return (int)index;
    return -1;
}

void trainlog_dashboard_layout_default(TrainlogDashboardLayout *layout)
{
    if (layout == NULL) return;
    layout->revision = 0U;
    (void)memcpy(layout->tiles, defaults, sizeof(defaults));
}

static bool overlap(const TrainlogDashboardTileLayout *a,
    const TrainlogDashboardTileLayout *b)
{
    return a->x < b->x + b->width && a->x + a->width > b->x &&
        a->y < b->y + b->height && a->y + a->height > b->y;
}

bool trainlog_dashboard_layout_validate(const TrainlogDashboardLayout *layout)
{
    bool seen[TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT] = {false};
    size_t index;
    if (layout == NULL || layout->revision > TRAINLOG_DASHBOARD_LAYOUT_MAX_REVISION)
        return false;
    for (index = 0U; index < TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT; ++index) {
        const TrainlogDashboardTileLayout *tile = &layout->tiles[index];
        int found = contract_index(tile->id);
        const TileContract *contract;
        size_t other;
        if (found < 0 || seen[(size_t)found]) return false;
        seen[(size_t)found] = true;
        contract = &contracts[(size_t)found];
        if (tile->y > TRAINLOG_DASHBOARD_LAYOUT_MAX_Y ||
            tile->width < contract->min_width || tile->width > contract->max_width ||
            tile->height < contract->min_height || tile->height > contract->max_height ||
            tile->x + tile->width > TRAINLOG_DASHBOARD_LAYOUT_COLUMNS)
            return false;
        for (other = index + 1U; other < TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT; ++other)
            if (overlap(tile, &layout->tiles[other])) return false;
    }
    return true;
}

static bool exact_object(const yyjson_val *value, size_t expected)
{
    return yyjson_is_obj(value) && yyjson_obj_size(value) == expected;
}

static bool read_u16(yyjson_val *value, uint16_t *output)
{
    uint64_t number;
    if (!yyjson_is_uint(value)) return false;
    number = yyjson_get_uint(value);
    if (number > UINT16_MAX) return false;
    *output = (uint16_t)number;
    return true;
}

static bool equals_uint(yyjson_val *value, uint64_t expected)
{
    return yyjson_is_uint(value) && yyjson_get_uint(value) == expected;
}

TrainlogDashboardLayoutResult trainlog_dashboard_layout_parse(
    const char *json, size_t size, TrainlogDashboardLayout *layout)
{
    yyjson_read_err error;
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *tiles;
    size_t index;
    if (json == NULL || layout == NULL || size == 0U ||
        size >= TRAINLOG_DASHBOARD_LAYOUT_JSON_CAPACITY)
        return TRAINLOG_DASHBOARD_LAYOUT_INVALID;
    document = yyjson_read_opts((char *)json, size, YYJSON_READ_NOFLAG, NULL, &error);
    if (document == NULL) return TRAINLOG_DASHBOARD_LAYOUT_INVALID;
    root = yyjson_doc_get_root(document);
    if (!exact_object(root, 5U) ||
        !yyjson_equals_str(yyjson_obj_get(root, "format"), "trainlog-dashboard-layout") ||
        !equals_uint(yyjson_obj_get(root, "version"), 1U) ||
        !equals_uint(yyjson_obj_get(root, "columns"), 12U) ||
        !yyjson_is_uint(yyjson_obj_get(root, "revision"))) {
        yyjson_doc_free(document); return TRAINLOG_DASHBOARD_LAYOUT_INVALID;
    }
    layout->revision = yyjson_get_uint(yyjson_obj_get(root, "revision"));
    tiles = yyjson_obj_get(root, "tiles");
    if (!yyjson_is_arr(tiles) || yyjson_arr_size(tiles) != TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT) {
        yyjson_doc_free(document); return TRAINLOG_DASHBOARD_LAYOUT_INVALID;
    }
    for (index = 0U; index < TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT; ++index) {
        yyjson_val *tile = yyjson_arr_get(tiles, index);
        yyjson_val *id = yyjson_obj_get(tile, "id");
        const char *text;
        size_t length;
        if (!exact_object(tile, 5U) || !yyjson_is_str(id) ||
            !read_u16(yyjson_obj_get(tile, "x"), &layout->tiles[index].x) ||
            !read_u16(yyjson_obj_get(tile, "y"), &layout->tiles[index].y) ||
            !read_u16(yyjson_obj_get(tile, "width"), &layout->tiles[index].width) ||
            !read_u16(yyjson_obj_get(tile, "height"), &layout->tiles[index].height)) {
            yyjson_doc_free(document); return TRAINLOG_DASHBOARD_LAYOUT_INVALID;
        }
        text = yyjson_get_str(id); length = yyjson_get_len(id);
        if (length == 0U || length >= sizeof(layout->tiles[index].id)) {
            yyjson_doc_free(document); return TRAINLOG_DASHBOARD_LAYOUT_INVALID;
        }
        (void)memcpy(layout->tiles[index].id, text, length);
        layout->tiles[index].id[length] = '\0';
    }
    yyjson_doc_free(document);
    return trainlog_dashboard_layout_validate(layout)
        ? TRAINLOG_DASHBOARD_LAYOUT_OK : TRAINLOG_DASHBOARD_LAYOUT_INVALID;
}

bool trainlog_dashboard_layout_path(char *output, size_t capacity)
{
    const char *base = getenv("XDG_CONFIG_HOME");
    const char *home;
    int length;
    if (output == NULL || capacity == 0U) return false;
    if (base != NULL && base[0] != '\0')
        length = snprintf(output, capacity, "%s/trainlog/web/dashboard-layout-v1.json", base);
    else {
        home = getenv("HOME");
        if (home == NULL || home[0] == '\0') return false;
        length = snprintf(output, capacity, "%s/.config/trainlog/web/dashboard-layout-v1.json", home);
    }
    return length > 0 && (size_t)length < capacity;
}

static bool ensure_private_directories(const char *path)
{
    char web[4096];
    char trainlog[4096];
    char *last;
    struct stat status;
    if (path == NULL || strlen(path) >= sizeof(web)) return false;
    (void)snprintf(web, sizeof(web), "%s", path);
    last = strrchr(web, '/'); if (last == NULL) return false; *last = '\0';
    (void)snprintf(trainlog, sizeof(trainlog), "%s", web);
    last = strrchr(trainlog, '/'); if (last == NULL) return false; *last = '\0';
    /* CONTRACT: only the two Trainlog-owned directories are created/chmodded;
     * an existing symlink or non-directory is never followed. */
    {
        if (mkdir(trainlog, 0700) != 0 && errno != EEXIST) return false;
        if (lstat(trainlog, &status) != 0 || !S_ISDIR(status.st_mode)) return false;
        if (chmod(trainlog, 0700) != 0) return false;
    }
    if (mkdir(web, 0700) != 0 && errno != EEXIST) return false;
    if (lstat(web, &status) != 0 || !S_ISDIR(status.st_mode)) return false;
    return chmod(web, 0700) == 0;
}

static bool sync_parent(const char *path)
{
    char parent[4096]; char *slash; int fd; bool ok;
    if (strlen(path) >= sizeof(parent)) return false;
    (void)snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '/'); if (slash == NULL) return false; *slash = '\0';
    fd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;
    ok = fsync(fd) == 0; if (close(fd) != 0) ok = false; return ok;
}

bool trainlog_dashboard_layout_serialize(const TrainlogDashboardLayout *layout,
    TrainlogDashboardLayoutSource source, bool include_source,
    char *output, size_t capacity, size_t *size)
{
    size_t used = 0U; size_t index; int count;
    const char *source_name = source == TRAINLOG_DASHBOARD_LAYOUT_PERSISTED
        ? "persisted" : source == TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED
        ? "invalid_persisted" : "default";
    if (!trainlog_dashboard_layout_validate(layout) || output == NULL || size == NULL)
        return false;
#define APPEND(...) do { count = snprintf(output + used, capacity - used, __VA_ARGS__); \
    if (count < 0 || (size_t)count >= capacity - used) return false; used += (size_t)count; } while (0)
    APPEND("{\"format\":\"trainlog-dashboard-layout\",\"version\":1,"
        "\"revision\":%llu,\"columns\":12", (unsigned long long)layout->revision);
    if (include_source) APPEND(",\"source\":\"%s\"", source_name);
    APPEND(",\"tiles\":[");
    for (index = 0U; index < TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT; ++index) {
        const TrainlogDashboardTileLayout *tile = &layout->tiles[index];
        APPEND("%s{\"id\":\"%s\",\"x\":%u,\"y\":%u,\"width\":%u,\"height\":%u}",
            index == 0U ? "" : ",", tile->id, tile->x, tile->y,
            tile->width, tile->height);
    }
    APPEND("]}\n");
#undef APPEND
    *size = used; return true;
}

TrainlogDashboardLayoutResult trainlog_dashboard_layout_load(
    TrainlogDashboardLayout *layout, TrainlogDashboardLayoutSource *source)
{
    char path[4096]; char buffer[TRAINLOG_DASHBOARD_LAYOUT_JSON_CAPACITY];
    struct stat status; ssize_t count; int fd; TrainlogDashboardLayoutResult result;
    if (layout == NULL || source == NULL || !trainlog_dashboard_layout_path(path, sizeof(path)))
        return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    trainlog_dashboard_layout_default(layout); *source = TRAINLOG_DASHBOARD_LAYOUT_DEFAULT;
    fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        if (errno == ENOENT || errno == ELOOP) {
            if (errno == ELOOP) *source = TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED;
            return TRAINLOG_DASHBOARD_LAYOUT_OK;
        }
        return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    }
    if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size <= 0 ||
        status.st_size >= (off_t)sizeof(buffer)) {
        (void)close(fd); *source = TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED;
        return TRAINLOG_DASHBOARD_LAYOUT_OK;
    }
    count = read(fd, buffer, (size_t)status.st_size);
    if (close(fd) != 0 || count != status.st_size) {
        *source = TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED;
        return TRAINLOG_DASHBOARD_LAYOUT_OK;
    }
    result = trainlog_dashboard_layout_parse(buffer, (size_t)count, layout);
    if (result != TRAINLOG_DASHBOARD_LAYOUT_OK) {
        trainlog_dashboard_layout_default(layout);
        *source = TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED;
    } else *source = TRAINLOG_DASHBOARD_LAYOUT_PERSISTED;
    return TRAINLOG_DASHBOARD_LAYOUT_OK;
}

static bool write_all(int fd, const char *data, size_t size)
{
    size_t used = 0U;
    while (used < size) { ssize_t count = write(fd, data + used, size - used);
        if (count < 0 && errno == EINTR) continue; if (count <= 0) return false; used += (size_t)count; }
    return true;
}

TrainlogDashboardLayoutResult trainlog_dashboard_layout_save(
    const TrainlogDashboardLayout *layout, uint64_t expected_revision,
    TrainlogDashboardLayout *saved)
{
    TrainlogDashboardLayout current; TrainlogDashboardLayoutSource source;
    char path[4096], temporary[4128], json[TRAINLOG_DASHBOARD_LAYOUT_JSON_CAPACITY];
    size_t size; int fd; bool ok;
    if (layout == NULL || saved == NULL || !trainlog_dashboard_layout_validate(layout) ||
        layout->revision != expected_revision || expected_revision >= TRAINLOG_DASHBOARD_LAYOUT_MAX_REVISION)
        return TRAINLOG_DASHBOARD_LAYOUT_INVALID;
    if (trainlog_dashboard_layout_load(&current, &source) != TRAINLOG_DASHBOARD_LAYOUT_OK)
        return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    if (current.revision != expected_revision) return TRAINLOG_DASHBOARD_LAYOUT_CONFLICT;
    *saved = *layout; saved->revision = expected_revision + 1U;
    if (!trainlog_dashboard_layout_path(path, sizeof(path)) ||
        !ensure_private_directories(path) ||
        !trainlog_dashboard_layout_serialize(saved, TRAINLOG_DASHBOARD_LAYOUT_PERSISTED,
            false, json, sizeof(json), &size) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >= (int)sizeof(temporary))
        return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    fd = mkstemp(temporary);
    if (fd < 0) return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    ok = fchmod(fd, 0600) == 0 && fcntl(fd, F_SETFD, FD_CLOEXEC) == 0 &&
        write_all(fd, json, size) && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (!ok || rename(temporary, path) != 0) {
        (void)unlink(temporary); return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    }
    /* INVARIANT: after rename the valid file is committed. Directory fsync is
     * attempted for crash durability, but its failure cannot roll back or make
     * a successful visible commit safe to retry against the old revision. */
    (void)sync_parent(path);
    return TRAINLOG_DASHBOARD_LAYOUT_OK;
}

TrainlogDashboardLayoutResult trainlog_dashboard_layout_delete(uint64_t expected_revision)
{
    TrainlogDashboardLayout current; TrainlogDashboardLayoutSource source; char path[4096];
    if (trainlog_dashboard_layout_load(&current, &source) != TRAINLOG_DASHBOARD_LAYOUT_OK)
        return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    if (current.revision != expected_revision) return TRAINLOG_DASHBOARD_LAYOUT_CONFLICT;
    if (!trainlog_dashboard_layout_path(path, sizeof(path))) return TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    if (unlink(path) != 0) {
        return errno == ENOENT ? TRAINLOG_DASHBOARD_LAYOUT_OK
            : TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR;
    }
    (void)sync_parent(path);
    return TRAINLOG_DASHBOARD_LAYOUT_OK;
}
