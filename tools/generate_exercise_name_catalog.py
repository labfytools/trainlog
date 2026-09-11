#!/usr/bin/env python3
"""Validate exercise-names-v1 and generate its bounded C lookup table."""

import json
import re
import sys


source, output = sys.argv[1:]
with open(source, encoding="utf-8") as handle:
    root = json.load(handle)

if set(root) != {"format", "version", "names"} or \
        root.get("format") != "trainlog-exercise-names-v1" or \
        root.get("version") != 1 or isinstance(root.get("version"), bool):
    raise SystemExit("unsupported exercise-name catalogue")

names = root.get("names")
if not isinstance(names, list) or not names:
    raise SystemExit("names must be a non-empty list")

seen_ids = set()
for item in names:
    if set(item) != {"exercise_id", "display_name", "normalized_name",
                     "previous_display_names"}:
        raise SystemExit("invalid exercise-name entry shape")
    exercise_id = item["exercise_id"]
    display_name = item["display_name"]
    if not isinstance(exercise_id, str) or re.fullmatch(
            r"ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
            exercise_id,
    ) is None or exercise_id in seen_ids:
        raise SystemExit("duplicate or invalid exercise_id")
    if not isinstance(display_name, str) or not display_name.strip() or \
            display_name != display_name.strip():
        raise SystemExit("invalid exercise display_name")
    seen_ids.add(exercise_id)


def c(value):
    return json.dumps(value, ensure_ascii=False)


with open(output, "w", encoding="utf-8") as generated:
    generated.write('#include "trainlog/exercise_name_catalog.h"\n')
    generated.write('#include <stddef.h>\n#include <string.h>\n\n')
    generated.write("typedef struct TrainlogExerciseNameEntry {\n"
                    "  const char *exercise_id;\n"
                    "  const char *display_name;\n"
                    "} TrainlogExerciseNameEntry;\n\n")
    generated.write("static const TrainlogExerciseNameEntry entries[] = {\n")
    for item in names:
        generated.write("  {%s, %s},\n" % (
            c(item["exercise_id"]), c(item["display_name"])))
    generated.write("};\n\n")
    generated.write("const char *trainlog_exercise_name_catalog_lookup(const char *exercise_id) {\n"
                    "  size_t index;\n"
                    "  if (exercise_id == NULL) return NULL;\n"
                    "  for (index = 0U; index < sizeof(entries) / sizeof(entries[0]); ++index)\n"
                    "    if (strcmp(entries[index].exercise_id, exercise_id) == 0)\n"
                    "      return entries[index].display_name;\n"
                    "  return NULL;\n"
                    "}\n")
