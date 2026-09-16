#include "web_assets.h"

bool trainlog_web_assets_available(void)
{
    return false;
}

const TrainlogWebAsset *trainlog_web_asset_find(const char *path)
{
    (void)path;
    return NULL;
}

const TrainlogWebAsset *trainlog_web_index_asset(void)
{
    return NULL;
}
