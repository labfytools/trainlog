#ifndef TRAINLOG_WEB_SERVER_H
#define TRAINLOG_WEB_SERVER_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/database.h"

/* Blocking, single-threaded local HTTP adapter. Port zero is reserved for
 * controlled test harnesses and is rejected by the CLI. The database remains
 * borrowed and process-owned; this infrastructure route never reads it. */
int trainlog_web_server_run(TrainlogDatabase *database, const char *database_path,
    uint16_t port,
    const volatile sig_atomic_t *stop_requested,
    char *diagnostic, size_t diagnostic_capacity);

#endif
