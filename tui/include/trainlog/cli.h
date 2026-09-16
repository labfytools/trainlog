#ifndef TRAINLOG_CLI_H
#define TRAINLOG_CLI_H

#include <stddef.h>
#include <stdint.h>

typedef enum TrainlogRunMode {
    TRAINLOG_RUN_TUI = 0,
    TRAINLOG_RUN_WEB,
    TRAINLOG_RUN_HELP,
    TRAINLOG_RUN_VERSION
} TrainlogRunMode;

typedef struct TrainlogCliOptions {
    TrainlogRunMode mode;
    uint16_t port;
} TrainlogCliOptions;

/* CONTRACT: parsing is side-effect free apart from getopt's process-local
 * state, which is reset for every call. No database, terminal or server is
 * initialized. diagnostic is always NUL-terminated when capacity is nonzero. */
int trainlog_cli_parse(int argc, char *const argv[], TrainlogCliOptions *options,
    char *diagnostic, size_t diagnostic_capacity);

void trainlog_cli_print_usage(const char *program_name);

#endif
