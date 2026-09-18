/* Trainlog Web Sessions deletion command service. */
#ifndef TRAINLOG_WEB_SESSION_DELETIONS_H
#define TRAINLOG_WEB_SESSION_DELETIONS_H

#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

/*
 * CONTRACT: kind is proposal, draft, or history. Preparation withdrawal keeps
 * its separate delivery-aware service. expected_revision and request_id are
 * required bounded strings. Exact request replay returns the original JSON.
 * No performed session is deleted by proposal or draft removal.
 *
 * On success output_json transfers one heap allocation to the caller, which
 * must release it with free(). No allocation is returned on failure.
 */
TrainlogStatus trainlog_web_session_delete_json(TrainlogDatabase *database,
                                                const char *kind,
                                                const char *target_id,
                                                const char *expected_revision,
                                                const char *request_id,
                                                char **output_json,
                                                size_t *output_size);

#endif
