#ifndef TRAINLOG_WEB_HEART_RATE_H
#define TRAINLOG_WEB_HEART_RATE_H

#include <stddef.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

#define TRAINLOG_WEB_HEART_RATE_JSON_CAPACITY (16U * 1024U * 1024U)

TrainlogStatus trainlog_web_heart_rate_timeline_json(TrainlogDatabase *database,
                                                     const char *context_id,
                                                     char *output,
                                                     size_t capacity,
                                                     size_t *output_size);

#endif
