#ifndef TRAINLOG_EQUIPMENT_CATALOG_H
#define TRAINLOG_EQUIPMENT_CATALOG_H

#include <stddef.h>

/* CONTRACT: data is generated at build time from catalog/equipment-v1.json.
 * Callers borrow returned strings for process lifetime and must not free them. */
typedef struct TrainlogEquipment {
    const char *equipment_id;
    const char *label_name;
    const char *display_name;
    const char *equipment_type;
    const char *load_semantics;
    const char *aliases; /* newline-separated canonical aliases */
} TrainlogEquipment;

typedef struct TrainlogExerciseEquipmentRelation {
    const char *exercise_id;
    const char *equipment_id;
    const char *load_semantics;
} TrainlogExerciseEquipmentRelation;

size_t trainlog_equipment_catalog_count(void);
const TrainlogEquipment *trainlog_equipment_catalog_at(size_t index);
const TrainlogEquipment *trainlog_equipment_catalog_lookup(const char *equipment_id);
/* Search is deterministic and ASCII-case-insensitive over label, display and aliases. */
size_t trainlog_equipment_catalog_search(const char *query, const TrainlogEquipment **output, size_t capacity);
size_t trainlog_equipment_catalog_relation_count(void);
const TrainlogExerciseEquipmentRelation *trainlog_equipment_catalog_relation_at(size_t index);

#endif
