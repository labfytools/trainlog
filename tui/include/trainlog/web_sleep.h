#ifndef TRAINLOG_WEB_SLEEP_H
#define TRAINLOG_WEB_SLEEP_H

#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

#define TRAINLOG_WEB_SLEEP_BYTES_MAX (512U * 1024U)

/* Successful calls transfer one free()-owned UTF-8 JSON allocation. */
TrainlogStatus trainlog_web_sleep_list_json(TrainlogDatabase *database,
                                            const char *start_date,
                                            const char *end_date,
                                            size_t limit,
                                            char **output_json,
                                            size_t *output_size);

/* expected_revision is NULL only for creation. The complete snapshot is
 * validated before the Core starts its transaction. */
TrainlogStatus trainlog_web_sleep_save_json(TrainlogDatabase *database,
                                            const char *body,
                                            size_t body_size,
                                            char **output_json,
                                            size_t *output_size);

TrainlogStatus trainlog_web_sleep_delete_json(TrainlogDatabase *database,
                                              const char *entry_id,
                                              const char *expected_revision,
                                              const char *deleted_at,
                                              char **output_json,
                                              size_t *output_size);

TrainlogStatus trainlog_web_sleep_delete_request_json(TrainlogDatabase *database,
                                                      const char *body,
                                                      size_t body_size,
                                                      char **output_json,
                                                      size_t *output_size);

TrainlogStatus trainlog_web_sleep_validate_json(TrainlogDatabase *database,
                                                const char *body,
                                                size_t body_size,
                                                char **output_json,
                                                size_t *output_size);

TrainlogStatus trainlog_web_medication_list_json(TrainlogDatabase *database,
                                                 char **output_json,
                                                 size_t *output_size);
TrainlogStatus trainlog_web_medication_save_json(TrainlogDatabase *database,
                                                 const char *body,
                                                 size_t body_size,
                                                 char **output_json,
                                                 size_t *output_size);

#endif
