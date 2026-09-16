#include "trainlog/paths.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void path_diagnostic(char *output, size_t capacity, const char *message)
{
    if (output != NULL && capacity > 0U)
        (void)snprintf(output, capacity, "%s", message);
}

static int ensure_private_directory(const char *path)
{
    struct stat info;
    if (mkdir(path, 0700) == 0) return 0;
    if (errno != EEXIST || stat(path, &info) != 0 || !S_ISDIR(info.st_mode))
        return -1;
    return 0;
}

int trainlog_paths_database(char *output, size_t output_capacity,
    char *diagnostic, size_t diagnostic_capacity)
{
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char base[PATH_MAX];
    char trainlog_dir[PATH_MAX];
    int written;
    if (output == NULL || output_capacity == 0U) return -1;
    output[0] = '\0';
    path_diagnostic(diagnostic, diagnostic_capacity, "");
    if (xdg != NULL && xdg[0] != '\0')
        written = snprintf(base, sizeof(base), "%s", xdg);
    else if (home != NULL && home[0] != '\0')
        written = snprintf(base, sizeof(base), "%s/.local/share", home);
    else {
        path_diagnostic(diagnostic, diagnostic_capacity,
            "XDG_DATA_HOME et HOME sont absents");
        return -1;
    }
    if (written < 0 || (size_t)written >= sizeof(base)) {
        path_diagnostic(diagnostic, diagnostic_capacity,
            "chemin de données trop long");
        return -1;
    }
    if (ensure_private_directory(base) != 0) {
        path_diagnostic(diagnostic, diagnostic_capacity,
            "répertoire de données inaccessible");
        return -1;
    }
    written = snprintf(trainlog_dir, sizeof(trainlog_dir), "%s/trainlog", base);
    if (written < 0 || (size_t)written >= sizeof(trainlog_dir) ||
        ensure_private_directory(trainlog_dir) != 0) {
        path_diagnostic(diagnostic, diagnostic_capacity,
            "répertoire Trainlog inaccessible ou trop long");
        return -1;
    }
    if (chmod(trainlog_dir, 0700) != 0) {
        path_diagnostic(diagnostic, diagnostic_capacity,
            "permissions du répertoire Trainlog impossibles à garantir");
        return -1;
    }
    written = snprintf(output, output_capacity, "%s/trainlog.db", trainlog_dir);
    if (written < 0 || (size_t)written >= output_capacity) {
        output[0] = '\0';
        path_diagnostic(diagnostic, diagnostic_capacity,
            "chemin de base de données trop long");
        return -1;
    }
    return 0;
}
