#include "trainlog/cli.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRAINLOG_WEB_DEFAULT_PORT 8080U

static void cli_diagnostic(char *output, size_t capacity, const char *message)
{
    if (output != NULL && capacity > 0U)
        (void)snprintf(output, capacity, "%s", message);
}

static int parse_port(const char *value, uint16_t *port)
{
    char *end = NULL;
    unsigned long parsed;
    if (value == NULL || value[0] == '\0' || value[0] == '-') return -1;
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0UL ||
        parsed > 65535UL) return -1;
    *port = (uint16_t)parsed;
    return 0;
}

int trainlog_cli_parse(int argc, char *const argv[], TrainlogCliOptions *options,
    char *diagnostic, size_t diagnostic_capacity)
{
    static const struct option long_options[] = {
        {"web", no_argument, NULL, 'w'},
        {"port", required_argument, NULL, 1},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {NULL, 0, NULL, 0}
    };
    bool web_seen = false;
    bool port_seen = false;
    bool help_seen = false;
    bool version_seen = false;
    int option;
    if (options == NULL || argc < 1 || argv == NULL) return -1;
    options->mode = TRAINLOG_RUN_TUI;
    options->port = TRAINLOG_WEB_DEFAULT_PORT;
    cli_diagnostic(diagnostic, diagnostic_capacity, "");
    opterr = 0;
    optind = 0;
    while ((option = getopt_long(argc, argv, "whV", long_options, NULL)) != -1) {
        switch (option) {
            case 'w':
                if (web_seen) {
                    cli_diagnostic(diagnostic, diagnostic_capacity,
                        "l'option Web est répétée");
                    return -1;
                }
                web_seen = true;
                break;
            case 1:
                if (port_seen) {
                    cli_diagnostic(diagnostic, diagnostic_capacity,
                        "l'option --port est répétée");
                    return -1;
                }
                if (parse_port(optarg, &options->port) != 0) {
                    cli_diagnostic(diagnostic, diagnostic_capacity,
                        "--port attend un entier compris entre 1 et 65535");
                    return -1;
                }
                port_seen = true;
                break;
            case 'h': help_seen = true; break;
            case 'V': version_seen = true; break;
            case '?':
            default:
                cli_diagnostic(diagnostic, diagnostic_capacity,
                    "option inconnue ou argument manquant");
                return -1;
        }
    }
    if (optind != argc) {
        cli_diagnostic(diagnostic, diagnostic_capacity,
            "aucun argument positionnel n'est accepté");
        return -1;
    }
    if ((help_seen && version_seen) ||
        ((help_seen || version_seen) && (web_seen || port_seen))) {
        cli_diagnostic(diagnostic, diagnostic_capacity,
            "--help et --version doivent être utilisés seuls");
        return -1;
    }
    if (port_seen && !web_seen) {
        cli_diagnostic(diagnostic, diagnostic_capacity,
            "--port n'est valide qu'avec -w ou --web");
        return -1;
    }
    if (help_seen) options->mode = TRAINLOG_RUN_HELP;
    else if (version_seen) options->mode = TRAINLOG_RUN_VERSION;
    else if (web_seen) options->mode = TRAINLOG_RUN_WEB;
    return 0;
}

void trainlog_cli_print_usage(const char *program_name)
{
    const char *name = program_name != NULL && program_name[0] != '\0'
        ? program_name : "trainlog";
    (void)printf(
        "Usage : %s [--help | --version | -w|--web [--port PORT]]\n"
        "  sans option       lancer la TUI\n"
        "  -w, --web         lancer le serveur Web local\n"
        "  --port PORT       port Web explicite (1..65535, défaut 8080)\n"
        "  --help            afficher cette aide\n"
        "  --version         afficher la version\n", name);
}
