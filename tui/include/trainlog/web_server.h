#ifndef TRAINLOG_WEB_SERVER_H
#define TRAINLOG_WEB_SERVER_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/database.h"

/* Serialize the exact successful sync-admission document. Both identities are
 * borrowed, validated sy_<uuid-v4> values. On failure output is empty. */
bool trainlog_web_sync_accepted_serialize(
    const char *request_id, const char *run_id, char *output, size_t capacity, size_t *output_size);

/* Blocking, single-threaded local HTTP adapter. Port zero is reserved for
 * controlled test harnesses and is rejected by the CLI. The database remains
 * borrowed and process-owned; this infrastructure route never reads it. */
int trainlog_web_server_run(TrainlogDatabase *database,
                            const char *database_path,
                            uint16_t port,
                            const volatile sig_atomic_t *stop_requested,
                            char *diagnostic,
                            size_t diagnostic_capacity);

#endif
