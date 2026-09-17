/* Trainlog Web Sessions V1 application service. */
#ifndef TRAINLOG_WEB_SESSIONS_H
#define TRAINLOG_WEB_SESSIONS_H

#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

#define TRAINLOG_WEB_SESSIONS_PAGE_MAX 64U
#define TRAINLOG_WEB_SESSIONS_JSON_MAX (512U * 1024U)

typedef enum TrainlogWebSessionCollection {
    TRAINLOG_WEB_SESSION_PREPARATIONS = 0,
    TRAINLOG_WEB_SESSION_PROPOSALS,
    TRAINLOG_WEB_SESSION_DRAFTS,
    TRAINLOG_WEB_SESSION_HISTORY
} TrainlogWebSessionCollection;

typedef struct TrainlogWebSessionsPageQuery {
    TrainlogWebSessionCollection collection;
    size_t offset;
    size_t limit;
    const char *search;
} TrainlogWebSessionsPageQuery;

/*
 * CONTRACT: database and query strings are borrowed for the call. On success,
 * output_json is heap-owned by the caller and must be released with free().
 * A page contains at most TRAINLOG_WEB_SESSIONS_PAGE_MAX rows plus a `more`
 * flag derived from one additional row observed in the same read snapshot.
 */
TrainlogStatus trainlog_web_sessions_list_json(TrainlogDatabase *database,
                                               const TrainlogWebSessionsPageQuery *query,
                                               char **output_json,
                                               size_t *output_size);

/* CONTRACT: kind is one of preparation, proposal, draft, or history. The
 * returned detail is a factual snapshot and performs no lifecycle transition. */
TrainlogStatus trainlog_web_sessions_detail_json(TrainlogDatabase *database,
                                                 const char *kind,
                                                 const char *identity,
                                                 char **output_json,
                                                 size_t *output_size);

/* CONTRACT: the catalogue contains only currently selectable exercises. */
TrainlogStatus trainlog_web_sessions_catalog_json(TrainlogDatabase *database,
                                                  const char *search,
                                                  size_t offset,
                                                  size_t limit,
                                                  char **output_json,
                                                  size_t *output_size);

/*
 * CONTRACT: save_body is a complete revision document and request_id is a
 * durable idempotency key. expected_revision is empty only for creation.
 * Validation and the head comparison occur in one immediate transaction.
 */
TrainlogStatus trainlog_web_sessions_save_json(TrainlogDatabase *database,
                                               const char *preparation_id,
                                               const char *expected_revision,
                                               const char *request_id,
                                               const char *save_body,
                                               size_t save_body_size,
                                               char **output_json,
                                               size_t *output_size);

#endif
