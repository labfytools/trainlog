#ifndef TRAINLOG_INTERNAL_TIMESTAMP_H
#define TRAINLOG_INTERNAL_TIMESTAMP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct TrainlogTimestampKey {
    int64_t utc_second;
    const char *fraction;
    size_t fraction_length;
} TrainlogTimestampKey;

/* CONTRACT: Parses the frozen Trainlog timestamp grammar from exactly length
 * bytes. The key borrows fraction storage from value and remains valid only
 * while value does. */
bool trainlog_timestamp_parse(
    const char *value,
    size_t length,
    TrainlogTimestampKey *output
);

/* Returns less than, zero, or greater than zero by exact represented instant. */
int trainlog_timestamp_compare(
    const TrainlogTimestampKey *left,
    const TrainlogTimestampKey *right
);

#endif
