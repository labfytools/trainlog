#ifndef TRAINLOG_BODY_ZONE_CATALOG_H
#define TRAINLOG_BODY_ZONE_CATALOG_H

#include <stdbool.h>
#include <stddef.h>

/* CONTRACT: strings are generated from catalog/body-zones-v1.json and are
 * borrowed for process lifetime. Stable zone_id values, never display names,
 * cross persistence and synchronization boundaries. */
typedef struct TrainlogBodyZone {
    const char *zone_id;
    const char *display_name;
    const char *parent_zone_id;
    int sort_order;
    bool is_group;
} TrainlogBodyZone;

typedef struct TrainlogBodyZoneInitialMapping {
    const char *exercise_id;
    const char *exercise_name;
    const char *primary_zone_id;
    const char *secondary_zone_ids; /* newline-separated canonical IDs */
    const char *decision_source;
} TrainlogBodyZoneInitialMapping;

/* All returned catalogue pointers are borrowed and remain valid for process
 * lifetime. A zero count from children/ancestors is either an empty relation
 * or invalid input; callers can distinguish unknown IDs with lookup(). */
size_t trainlog_body_zone_catalog_count(void);
const TrainlogBodyZone *trainlog_body_zone_catalog_at(size_t index);
const TrainlogBodyZone *trainlog_body_zone_catalog_lookup(const char *zone_id);
size_t trainlog_body_zone_catalog_children(
    const char *zone_id,
    const TrainlogBodyZone **output,
    size_t capacity
);
size_t trainlog_body_zone_catalog_ancestors(
    const char *zone_id,
    const TrainlogBodyZone **output,
    size_t capacity
);
bool trainlog_body_zone_catalog_is_descendant(
    const char *zone_id,
    const char *ancestor_zone_id
);
/* Initial mappings are migration evidence, not a mutable runtime catalogue. */
size_t trainlog_body_zone_initial_mapping_count(void);
const TrainlogBodyZoneInitialMapping *trainlog_body_zone_initial_mapping_at(size_t index);

#endif
