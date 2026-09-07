#include <assert.h>
#include <string.h>

#include "trainlog/equipment_catalog.h"

int main(void) {
    const TrainlogEquipment *matches[8];
    const TrainlogEquipment *machine;
    size_t count;

    assert(trainlog_equipment_catalog_count() == 38U);
    machine = trainlog_equipment_catalog_lookup("assisted_dip_chin_machine");
    assert(machine != NULL);
    assert(strcmp(machine->load_semantics, "assistance") == 0);
    count = trainlog_equipment_catalog_search("TIRAGE", matches, 8U);
    assert(count >= 6U);
    count = trainlog_equipment_catalog_search("ischio", matches, 8U);
    assert(count >= 2U);
    assert(trainlog_equipment_catalog_relation_count() == 2U);
    assert(strcmp(trainlog_equipment_catalog_relation_at(0U)->equipment_id,
                  "assisted_dip_chin_machine") == 0);
    assert(trainlog_equipment_catalog_lookup("missing") == NULL);
    return 0;
}
