#!/usr/bin/env python3
"""Validate body-zones-v1 and generate its bounded C representation."""

import json
import re
import sys


source, output = sys.argv[1:]
with open(source, encoding="utf-8") as handle:
    root = json.load(handle)

if set(root) != {"format", "version", "zones", "exercise_mappings"} or \
        root.get("format") != "trainlog-body-zone-catalog" or root.get("version") != 1 or \
        isinstance(root.get("version"), bool):
    raise SystemExit("unsupported body-zone catalogue")
zones = root.get("zones")
if not isinstance(zones, list) or not zones:
    raise SystemExit("zones must be a non-empty list")

zone_ids = set()
orders = set()
by_id = {}
for zone in zones:
    if set(zone) != {"zone_id", "display_name", "parent_zone_id", "sort_order", "kind"}:
        raise SystemExit("invalid body-zone entry shape")
    zone_id = zone["zone_id"]
    if not isinstance(zone_id, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", zone_id):
        raise SystemExit("invalid body-zone id")
    if zone_id in zone_ids or zone["sort_order"] in orders:
        raise SystemExit("duplicate body-zone id or sort_order")
    if not isinstance(zone["display_name"], str) or not zone["display_name"].strip():
        raise SystemExit("invalid body-zone display_name")
    if not isinstance(zone["sort_order"], int) or isinstance(zone["sort_order"], bool) or zone["sort_order"] < 0:
        raise SystemExit("invalid body-zone sort_order")
    if zone["kind"] not in {"group", "leaf", "standalone"}:
        raise SystemExit("invalid body-zone kind")
    zone_ids.add(zone_id)
    orders.add(zone["sort_order"])
    by_id[zone_id] = zone

for zone in zones:
    parent = zone["parent_zone_id"]
    if parent is not None and parent not in zone_ids:
        raise SystemExit("body-zone parent does not exist")
    seen = {zone["zone_id"]}
    while parent is not None:
        if parent in seen:
            raise SystemExit("body-zone hierarchy contains a cycle")
        seen.add(parent)
        parent = by_id[parent]["parent_zone_id"]
    children = [item for item in zones if item["parent_zone_id"] == zone["zone_id"]]
    if (zone["kind"] == "group") != bool(children):
        raise SystemExit("body-zone group/leaf declaration disagrees with hierarchy")

if by_id.get("full_body", {}).get("parent_zone_id") is not None or \
        by_id.get("full_body", {}).get("kind") != "standalone":
    raise SystemExit("full_body must be autonomous")
if by_id.get("upper_body", {}).get("kind") != "group" or by_id.get("lower_body", {}).get("kind") != "group":
    raise SystemExit("upper_body and lower_body must be groups")

mappings = root["exercise_mappings"]
if not isinstance(mappings, list):
    raise SystemExit("exercise_mappings must be a list")
seen_exercises = set()
for mapping in mappings:
    if set(mapping) != {"exercise_id", "exercise_name", "primary_zone_id", "secondary_zone_ids", "decision_source"}:
        raise SystemExit("invalid exercise body-zone mapping shape")
    exercise_id = mapping["exercise_id"]
    secondary = mapping["secondary_zone_ids"]
    if not isinstance(exercise_id, str) or re.fullmatch(
            r"ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
            exercise_id,
    ) is None or exercise_id in seen_exercises:
        raise SystemExit("duplicate or invalid mapped exercise_id")
    if not isinstance(mapping["exercise_name"], str) or not mapping["exercise_name"].strip():
        raise SystemExit("mapping lacks exercise name")
    if not isinstance(mapping["primary_zone_id"], str) or \
            mapping["primary_zone_id"] not in zone_ids or \
            by_id[mapping["primary_zone_id"]]["kind"] == "group" or \
            not isinstance(secondary, list):
        raise SystemExit("mapping references unknown primary zone")
    if any(not isinstance(item, str) for item in secondary) or \
            len(secondary) != len(set(secondary)) or \
            any(item not in zone_ids or by_id[item]["kind"] == "group" for item in secondary):
        raise SystemExit("mapping references duplicate/unknown secondary zone")
    if mapping["primary_zone_id"] in secondary:
        raise SystemExit("mapping repeats primary as secondary")
    if not isinstance(mapping["decision_source"], str) or not mapping["decision_source"].strip():
        raise SystemExit("mapping lacks decision source")
    seen_exercises.add(exercise_id)


def c(value):
    return json.dumps(value, ensure_ascii=False)


with open(output, "w", encoding="utf-8") as generated:
    generated.write('#include "trainlog/body_zone_catalog.h"\n#include <string.h>\n\n')
    generated.write("static const TrainlogBodyZone zones[] = {\n")
    for zone in sorted(zones, key=lambda item: item["sort_order"]):
        parent = "NULL" if zone["parent_zone_id"] is None else c(zone["parent_zone_id"])
        generated.write("  {%s, %s, %s, %d, %s},\n" % (
            c(zone["zone_id"]), c(zone["display_name"]), parent,
            zone["sort_order"], "true" if zone["kind"] == "group" else "false"))
    generated.write("};\nstatic const TrainlogBodyZoneInitialMapping mappings[] = {\n")
    for mapping in mappings:
        generated.write("  {%s, %s, %s, %s, %s},\n" % (
            c(mapping["exercise_id"]), c(mapping["exercise_name"]),
            c(mapping["primary_zone_id"]), c("\n".join(mapping["secondary_zone_ids"])),
            c(mapping["decision_source"])))
    generated.write("};\n")
    generated.write(r'''
size_t trainlog_body_zone_catalog_count(void) { return sizeof(zones) / sizeof(zones[0]); }
const TrainlogBodyZone *trainlog_body_zone_catalog_at(size_t index) {
  return index < trainlog_body_zone_catalog_count() ? &zones[index] : NULL;
}
const TrainlogBodyZone *trainlog_body_zone_catalog_lookup(const char *zone_id) {
  size_t index;
  if (zone_id == NULL) return NULL;
  for (index = 0; index < trainlog_body_zone_catalog_count(); ++index)
    if (strcmp(zones[index].zone_id, zone_id) == 0) return &zones[index];
  return NULL;
}
size_t trainlog_body_zone_catalog_children(const char *zone_id, const TrainlogBodyZone **output, size_t capacity) {
  size_t index, count = 0;
  if (zone_id == NULL || (capacity > 0 && output == NULL)) return 0;
  for (index = 0; index < trainlog_body_zone_catalog_count(); ++index) {
    if (zones[index].parent_zone_id != NULL && strcmp(zones[index].parent_zone_id, zone_id) == 0) {
      if (count < capacity) output[count] = &zones[index];
      ++count;
    }
  }
  return count;
}
size_t trainlog_body_zone_catalog_ancestors(const char *zone_id, const TrainlogBodyZone **output, size_t capacity) {
  const TrainlogBodyZone *current = trainlog_body_zone_catalog_lookup(zone_id);
  size_t count = 0;
  if (current == NULL || (capacity > 0 && output == NULL)) return 0;
  while (current->parent_zone_id != NULL) {
    current = trainlog_body_zone_catalog_lookup(current->parent_zone_id);
    if (current == NULL) return 0;
    if (count < capacity) output[count] = current;
    ++count;
  }
  return count;
}
bool trainlog_body_zone_catalog_is_descendant(const char *zone_id, const char *ancestor_zone_id) {
  const TrainlogBodyZone *current = trainlog_body_zone_catalog_lookup(zone_id);
  if (current == NULL || ancestor_zone_id == NULL) return false;
  while (current->parent_zone_id != NULL) {
    if (strcmp(current->parent_zone_id, ancestor_zone_id) == 0) return true;
    current = trainlog_body_zone_catalog_lookup(current->parent_zone_id);
    if (current == NULL) return false;
  }
  return false;
}
size_t trainlog_body_zone_initial_mapping_count(void) { return sizeof(mappings) / sizeof(mappings[0]); }
const TrainlogBodyZoneInitialMapping *trainlog_body_zone_initial_mapping_at(size_t index) {
  return index < trainlog_body_zone_initial_mapping_count() ? &mappings[index] : NULL;
}
''')
