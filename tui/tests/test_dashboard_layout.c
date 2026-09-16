#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "trainlog/dashboard_layout.h"

#define CHECK(value) do { if (!(value)) { (void)fprintf(stderr, \
    "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #value); return false; } } while (0)

static bool serialize_file_layout(TrainlogDashboardLayout *layout,
    char *json, size_t capacity, size_t *size)
{
    return trainlog_dashboard_layout_serialize(layout,
        TRAINLOG_DASHBOARD_LAYOUT_PERSISTED, false, json, capacity, size);
}

static bool test_validation(void)
{
    TrainlogDashboardLayout layout;
    char json[4096]; size_t size;
    trainlog_dashboard_layout_default(&layout);
    CHECK(trainlog_dashboard_layout_validate(&layout));
    CHECK(serialize_file_layout(&layout, json, sizeof(json), &size));
    CHECK(trainlog_dashboard_layout_parse(json, size, &layout) == TRAINLOG_DASHBOARD_LAYOUT_OK);
    layout.tiles[0].width = 1U; CHECK(!trainlog_dashboard_layout_validate(&layout));
    trainlog_dashboard_layout_default(&layout);
    (void)snprintf(layout.tiles[1].id, sizeof(layout.tiles[1].id), "%s", layout.tiles[0].id);
    CHECK(!trainlog_dashboard_layout_validate(&layout));
    trainlog_dashboard_layout_default(&layout); layout.tiles[1].x = 0U;
    CHECK(!trainlog_dashboard_layout_validate(&layout));
    trainlog_dashboard_layout_default(&layout); layout.tiles[0].y = 201U;
    CHECK(!trainlog_dashboard_layout_validate(&layout));
    CHECK(trainlog_dashboard_layout_parse("{}", 2U, &layout) == TRAINLOG_DASHBOARD_LAYOUT_INVALID);
    return true;
}

static bool write_raw(const char *path, const char *text)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600); size_t size = strlen(text);
    ssize_t count; if (fd < 0) return false; count = write(fd, text, size);
    return close(fd) == 0 && count == (ssize_t)size;
}

static bool test_persistence(void)
{
    char root[] = "/tmp/trainlog-layout-test-XXXXXX";
    char path[4096]; char target[4096]; char json[4096]; size_t size;
    TrainlogDashboardLayout layout, saved, loaded;
    TrainlogDashboardLayoutSource source; struct stat status;
    CHECK(mkdtemp(root) != NULL); CHECK(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK);
    CHECK(source == TRAINLOG_DASHBOARD_LAYOUT_DEFAULT && loaded.revision == 0U);
    trainlog_dashboard_layout_default(&layout);
    layout.tiles[3].y = 14U;
    CHECK(trainlog_dashboard_layout_validate(&layout));
    CHECK(trainlog_dashboard_layout_save(&layout, 0U, &saved) == TRAINLOG_DASHBOARD_LAYOUT_OK);
    CHECK(saved.revision == 1U);
    CHECK(trainlog_dashboard_layout_path(path, sizeof(path)));
    CHECK(lstat(path, &status) == 0 && S_ISREG(status.st_mode) &&
        (status.st_mode & 0777U) == 0600U);
    CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK);
    CHECK(source == TRAINLOG_DASHBOARD_LAYOUT_PERSISTED && loaded.revision == 1U);
    layout = loaded; layout.tiles[0].width = 1U;
    CHECK(trainlog_dashboard_layout_save(&layout, 1U, &saved) == TRAINLOG_DASHBOARD_LAYOUT_INVALID);
    CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK &&
        loaded.revision == 1U);
    trainlog_dashboard_layout_default(&layout);
    layout.tiles[3].y = 14U;
    CHECK(trainlog_dashboard_layout_save(&layout, 0U, &saved) == TRAINLOG_DASHBOARD_LAYOUT_CONFLICT);
    layout = loaded; layout.tiles[3].y = 18U;
    CHECK(trainlog_dashboard_layout_save(&layout, 1U, &saved) == TRAINLOG_DASHBOARD_LAYOUT_OK);
    CHECK(saved.revision == 2U);
    CHECK(trainlog_dashboard_layout_delete(1U) == TRAINLOG_DASHBOARD_LAYOUT_CONFLICT);
    CHECK(trainlog_dashboard_layout_delete(2U) == TRAINLOG_DASHBOARD_LAYOUT_OK);
    CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK &&
        source == TRAINLOG_DASHBOARD_LAYOUT_DEFAULT);
    CHECK(snprintf(target, sizeof(target), "%s/target", root) > 0);
    CHECK(write_raw(target, "sentinel")); CHECK(symlink(target, path) == 0);
    CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK &&
        source == TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED);
    CHECK(unlink(path) == 0);
    CHECK(write_raw(path, "{broken"));
    CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK &&
        source == TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED);
    CHECK(access(path, F_OK) == 0);
    CHECK(unlink(path) == 0);
    {
        char oversized[TRAINLOG_DASHBOARD_LAYOUT_JSON_CAPACITY];
        (void)memset(oversized, 'x', sizeof(oversized));
        {
            int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
            CHECK(fd >= 0 && write(fd, oversized, sizeof(oversized)) == (ssize_t)sizeof(oversized));
            CHECK(close(fd) == 0);
        }
        CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK &&
            source == TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED);
    }
    CHECK(unlink(path) == 0 && mkdir(path, 0700) == 0);
    CHECK(trainlog_dashboard_layout_load(&loaded, &source) == TRAINLOG_DASHBOARD_LAYOUT_OK &&
        source == TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED);
    CHECK(rmdir(path) == 0);
    CHECK(serialize_file_layout(&loaded, json, sizeof(json), &size));
    CHECK(size < sizeof(json));
    return true;
}

int main(void)
{
    return test_validation() && test_persistence() ? 0 : 1;
}
