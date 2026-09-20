/* Trainlog Web Exercises V1 application service. */
#ifndef TRAINLOG_WEB_EXERCISES_H
#define TRAINLOG_WEB_EXERCISES_H

#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

#define TRAINLOG_WEB_EXERCISES_PAGE_MAX 64U
#define TRAINLOG_WEB_EXERCISES_BODY_MAX (64U * 1024U)

typedef struct TrainlogWebExercisesQuery {
    size_t offset;
    size_t limit;
    const char *search;
    const char *profile;
    const char *zone_id;
    bool unclassified;
} TrainlogWebExercisesQuery;

/* CONTRACT: successful JSON functions transfer one heap allocation to the
 * caller, which releases it with free(). List and detail expose only current
 * Core state; no SQLite handle, row ID, or borrowed database text escapes. */
TrainlogStatus trainlog_web_exercises_list_json(TrainlogDatabase *database,
                                                const TrainlogWebExercisesQuery *query,
                                                char **output_json,
                                                size_t *output_size);
TrainlogStatus trainlog_web_exercises_detail_json(TrainlogDatabase *database,
                                                  const char *exercise_id,
                                                  char **output_json,
                                                  size_t *output_size);
TrainlogStatus trainlog_web_exercises_zones_json(char **output_json, size_t *output_size);

/* CONTRACT: body is strict JSON containing the complete editable definition.
 * Creation delegates UUID/name/profile/zone authority to catalog/database Core.
 * Update compares expected_revision under BEGIN IMMEDIATE and preserves the
 * stable exercise_id. Neither operation starts synchronization. */
TrainlogStatus trainlog_web_exercises_create_json(TrainlogDatabase *database,
                                                  const char *body,
                                                  size_t body_size,
                                                  char **output_json,
                                                  size_t *output_size);
TrainlogStatus trainlog_web_exercises_update_json(TrainlogDatabase *database,
                                                  const char *exercise_id,
                                                  const char *expected_revision,
                                                  const char *body,
                                                  size_t body_size,
                                                  char **output_json,
                                                  size_t *output_size);

/* CONTRACT: retirement is one durable causal operation. Built-in identities
 * are forbidden, history remains resolvable, and exact repeat requests are
 * idempotent through existing causal state. */
TrainlogStatus trainlog_web_exercises_retire_json(TrainlogDatabase *database,
                                                  const char *exercise_id,
                                                  const char *expected_revision,
                                                  char **output_json,
                                                  size_t *output_size);

#endif
