#include "json_writer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <utf8proc.h>

static bool append(TrainlogJsonWriter *w, const char *s, size_t n) {
    if (w == NULL || s == NULL || w->failed ||
        n > w->capacity - (w->size < w->capacity ? w->size : w->capacity) - 1U) {
        if (w != NULL) {
            w->failed = true;
        }
        return false;
    }
    (void)memcpy(w->data + w->size, s, n);
    w->size += n;
    w->data[w->size] = '\0';
    return true;
}
void trainlog_json_init(TrainlogJsonWriter *w, char *data, size_t capacity) {
    if (w == NULL) {
        return;
    }
    w->data = data;
    w->capacity = capacity;
    w->size = 0U;
    w->failed = data == NULL || capacity == 0U;
    if (!w->failed) {
        data[0] = '\0';
    }
}
bool trainlog_json_raw(TrainlogJsonWriter *w, const char *v) {
    return v != NULL && append(w, v, strlen(v));
}
bool trainlog_json_string(TrainlogJsonWriter *w, const char *v) {
    const unsigned char *p = (const unsigned char *)v;
    char escape[7];
    if (v == NULL || !append(w, "\"", 1U)) {
        return false;
    }
    while (*p != '\0') {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t sequence = utf8proc_iterate(p, -1, &codepoint);
        /* CONTRACT: valid UTF-8 is preserved byte-for-byte; corrupt storage
         * must fail serialization instead of producing invalid JSON. */
        (void)codepoint;
        if (sequence <= 0) {
            w->failed = true;
            return false;
        }
        if (*p == '\"' || *p == '\\') {
            escape[0] = '\\';
            escape[1] = (char)*p;
            if (!append(w, escape, 2U)) {
                return false;
            }
        } else if (*p == '\b' || *p == '\f' || *p == '\n' || *p == '\r' || *p == '\t') {
            const char codes[] = "bfnrt";
            const char chars[] = "\b\f\n\r\t";
            size_t i;
            escape[0] = '\\';
            for (i = 0U; i < 5U && chars[i] != (char)*p; ++i) {
            }
            escape[1] = codes[i];
            if (!append(w, escape, 2U)) {
                return false;
            }
        } else if (*p < 0x20U) {
            int n = snprintf(escape, sizeof(escape), "\\u%04x", (unsigned int)*p);
            if (n != 6 || !append(w, escape, 6U)) {
                return false;
            }
        } else if (!append(w, (const char *)p, (size_t)sequence)) {
            return false;
        }
        p += (size_t)sequence;
    }
    return append(w, "\"", 1U);
}
bool trainlog_json_size(TrainlogJsonWriter *w, size_t v) {
    char b[32];
    int n = snprintf(b, sizeof(b), "%zu", v);
    return n > 0 && (size_t)n < sizeof(b) && append(w, b, (size_t)n);
}
bool trainlog_json_i64(TrainlogJsonWriter *w, long long v) {
    char b[32];
    int n = snprintf(b, sizeof(b), "%lld", v);
    return n > 0 && (size_t)n < sizeof(b) && append(w, b, (size_t)n);
}
bool trainlog_json_double(TrainlogJsonWriter *w, double v) {
    char b[64];
    int n;
    if (!isfinite(v)) {
        w->failed = true;
        return false;
    }
    n = snprintf(b, sizeof(b), "%.17g", v);
    return n > 0 && (size_t)n < sizeof(b) && append(w, b, (size_t)n);
}
