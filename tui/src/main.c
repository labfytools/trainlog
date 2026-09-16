/** @file main.c @brief Trainlog process owner and interface dispatcher. */

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "trainlog/cli.h"
#include "trainlog/database.h"
#include "trainlog/paths.h"
#include "trainlog/tui.h"
#include "trainlog/web_server.h"

static volatile sig_atomic_t web_stop_requested;

static void request_web_stop(int signal_number)
{
    (void)signal_number;
    web_stop_requested = 1;
}

static int run_web(TrainlogDatabase *database, uint16_t port)
{
    struct sigaction action;
    struct sigaction previous_interrupt;
    struct sigaction previous_terminate;
    char diagnostic[256];
    int result;
    (void)memset(&action, 0, sizeof(action));
    action.sa_handler = request_web_stop;
    (void)sigemptyset(&action.sa_mask);
    web_stop_requested = 0;
    if (sigaction(SIGINT, &action, &previous_interrupt) != 0) {
        (void)fprintf(stderr,
            "trainlog: impossible d'installer les gestionnaires de signaux\n");
        return 1;
    }
    if (sigaction(SIGTERM, &action, &previous_terminate) != 0) {
        (void)sigaction(SIGINT, &previous_interrupt, NULL);
        (void)fprintf(stderr,
            "trainlog: impossible d'installer les gestionnaires de signaux\n");
        return 1;
    }
    result = trainlog_web_server_run(database, port, &web_stop_requested,
        diagnostic, sizeof(diagnostic));
    (void)sigaction(SIGINT, &previous_interrupt, NULL);
    (void)sigaction(SIGTERM, &previous_terminate, NULL);
    if (result != 0) {
        (void)fprintf(stderr, "trainlog: serveur Web : %s\n",
            diagnostic[0] == '\0' ? "échec non détaillé" : diagnostic);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char database_path[PATH_MAX];
    char path_diagnostic[256];
    char database_diagnostic[256];
    char cli_diagnostic[256];
    TrainlogCliOptions options;
    TrainlogDatabase *database = NULL;
    TrainlogStatus status;
    int result;

    if (trainlog_cli_parse(argc, argv, &options, cli_diagnostic,
            sizeof(cli_diagnostic)) != 0) {
        (void)fprintf(stderr, "trainlog: %s\n", cli_diagnostic);
        trainlog_cli_print_usage(argv[0]);
        return 2;
    }
    if (options.mode == TRAINLOG_RUN_HELP) {
        trainlog_cli_print_usage(argv[0]);
        return 0;
    }
    if (options.mode == TRAINLOG_RUN_VERSION) {
        (void)printf("trainlog %s\n", TRAINLOG_VERSION);
        return 0;
    }
    /* CONTRACT: newly created data directories and SQLite files are private
     * to the local user, independently of the invoking shell's umask. */
    (void)umask(0077);
    if (trainlog_paths_database(database_path, sizeof(database_path),
            path_diagnostic, sizeof(path_diagnostic)) != 0) {
        (void)fprintf(stderr, "trainlog: chemin de données : %s\n",
            path_diagnostic);
        return 1;
    }
    status = trainlog_database_open_with_diagnostic(database_path, &database,
        database_diagnostic, sizeof(database_diagnostic));
    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(stderr,
            "trainlog: impossible d'ouvrir la base '%s' (statut %d) : %s\n",
            database_path, (int)status,
            database_diagnostic[0] == '\0'
                ? "diagnostic SQLite indisponible" : database_diagnostic);
        return 1;
    }
    if (chmod(database_path, 0600) != 0) {
        (void)fprintf(stderr,
            "trainlog: impossible de protéger les permissions de la base\n");
        trainlog_database_close(database);
        return 1;
    }
    result = options.mode == TRAINLOG_RUN_WEB
        ? run_web(database, options.port)
        : trainlog_tui_run(database);
    trainlog_database_close(database);
    return result;
}
