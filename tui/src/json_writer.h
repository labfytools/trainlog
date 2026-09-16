#ifndef TRAINLOG_JSON_WRITER_H
#define TRAINLOG_JSON_WRITER_H
#include <stdbool.h>
#include <stddef.h>
typedef struct TrainlogJsonWriter { char *data; size_t capacity; size_t size; bool failed; } TrainlogJsonWriter;
void trainlog_json_init(TrainlogJsonWriter *writer, char *data, size_t capacity);
bool trainlog_json_raw(TrainlogJsonWriter *writer, const char *value);
bool trainlog_json_string(TrainlogJsonWriter *writer, const char *value);
bool trainlog_json_size(TrainlogJsonWriter *writer, size_t value);
bool trainlog_json_i64(TrainlogJsonWriter *writer, long long value);
bool trainlog_json_double(TrainlogJsonWriter *writer, double value);
#endif
