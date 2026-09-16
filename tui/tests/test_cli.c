#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/cli.h"

#define CHECK(value) do { if (!(value)) {                                  \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                 \
        __FILE__, __LINE__, #value); return false; } } while (0)

static bool parses(int argc, char **argv, TrainlogRunMode mode,
    unsigned int port)
{
    TrainlogCliOptions options;
    char diagnostic[128];
    CHECK(trainlog_cli_parse(argc, argv, &options, diagnostic,
        sizeof(diagnostic)) == 0);
    CHECK(diagnostic[0] == '\0');
    CHECK(options.mode == mode);
    CHECK((unsigned int)options.port == port);
    return true;
}

static bool rejects(int argc, char **argv)
{
    TrainlogCliOptions options;
    char diagnostic[128];
    CHECK(trainlog_cli_parse(argc, argv, &options, diagnostic,
        sizeof(diagnostic)) != 0);
    CHECK(diagnostic[0] != '\0');
    return true;
}

int main(void)
{
    char *plain[] = {"trainlog", NULL};
    char *short_web[] = {"trainlog", "-w", NULL};
    char *long_web[] = {"trainlog", "--web", NULL};
    char *port[] = {"trainlog", "-w", "--port", "65535", NULL};
    char *port_first[] = {"trainlog", "--port", "8081", "--web", NULL};
    char *help[] = {"trainlog", "--help", NULL};
    char *version[] = {"trainlog", "--version", NULL};
    char *unknown[] = {"trainlog", "--unknown", NULL};
    char *port_alone[] = {"trainlog", "--port", "8080", NULL};
    char *port_missing[] = {"trainlog", "-w", "--port", NULL};
    char *port_text[] = {"trainlog", "-w", "--port", "abc", NULL};
    char *port_negative[] = {"trainlog", "-w", "--port", "-1", NULL};
    char *port_zero[] = {"trainlog", "-w", "--port", "0", NULL};
    char *port_large[] = {"trainlog", "-w", "--port", "65536", NULL};
    char *duplicate_web[] = {"trainlog", "-w", "--web", NULL};
    char *duplicate_port[] = {"trainlog", "-w", "--port", "80",
        "--port", "81", NULL};
    char *mixed_help[] = {"trainlog", "--help", "--web", NULL};
    char *positional[] = {"trainlog", "extra", NULL};
    return parses(1, plain, TRAINLOG_RUN_TUI, 8080U) &&
        parses(2, short_web, TRAINLOG_RUN_WEB, 8080U) &&
        parses(2, long_web, TRAINLOG_RUN_WEB, 8080U) &&
        parses(4, port, TRAINLOG_RUN_WEB, 65535U) &&
        parses(4, port_first, TRAINLOG_RUN_WEB, 8081U) &&
        parses(2, help, TRAINLOG_RUN_HELP, 8080U) &&
        parses(2, version, TRAINLOG_RUN_VERSION, 8080U) &&
        rejects(2, unknown) && rejects(3, port_alone) &&
        rejects(3, port_missing) && rejects(4, port_text) &&
        rejects(4, port_negative) && rejects(4, port_zero) &&
        rejects(4, port_large) && rejects(3, duplicate_web) &&
        rejects(6, duplicate_port) && rejects(3, mixed_help) &&
        rejects(2, positional) ? 0 : 1;
}
