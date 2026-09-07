#!/usr/bin/env python3
"""Generate the desktop C catalogue from the one versioned JSON source."""
import json
import re
import sys

src, out = sys.argv[1:]
root = json.load(open(src, encoding="utf-8"))
if root.get("format") != "trainlog-equipment-catalog" or root.get("version") != 1:
    raise SystemExit("unsupported equipment catalogue")
items = root.get("equipment")
if not isinstance(items, list):
    raise SystemExit("equipment must be a list")
ids = set()
for item in items:
    required = ("id", "label_name", "display_name", "aliases", "type", "load_semantics")
    if not all(key in item for key in required) or not isinstance(item["aliases"], list):
        raise SystemExit("invalid equipment entry")
    if not item["id"] or item["id"] in ids:
        raise SystemExit("empty or duplicate equipment id")
    ids.add(item["id"])
    if item["load_semantics"] not in {"external", "assistance", "bodyweight", "cardio"}:
        raise SystemExit("invalid load semantics")
for relation in root.get("exercise_equipment", []):
    if relation.get("equipment_id") not in ids:
        raise SystemExit("relation references unknown equipment")

def c(value):
    return json.dumps(value, ensure_ascii=False)

with open(out, "w", encoding="utf-8") as f:
    f.write('#include "trainlog/equipment_catalog.h"\n#include <string.h>\n\n')
    f.write('static const TrainlogEquipment entries[] = {\n')
    for item in items:
        f.write('  {%s, %s, %s, %s, %s, %s},\n' % (
            c(item['id']), c(item['label_name']), c(item['display_name']),
            c(item['type']), c(item['load_semantics']), c('\n'.join(item['aliases']))))
    f.write('};\nstatic const TrainlogExerciseEquipmentRelation relations[] = {\n')
    for rel in root.get('exercise_equipment', []):
        f.write('  {%s, %s, %s},\n' % (c(rel['exercise_id']), c(rel['equipment_id']), c(rel['load_semantics'])))
    f.write('};\n')
    f.write(r'''
static int folded_contains(const char *haystack, const char *needle) {
  size_t i, j;
  if (needle[0] == '\0') return 1;
  for (i = 0; haystack[i] != '\0'; ++i) {
    for (j = 0; needle[j] != '\0'; ++j) {
      unsigned char a = (unsigned char)haystack[i + j];
      unsigned char b = (unsigned char)needle[j];
      if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
      if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
      if (a == '\0' || a != b) break;
    }
    if (needle[j] == '\0') return 1;
  }
  return 0;
}
size_t trainlog_equipment_catalog_count(void) { return sizeof(entries) / sizeof(entries[0]); }
const TrainlogEquipment *trainlog_equipment_catalog_at(size_t i) { return i < trainlog_equipment_catalog_count() ? &entries[i] : NULL; }
const TrainlogEquipment *trainlog_equipment_catalog_lookup(const char *id) {
  size_t i; if (id == NULL) return NULL;
  for (i = 0; i < trainlog_equipment_catalog_count(); ++i) if (strcmp(entries[i].equipment_id, id) == 0) return &entries[i];
  return NULL;
}
size_t trainlog_equipment_catalog_search(const char *q, const TrainlogEquipment **out, size_t cap) {
  size_t i, found = 0; if (q == NULL || out == NULL) return 0;
  for (i = 0; i < trainlog_equipment_catalog_count(); ++i) if (folded_contains(entries[i].label_name, q) || folded_contains(entries[i].display_name, q) || folded_contains(entries[i].aliases, q)) { if (found < cap) out[found] = &entries[i]; ++found; }
  return found;
}
size_t trainlog_equipment_catalog_relation_count(void) { return sizeof(relations) / sizeof(relations[0]); }
const TrainlogExerciseEquipmentRelation *trainlog_equipment_catalog_relation_at(size_t i) { return i < trainlog_equipment_catalog_relation_count() ? &relations[i] : NULL; }
''')
