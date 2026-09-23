/* Trainlog loopback Web equipment projection and transactional merge. */
#ifndef TRAINLOG_WEB_EQUIPMENT_H
#define TRAINLOG_WEB_EQUIPMENT_H

#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

/* CONTRACT: returned JSON is heap-owned by the caller and must be freed. */
TrainlogStatus
trainlog_web_equipment_list_json(TrainlogDatabase *database, char **output, size_t *output_size);

/* WHY: duplicate physical-machine identities split historical context and MAX
 * lookup. CONTRACT: only a custom duplicate may be absorbed; every relational
 * reference is repointed in one IMMEDIATE transaction before the definition is
 * deleted. INVARIANT: performed facts, timestamps, provenance and values are
 * never rewritten, a matching causal marker makes exact replay idempotent, and
 * embedded draft JSON causes a conflict rather than a partial or textual
 * rewrite. */
TrainlogStatus trainlog_web_equipment_merge_json(TrainlogDatabase *database,
                                                 const char *input,
                                                 size_t input_size,
                                                 char **output,
                                                 size_t *output_size);

#endif
