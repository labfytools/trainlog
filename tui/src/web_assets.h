#ifndef TRAINLOG_WEB_ASSETS_H
#define TRAINLOG_WEB_ASSETS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct TrainlogWebAsset {
    const char *path;
    const char *mime;
    const unsigned char *bytes;
    size_t size;
    const char *etag;
    bool immutable;
} TrainlogWebAsset;

/* CONTRACT: generated assets have static process lifetime. Lookups are exact
 * canonical URL matches; callers never derive filesystem paths at runtime. */
bool trainlog_web_assets_available(void);
const TrainlogWebAsset *trainlog_web_asset_find(const char *path);
const TrainlogWebAsset *trainlog_web_index_asset(void);

#endif
