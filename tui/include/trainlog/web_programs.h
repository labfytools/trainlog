/* Trainlog Web Program V1 application service. */
#ifndef TRAINLOG_WEB_PROGRAMS_H
#define TRAINLOG_WEB_PROGRAMS_H

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

#define TRAINLOG_WEB_PROGRAM_BYTES_MAX (512U * 1024U)
#define TRAINLOG_WEB_PROGRAM_SESSIONS_MAX 64U
#define TRAINLOG_WEB_PROGRAM_OCCURRENCES_MAX 64U

/* CONTRACT: successful JSON functions transfer one free()-owned allocation. */
TrainlogStatus trainlog_web_programs_list_json(TrainlogDatabase *database,
                                               const char *search,
                                               const char *state,
                                               size_t offset,
                                               size_t limit,
                                               char **output_json,
                                               size_t *output_size);

TrainlogStatus trainlog_web_programs_detail_json(TrainlogDatabase *database,
                                                 const char *program_id,
                                                 char **output_json,
                                                 size_t *output_size);

/* CONTRACT: commit=false performs the same strict validation without writes.
 * commit=true imports the complete document in one immediate transaction. */
TrainlogStatus trainlog_web_programs_import_json(TrainlogDatabase *database,
                                                 const char *body,
                                                 size_t body_size,
                                                 bool commit,
                                                 char **output_json,
                                                 size_t *output_size);

/* CONTRACT: expected_revision prevents stale archive decisions; request_id
 * makes a lost successful response safe to replay. */
TrainlogStatus trainlog_web_programs_archive_json(TrainlogDatabase *database,
                                                  const char *program_id,
                                                  const char *expected_revision,
                                                  const char *request_id,
                                                  char **output_json,
                                                  size_t *output_size);

/* Creates a separate draft preparation and never changes or delivers the
 * source program. request_id is forwarded to the preparation service. */
TrainlogStatus trainlog_web_programs_prepare_json(TrainlogDatabase *database,
                                                  const char *program_id,
                                                  const char *program_session_id,
                                                  const char *request_id,
                                                  char **output_json,
                                                  size_t *output_size);

#endif
