#ifndef TRAINLOG_DASHBOARD_LAYOUT_H
#define TRAINLOG_DASHBOARD_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT 7U
#define TRAINLOG_DASHBOARD_LAYOUT_COLUMNS 12U
#define TRAINLOG_DASHBOARD_LAYOUT_MAX_Y 200U
#define TRAINLOG_DASHBOARD_LAYOUT_JSON_CAPACITY 4096U
#define TRAINLOG_DASHBOARD_LAYOUT_MAX_REVISION 9007199254740991ULL

typedef struct TrainlogDashboardTileLayout {
    char id[32];
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} TrainlogDashboardTileLayout;

typedef struct TrainlogDashboardLayout {
    uint64_t revision;
    TrainlogDashboardTileLayout tiles[TRAINLOG_DASHBOARD_LAYOUT_TILE_COUNT];
} TrainlogDashboardLayout;

typedef enum TrainlogDashboardLayoutSource {
    TRAINLOG_DASHBOARD_LAYOUT_DEFAULT = 0,
    TRAINLOG_DASHBOARD_LAYOUT_PERSISTED,
    TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED
} TrainlogDashboardLayoutSource;

typedef enum TrainlogDashboardLayoutResult {
    TRAINLOG_DASHBOARD_LAYOUT_OK = 0,
    TRAINLOG_DASHBOARD_LAYOUT_INVALID,
    TRAINLOG_DASHBOARD_LAYOUT_CONFLICT,
    TRAINLOG_DASHBOARD_LAYOUT_IO_ERROR
} TrainlogDashboardLayoutResult;

/* CONTRACT: this is the sole backend owner of the versioned Dashboard UI
 * preference. It never accesses SQLite. Callers own all buffers and layouts.
 * Revision zero denotes the default/no-valid-file state. */
void trainlog_dashboard_layout_default(TrainlogDashboardLayout *layout);
bool trainlog_dashboard_layout_validate(const TrainlogDashboardLayout *layout);
TrainlogDashboardLayoutResult trainlog_dashboard_layout_parse(
    const char *json, size_t size, TrainlogDashboardLayout *layout);
TrainlogDashboardLayoutResult trainlog_dashboard_layout_load(
    TrainlogDashboardLayout *layout, TrainlogDashboardLayoutSource *source);
TrainlogDashboardLayoutResult trainlog_dashboard_layout_save(
    const TrainlogDashboardLayout *layout, uint64_t expected_revision,
    TrainlogDashboardLayout *saved);
TrainlogDashboardLayoutResult trainlog_dashboard_layout_delete(
    uint64_t expected_revision);
bool trainlog_dashboard_layout_serialize(const TrainlogDashboardLayout *layout,
    TrainlogDashboardLayoutSource source, bool include_source,
    char *output, size_t capacity, size_t *size);
bool trainlog_dashboard_layout_path(char *output, size_t capacity);

#endif
