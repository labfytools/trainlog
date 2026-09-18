#include "trainlog/web_prepared_items.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <utf8proc.h>

typedef struct JsonWriter {
    char *output;
    size_t capacity;
    size_t length;
    bool failed;
} JsonWriter;

static void append(JsonWriter *writer, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

static void append(JsonWriter *writer, const char *format, ...) {
    va_list arguments;
    int written;

    if (writer->failed || writer->length >= writer->capacity) {
        writer->failed = true;
        return;
    }
    va_start(arguments, format);
    written = vsnprintf(
        writer->output + writer->length, writer->capacity - writer->length, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= writer->capacity - writer->length) {
        writer->failed = true;
        return;
    }
    writer->length += (size_t)written;
}

static bool valid_utf8(const char *value) {
    const utf8proc_uint8_t *cursor = (const utf8proc_uint8_t *)value;

    while (*cursor != '\0') {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t sequence = utf8proc_iterate(cursor, -1, &codepoint);
        (void)codepoint;
        if (sequence <= 0) {
            return false;
        }
        cursor += (size_t)sequence;
    }
    return true;
}

static void string(JsonWriter *writer, const char *value) {
    const unsigned char *cursor = (const unsigned char *)value;

    if (!valid_utf8(value)) {
        writer->failed = true;
        return;
    }
    append(writer, "\"");
    while (!writer->failed && *cursor != '\0') {
        unsigned char byte = *cursor++;

        if (byte == '"' || byte == '\\') {
            append(writer, "\\%c", (int)byte);
        } else if (byte == '\b') {
            append(writer, "\\b");
        } else if (byte == '\f') {
            append(writer, "\\f");
        } else if (byte == '\n') {
            append(writer, "\\n");
        } else if (byte == '\r') {
            append(writer, "\\r");
        } else if (byte == '\t') {
            append(writer, "\\t");
        } else if (byte < 0x20U) {
            append(writer, "\\u%04x", (unsigned int)byte);
        } else {
            append(writer, "%c", (int)byte);
        }
    }
    append(writer, "\"");
}

TrainlogStatus trainlog_web_prepared_items_serialize(
    const TrainlogWebPreparedItems *items,
    char *output,
    size_t capacity,
    size_t *output_size) {
    JsonWriter writer = {output, capacity, 0U, false};
    size_t index;

    if (items == NULL || output == NULL || output_size == NULL || capacity == 0U ||
        items->item_count > TRAINLOG_WEB_PREPARED_ITEM_CAPACITY) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_size = 0U;
    append(&writer, "{\"api_version\":1,\"generated_at\":");
    string(&writer, items->generated_at);
    append(&writer, ",\"partial\":%s,\"items\":[", items->partial ? "true" : "false");
    for (index = 0U; index < items->item_count; ++index) {
        const TrainlogWebPreparedItem *item = &items->items[index];
        const char *kind;

        if (item->kind == TRAINLOG_WEB_PREPARED_AI_PROPOSAL) {
            kind = "ai_proposal";
        } else if (item->kind == TRAINLOG_WEB_PREPARED_EXECUTION_DRAFT) {
            kind = "execution_draft";
        } else if (item->kind == TRAINLOG_WEB_PREPARED_MANUAL_PREPARATION) {
            kind = "manual_preparation";
        } else {
            output[0] = '\0';
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }

        append(&writer, "%s{\"identity\":", index == 0U ? "" : ",");
        string(&writer, item->identity);
        append(&writer, ",\"kind\":");
        string(&writer, kind);
        append(&writer, ",\"title\":");
        string(&writer, item->title);
        append(&writer, ",\"planned_for\":");
        if (item->planned_for[0] == '\0') {
            append(&writer, "null");
        } else {
            string(&writer, item->planned_for);
        }
        append(&writer, ",\"state\":");
        string(&writer, item->state);
        append(&writer, ",\"sort_timestamp\":");
        string(&writer, item->sort_timestamp);
        append(&writer, ",\"occurrence_count\":%zu,\"provenance\":", item->occurrence_count);
        string(&writer, item->provenance);
        append(&writer, "}");
    }
    append(&writer, "]}\n");
    if (writer.failed) {
        if (capacity > 0U) {
            output[0] = '\0';
        }
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    *output_size = writer.length;
    return TRAINLOG_STATUS_OK;
}
