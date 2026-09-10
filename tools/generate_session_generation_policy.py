#!/usr/bin/env python3
"""Generate immutable C data from the canonical session-generation policy."""

from __future__ import annotations

import sys
from pathlib import Path

from validate_session_generation_policy import validate


def c_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'"{escaped}"'


def main() -> None:
    policy_path, catalog_dir, output_path = map(Path, sys.argv[1:4])
    root = validate(policy_path, catalog_dir)
    lines = [
        "/* Generated from session-generation-policy-v1.json; do not edit. */",
        '#include "trainlog/session_generation_policy_internal.h"',
        "static const TrainlogSessionGenerationGoalPolicy goal_policies[] = {",
    ]
    for goal in ("general", "strength", "hypertrophy", "endurance"):
        row = root["goals"][goal]
        lines.append("  {%s,%d,%d,%d,%d,%d,%d,%d,%d,%d}," % (
            c_string(goal), row["sets"], row["repetitions"], row["rest_seconds"],
            *row["sets_range"], *row["repetitions_range"], *row["rest_seconds_range"]))
    lines += ["};", "static const int duration_presets_minutes[] = {" +
              ",".join(str(value) for value in root["duration"]["presets_minutes"]) + "};",
              "static const TrainlogSessionGenerationZoneExpansion zone_expansions[] = {"]
    for zone_id, expanded in root["zone_expansion"].items():
        lines.append("  {%s,%s}," % (c_string(zone_id), c_string("\n".join(expanded))))
    lines += ["};", "const TrainlogSessionGenerationPolicy trainlog_session_generation_policy_v1 = {",
              f"  {root['load']['lookback_seconds']},",
              f"  {root['exposure']['short_window_seconds']}, {root['exposure']['long_window_seconds']},",
              f"  {root['selection']['recency']['same_exercise_window_seconds']}, {root['selection']['recency']['same_pattern_window_seconds']},",
              f"  {root['selection']['max_exercises']}, {root['duration']['custom_min_minutes']}, {root['duration']['custom_max_minutes']},",
              "  duration_presets_minutes, sizeof(duration_presets_minutes)/sizeof(duration_presets_minutes[0]),",
              f"  {root['duration']['preparation_seconds']}, {root['duration']['setup_and_transition_seconds_per_exercise']}, {root['duration']['estimated_seconds_per_repetition']},",
              f"  {root['exposure']['recent_primary_sets_24h_threshold']}, {root['exposure']['recent_secondary_sets_24h_threshold']},",
              f"  {root['exposure']['repeated_primary_sets_72h_threshold']}, {root['exposure']['repeated_secondary_sets_72h_threshold']},",
              "  {" + ",".join(str(root["selection"]["score"][key]) for key in (
                  "requested_primary_zone", "requested_secondary_zone_only", "new_primary_zone", "new_pattern",
                  "qualifying_working_load_history", "preferred_exercise", "recent_same_exercise", "recent_same_pattern",
                  "recent_primary_threshold_on_candidate_primary", "recent_secondary_threshold_on_candidate_primary",
                  "repeated_primary_threshold_on_candidate_primary", "repeated_secondary_threshold_on_candidate_primary",
                  "any_exposure_flag_on_candidate_secondary_zones")) + "},",
              "  " + ",".join(c_string("\n".join(root["selection"][key])) for key in (
                  "upper_push_patterns", "upper_pull_patterns", "lower_extension_patterns", "lower_flexion_patterns")) + ",",
              "  zone_expansions, sizeof(zone_expansions)/sizeof(zone_expansions[0]),",
              "  goal_policies, sizeof(goal_policies)/sizeof(goal_policies[0])", "};"]
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
