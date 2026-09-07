/**
 * @file main.c
 * @brief Trainlog TUI process entry point.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "trainlog/database.h"
#include "trainlog/tui.h"

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0700) == 0 || errno == EEXIST) {
        return 0;
    }

    return -1;
}

static int build_database_path(char *output, size_t output_size)
{
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char base[PATH_MAX];
    char trainlog_dir[PATH_MAX];
    int written;

    if (xdg != NULL && xdg[0] != '\0') {
        written = snprintf(base, sizeof(base), "%s", xdg);
    } else if (home != NULL && home[0] != '\0') {
        written = snprintf(base, sizeof(base), "%s/.local/share", home);
    } else {
        return -1;
    }

    if (written < 0 || (size_t)written >= sizeof(base)) {
        return -1;
    }

    /*
     * Creating ~/.local and ~/.local/share recursively would normally require
     * a generic mkdir -p helper. On Arch these parents already exist for a
     * desktop user; XDG_DATA_HOME custom paths are likewise expected to have
     * their parent created by the user.
     */
    if (ensure_directory(base) != 0 && errno != EEXIST) {
        /* Continue only when the base already exists. */
        struct stat info;
        if (stat(base, &info) != 0 || !S_ISDIR(info.st_mode)) {
            return -1;
        }
    }

    written = snprintf(
        trainlog_dir,
        sizeof(trainlog_dir),
        "%s/trainlog",
        base
    );
    if (written < 0 || (size_t)written >= sizeof(trainlog_dir)) {
        return -1;
    }

    if (ensure_directory(trainlog_dir) != 0) {
        return -1;
    }

    written = snprintf(
        output,
        output_size,
        "%s/trainlog.db",
        trainlog_dir
    );

    return written >= 0 && (size_t)written < output_size ? 0 : -1;
}

int main(void)
{
    char database_path[PATH_MAX];
    char database_diagnostic[256];
    TrainlogDatabase *database = NULL;
    TrainlogStatus status;
    int result;

    if (build_database_path(database_path, sizeof(database_path)) != 0) {
        (void)fprintf(stderr, "trainlog: unable to create data directory\n");
        return 1;
    }

    status = trainlog_database_open_with_diagnostic(
        database_path,
        &database,
        database_diagnostic,
        sizeof(database_diagnostic)
    );
    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "trainlog: unable to open database '%s' (status %d): %s\n",
            database_path,
            (int)status,
            database_diagnostic[0] != '\0'
                ? database_diagnostic
                : "no SQLite diagnostic available"
        );
        return 1;
    }

    result = trainlog_tui_run(database);
    trainlog_database_close(database);
    return result;
}
